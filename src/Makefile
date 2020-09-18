
#CXXFLAGS = -std=c++17 -pthread -I/usr/local/include -O2 -Wall
#LDFLAGS = -L/usr/local/lib -ltkrzw -lstdc++ -lpthread

CXXFLAGS = -std=c++17 -O2 -Wall
LDFLAGS = 

CXXFLAGS += -I/usr/local/include
LDFLAGS  += -L/usr/local/lib -lzmq -pthread

CXXFLAGS += $(shell tkrzw_build_util config -i)
LDFLAGS += $(shell tkrzw_build_util config -l)

server: server.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

clean:
	rm server

