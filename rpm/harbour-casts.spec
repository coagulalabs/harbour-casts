Name:       harbour-casts

Summary:    Native podcast player for Sailfish OS
Version:    1.1.3
Release:    1
License:    BSD-3-Clause
URL:        https://github.com/coagulalabs/harbour-casts
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  desktop-file-utils

%description
Casts is a native podcast player for Sailfish OS. Subscribe via RSS URL
or OPML import, stream or download episodes, read show notes, manage a
playback queue, adjust speed, set a sleep timer, and resume where you
left off.


%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 

%make_build


%install
%qmake5_install


desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

install -D -m 0644 rpm/harbour-casts.profile \
  %{buildroot}%{_sysconfdir}/sailjail/applications/harbour-casts.profile

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_sysconfdir}/sailjail/applications/harbour-casts.profile
%{_datadir}/icons/hicolor/*/apps/%{name}.png
