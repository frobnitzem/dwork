/*
    Multithreaded Manager + Worker Process
*/

#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <signal.h>

#include "proto.hpp"

const int num_threads = 4;

// send non-NULL terminated string
void sendString(zmq::socket_t &socket, std::string_view x) {
    zmq::message_t msg(x.size());
    memcpy ((void *)msg.data(), &x.front(), x.size());
    socket.send(msg, zmq::send_flags::none);
}

void *WorkerThread(void *arg) {
    zmq::context_t &context = *(zmq::context_t *) arg;

    zmq::socket_t socket (context, zmq::socket_type::req);
    socket.connect ("inproc://workers");

    // Repeatedly pull tasks
    while (true) {
        sendString(socket, "NEXT"); // get work

        zmq::message_t reply;
        const auto ret = socket.recv(reply, zmq::recv_flags::none);
        if(!ret.has_value()) {
            break;
        }

        std::cout << "Received WORK." << std::endl;

        sleep (1); // process the request

        sendString(socket, "DONE"); // push result
        // expecting OK
        const auto ret2 = socket.recv(reply, zmq::recv_flags::none);
        if(!ret2.has_value()) {
            break;
        }
        std::cout << "Received OK." << std::endl;
    }

    return NULL;
}

int main () {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    pthread_t threads[num_threads];

    zmq::context_t context (1);
    zmq::socket_t hub (context, zmq::socket_type::req);
    hub.connect ("tcp://localhost:5555");

    zmq::socket_t workers (context, zmq::socket_type::rep);
    workers.bind ("inproc://workers");

    //  Launch pool of worker threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, WorkerThread, (void *) &context);
    }

    // Process incoming requests from workers
    while (true) {
        zmq::message_t request;

        const auto ret = workers.recv(request, zmq::recv_flags::none);
        if(!ret.has_value()) {
            break;
        }
        int sz = ret.value();

        if( sz == 4 && !strncmp((char *)request.data(), "NEXT", 4) ) {
            // pull from ready deque
            sendString(workers, "WORK");
        } else if( sz == 4 && !strncmp((char *)request.data(), "DONE", 4) ) {
            sendString(workers, "OK");
            // mark completion
        } else {
            sendString(workers, "ERR");
            std::cout << "Received invalid message. len = " << sz << std::endl;
            break;
        }
    }

    std::cout << "Process completed." << std::endl;

    for (int i = 0; i < num_threads; i++) {
        void *ret;
        pthread_join(threads[i], &ret);
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

