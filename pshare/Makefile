MIPSEL  = /usr/local/toolchain-mipsel_gcc3.4.6/mipsel-linux/bin
MIPS    = /usr/local/toolchain-mips_gcc4.1.2/mips-linux/bin

all:
	gcc -fno-exceptions -fno-rtti -O2 -DWITH_LIBUUID -o pshare main.cpp upnp.cpp soap.cpp tmpl.cpp mem.cpp proxy.cpp common.cpp -luuid
	strip pshare

mipsel:
	$(MIPSEL)/gcc -fno-exceptions -fno-rtti -O2 -DWITH_URANDOM -o pshare main.cpp upnp.cpp soap.cpp tmpl.cpp mem.cpp proxy.cpp common.cpp
	$(MIPSEL)/strip pshare
	fakeroot ./mkipkg.sh

tests:
	g++ -fno-exceptions -fno-rtti -o test test.cpp upnp.cpp -luuid
