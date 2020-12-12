/*
    Multithreaded Dwork Hub
*/

#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <signal.h>

#include <qredis.hh>
#include <proto.hh>

#include "taskDB.cc"

const int num_threads = 1;

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

class Hub {
    zmq::context_t &context;
    char host[256];
    Redis r;
    dwork::TaskDB db;

  public:
    Hub(void *arg) : context( *(zmq::context_t *) arg )
                   , r("127.0.0.1", 6379)
                   , db(1<<20)
    {
        if(gethostname(host, sizeof(host))) {
            perror("Error in gethostname");
        }
        fprintf(stderr, "Hub: hostname %s\n", host);
    }
    void *operator()();
    int  handle_query(dwork::QueryMsg &);
    bool add_task(dwork::TaskMsg &t);

    //void join_task(std::string_view name); //< Add the task to the ready list
    void complete_task(std::string_view name); //< Mark task as completed.
    static void enque_ready(std::string_view name, void *); //< Add the task to the ready list
};

void Hub::enque_ready(std::string_view name, void *info) {
    Hub *h = (Hub *)info;
    h->r.lpush("ready", name);
}
void Hub::complete_task(std::string_view name) {
    db.complete_task(name, enque_ready, this);
}

void *Hub::operator()() {
    fprintf(stderr, "Subscribing to control socket.\n");
    zmq::socket_t control = SubscribeControl(context);
    zmq::socket_t socket( context, zmq::socket_type::rep );
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

        auto msg = recvProto<dwork::QueryMsg>(socket);
        if(!msg.has_value()) { // bad query
            std::cout << "Received bad query." << std::endl;
            continue;
        }
        auto &query = msg.value();
        handle_query(query);
        sendProto(socket, query);
    }

    return NULL;
}

int Hub::handle_query(dwork::QueryMsg &query) {
    std::cout << "Received request: " << query.DebugString() << std::endl;

    switch(query.type()) {
    case dwork::QueryMsg::Steal: {
        if(!query.has_name()) { // invalid msg
            std::cout << "Received invalid Steal() with no hostname." << std::endl;
            goto err;
        }
        auto ret = r.rpop("ready");
        auto host = query.name();
        query.clear_name();

        if(!ret.has_value()) // out of work - time to exit
            goto err;
        std::string name = std::move(ret.value());
        std::cout << "Host " << host << " stealing task " << name << std::endl;

        // update task-tracking
        r.sadd("workers", host);
        r.sadd("assigned/"+host, name);

        // send task
        query.set_type(dwork::QueryMsg::Transfer);
        query.clear_task();
        dwork::TaskMsg *task = query.add_task();
        task->set_name(name);
        task->set_origin("hub");
        //logTransition(task, dwork::TaskMsg::Stolen);

        break;
    }
    /*case dwork::QueryMsg::CreateTask: {
        for(auto &t : query.task()) {
            add_task(t);
        }
        query.clear_task();
        query.set_type(dwork::QueryMsg::OK);
    }*/
    /*case dwork::QueryMsg::Transfer: {
        if(query.task_size() <= 0) {
            std::cout << "Received invalid Transfer() with no TaskMsg." << std::endl;
            goto err;
        }
        for(const auto task : query.task()) {
            if(!task.has_location()) {
                std::cout << "Received invalid Transfer() with no location." << std::endl;
                goto err;
            }
            // TODO: check task->origin

            //std::string name(task.name());
            //r.set("done/"+name, task.location());

            auto host = r.get("tasks/"+name);
            if(host.has_value()) {
                r.del("tasks/"+name);
                r.srem(host.value(), name);
            }
        }
        query.set_type(dwork::QueryMsg::OK);
        //logTransition(task, dwork::TaskMsg::Recorded);
        //TODO: store task info. to logfile
        break;
    }
    case dwork::QueryMsg::Lookup: {
        auto loc = r.get("done/"+query.name());
        if(loc.has_value()) {
            query.set_type(dwork::QueryMsg::OK);
            query.set_location(loc.value());
        } else {
            query.set_type(dwork::QueryMsg::NotFound);
        }
        break;
    }*/
    default:
    err:
        query.set_type(dwork::QueryMsg::Exit);
        break;
    }

    return 0;
}

#define id_from_taskname(x) (x);

bool Hub::add_task(dwork::TaskMsg &t) {
    std::vector<std::string_view> deps(t.pred_size());
    //for( const auto &dep : t.pred() ) {
    for(int i=0; i<t.pred_size(); i++) {
        //deps += add_dep(dep.id(), id);
        deps[i] = t.pred(i).id();
    }

    std::string_view id = id_from_taskname(t.name());
    return db.new_task(id, deps, enque_ready, (void *)this);
}

template <typename W>
void *runWorker(void *arg) {
    W work(arg);
    return work();
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
        pthread_create(&threads[i], NULL, runWorker<Hub>, (void *) &context);
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

