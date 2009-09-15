%global	pidgin_version 2.0.0

Name:		pidgin-musictracker
Version:	0.4.16
Release:	1%{?dist}
Summary:	Musictracker plugin for Pidgin

Group:		Applications/Internet
License:	GPLv2+
URL:		http://code.google.com/p/pidgin-musictracker/
Source0:	http://%{name}.googlecode.com/files/%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	gtk2-devel
BuildRequires:	dbus-glib-devel
BuildRequires:	pcre-devel
BuildRequires:	pidgin-devel >= %{pidgin_version}
BuildRequires:	gettext-devel

Requires:	pidgin >= %{pidgin_version}

%description
Musictracker is a plugin for Pidgin which displays the media currently
playing in the status message for any protocol Pidgin supports custom
statuses on.

%prep
%setup -q


%build
%configure --disable-werror --disable-static
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=%{buildroot} INSTALL="install -p" install
rm -f %{buildroot}/%{_libdir}/pidgin/musictracker.la
%find_lang musictracker

%clean
rm -rf $RPM_BUILD_ROOT

%files -f musictracker.lang
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README THANKS
%{_libdir}/pidgin/musictracker.so

%changelog
* Wed Apr  9 2009 Jan Klepek <jan.klepek@hp.com> 0.4.16-1
- combined spec file from all below people and builded package for 0.4.16 version, created static subpackage
 
* Wed Mar  4 2009 Jon TURNEY <jon.turney@dronecode.org.uk>
- Cut-n-shut from .spec files cotributed by Jon Hermansen <jon.hermansen@gmail.com>, Julio Cezar <watchman777@gmail.com> and Mattia Verga <mattia.verga@tiscali.it>

* Wed Mar  4 2009 Mattia Verga <mattia.verga@tiscali.it> 0.4.15-2.fc10
- Added BuildRequires section and corrected the Requires section to Pidgin version >= 2.0. Added THANKS file to documentation

* Tue Mar  3 2009 Mattia Verga <mattia.verga@tiscali.it> 0.4.15-1.fc10
- Upgrade to version 0.4.15. See full changelog at http://code.google.com/p/pidgin-musictracker/wiki/ChangeLog0point4

* Mon Feb  2 2009 Mattia Verga <mattia.verga@tiscali.it> 0.4.14-2.fc10
- Added requires

* Sun Feb  1 2009 Mattia Verga <mattia.verga@tiscali.it> 0.4.14-1.fc10
- Initial package
