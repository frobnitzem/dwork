/*
    Multithreaded Dwork Hub
*/

#include <unistd.h>
#include <cassert>
#include <iostream>
#include <stdio.h>

#include <proto.hh>

int sendrecv(zmq::socket_t &hub, dwork::QueryMsg &query) {
    std::cout << "Sending:" << std::endl << query.DebugString() << std::endl;

    return 0;
}

int create(zmq::socket_t &hub, int argc, char *argv[]) {
    if(argc < 3) {
        std::cout << "Error in " << argv[1] << ": task name required." << std::endl;
        return 1;
    }
    dwork::QueryMsg query;
    dwork::TaskMsg *task = query.add_task();

    query.set_type(dwork::QueryMsg::Create);
    task->set_name(argv[2]);
    for(int i=3; i<argc; i++) {
        dwork::TaskMsg::Dep *dep = task->add_pred();
        dep->set_name(argv[i]);
    }

    return sendrecv(hub, query);
}

int steal(zmq::socket_t &hub, int argc, char *argv[]) {
    dwork::QueryMsg query;
    query.set_type(dwork::QueryMsg::Steal);

    return sendrecv(hub, query);
}

int complete(zmq::socket_t &hub, int argc, char *argv[]) {
    if(argc != 3) {
        std::cout << "Error in " << argv[1] << ": task name required." << std::endl;
        return 1;
    }
    dwork::QueryMsg query;
    dwork::TaskMsg *task = query.add_task();

    query.set_type(dwork::QueryMsg::Transfer);
    task->set_name(argv[2]);

    return sendrecv(hub, query);
}

int worker_exit(zmq::socket_t &hub, int argc, char *argv[]) {
    char hostname[256];
    char *host = hostname;
    if(argc > 2) {
        host = argv[2];
    } else if(gethostname(hostname, sizeof(hostname))) {
        perror("Error in gethostname");
        return 1;
    }
    dwork::QueryMsg query;

    query.set_type(dwork::QueryMsg::Exit);
    query.set_location(host);
    return sendrecv(hub, query);
}


typedef int (*action_f)(zmq::socket_t &hub, int argc, char *argv[]);
struct Command {
    action_f run;
    std::string help;

    Command() {}
    Command(action_f _run, std::string_view _help) : run(_run), help(_help) {}
};


int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::map<std::string, Command> action{
        {"create",   Command(create,   "task [dep1] [dep2] ...")}
    ,   {"steal",    Command(steal,    "")}
    ,   {"complete", Command(complete, "task")}
    ,   {"exit",     Command(worker_exit, "[hostname]")}
    };

    zmq::context_t context (1);
    zmq::socket_t  hub(context, zmq::socket_type::req);
    hub.connect ("tcp://localhost:5555");

    // No args at all
    if(argc < 2) {
        std::cout << "Usage: " << argv[0] << " [-h] <cmd name>" << std::endl;
        return 1;
    }

    // Help for arguments
    if(!std::string("-h").compare(argv[1])) {
        if(argc == 2) {
            std::cout << "Commands:" << std::endl;
            for(auto c : action) {
                std::cout << "    " << c.first << " " << c.second.help << std::endl;
            }
            return 0;
        }
        Command &c = action[ argv[2] ];
        if(c.run == NULL) {
            std::cout << "Invalid command: " << argv[1] << std::endl;
            return 1;
        }
        std::cout << "    " << argv[2] << " " << c.help << std::endl;
        return 0;
    }

    Command &c = action[ argv[1] ];
    if(c.run == NULL) {
        std::cout << "Invalid command: " << argv[1] << std::endl;
        return 1;
    }
    return c.run(hub, argc, argv);
}

