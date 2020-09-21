
#CXXFLAGS = -std=c++17 -pthread -I/usr/local/include -O2 -Wall
#LDFLAGS = -L/usr/local/lib -ltkrzw -lstdc++ -lpthread
#--std=c++17 -I/usr/local/include/hiredis -o example qredis.cpp -L/usr/local/lib -lhiredis

CXXFLAGS = -std=c++17 -O2 -Wall
LDFLAGS = 

CXXFLAGS += $(shell pkg-config --cflags hiredis)
LDFLAGS += $(shell pkg-config --libs hiredis)

CXXFLAGS += -I/usr/local/include
LDFLAGS  += -L/usr/local/lib -lzmq -pthread -lprotobuf

CXXFLAGS += $(shell tkrzw_build_util config -i)
LDFLAGS += $(shell tkrzw_build_util config -l)

SOURCES = proto/TaskMsg.pb.cc 

manager: manager.cpp $(SOURCES)
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

hub: hub.cpp $(SOURCES)
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

proto/TaskMsg.pb.cc: proto/TaskMsg.proto
	protoc -I=proto --cpp_out=proto $^

qredis: qredis.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

clean:
	rm server

