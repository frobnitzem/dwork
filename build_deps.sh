#!/bin/bash
# Script to build dependencies of dwork

module load gcc/10
export CC=gcc
export CXX=g++

INSTALL=$PWD/deps
WORK=/dev/shm

cd $WORK
ZMQ_VERSION=4.2.5
CPPZMQ_VERSION=4.7.1
PROTOBUF_VERSION=3.14.0
TKRZW_VERSION=0.9.3
curl -L https://github.com/zeromq/libzmq/archive/v"${ZMQ_VERSION}".tar.gz \
      >zeromq.tar.gz
curl -L https://github.com/zeromq/cppzmq/archive/v"${CPPZMQ_VERSION}".tar.gz \
      >cppzmq.tar.gz &
curl -L https://github.com/protocolbuffers/protobuf/archive/v"${PROTOBUF_VERSION}".tar.gz \
      >protobuf.tar.gz &
curl -L https://dbmx.net/tkrzw/pkg/tkrzw-$TKRZW_VERSION.tar.gz \
      >tkrzw.tar.gz &

tar xzf libzmq.tar.gz
mkdir libzmq-$ZMQ_VERSION/build
pushd libzmq-ZMQ_VERSION/build
cmake -DCMAKE_INSTALL_PREFIX=$INSTALL ..
make -j8 install
popd
rm -fr libzmq-$ZMQ_VERSION

wait
tar xzf cppzmq.tar.gz
mkdir cppzmq-$CPPZMQ_VERSION/build
pushd cppzmq-$CPPZMQ_VERSION/build
cmake -DCMAKE_PREFIX_PATH=$INSTALL -DCMAKE_INSTALL_PREFIX=$INSTALL ..
make -j8 install
popd
rm -fr cppzmq-$CPPZMQ_VERSION

tar xzf protobuf.tar.gz
pushd protobuf-$PROTOBUF_VERSION
./configure --prefix=$INSTALL
make -j8 install
popd
rm -fr protobuf-$PROTOBUF_VERSION

tar xzf tkrzw.tar.gz
pushd tkrzw-$TKRZW_VERSION
./configure --prefix=$INSTALL
make -j8 install
popd
rm -fr tkrzw-$TKRZW_VERSION

# see build.sh for this
#git clone https://github.com/frobnitzem/dwork
#mkdir dwork/build
#pushd dwork/build
#cmake -DCMAKE_PREFIX_PATH=$INSTALL -DCMAKE_INSTALL_PREFIX=$INSTALL ..
#make -j8 install
#popd
