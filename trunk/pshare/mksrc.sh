#!/bin/bash

mkdir -p src/
cp -r ipkg/ src/
cp -r playlists src/
cp -r www src/
cp ipkg-build.sh src/
cp mkipkg.sh src/
cp LICENSE src/
cp README src/
cp Makefile src/
cp *.cpp src/
cp *.h src/

tar c src | gzip -c > pshare_0.0.1_src.tar.gz
rm -rf src/