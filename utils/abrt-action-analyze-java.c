/*
    Copyright (C) 2014  Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <satyr/location.h>
#include <satyr/thread.h>
#include <satyr/java/stacktrace.h>
#include <satyr/java/thread.h>
#include <satyr/java/frame.h>

#include <rpm/rpmts.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmdb.h>

#include <abrt/libabrt.h>
#include <stdlib.h>

/* 4 = 1 exception + 3 methods */
#define FRAMES_FOR_DUPHASH 4

typedef struct
{
    const char *name;
    char *data;
    int nofree;
} analysis_result_t;

static char *
backtrace_from_dump_dir(const char *dir_name)
{
    struct dump_dir *dd = dd_opendir(dir_name, DD_OPEN_READONLY);
    if (NULL == dd)
    {
        return NULL;
    }

    /* Read backtrace */
    /* Prints an error message if the file cannot be loaded */
    char *backtrace_str = dd_load_text_ext(dd, FILENAME_BACKTRACE,
                                           DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);

    dd_close(dd);

    return backtrace_str;
}

static void
write_results_to_dump_dir(const char *dir_name,
        const analysis_result_t *res_begin, const analysis_result_t *res_end)
{
    struct dump_dir *dd = dd_opendir(dir_name, /*Open for writing*/0);
    if (NULL != dd)
    {
        const analysis_result_t *res = res_begin;

        for ( ; res != res_end; ++res)
            dd_save_text(dd, res->name, res->data);

        dd_close(dd);
    }
}

static void
write_to_fd(int fdout, const char *message)
{
    libreport_full_write(fdout, message, strlen(message));
    libreport_full_write(fdout, "\n", 1);
}


static void
write_results_to_fd(int fdout,
        const analysis_result_t *res_begin, const analysis_result_t *res_end)
{
    const analysis_result_t *res = res_begin;

    for ( ; res != res_end; ++res)
    {
        write_to_fd(fdout, res->name);
        write_to_fd(fdout, res->data);
    }
}

static void
write_results_to_file(const analysis_result_t *res_begin, const analysis_result_t *res_end)
{
    const analysis_result_t *res = res_begin;

    for ( ; res != res_end; ++res)
    {
        int fdout = open(res->name,
                O_WRONLY | O_TRUNC | O_CREAT | O_NOFOLLOW,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );

        if (0 > fdout)
        {
            perror_msg("Can't open file '%s' for writing", res->name);
            continue;
        }

        write_to_fd(fdout, res->data);
        close(fdout);
    }
}

static char *
backtrace_from_fd(int fdin)
{
    return libreport_xmalloc_read(fdin, /*no size limit*/NULL);
}

static char *
backtrace_from_file(const char *file_name)
{
    return libreport_xmalloc_xopen_read_close(file_name, /*no size limit*/NULL);
}

static char *
work_out_list_of_remote_urls(struct sr_java_stacktrace *stacktrace)
{
    GString *remote_files_csv = g_string_new(NULL);
    struct sr_java_thread *thread = stacktrace->threads;
    while (NULL != thread)
    {
        struct sr_java_frame *frame = thread->frames;
        while (NULL != frame)
        {
            if (NULL != frame->class_path && g_str_has_prefix(frame->class_path, "file://") == 0)
            {
                struct stat buf;
                if (stat(frame->class_path, &buf) && errno == ENOENT)
                {
                    if (strstr(remote_files_csv->str, frame->class_path) == NULL)
                    {
                        log_debug("Adding a new path to the list of remote paths: '%s'", frame->class_path);
                        g_string_append_printf(remote_files_csv, "%s%s",
                                remote_files_csv->str[0] != '\0' ? ", " : "",
                                frame->class_path);
                    }
                    else
                        log_debug("The list of remote paths already contains path: '%s'", frame->class_path);
                }
                else
                    log_debug("Class path exists or is malformed: '%s'", frame->class_path);
            }
            frame = frame->next;
        }
        thread = thread->next;
    }

    if (remote_files_csv->str[0] != '\0')
    {
        return g_string_free(remote_files_csv, FALSE);
    }

    g_string_free(remote_files_csv, TRUE);
    return NULL;
}

static int
contains_unkown_class(struct sr_java_stacktrace *stacktrace)
{
    struct sr_java_thread *thread = stacktrace->threads;
    while (NULL != thread)
    {
        struct sr_java_frame *frame = thread->frames;
        while (NULL != frame)
        {
            if (NULL == frame->class_path
                && !frame->is_native && !frame->is_exception)
            {
                return 1;
            }
            frame = frame->next;
        }
        thread = thread->next;
    }

    return 0;
}

static int
contains_unpackaged_path(struct sr_java_stacktrace *stacktrace)
{
    int rc = rpmReadConfigFiles((const char *) NULL, (const char *) NULL);
    if (rc)
    {
        error_msg("Could not read RPM config files");
        return 0;
    }

    int retval = 0;
    rpmts ts = rpmtsCreate();
    rc = rpmtsOpenDB(ts, O_RDONLY);
    if (rc)
    {
        error_msg("Could not open RPM database for reading");
        goto contains_unpackaged_path_finish;
    }

    struct sr_java_thread *thread = stacktrace->threads;
    while (0 == retval && NULL != thread)
    {
        struct sr_java_frame *frame = thread->frames;
        while (0 == retval && NULL != frame)
        {
            if (NULL != frame->class_path)
            {
                rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES,
                        frame->class_path, /*length: NULL terminated*/ 0);
                Header header = rpmdbNextIterator(iter);
                rpmdbFreeIterator(iter);

                if (NULL == header)
                {
                    retval = 1;
                    break;
                }
            }
            frame = frame->next;
        }
        thread = thread->next;
    }

