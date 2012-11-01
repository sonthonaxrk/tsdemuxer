# Copyright (C) 2012 Igor Drach
# leaigor@gmail.com

DESCRIPTION = "xupnpd - eXtensible UPnP agent"
LICENSE = "GPL"
HOMEPAGE = "http://xupnpd.org"

DEPENDS += "lua5.1"
PR = "r7"
SRCREV=325
SRC_URI = "svn://tsdemuxer.googlecode.com/svn/trunk/xupnpd;proto=http;module=src \
	    file://xupnpd.init \
	    file://xupnpd.lua"
S = "${WORKDIR}/src"

inherit base


SRC = "main.cpp soap.cpp mem.cpp mcast.cpp luaxlib.cpp luaxcore.cpp luajson.cpp luajson_parser.cpp"

do_compile () {
  ${CC} -O2 -c -o md5.o md5c.c
  ${CC} ${CFLAGS} ${LDFLAGS} -DWITH_URANDOM -o xupnpd ${SRC} md5.o -lm -llua -ldl -lstdc++
}


do_install () {
  install -d ${D}/usr/bin ${D}/usr/share/xupnpd ${D}/usr/share/xupnpd/config ${D}/usr/share/xupnpd/playlists ${D}/etc/init.d
  cp ${WORKDIR}/xupnpd.init ${D}/etc/init.d/
  cp ${S}/xupnpd ${D}/usr/bin/
  cp -r ${S}/plugins ${D}/usr/share/xupnpd/
  cp -r ${S}/ui ${D}/usr/share/xupnpd/
  cp -r ${S}/www ${D}/usr/share/xupnpd/
  cp  ${S}/*.lua ${D}/usr/share/xupnpd/
  cp ${WORKDIR}/xupnpd.lua ${D}/usr/share/xupnpd/
}