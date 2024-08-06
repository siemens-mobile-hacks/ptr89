Name:     ptr89
Version:    {{{ git_dir_version }}}
Release:  %autorelease
Summary:  Yet another binary pattern finder.
License:  MIT
URL:      https://github.com/siemens-mobile-hacks/ptr89
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
