Name:         BlackBird
Version:      0.0.1
Release:      1.el5
Summary:      TCP server skeleton.
Group:        Applications/Internet
License:      GPL3
URL:          https://github.com/h0tbird/BlackBird
Source0:      %{name}-%{version}.tar.gz
Packager:     Marc Villacorta Morera <marc.villacorta@gmail.com>
BuildRoot:    %{_tmppath}/%{name}-%{version}-%{release}

%description
High-performance, high-scalable, epoll-based, edge-triggered,
non-blocking, pre-threaded and multiplexed generic TCP server skeleton.

%prep
%setup -q

%build
make

%install
make install basedir=$RPM_BUILD_ROOT

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root)
%dir /etc/BlackBird
/usr/sbin/bb
