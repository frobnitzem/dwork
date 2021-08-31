/*
    Multithreaded Dwork Hub
*/

#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <sstream>
#include <signal.h>

#include <deque>
#include <set>
#include <proto.hh>
#include <string>

#include "taskDB.cc"


const int num_threads = 4;

#include <sys/resource.h>
void maximizeFileLimit() {
    rlim_t maxfiles = 30000;
    struct rlimit limit;
    const rlim_t decr_step = 16;

    if (getrlimit(RLIMIT_NOFILE,&limit) == -1) {
        perror("Error calling getrlimit");
        return;
    }
    rlim_t oldlimit = limit.rlim_cur;
    if (oldlimit < maxfiles) {
        rlim_t bestlimit = maxfiles;
        int setrlimit_error = 0;

        // loop until setrlimit succeeds
        for(; bestlimit > oldlimit && bestlimit >= decr_step; bestlimit -= decr_step) {
            limit.rlim_cur = bestlimit;
            limit.rlim_max = bestlimit;
            if (setrlimit(RLIMIT_NOFILE,&limit) != -1) break;
            setrlimit_error = errno;
        }
        if (bestlimit < oldlimit)
            bestlimit = oldlimit;
        printf("Set max open files to %lu (was %lu)\n", bestlimit, oldlimit);
    }
}

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

/**
 * Internal database of ready tasks and their assignments to workers.
 */
class TaskMgr {
    std::deque<std::string> ready; ///< list of ready tasks (lpush, rpop)
    /** assignments from hosts to tasks */
    std::map<std::string,std::set<std::string>> assignments;
    std::mutex mtx;

    public:
    /** push a task on the left (lowest prio) */
    void lpush(std::string_view name) {
        std::lock_guard<std::mutex> guard(mtx);
        ready.emplace_front(name);
    }
    /** push a task on the right (highest prio) */
    void rpush(std::string_view name) {
        std::lock_guard<std::mutex> guard(mtx);
        ready.emplace_back(name);
    }

    size_t ready_size() {
        std::lock_guard<std::mutex> guard(mtx);
        return ready.size();
    }

    /**
     * Assign highest priority task to given host.
     *
     * @param name  (output) set to name of assigned task
     * @param host  (input) name of host to assign
     * @return      false on successful assignment
     */
    bool assign(std::string *name, std::string &host) {
        std::lock_guard<std::mutex> guard(mtx);
        if(ready.size() == 0) return true;

        // deque the next task name
        std::swap(*name, ready.back());
        ready.pop_back();

        // assign to host
        std::set<std::string> &a = assignments[ host ];
        a.emplace( *name );

        return false;
    }

    /**
     * Mark the task name as completed by host.
     * Removes the corresponding assignment record
     * from the host's set.
     *
     * @return  false on success, true if not found
     */
    bool complete(const std::string &name, const std::string &host) {
        std::lock_guard<std::mutex> guard(mtx);
        return assignments[host].erase(name) < 1;
    }

    /**
     * Delete the worker `host`.
     *
     * @return  false on success, true on notfound
     */
    bool del_worker(std::string &host) {
        std::lock_guard<std::mutex> guard(mtx);
        auto nh = assignments.extract(host);
        if(nh.empty()) return true;

        for(auto &name : nh.mapped()) {
            ready.emplace_back(name);
        }
        return false;
    }
};

class Hub {
    zmq::context_t &context;
    char hostname[256];
    TaskMgr mgr;
    dwork::TaskDB db;

  public:
    Hub(zmq::context_t &arg, std::string_view fname)
                : context( arg )
                , db(1<<20, fname, fname.size() == 0 ? NULL : Hub::enque_new, (void *)this)
    {
        if(gethostname(hostname, sizeof(hostname))) {
            perror("Error in gethostname");
        }
        fprintf(stderr, "Hub: hostname %s\n", hostname);
    }
    void *operator()();
    bool valid_query(dwork::QueryMsg &query); ///< returns true if valid
    int  handle_query(dwork::QueryMsg &); ///< replaces the query with its response
    bool add_task(const dwork::TaskMsg &t);

