# Maintainer: instellate
pkgname='pacgrade'
pkgver='1.0.0'
pkgrel=1
pkgdesc='A application that tells you if you have out of date packages'
arch=('x86_64') # Might work on aarm64, not tried
url='https://github.com/Instellate/pacgrade'
license=('GPL-3.0-only')
depends=('pacman' 'pacutils' 'libnotify' 'curl' 'gcc-libs')
makedepends=('gcc' 'cmake' 'make' 'git' 'nlohmann-json')
source=('git+https://github.com/Instellate/pacgrade.git#commit=17a63d42a31bb27071d2d125480ad4d8bf758b2b')
sha256sums=('a3c3c28d3b7806c2cc8ddee8ca6455429917dd65523e99bd7cbd5af4a78322a9')

prepare() {
    mkdir "${srcdir}/build"
    cmake -B "${srcdir}/build" -S "${srcdir}/pacgrade" -DCMAKE_BUILD_TYPE=Release
}

build() {
    cmake --build "${srcdir}/build"
}

package() {
    cmake --install "${srcdir}/build" --prefix "${pkgdir}/usr"
    install -Dm644 "${srcdir}/pacgrade/LICENSE" "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}

