%global snapshot 0
%global commit bef7e39ce5fdc4a8a620d56be186d4463ed761a8
%global shortcommit %(c=%{commit}; echo ${c:0:7})

Name:          abrt-java-connector
Version:       1.2.0
Release:       1%{?dist}
Summary:       JNI Agent library converting Java exceptions to ABRT problems

Group:         System Environment/Libraries
License:       GPLv2+
URL:           https://github.com/abrt/abrt-java-connector
%if 0%{?snapshot}
Source0:       %{url}/archive/%{commit}/%{name}-%{version}-%{shortcommit}.tar.gz
%else
Source0:       %{url}/archive/%{version}/%{name}-%{version}.tar.gz
%endif

BuildRequires: pkgconfig(abrt) >= 2.14.1
BuildRequires: check-devel
BuildRequires: cmake
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: gettext
# Tests have been redone to work under Java 11, but they are not backwards-compatible.
BuildRequires: java-11-devel
BuildRequires: make
BuildRequires: pkgconfig(libreport) >= 2.14.0
BuildRequires: rpm-devel
BuildRequires: satyr-devel
BuildRequires: systemd-devel

Requires:      abrt

%description
JNI library providing an agent capable to process both caught and uncaught
exceptions and transform them to ABRT problems.

%package container
Summary: JNI Agent library converting Java exceptions to ABRT problems (minimal version)
Requires: container-exception-logger
conflicts: %{name}

%description container
JNI library providing an agent capable to process both caught and uncaught
exceptions and transform them to ABRT problems

This package contains only minimal set of files needed for container exception
logging.

%prep
%if 0%{?snapshot}
%autosetup -n %{name}-%{commit}
%else
%autosetup
%endif


%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build


%check
%ctest


%install
%cmake_install


%files
%doc README AUTHORS
%license LICENSE
%config(noreplace) %{_sysconfdir}/libreport/plugins/bugzilla_format_java.conf
%config(noreplace) %{_sysconfdir}/libreport/plugins/bugzilla_formatdup_java.conf
%config(noreplace) %{_sysconfdir}/libreport/events.d/java_event.conf
%config(noreplace) %{_sysconfdir}/abrt/plugins/java.conf
%{_bindir}/abrt-action-analyze-java
%{_mandir}/man1/abrt-action-analyze-java.1*
%{_mandir}/man5/java_event.conf.5*
%{_mandir}/man5/bugzilla_format_java.conf.5*
%{_mandir}/man5/bugzilla_formatdup_java.conf.5*
%{_datadir}/abrt/conf.d/plugins/java.conf

# Applications may use a single subdirectory under/usr/lib.
# http://www.pathname.com/fhs/pub/fhs-2.3.html#PURPOSE22
#
# Java does not support multilib.
# https://fedorahosted.org/fesco/ticket/961
%{_prefix}/lib/abrt-java-connector

%files container
%doc README AUTHORS
%license LICENSE
# Applications may use a single subdirectory under/usr/lib.
# http://www.pathname.com/fhs/pub/fhs-2.3.html#PURPOSE22
#
# Java does not support multilib.
# https://fedorahosted.org/fesco/ticket/961
%{_prefix}/lib/abrt-java-connector


%changelog
* Tue Aug 25 2020 Michal Fabik <mfabik@redhat.com> - 1.2.0-1
- Replace various libreport helper functions with GLib ones
- Require java-11-devel for builds
- Add indices to out-of-bounds exception output files
- Drop unused test output files
- Adapt to different exception messages in Java 11
- Various minor fixes

* Tue May 05 2020 Michal Fabik <mfabik@redhat.com> - 1.1.5-1
- Add namespace to libreport function and global names

* Fri Feb 07 2020 Ernestas Kulik <ekulik@redhat.com> - 1.1.4-1
- Fix build failure with GCC 10

* Fri Feb 07 2020 Ernestas Kulik <ekulik@redhat.com> - 1.1.3-1
- Fix stack traces not being logged in journald

* Tue Jun 11 2019 Martin Kutlak <mkutlak@redhat.com> - 1.1.2-1
- Fix gcc string truncation warning for strncpy
- Replace legacy backticked with $() notation

* Wed May 16 2018 Matej Habrnal <mhabrnal@redhat.com> - 1.1.1-1
- Add possibility report reports problems to CEL
- Upate test results
- Change log() to log_warning()
- Correct includes for ABRT
- Make the dependency on systemd optional

* Wed Oct 29 2014 Jakub Filak <jfilak@redhat.com> - 1.1.0-1
- Support java-1.8-openjdk
- Install the library to /usr/lib/abrt-java-connector on all arches

* Fri Apr 4 2014 Jakub Filak <jfilak@redhat.com> - 1.0.10-1
- Temporarily ignore failures of reporter-ureport until ABRT start using FAF2
- Prevent users from reporting low quality stack traces

* Tue Mar 18 2014 Jakub Filak <jfilak@redhat.com> - 1.0.9-1
- Make the agent configurable via a configuration file
- Include custom debug info in bug reports
- Make the detection of 'executable' working with JAR files

* Wed Jan 22 2014 Jakub Filak <jfilak@redhat.com> - 1.0.8-1
- Do not report exceptions caught in a native method
- Mark stack traces with 3rd party classes as not-reportable
- Calculate 'duphash' & 'uuid' in satyr
- Use the main class URL for 'executable'
- Do not ship own reporting workflow definitions
- Code optimizations

* Fri Jan 10 2014 Jakub Filak <jfilak@redhat.com> - 1.0.7-1
- Use the last frame class path for executable
- Gracefully handle JVMTI errors
- Add an abstract to README
- Add support for journald and syslog
- Make log output disabled by default
- Add support for changing log directory
- Fix a race condition causing a crash of JVM

* Tue Oct 01 2013 Jakub Filak <jfilak@redhat.com> - 1.0.6-1
- Fix a deadlock in GC start callback
- Disable experimental features in production releases

* Tue Jul 30 2013 Jakub Filak <jfilak@redhat.com> - 1.0.5-1
- Provide a proper configuration for libreport

* Thu Jul 18 2013 Jakub Filak <jfilak@redhat.com> - 1.0.4-1
- Stop creating an empty log file

* Tue Jul 16 2013 Jakub Filak <jfilak@redhat.com> - 1.0.3-1
- Fix tests on arm

* Tue Jul 09 2013 Jakub Filak <jfilak@redhat.com> - 1.0.2-1
- Do not crash on empty command line options

* Mon Jul 08 2013 Jakub Filak <jfilak@redhat.com> - 1.0.1-1
- Fix tests on ppc and s390 on both 32 and 64 bit

* Thu Jun 27 2013 Jakub Filak <jfilak@redhat.com> - 1.0.0-1
- Publicly releasable version

* Mon Jun 03 2013 Jakub Filak <jfilak@redhat.com> - 0.1.2-1
- Start versioning library
- Drop build dependency on abrt-devel

* Mon Jun 03 2013 Jakub Filak <jfilak@redhat.com> - 0.1.1-2
- Provide ABRT configuration

* Mon Jun 03 2013 Jakub Filak <jfilak@redhat.com> - 0.1.1-1
- New release

* Fri May 31 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-3
- Build with the library name same as the package name

* Fri May 31 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-2
- Build with ABRT enabled

* Fri May 31 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-1
- Initial version
