#! /bin/sh

set -e

if [ "${1}" = "" ]
then
	echo " * Update VERSIONSTR in global.h"
	echo " * Update Changelog (822-date)"
	echo " * Update debian/changelog"
	echo " * Update the date in mooproxy.1"
	echo
	echo " * Do ./release.sh <mooproxy version>"
	echo " * Change dist to <version>-luon and do release again"
	exit
fi

VERSION="${1}"

echo "Building distributable files for mooproxy ${VERSION}"
echo

rm -rf build/
mkdir -p build/mooproxy-${VERSION}/
cp * build/mooproxy-${VERSION}/ || true
rm build/mooproxy-${VERSION}/release.sh

cd build/
tar cvzf mooproxy_${VERSION}.orig.tar.gz mooproxy-${VERSION}/
cp -r ../debian mooproxy-${VERSION}/
cd mooproxy-${VERSION}/
dpkg-buildpackage -us -uc -rfakeroot
cd ..

tar cvzf mooproxy-debsrc-${VERSION}.tar.gz \
  mooproxy_${VERSION}*.diff.gz \
  mooproxy_${VERSION}*.dsc \
  mooproxy_${VERSION}.orig.tar.gz

cp -f mooproxy_${VERSION}.orig.tar.gz mooproxy-${VERSION}.tar.gz

cd ..
