
include conf.mk

PROTO_OBJS = proto/TaskMsg.pb.o

all: manager hub

manager: manager.o $(PROTO_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

hub: hub.o $(PROTO_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# test of redis store/load
qredis: qredis.o
	$(CXX) -o $@ $^ $(LDFLAGS)

#db: local_db.o
#	$(CXX) -o $@ $^ $(LDFLAGS)

proto/TaskMsg.pb.cc: proto/TaskMsg.proto
	protoc -I=proto --cpp_out=proto $^

clean:
	rm -f $(PROTO_OBJS)
	rm manager hub qredis

.SUFFIXES: .cc

.cc.o:
	$(CXX) -c -o $@ $(CXXFLAGS) $^ $(LDFLAGS)

