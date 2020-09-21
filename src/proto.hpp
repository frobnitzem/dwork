#include <string>
#include <string.h>
#include <zmq.hpp>
#include "proto/TaskMsg.pb.h"

// Send a protobuf over ZeroMQ socket.
template <typename P>
void SendProto(zmq::socket_t &socket, P &q) {
    std::string str;
    q.SerializeToString(&str);
    int sz = str.length();

    zmq::message_t msg(sz);
    memcpy((void *)msg.data(), (void *)str.c_str(), sz);
    socket.send(msg, zmq::send_flags::none);
}

// Receive a protobuf over ZeroMQ socket.
template <typename P>
P RecvProto(zmq::socket_t &socket) {
    P pb = P();
    zmq::message_t msg;
    const auto ret = socket.recv(msg, zmq::recv_flags::none);
    if(ret.has_value()) {
        //pb.ParseFromString((char*) msg.data());
        pb.ParseFromArray(msg.data(), msg.size());
    }
    return pb;
}