    //void join_task(std::string_view name); //< Add the task to the ready list
    bool complete_task(std::string &name, std::string &host); //< Mark task as completed.
    /** Add the task to the ready list
     *  New tasks go in on the left (lowest prio)
     */
    static void enque_new(std::string_view name, void *);
    /** Add the task to the ready list
     *  Tasks enabled by other tasks go in on the right (highest prio)
     */
    static void enque_ready(std::string_view name, void *);
};

void Hub::enque_new(std::string_view name, void *info) {
    Hub *h = (Hub *)info;
    //std::cout << "Adding to ready: " << name << std::endl;
    h->mgr.lpush( name );
}
void Hub::enque_ready(std::string_view name, void *info) {
    Hub *h = (Hub *)info;
    h->mgr.rpush( name );
}
/** returns false on success, true if not found */
bool Hub::complete_task(std::string &name, std::string &host) {
    db.complete_task(name, enque_ready, this);
    return mgr.complete(name, host);
}

void *Hub::operator()() {
    //fprintf(stderr, "Subscribing to control socket.\n");
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
        if(!msg.has_value()) {
            std::cout << "Received unparsable query." << std::endl;
            goto err;
        }
        if(!valid_query(msg.value())) {
            std::cout << "Received ill-formed query." << std::endl;
        err:
            dwork::QueryMsg err;
            err.set_type(dwork::QueryMsg::Error);
            sendProto(socket, err);
            continue;
        }
        auto &query = msg.value();
        handle_query(query);
        sendProto(socket, query);
    }

    return NULL;
}

bool Hub::valid_query(dwork::QueryMsg &query) {
    //std::cout << "Received request: " << query.DebugString() << std::endl;

    switch(query.type()) {
        case dwork::QueryMsg::Create: {
            if(query.task_size() < 1) return false;
            if(query.has_name() || query.has_n() || query.has_location()) return false;
        } break;
        case dwork::QueryMsg::Transfer: {
            if(query.task_size() < 1) return false;
            if(query.has_name() || query.has_n() || !query.has_location()) return false;
        } break;
        case dwork::QueryMsg::Steal: {
            if(query.task_size() != 0) return false;
            if(query.has_name() || !query.has_location()) return false;
        } break;
        case dwork::QueryMsg::Complete: {
            if(query.task_size() < 1) return false;
            if(query.has_name() || query.has_n() || !query.has_location()) return false;
        } break;
        case dwork::QueryMsg::Exit: {
            if(query.task_size() != 0) return false;
            if(query.has_name() || query.has_n() || !query.has_location()) return false;
        } break;
        case dwork::QueryMsg::OK: {
            if(query.task_size() != 0) return false;
            if(query.has_name() || query.has_n() || query.has_location()) return false;
        } break;
        default:
        err:
            return false;
    }

    return true;
}

