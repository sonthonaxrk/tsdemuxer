#!/bin/bash

mkdir -p foo/data/opt/bin
mkdir -p foo/data/opt/share/pshare/www
mkdir -p foo/data/opt/share/pshare/playlists

cp pshare foo/data/opt/bin/
cp -r www/* foo/data/opt/share/pshare/www/
cp -r playlists/* foo/data/opt/share/pshare/playlists/

tar -C ipkg -czf foo/control.tar.gz ./control
tar -C foo/data -czf foo/data.tar.gz ./opt
echo "2.0" > foo/debian-binary

rm -rf foo/data

tar -C foo -cz ./debian-binary ./data.tar.gz ./control.tar.gz > pshare_0.0.2_mipsel.ipk

rm -rf foo/
