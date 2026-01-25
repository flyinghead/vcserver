#
# dependencies: libasio-dev libsqlite3-dev libdcserver.so
#
prefix = /usr/local
exec_prefix = $(prefix)
sbindir = $(exec_prefix)/sbin
sysconfdir = $(prefix)/etc
localstatedir = /var/local
CFLAGS = -Wall -g "-DLOCALSTATEDIR=\"$(localstatedir)\""
CFLAGS += -O3 -DNDEBUG
#CFLAGS += -fsanitize=address
CXXFLAGS = $(CFLAGS) -std=c++17
DEPS = blowfish.h discord.h vcserver.h log.h
user = dcnet

all: vcserver

vcserver: vcserver.o discord.o blowfish.o db.o log.o
	$(CXX) $(CXXFLAGS) vcserver.o discord.o blowfish.o db.o log.o -o vcserver -lpthread -lsqlite3 -ldcserver -Wl,-rpath,/usr/local/lib

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f vcserver *.o vcserver.service

install: all
	mkdir -p $(DESTDIR)$(sbindir)
	install vcserver $(DESTDIR)$(sbindir)
	mkdir -p $(DESTDIR)$(sysconfdir)
	cp -n vcserver.cfg $(DESTDIR)$(sysconfdir)

vcserver.service: vcserver.service.in Makefile
	sed -e "s/INSTALL_USER/$(user)/g" -e "s:SBINDIR:$(sbindir):g" -e "s:SYSCONFDIR:$(sysconfdir):g" -e "s:LOCALSTATEDIR:$(localstatedir):g" < $< > $@

installservice: vcserver.service
	mkdir -p $(localstatedir)/log
	mkdir -p /usr/lib/systemd/system/
	cp $< /usr/lib/systemd/system/
	systemctl enable vcserver.service

createdb:
	mkdir -p $(localstatedir)/lib/vcserver/
	sqlite3 $(localstatedir)/lib/vcserver/vcserver.db < createdb.sql
	chown $(user):$(user) $(localstatedir)/lib/vcserver/vcserver.db
