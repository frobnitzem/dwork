#include <string>
#include <string.h>
#include <zmq.hpp>
#include <iostream>

#include <TaskMsg.pb.h>

#include <sys/time.h>
inline int64_t time_in_us() {
    struct timeval  time;
    struct timezone zone;
    gettimeofday(&time, &zone);
    return time.tv_sec*1000000 + time.tv_usec;
}

/** Send a protobuf over ZeroMQ socket. */
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

namespace dwork {
    void logTransition(TaskMsg *task, const TaskMsg_State &state) {
        TaskMsg::LogMsg *log = task->add_log();
        log->set_state(state);
        log->set_time( time_in_us() );
    }

    /** Friendly printing functions. */
    std::ostream &operator<<(std::ostream &os, const TaskMsg::State &s);
    std::ostream &operator<<(std::ostream &os, const TaskMsg::Dep &d);
    std::ostream &operator<<(std::ostream &os, const TaskMsg::LogMsg &l);
    std::ostream &operator<<(std::ostream &os, const TaskMsg &t);
    std::ostream &operator<<(std::ostream &os, const QueryMsg::Type &s);
    std::ostream &operator<<(std::ostream &os, const QueryMsg &q);
}
