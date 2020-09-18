/*
    Multithreaded Hello World server in C++
*/

#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <string>
#include <iostream>
#include <signal.h>
#include <zmq.hpp>

const int num_threads = 5;

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
    int sndhwm = 0;
    zmq::socket_t publisher (context, zmq::socket_type::pub);
    publisher.setsockopt(ZMQ_SNDHWM, &sndhwm, sizeof(sndhwm));
    publisher.bind("inproc://signals");

    // Socket to receive signals
    zmq::socket_t syncservice (context, zmq::socket_type::rep);
    syncservice.bind("inproc://sig_ready");

    // Get synchronization from subscribers
    for(int subscribers = 0; subscribers <= num_threads; subscribers++) {
        zmq::message_t sub{};
        syncservice.recv(sub, zmq::recv_flags::none);
        syncservice.send(zmq::buffer(""), zmq::send_flags::none);
        std::cout << "Subscribed" << std::endl;
    }
    std::cout << "Waiting" << std::endl;

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
    control.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    //  Second, synchronize with publisher
    zmq::socket_t syncclient(context, zmq::socket_type::req);
    syncclient.connect("inproc://sig_ready");

    zmq::message_t resp{};
    syncclient.send(zmq::buffer(""), zmq::send_flags::none);
    syncclient.recv(resp, zmq::recv_flags::none);

    return control;
}

void *WorkerThread(void *arg) {
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
        zmq::message_t request;
        zmq::poll (&items [0], 2, -1);
        
        // Any waiting controller command acts as 'KILL'
        if (items[1].revents & ZMQ_POLLIN) {
            std::cout << "Exiting" << std::endl;
            break;
        }

        if ( !(items[0].revents & ZMQ_POLLIN)) {
            continue;
        }

        socket.recv(request, zmq::recv_flags::none);
        std::cout << "Received request: [" << (char*) request.data() << "]" << std::endl;

        //  Do some 'work'
        sleep (1);

        //  Send reply back to client
        zmq::message_t reply (6);
        memcpy ((void *) reply.data (), "World", 6);
        socket.send(reply, zmq::send_flags::none);
    }

    return NULL;
}

int main () {
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
    zmq::proxy_steerable(clients, workers, nullptr, control);
    std::cout << "Proxy completed." << std::endl;

    for (int i = 0; i <= num_threads; i++) {
        void *ret;
        pthread_join(threads[i], &ret);
    }

    return 0;
}

