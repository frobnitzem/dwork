
include conf.mk

SOURCES = proto/TaskMsg.pb.cc 

manager: manager.cpp $(SOURCES)
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

hub: hub.cpp $(SOURCES)
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

proto/TaskMsg.pb.cc: proto/TaskMsg.proto
	protoc -I=proto --cpp_out=proto $^

qredis: qredis.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

db: local_db.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

clean:
	rm server

