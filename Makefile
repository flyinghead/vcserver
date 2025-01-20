# deps: libcurl-dev libasio-dev libsqlite3-dev
CFLAGS = -O3 -Wall
#CFLAGS = -g -Wall -fsanitize=address
CXXFLAGS = $(CFLAGS) -std=c++17 -I/home/raph/flycast-dev-clean/core/deps/asio/asio/include
DEPS = blowfish.h discord.h json.hpp vcserver.h
USER = dcnet
INSTALL_DIR = /usr/local/vcserver

all: vcserver

vcserver: vcserver.o discord.o blowfish.o db.o
	$(CXX) $(CXXFLAGS) vcserver.o discord.o blowfish.o db.o -o vcserver -lpthread -lcurl -lsqlite3

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f vcserver *.o

install: all
	install -o $(USER) -g $(USER) -d $(INSTALL_DIR)
	install --strip -o $(USER) -g $(USER) vcserver $(INSTALL_DIR)

vcserver.service: vcserver.service.in Makefile
	cp vcserver.service.in vcserver.service
	sed -e "s/INSTALL_USER/$(USER)/g" -e "s:INSTALL_DIR:$(INSTALL_DIR):g" < $< > $@

installservice: vcserver.service
	install -o root -g root -d /usr/local/lib/systemd/
	install -m 0644 -o root -g root $< /usr/local/lib/systemd/
	systemctl enable /usr/local/lib/systemd/vcserver.service

createdb:
	sqlite3 $(INSTALL_DIR)/vcserver.db < createdb.sql
	chown $(USER):$(USER) $(INSTALL_DIR)/vcserver.db