contains_unpackaged_path_finish:
    /* Closes the database as well */
    rpmtsFree(ts);

    rpmFreeRpmrc();
    rpmFreeCrypto();
    rpmFreeMacros(NULL);

    return retval;
}

int main(int argc, char *argv[])
{
#if ENABLE_NLS
    /* I18n */
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *dump_dir_name = NULL;
    const char *backtrace_file = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [[-d DIR] | [-f FILE]] [-o]\n"
        "\n"
        "Analyzes Java backtrace, generates duplication hash and creates\n"
        "not-reportable file for bracktraces whose frames have remote files in their\n"
        "class path\n"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_f = 1 << 2,
        OPT_r = 1 << 3,
        OPT_o = 1 << 4,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&libreport_g_verbose),
        OPT_STRING('d', "dumpdir", &dump_dir_name, "DIR", _("Problem directory")),
        OPT_STRING('f', "backtrace", &backtrace_file, "FILE", _("Path to backtrace")),
        OPT_BOOL('r',   "norpmverify", NULL, _("Do not verify that all paths belongs to an rpm package")),
        OPT_BOOL('o', "stdout", NULL, _("Print results on standard output")),
        { 0 }
    };
    program_options[ARRAY_SIZE(program_options) - 1].type = OPTION_END;

    unsigned opts = libreport_parse_opts(argc, argv, program_options, program_usage_string);

    libreport_export_abrt_envvars(0);

    if (NULL != dump_dir_name && NULL != backtrace_file)
        error_msg_and_die("You need to pass either DIR or FILE");

    int retval = 1;
    char *backtrace_str = NULL;
    if (NULL != dump_dir_name)
    {
        backtrace_str = backtrace_from_dump_dir(dump_dir_name);
    }
    else if (NULL != backtrace_file)
    {
        backtrace_str = backtrace_from_file(backtrace_file);
    }
    else
    {
        backtrace_str = backtrace_from_fd(STDIN_FILENO);
    }

    if (NULL == backtrace_str)
        goto finish;

    struct sr_location location;
    sr_location_init(&location);
    const char *backtrace_str_ptr = backtrace_str;
    struct sr_java_stacktrace *stacktrace = sr_java_stacktrace_parse(&backtrace_str_ptr, &location);
    free(backtrace_str);

    if (NULL == stacktrace)
    {
        error_msg("Could not parse the stack trace");
        goto finish;
    }

    analysis_result_t results[3] = { { 0 } };
    analysis_result_t *results_iter = results;

    char *remote_files_csv = work_out_list_of_remote_urls(stacktrace);

    char *hash_str = NULL;
    struct sr_thread *crash_thread = (struct sr_thread *)stacktrace->threads;
    if (libreport_g_verbose >= 3)
    {
        hash_str = sr_thread_get_duphash(crash_thread, FRAMES_FOR_DUPHASH,
                /*noprefix*/NULL, SR_DUPHASH_NOHASH);
        log_warning("Generating duphash from string: '%s'", hash_str);
        free(hash_str);
    }

    hash_str = sr_thread_get_duphash(crash_thread, FRAMES_FOR_DUPHASH,
            /*noprefix*/NULL, SR_DUPHASH_NORMAL);

    /* DUPHASH is used for searching for duplicates in Bugzilla */
    results_iter->name = FILENAME_DUPHASH;
    results_iter->data = hash_str;
    ++results_iter;

    /* UUID is used for local deduplication */
    results_iter->name = FILENAME_UUID;
    results_iter->data = hash_str;
    results_iter->nofree = 1;
    ++results_iter;


    if (NULL != remote_files_csv)
    {
        results_iter->name = FILENAME_NOT_REPORTABLE;
        results_iter->data = g_strdup_printf(
        _("This problem can be caused by a 3rd party code from the "\
        "jar/class at %s. In order to provide valuable problem " \
        "reports, ABRT will not allow you to submit this problem. If you " \
        "still want to participate in solving this problem, please contact " \
        "the developers directly."), remote_files_csv);
        ++results_iter;
        free(remote_files_csv);
    }
    else if (contains_unkown_class(stacktrace))
    {
        results_iter->name = FILENAME_NOT_REPORTABLE;
        results_iter->data = g_strdup_printf(
        _("This problem is not reportable because of the low quality of stack trace. " \
        "The stack trace contains lines having unknown source files and unknown " \
        "classes. Such stack traces are usually produced by a non-official code. " \
        "If this is your case, please contact the provider of the failing application. " \
        "Otherwise contact the package maintainers directly via e-mail." )
        );
        ++results_iter;
    }
    else if ((opts & OPT_r) == 0 && contains_unpackaged_path(stacktrace))
    {
        results_iter->name = FILENAME_NOT_REPORTABLE;
        results_iter->data = g_strdup_printf(
        _("This problem has been caused by a proprietary code which has not been "
        "provided any official package. Please contact the provider of the "
        "proprietary code.")
        );
        ++results_iter;
    }

    sr_java_stacktrace_free(stacktrace);

    if (opts & OPT_o)
    {
        write_results_to_fd(STDOUT_FILENO, results, results_iter);
    }
    else if (NULL != dump_dir_name)
    {
        write_results_to_dump_dir(dump_dir_name, results, results_iter);
    }
    else
    {   /* Just write it to the current working directory */
        write_results_to_file(results, results_iter);
    }

    const analysis_result_t *res = results;
    for (; res != results_iter; ++res)
    {
        if (!res->nofree)
        {
            free(res->data);
        }
    }

    retval = 0;
finish:

    return retval;
}
