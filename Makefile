# deps: libcurl-dev libasio-dev
CFLAGS = -O3 -Wall
#CFLAGS = -g -Wall -fsanitize=address
CXXFLAGS = $(CFLAGS) -std=c++17 -I/home/raph/flycast-dev-clean/core/deps/asio/asio/include
DEPS = blowfish.h discord.h json.hpp vcserver.h

all: vcserver

vcserver: vcserver.o discord.o blowfish.o
	$(CXX) $(CXXFLAGS) vcserver.o discord.o blowfish.o -o vcserver -lpthread -lcurl

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f vcserver *.o
