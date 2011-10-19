
CFLAGS = -Wall -std=c99 `pkg-config --cflags blkid`
LIBS = `pkg-config --libs blkid`

all: pam-phy-token module.so

pam-phy-token: src/tool.c src/pam-phy-token.o
	cc $(CFLAGS) -o $@ $^ $(LIBS)

module.so: src/module.o src/pam-phy-token.o
	cc -shared -o $@ $^ $(LIBS) -lpam

%.o: %.c
	cc $(CFLAGS) -fPIC -c $< -o $@

install: all
	install -m 0755 module.so $(DESTDIR)/lib/security/pam_phy_token.so
	install -m 4755 pam-phy-token $(DESTDIR)/usr/bin
	install -m 0700 -d $(DESTDIR)/etc/pam-phy-token

clean:
	rm -rf pam-phy-token module.so src/*.o
