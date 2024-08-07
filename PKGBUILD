# Maintainer: Kirill Zhumarin <kirill.zhumarin@gmail.com>
pkgname=ptr89
pkgver=v1.0.2
pkgrel=1
pkgdesc='Yet another binary pattern finder.'
arch=(any)
url='https://github.com/siemens-mobile-hacks/ptr89'
license=(MIT)
depends=()
makedepends=()
source=($pkgname-$pkgver.tar.gz)
sha256sums=('SKIP')

prepare() {
	cd $pkgname-$pkgver
	git submodule init
	git submodule update
}

build() {
	cmake -B build -S $pkgname-$pkgver
	cmake --build build
}

package() {
	cd $pkgname-$pkgver
	make install
}

pkgver() {
	cd $pkgname-$pkgver
	git describe --long --tags --abbrev=7 | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}
