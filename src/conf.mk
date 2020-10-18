CXXFLAGS = -std=c++17 -O2 -Wall
LDFLAGS = 

# hiredis
CXXFLAGS += $(shell pkg-config --cflags hiredis)
LDFLAGS += $(shell pkg-config --libs hiredis)

# protobuf
CXXFLAGS += -I/usr/local/include
LDFLAGS  += -L/usr/local/lib -lzmq -pthread -lprotobuf

# tkrzw
CXXFLAGS += $(shell tkrzw_build_util config -i)
LDFLAGS += $(shell tkrzw_build_util config -l)

