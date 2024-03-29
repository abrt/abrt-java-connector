cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)

# Beware, PROJECT_VERSION will be overridden by information from git
project(
    abrt-java-connector
    VERSION 1.3.2
    LANGUAGES C)
enable_testing()

message("Configuring ${PROJECT_NAME}-${PROJECT_VERSION} on ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE git_result
    OUTPUT_VARIABLE git_short_commit
    ERROR_QUIET)

if(git_result EQUAL 0)
    execute_process(
        COMMAND git describe --tags --match "[0-9]*" HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        RESULT_VARIABLE git_result
        OUTPUT_VARIABLE git_tag
        ERROR_QUIET)

    if(git_result EQUAL 0)
        string(REGEX REPLACE "(\r?\n)+$" "" git_tag "${git_tag}")
        string(REGEX REPLACE "-[0-9]+-g" ".g" git_tag "${git_tag}")
        set(PROJECT_VERSION ${git_tag})
    endif()

    execute_process(
        COMMAND git log -1 --format=%H
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        RESULT_VARIABLE git_result
        OUTPUT_VARIABLE git_commit)
    string(REGEX REPLACE "(\r?\n)+$" "" git_commit "${git_commit}")
endif()

set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})

if(NOT BIN_INSTALL_DIR)
    set(BIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()

if(NOT JNIAGENTLIB_INSTALL_DIR)
    set(JNIAGENTLIB_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_PROJECT_NAME})
endif()

if(NOT SYSCONF_INSTALL_DIR)
    set(SYSCONF_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/etc)
endif()

if(NOT SHARE_INSTALL_PREFIX)
    set(SHARE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/usr/share)
endif()

if(NOT MAN_INSTALL_DIR)
    set(MAN_INSTALL_DIR ${SHARE_INSTALL_PREFIX}/man)
endif()

if(NOT LOCALE_INSTALL_DIR)
    set(LOCALE_INSTALL_DIR ${SHARE_INSTALL_PREFIX}/locale)
endif()


add_custom_target(
    dist
    COMMAND git archive --prefix=${CMAKE_PROJECT_NAME}-${git_tag}/ HEAD | gzip > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

include(FindPkgConfig)
pkg_check_modules(PC_SATYR REQUIRED satyr)
pkg_check_modules(PC_LIBREPORT REQUIRED libreport>=2.14.0)
pkg_check_modules(PC_ABRT REQUIRED abrt>=2.14.1)

add_definitions(-D_GNU_SOURCE)
set(AJC_ENABLE_NLS true)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=c99")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -DVERBOSE")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Werror -DSILENT")

subdirs(src etc test utils po)
