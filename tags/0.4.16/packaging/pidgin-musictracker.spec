%global	pidgin_version 2.0.0
%define debug_package %{nil}

Name:           pidgin-musictracker
Version:        0.4.15
Release:        1%{?dist}
Summary:        Musictracker plugin for Pidgin

Group:          Applications/Internet
License:        GPLv2+
URL:            http://code.google.com/p/pidgin-musictracker/
Source0:        http://%{name}.googlecode.com/files/%{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  gtk2-devel
BuildRequires:  dbus-glib-devel dbus-devel
BuildRequires:  pcre-devel
BuildRequires:  pidgin-devel >= %{pidgin_version}
BuildRequires:  gettext-devel

Requires:	dbus
Requires:	pcre
Requires:       pidgin >= %{pidgin_version}

%description
Musictracker is a plugin for Pidgin which displays the media currently
playing in the status message for any protocol Pidgin supports custom
statuses on.

%prep
%setup -q


%build
%configure --disable-werror
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=%{buildroot} INSTALL="install -p" install


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README THANKS
%{_libdir}/pidgin/musictracker.a
%{_libdir}/pidgin/musictracker.la
%{_libdir}/pidgin/musictracker.so
%{_datadir}/locale/en@quot/LC_MESSAGES/musictracker.*
%{_datadir}/locale/en@boldquot/LC_MESSAGES/musictracker.*
%lang(cs) %{_datadir}/locale/cs/LC_MESSAGES/musictracker.*
%lang(de) %{_datadir}/locale/de/LC_MESSAGES/musictracker.*
%lang(en_GB) %{_datadir}/locale/en_GB/LC_MESSAGES/musictracker.*
%lang(es) %{_datadir}/locale/es/LC_MESSAGES/musictracker.*
%lang(fi) %{_datadir}/locale/fi/LC_MESSAGES/musictracker.*
%lang(fr) %{_datadir}/locale/fr/LC_MESSAGES/musictracker.*
%lang(he) %{_datadir}/locale/he/LC_MESSAGES/musictracker.*
%lang(it) %{_datadir}/locale/it/LC_MESSAGES/musictracker.*
%lang(nds) %{_datadir}/locale/nds/LC_MESSAGES/musictracker.*
%lang(pl) %{_datadir}/locale/pl/LC_MESSAGES/musictracker.*
%lang(pt) %{_datadir}/locale/pt/LC_MESSAGES/musictracker.*
%lang(pt_BR) %{_datadir}/locale/pt_BR/LC_MESSAGES/musictracker.*
%lang(sl) %{_datadir}/locale/sl/LC_MESSAGES/musictracker.*


%changelog
* Wed Mar  4 2009 Jon TURNEY <jon.turney@dronecode.org.uk>
- Cut-n-shut from .spec files cotributed by Jon Hermansen <jon.hermansen@gmail.com>, Julio Cezar <watchman777@gmail.com> and Mattia Verga <mattia.verga@tiscali.it>
