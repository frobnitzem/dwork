#!/bin/bash

LOCALROOT=$PWD/deps

CXX=g++
# hiredis
#CXXFLAGS += $(shell pkg-config --cflags hiredis)
#LDFLAGS += $(shell pkg-config --libs hiredis)
# protobuf
#CXXFLAGS += -I/usr/local/include
#LDFLAGS  += -L/usr/local/lib -lzmq -pthread -lprotobuf
# tkrzw
#CXXFLAGS += $(shell tkrzw_build_util config -i)
#LDFLAGS += $(shell tkrzw_build_util config -l)
# yaml-cpp
#YAML_PATH = $(shell spack find --format '{prefix}' yaml-cpp)/lib/pkgconfig
#CXXFLAGS += $(shell PKG_CONFIG_PATH=$(YAML_PATH) pkg-config --cflags yaml-cpp)
#LDFLAGS  += $(shell PKG_CONFIG_PATH=$(YAML_PATH) pkg-config --libs yaml-cpp)

set -e
[ -d build ] && rm -fr build

mkdir build
cd build

cmake -DBUILD_DOCS=ON -DCMAKE_PREFIX_PATH=$LOCALROOT -DCMAKE_INSTALL_PREFIX=$LOCALROOT ..
make -j4 VERBOSE=1

(cd tests && ctest -N)
make install
