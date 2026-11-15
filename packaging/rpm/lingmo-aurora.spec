Name:           lingmo-aurora
Version:        5.0.41
Release:        1.lingmo5u1%{?dist}
Summary:        Lingmo Aurora Wayland compositor and desktop shell
License:        GPL-3.0-or-later AND BSD-3-Clause AND MIT
URL:            https://github.com/LingmoOS/aurora
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(fontconfig)
BuildRequires:  pkgconfig(freetype2)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(glesv2)
BuildRequires:  pkgconfig(harfbuzz)
BuildRequires:  pkgconfig(icu-i18n)
BuildRequires:  pkgconfig(icu-uc)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(libinput)
BuildRequires:  pkgconfig(libseat)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  wayland-devel >= 1.23
BuildRequires:  wayland-protocols-devel >= 1.39
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(xcb-composite)
BuildRequires:  pkgconfig(xcb-shape)
BuildRequires:  pkgconfig(xcb-sync)
BuildRequires:  pkgconfig(xcb-xfixes)
BuildRequires:  pkgconfig(libspa-0.2)
BuildRequires:  pkgconfig(libsystemd)
BuildRequires:  pkgconfig(libpipewire-0.3)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  lingmo-sdk-devel >= 5.0.7
BuildRequires:  lingmo-framework-devel >= 5.0.6
BuildRequires:  pam-devel

Requires:       lingmo-sdk >= 5.0.7
Requires:       lingmo-framework >= 5.0.6
Recommends:     pipewire
Recommends:     xwayland

%description
Lingmo Aurora is the Lingmo desktop's Wayland compositor and shell. It
provides display and input management, window management, server-side
decorations, the status bar, dock, launcher, notifications and screen capture.

%prep
%autosetup -p1

%build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_INSTALL_LIBEXECDIR=%{_libexecdir} \
    -DBUILD_TESTING=OFF \
    -DLINGMO_ENABLE_XWAYLAND=ON
cmake --build build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license LICENSE LICENSE.chromium LICENSE.sommelier
%{_bindir}/lingmo-aurora
%{_bindir}/lingmo-displayctl
%{_bindir}/lingmo-screenshot
%{_bindir}/lingmo-screen-keyboard
%{_libexecdir}/lingmo-auth-helper
%config(noreplace) %{_sysconfdir}/pam.d/lingmo-auth
%{_datadir}/applications/lingmo-screenshot.desktop
%{_datadir}/applications/lingmo-screen-keyboard.desktop
%{_datadir}/dbus-1/interfaces/*
%{_datadir}/lingmo-aurora/
%{_includedir}/lingmo-aurora/

%changelog
* Mon Nov 16 2026 Lingmo OS Team <team@lingmo.org> - 5.0.41-1.lingmo5u1
- Track the validated SDK and framework RPM package versions.
