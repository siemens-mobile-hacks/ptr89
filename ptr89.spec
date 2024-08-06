Name:     ptr89
Version:    {{{ git_dir_version }}}
Release:  %autorelease
Summary:  Yet another binary pattern finder.
License:  MIT
URL:      https://github.com/siemens-mobile-hacks/ptr89
%undefine _disable_source_fetch
Source:   {{{ git_dir_pack }}}
BuildRequires: gcc
BuildRequires: git
BuildRequires: g++
BuildRequires: cmake

%description
Yet another binary pattern finder.

%prep
{{{ git_dir_setup_macro }}}

%build
%cmake
%cmake_build

%install
%cmake_install

%check
%ctest