int Hub::handle_query(dwork::QueryMsg &query) {
    switch(query.type()) {
    case dwork::QueryMsg::Create: {
        for(auto &t : query.task()) {
            add_task(t);
        }
        query.clear_task();
        query.set_type(dwork::QueryMsg::OK);
    } break;
    case dwork::QueryMsg::Steal: {
        std::string host = query.location();
        query.clear_location();
        int32_t n = 1;
        if(query.has_n()) {
            n = query.n();
            query.clear_n();
        }

        for(int32_t i=0; i<n; i++) {
            std::string name;
            if( mgr.assign(&name, host) )
                break;
            //std::cout << "Host " << host << " stealing task " << name << std::endl;

            // send task
            dwork::TaskMsg *task = query.add_task();
            task->set_name(name); // TODO: lookup task itself...
            task->set_origin(hostname);
            dwork::logTransition(task, dwork::TaskMsg::Stolen);
        }

        if(query.task_size() == 0) {
            if(db.TH.active() > 0) {
                query.set_type(dwork::QueryMsg::NotFound);
            } else {
                query.set_type(dwork::QueryMsg::Exit);
            }
        } else {
            query.set_type(dwork::QueryMsg::Transfer);
        }
    } break;
    case dwork::QueryMsg::Complete: {
        std::string host = query.location();
        query.clear_location();

        for(auto &task : query.task()) {
            // TODO? check task->origin
            std::string name(task.name());

            // TODO handle name lookup errors
            complete_task(name, host);
            //dwork::logTransition(&task, dwork::TaskMsg::Recorded);
            //TODO: store task info. to logfile
        }
        query.clear_task();
        query.set_type(dwork::QueryMsg::OK);
    } break;
    case dwork::QueryMsg::Transfer: { // return back to queue
        std::string host = query.location();
        query.clear_location();

        auto *tasks = query.mutable_task();
        int errors = 0; // errors accumulate at front
        for(int i=tasks->size()-1; i>=errors; i--) {
            dwork::TaskMsg *task = tasks->Mutable(i);
            // remove the assignment
            if(mgr.complete(task->name(), host)) { // not found!
                tasks->SwapElements(errors, i);
                errors++;
                i++;
                continue;
            }
            // re-add task to the queue
            add_task(*task);
            tasks->RemoveLast();
        }
        query.clear_task();
        query.set_type(dwork::QueryMsg::OK);
    } break;
    case dwork::QueryMsg::Exit: {
        std::string host = query.location();
        query.clear_location();

        if( mgr.del_worker(host) ) {
            query.set_type(dwork::QueryMsg::NotFound);
        } else {
            query.set_type(dwork::QueryMsg::OK);
        }
    } break;
    case dwork::QueryMsg::OK: {
        size_t n = mgr.ready_size();
        std::ostringstream ss;
        ss << "dhub: " << db.TH.active() << " active tasks, " << n << " ready.";
        query.set_name( ss.str() );
        query.set_n( n );
    } break;
    /*case dwork::QueryMsg::Lookup: {
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
        query.set_type(dwork::QueryMsg::Error);
        break;
    }

    return 0;
}

#define id_from_taskname(x) (x);

bool Hub::add_task(const dwork::TaskMsg &t) {
    // TODO: store task data somewhere.
    //dwork::logTransition(&task, dwork::TaskMsg::Waiting);
    std::vector<std::string_view> deps(t.pred_size());
    //for( const auto &dep : t.pred() ) {
    for(int i=0; i<t.pred_size(); i++) {
        //deps += add_dep(dep.name(), id);
        deps[i] = t.pred(i).name();
    }

    std::string_view id = id_from_taskname(t.name());
    return db.new_task(id, deps, enque_new, (void *)this);
}

template <typename W>
void *runWorker(void *arg) {
    W *w = (W *)arg;
    return (*w)();
}

std::string bind_addr(int port) {
    std::ostringstream ss;
    ss << "tcp://*:" << port;
    return ss.str();
}

int main (int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    std::string_view fname("");
    int port = 6125;

    pthread_t threads[num_threads+1];
    maximizeFileLimit();

    while(argc > 1) {
        if(argc >= 3 && !std::string("-f").compare(argv[1])) {
            fname = argv[2];
            argv[2] = argv[0];
            argc -= 2;
            argv += 2;
        } else if(argc >= 3 && !std::string("-p").compare(argv[1])) {
            port = atoi(argv[2]);
            argv[2] = argv[0];
            argc -= 2;
            argv += 2;
        } else {
            break;
        }
    }

    //  Prepare our context and sockets
    zmq::context_t context (1);
    zmq::socket_t clients (context, zmq::socket_type::router);
    clients.bind (bind_addr(port));

    zmq::socket_t workers (context, zmq::socket_type::dealer);
    workers.bind ("inproc://workers");

    s_block_signals();

    Hub hub(context, fname);

    //  Launch pool of worker threads
    pthread_create(&threads[0], NULL, WaitForAbortThread, (void *) &context);
    for (int i = 1; i <= num_threads; i++) {
        pthread_create(&threads[i], NULL, runWorker<Hub>, (void *) &hub);
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

