# Maintainer: Kirill Zhumarin <kirill.zhumarin@gmail.com>
pkgname=ptr89
pkgver=v1.0.2
pkgrel=1
pkgdesc='Yet another binary pattern finder.'
arch=(any)
url='https://github.com/siemens-mobile-hacks/ptr89'
license=(MIT)
depends=()
makedepends=(cmake)
source=($pkgname-$pkgver.tar.gz)
sha256sums=('SKIP')

build() {
	cmake -B build -S $pkgname-$pkgver
	cmake --build build
}

package() {
	cd $pkgname-$pkgver
	make install
}
