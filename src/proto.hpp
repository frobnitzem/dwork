#include <string>
#include <string.h>
#include <zmq.hpp>

#include <google/protobuf/util/time_util.h>
#include "proto/TaskMsg.pb.h"

// Send a protobuf over ZeroMQ socket.
template <typename P>
void sendProto(zmq::socket_t &socket, P &q) {
    std::string str;
    q.SerializeToString(&str);
    int sz = str.length();

    zmq::message_t msg(sz);
    memcpy((void *)msg.data(), (void *)str.c_str(), sz);
    socket.send(msg, zmq::send_flags::none);
}

// Receive a protobuf over ZeroMQ socket.
template <typename P>
std::optional<P> recvProto(zmq::socket_t &socket) {
    zmq::message_t msg;
    const auto ret = socket.recv(msg, zmq::recv_flags::none);
    if(ret.value_or(0) == 0) {
        return {};
    }
    P pb;
    //pb.ParseFromString((char*) msg.data());
    pb.ParseFromArray(msg.data(), msg.size());
    return pb;
}

void logTransition(dwork::TaskMsg &task, const dwork::TaskMsg_State &state) {
    dwork::TaskMsg::LogMsg *log = task.add_log();
    log->set_state(state);
    auto time = google::protobuf::util::TimeUtil::GetCurrentTime();
    log->set_time( google::protobuf::util::TimeUtil::TimestampToMilliseconds(time) );
}

