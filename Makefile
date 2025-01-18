#CPPFLAGS= -std=c++17 -I/home/raph/flycast-dev-clean/core/deps/asio/asio/include -g -Wall -fsanitize=address -static-libasan
CPPFLAGS= -std=c++17 -I/home/raph/flycast-dev-clean/core/deps/asio/asio/include -O3 -Wall

all: vcserver

vcserver: vcserver.cpp discord.cpp
	c++ $(CPPFLAGS) vcserver.cpp discord.cpp blowfish.c -o vcserver -lpthread -lcurl

clean:
	rm -f vcserver *.o
