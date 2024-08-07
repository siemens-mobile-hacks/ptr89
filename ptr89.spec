Name: ptr89
Version: 1.0
Release: 0
Summary: Yet another binary pattern finder.
License: MIT
Source0: %{name}-%version.tar
URL: https://github.com/siemens-mobile-hacks/ptr89
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: git
BuildRequires: cmake
BuildRequires: libc-devel

%description
Yet another binary pattern finder.

%prep
%setup -q

%build
%cmake
%cmake_build

%install
%cmake_install

%check
%ctest
