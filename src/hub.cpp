/*
    Multithreaded Redis Hub
*/

#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <signal.h>

#include "qredis.hpp"
#include "proto.hpp"

const int num_threads = 4;


static void s_block_signals (void) {
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGTERM);
    
    sigprocmask(SIG_BLOCK, &signal_set, NULL);
}

static void* WaitForAbortThread(void* arg){
    zmq::context_t &context = *(zmq::context_t *) arg;
    sigset_t signal_set;
    int sig;

    // Socket to talk to other threads
    zmq::socket_t publisher (context, zmq::socket_type::pub);
    //publisher.setsockopt(ZMQ_SNDHWM, &sndhwm, sizeof(sndhwm));
    publisher.set(zmq::sockopt::sndhwm, 0);
    publisher.bind("inproc://signals");

    // Socket to receive signals
    zmq::socket_t syncservice (context, zmq::socket_type::rep);
    syncservice.bind("inproc://sig_ready");

    // Get synchronization from subscribers
    for(int subscribers = 0; subscribers <= num_threads; subscribers++) {
        zmq::message_t sub{};
        assert(syncservice.recv(sub, zmq::recv_flags::none).has_value());
        syncservice.send(zmq::buffer(""), zmq::send_flags::none);
    }

    sigemptyset(&signal_set);
    //sigaddset(&signal_set, SIGABRT);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGTERM);

    sigwait( &signal_set, &sig );
    printf("We get signal %o.\n", sig);

    const std::string_view msg = "TERMINATE";
    publisher.send(zmq::buffer(msg), zmq::send_flags::none);

    return NULL;
}

zmq::socket_t SubscribeControl(zmq::context_t &context) {
    //  First, connect our subscriber socket
    zmq::socket_t control(context, zmq::socket_type::sub);
    control.connect("inproc://signals");
    control.set(zmq::sockopt::subscribe, "");

    //  Second, synchronize with publisher
    zmq::socket_t syncclient(context, zmq::socket_type::req);
    syncclient.connect("inproc://sig_ready");

    zmq::message_t resp{};
    syncclient.send(zmq::buffer(""), zmq::send_flags::none);
    assert(syncclient.recv(resp, zmq::recv_flags::none).has_value());

    return control;
}

void *WorkerThread(void *arg) {
    Redis r("127.0.0.1", 6379);
    zmq::context_t &context = *(zmq::context_t *) arg;

    zmq::socket_t control = SubscribeControl(context);
    zmq::socket_t socket (context, zmq::socket_type::rep);
    socket.connect ("inproc://workers");

    //  Process messages from receiver and controller
    zmq::pollitem_t items [] = {
        { static_cast<void*>(socket),  0, ZMQ_POLLIN, 0 },
        { static_cast<void*>(control), 0, ZMQ_POLLIN, 0 }
    };
    while (true) {
        //  Wait for next request from client
        zmq::poll (&items [0], 2, -1);
        
        // Any waiting controller command acts as 'KILL'
        if (items[1].revents & ZMQ_POLLIN) {
            break;
        }

        if ( !(items[0].revents & ZMQ_POLLIN)) {
            continue;
        }

        auto query = RecvProto<dwork::QueryMsg>(socket);
        std::cout << "Received request: " << query.DebugString() << std::endl;

        SendProto(socket, query);
    }

    return NULL;
}

int main () {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    pthread_t threads[num_threads+1];

    //  Prepare our context and sockets
    zmq::context_t context (1);
    zmq::socket_t clients (context, zmq::socket_type::router);
    clients.bind ("tcp://*:5555");

    zmq::socket_t workers (context, zmq::socket_type::dealer);
    workers.bind ("inproc://workers");

    s_block_signals();

    //  Launch pool of worker threads
    pthread_create(&threads[0], NULL, WaitForAbortThread, (void *) &context);
    for (int i = 1; i <= num_threads; i++) {
        pthread_create(&threads[i], NULL, WorkerThread, (void *) &context);
    }

    zmq::socket_t control = SubscribeControl(context);

    //  Connect work threads to client threads via a queue
    //zmq::proxy_steerable(clients, workers, nullptr, control);
    zmq::proxy_steerable(clients, workers, zmq::socket_ref(), control);
    std::cout << "Process completed." << std::endl;

    for (int i = 0; i <= num_threads; i++) {
        void *ret;
        pthread_join(threads[i], &ret);
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

