/*
    Multithreaded Manager + Worker Process
*/

#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <deque>
#include <signal.h>

#include "proto.hpp"
#include "local_db.cpp"

const int num_threads = 4;

TaskDB db = TaskDB();

// send non-NULL terminated string
void sendString(zmq::socket_t &socket, std::string_view x) {
    zmq::message_t msg(x.size());
    memcpy ((void *)msg.data(), &x.front(), x.size());
    socket.send(msg, zmq::send_flags::none);
}

// There are some impressive routing challenges associated with maintaining
// a consistent view of the pending/waiting/copy/ready list.
// Especially, we have to track all jobs in progress so as not to duplicate effort in DAGs.
class Manager {
  using Task = dwork::TaskMsg;

  private:
    std::deque<Task> pending; // list of pending tasks (lpush, rpop)
    std::deque<Task> ready;   // list of ready tasks (lpush, rpop)
    //std::deque<std::string> copy;    // list of copy  tasks  (lpush, rpop)
    //std::unordered_map<std::string, std::pair<int,int> > waiting; // uncopied predecessors, copied predecessors
    zmq::socket_t hub;

  public:
    // creates a dummy task-list
    Manager(int n) {
        for(int i=0; i<n; i++) {
            dwork::TaskMsg task;
            task.set_name("step");
            task.set_origin("localhost");
            logTransition(task, dwork::TaskMsg::Ready);
            ready.push_back( task );
        }
    };

    // receives work from the hub
    Manager(zmq::context_t &context) : hub(context, zmq::socket_type::req) {
        hub.connect ("tcp://localhost:5555");
    }

    /*void notify(std::string_view name) {
        auto task = waiting[name];
        task->right -= 1;
        if(task->right == 0) {
            waiting.remove(name);
            copy
        }
    }*/

    // TODO: Send a list of tasks instead of a
    // single to make initial seeding fast.
    bool enqueue() { // enqueue a task from the hub
        if(hub == ZMQ_NULLPTR) {
            return false; // no tasks
        }
        /*sendString(hub, "MORE");
        auto task = recvProto<dwork::TaskMsg>(hub);
        // wrong test -- use task.has_value()
        if(task.name().size() == 0) { // empty task = "end"
            return false;
            // TODO: try work stealing (need root
            //    set counters to terminate correctly)
        }
        logTransition(task, dwork::TaskMsg::Stolen);
        ready.push_back( task );
        */
        return true;
    }
    // If bool == true, ret was deleted and replaced with the next task.
    // If bool == false, there are no tasks, and nothing happened.
    bool next(dwork::TaskMsg *ret) {
        if(ready.empty()) {
            if(!enqueue()) { // can't enqueue
                return false;
            }
        }
        ready.back().Swap( ret);
        ready.pop_back();
        return true;
    }
    void push(Task &&t) { // internal additions to queue are LIFO
        ready.push_back(t);
    }
    void done(Task &t) {
        for(int n=0; n<t.succ_size(); n++) {
            dwork::TaskMsg::Dep s = t.succ(n);

            /*if(s->location == self) { // FIXME: implement more locations
                removeDep(s->id);
                continue;
            }*/
            // FIXME: handle non-local returns
        }
        logTransition(t, dwork::TaskMsg::Recorded);
    }
    void removeDep(std::string_view id) {
    }

};

void *run_task(dwork::TaskMsg &task) {
    logTransition(task, dwork::TaskMsg::Started);
    /*switch(task->type) {
    case COPY:
    case RUN:
    }*/
    sleep(1);
    return NULL;
}

void *WorkerThread(void *arg) {
    zmq::context_t &context = *(zmq::context_t *) arg;

    zmq::socket_t socket (context, zmq::socket_type::req);
    socket.connect ("inproc://workers");

    dwork::TaskMsg task;

    // Repeatedly pull tasks
    while (true) {
        if(task.IsInitialized()) {
            sendProto(socket, task); // send last complete task
        } else {
            sendString(socket, "");
        }

        auto req = recvProto<dwork::TaskMsg>(socket);
        if(! req.has_value()) { // work end
            break;
        }
        task.Swap( &req.value() );

        void *ans = run_task(task);
        //db.set(task, ans); // TODO
        logTransition(task, dwork::TaskMsg::Done);
    }

    return NULL;
}

bool handle_notification(zmq::socket_t &notify) {
    zmq::message_t request;
    const auto ret = notify.recv(request, zmq::recv_flags::none);
    if(!ret.has_value()) {
        std::cout << "Received empty message on notify stream." << std::endl;
        return true;
    }

    std::cout << "Received notification" << std::endl;
    return true;
}

bool handle_worker(zmq::socket_t &workers, Manager &mgr) {
    auto task = recvProto<dwork::TaskMsg>(workers);
    if(task.has_value()) { // received prev. complete task
        std::cout << "Received complete task." << std::endl;
        mgr.done(*task);
    }

    dwork::TaskMsg t;

    bool ok = mgr.next(&t);
    if(ok) {
        sendProto(workers, t);
    } else {
        sendString(workers, "");
    }

    //dwork::QueryMsg ret;
    /*if(ok) { // task is new
        ret.set_type(dwork::QueryMsg::Transfer);
        t.Swap( ret.add_task() );
    } else { // task is untouched
        ret.set_type(dwork::QueryMsg::NotFound);
    }
    sendProto(workers, ret);*/
    return !ok;
}

int main () {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    pthread_t threads[num_threads];

    zmq::context_t context (1);
    zmq::socket_t notify(context, zmq::socket_type::rep); // completion notifications
    notify.bind("tcp://*:5556");

    zmq::socket_t workers(context, zmq::socket_type::rep); // requests for work
    workers.bind("inproc://workers");

    // Gather an initial task list
    //Manager mgr(context);
    Manager mgr(10);

    //  Launch pool of worker threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, WorkerThread, (void *) &context);
    }
    int live_workers = num_threads;

    //  Process messages from receiver and controller
    zmq::pollitem_t items [] = {
        { static_cast<void*>(notify),  0, ZMQ_POLLIN, 0 },
        { static_cast<void*>(workers), 0, ZMQ_POLLIN, 0 }
    };

    // Process incoming requests from workers
    while (live_workers > 0) {
        //  Wait for next request from client
        zmq::poll (&items [0], 2, -1);
        
        if (items[0].revents & ZMQ_POLLIN) { // handle notifications first
            if(handle_notification(notify)) {
                break;
            }
        }
        if (items[1].revents & ZMQ_POLLIN) {
            if(handle_worker(workers, mgr)) {
                //std::cout << "Error communicating with worker." << std::endl;
                //break;
                live_workers--;
            }
        }
    }

    std::cout << "Manager shutting down normally." << std::endl;

    for (int i = 0; i < num_threads; i++) {
        void *ret;
        pthread_join(threads[i], &ret);
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

