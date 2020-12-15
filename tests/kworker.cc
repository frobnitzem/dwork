/*
    Runner of kernels.
*/

#include <unistd.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>

#include <proto.hh>

struct Kernel {
    long iterations;
    void *x;
    Kernel(long y) : iterations(y) {}
};
const long iter = 1<<22; // 4M iterations
double execute_kernel_compute(const Kernel &k);

class Worker {
    zmq::context_t &context;
    zmq::socket_t hub;
    char hostname[256];

    public:

    Worker(zmq::context_t &_context)
        :  context(_context)
        ,  hub(context, zmq::socket_type::req)
    {
        hub.connect ("tcp://localhost:5555");
        if(gethostname(hostname, sizeof(hostname))) {
            perror("Error in gethostname");
            exit(1);
        }
    }

    /** Add a new task to the query.
     *  It has a name and an origin, but nothing else.
     */
    dwork::TaskMsg *new_task(dwork::QueryMsg &query, const std::string &name) {
        dwork::TaskMsg *task = query.add_task();
        task->set_origin(hostname);
        task->set_name(name);
        return task;
    }

    /** Send a message and over-write it with the response.
     */
    int sendrecv(dwork::QueryMsg &query) {
        //std::cout << "Sending:" << std::endl
        //          << query      << std::endl;
        sendProto(hub, query);

        zmq::message_t msg;
        const auto ret = hub.recv(msg, zmq::recv_flags::none);
        if(ret.value_or(0) == 0) {
            std::cout << "Received unparsable response to request:" << std::endl;
            std::cout << query << std::endl;
            return 1;
        }
        //query.clear();
        query.ParseFromArray(msg.data(), msg.size());

        if(query.type() == dwork::QueryMsg::Error) {
            std::cout << "Received error response!" << std::endl;
            return 1;
        }
        //std::cout << "Received:" << std::endl
        //          << query.value() << std::endl;

        return 0;
    }

    /** Shortcut to create a task of the given
     * type and fill in location = hostname.
     */
    dwork::QueryMsg loc_msg(dwork::QueryMsg::Type type) {
        dwork::QueryMsg query;

        query.set_type(type);
        query.set_location(hostname);
        return query;
    }

    int worker_exit() {
        dwork::QueryMsg query = loc_msg(dwork::QueryMsg::Exit);
        return sendrecv(query);
    }

    bool steal(std::vector<std::string> &ans, int n = 0) {
        ans.resize(0);

        dwork::QueryMsg query = loc_msg(dwork::QueryMsg::Steal);
        if(n > 0) {
            ans.reserve(n);
            query.set_n(n);
        }
        sendrecv(query);
        for(const auto &t : query.task()) {
            ans.emplace_back(t.name());
        }
        return query.type() == dwork::QueryMsg::Exit;
    }

    /** Send task completion notifications.
     */
    int complete(const std::vector<std::string> &tasks) {
        dwork::QueryMsg query = loc_msg(dwork::QueryMsg::Complete);
        for(auto &name : tasks) {
            new_task(query, name);
        }

        return sendrecv(query);
    }

    double run() {
        double x = 0.0;
        //worker_exit(); // re-queue is good practice, but requires unique hostnames (must add rank/CPU num, etc.)
        while(true) {
            std::vector<std::string> tasks;
            if( steal(tasks) ) break;
            if(tasks.size() == 0) continue; // usleep?

            for(auto &t : tasks) { // execute tasks in serial
                Kernel k(iter);
                x += execute_kernel_compute(k);
            }
            if( complete(tasks) ) break;
        }
        return x;
    }
};

int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    zmq::context_t context (1);

    Worker w(context);

    return w.run();
}
