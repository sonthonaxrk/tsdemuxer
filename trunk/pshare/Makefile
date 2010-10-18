MIPSEL_CC       = /usr/local/toolchain-mipsel_gcc3.4.6/mipsel-linux/bin/gcc

all:
	g++ -fno-exceptions -fno-rtti -o pshare main.cpp upnp.cpp soap.cpp -luuid
	strip pshare

mipsel:
	$(MIPSEL_CC) -fno-exceptions -fno-rtti -DNO_UUIDGEN -o pshare main.cpp upnp.cpp soap.cpp
