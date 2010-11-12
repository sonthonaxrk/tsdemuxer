MIPSEL  = /usr/local/toolchain-mipsel_gcc3.4.6/mipsel-linux/bin

all:
	gcc -fno-exceptions -fno-rtti -DWITH_LIBUUID -o pshare main.cpp upnp.cpp soap.cpp tmpl.cpp mem.cpp -luuid
	strip pshare

mipsel:
	$(MIPSEL)/gcc -fno-exceptions -fno-rtti -DWITH_URANDOM -o pshare main.cpp upnp.cpp soap.cpp tmpl.cpp mem.cpp
	$(MIPSEL)/strip pshare
	fakeroot ./mkipkg.sh

tests:
	g++ -fno-exceptions -fno-rtti -o test test.cpp upnp.cpp -luuid
