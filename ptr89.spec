Name: ptr89
Version: 1.0
Release: %autorelease
Summary: Yet another binary pattern finder.
License: MIT
Source0: ptr89-%{version}.tar.xz
URL: https://github.com/siemens-mobile-hacks/ptr89
BuildRequires: gcc
BuildRequires: git
BuildRequires: cmake

%description
Yet another binary pattern finder.

%build
%cmake
%cmake_build

%install
%cmake_install

%check
%ctest
