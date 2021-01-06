/*
    Multithreaded Dwork Hub
*/

#include <unistd.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>

#include <tkrzw_str_util.h>
#include <proto.hh>

dwork::TaskMsg *new_task(dwork::QueryMsg &query, const std::string &name) {
    dwork::TaskMsg *task = query.add_task();

    char hostname[256];
    if(gethostname(hostname, sizeof(hostname))) {
        perror("Error in gethostname");
        exit(1);
    }
    task->set_origin(hostname);
    task->set_name(name);

    return task;
}

int sendrecv(zmq::socket_t &hub, dwork::QueryMsg &query, bool show=true) {
    if(show)
        std::cout << " - sending -" << std::endl
                  << query      << std::endl;
    sendProto(hub, query);
    auto resp = recvProto<dwork::QueryMsg>(hub);
    if(!resp.has_value()) {
        std::cout << " - received unparsable query -" << std::endl;
        return 1;
    }
    if(show)
        std::cout << " - received -"  << std::endl
                  << resp.value() << std::endl;

    query.clear_location();
    query.clear_task();
    query.clear_n();

    return 0;
}

int create(zmq::socket_t &hub, int argc, char *argv[]) {
    if(argc < 3) {
        std::cout << "Error in " << argv[1] << ": task name required." << std::endl;
        return 1;
    }

    dwork::QueryMsg query;
    query.set_type(dwork::QueryMsg::Create);
    dwork::TaskMsg *task = new_task(query, argv[2]);
    for(int i=3; i<argc; i++) {
        dwork::TaskMsg::Dep *dep = task->add_pred();
        dep->set_name(argv[i]);
    }

    return sendrecv(hub, query);
}

int addfile(zmq::socket_t &hub, int argc, char *argv[]) {
    const int blksz = 16; // add tasks in blocks of 16 // add tasks in blocks of 16

    if(argc != 3) {
        std::cout << "Error in " << argv[1] << ": filename required." << std::endl;
        return 1;
    }
    std::ifstream file(argv[2]);
    if(!file.is_open()) {
        std::cout << "Error opening file " << argv[2] << std::endl;
        return 1;
    }

    dwork::QueryMsg query;
    query.set_type(dwork::QueryMsg::Create);

    int added = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> tok = tkrzw::StrSplit(line, ',', true);
        if(tok.size() == 0) continue;

        dwork::TaskMsg *task = new_task(query, tok[0]);
        for(int i=1; i<tok.size(); i++) {
            dwork::TaskMsg::Dep *dep = task->add_pred();
            dep->set_name(tok[i]);
        }
        
        added++;
        if(added % blksz == 0) {
            if( sendrecv(hub, query, false) ) return 1;
        }
    }
    if(added % blksz != 0) {
        if( sendrecv(hub, query, false) ) return 1;
    }

    return 0;
}

int complete(zmq::socket_t &hub, int argc, char *argv[]) {
    if(argc != 3) {
        std::cout << "Error in " << argv[1] << ": task name required." << std::endl;
        return 1;
    }
    dwork::QueryMsg query;
    query.set_type(dwork::QueryMsg::Complete);
    dwork::TaskMsg *task = new_task(query, argv[2]);
    query.set_location(task->origin());

    return sendrecv(hub, query);
}

dwork::QueryMsg loc_msg(dwork::QueryMsg::Type type, char *host) {
    char hostname[256];
    if(host == NULL) {
        host = hostname;
        if(gethostname(hostname, sizeof(hostname))) {
            perror("Error in gethostname");
            exit(1);
        }
    }
    dwork::QueryMsg query;

    query.set_type(type);
    query.set_location(host);
    return query;
}

int steal(zmq::socket_t &hub, int argc, char *argv[]) {
    char *host = NULL;
    bool quiet = false;
    int n = 0;
    if(argc > 3 && argc <= 5) {
        if(!std::string("-n").compare(argv[2])) {
            n = atoi(argv[3]);
        } else {
            std::cout << "Error in " << argv[1] << ": invalid args" << std::endl;
            return 1;
        }
        if(argc == 5) {
            host = argv[4];
        }
    } else if(argc == 3) {
        if(!std::string("-q").compare(argv[2])) {
            dwork::QueryMsg query = loc_msg(dwork::QueryMsg::Steal, host);
            quiet = true;
            sendProto(hub, query);
            auto resp = recvProto<dwork::QueryMsg>(hub);
            if(!resp.has_value()) {
                std::cout << "Received unparsable query." << std::endl;
                exit(1);
            }
            if(resp->task_size() <= 0) {
                std::cout << "No task" << std::endl;
                exit(1);
            }
            std::cout << resp->task(0).name() << std::endl;
            exit(0);
        }
        host = argv[2];
    } else if(argc != 2) {
        std::cout << "Error in " << argv[1] << ": invalid args" << std::endl;
        return 1;
    }
    dwork::QueryMsg query = loc_msg(dwork::QueryMsg::Steal, host);
    if(n > 0)
        query.set_n(n);
    return sendrecv(hub, query);
}

int worker_exit(zmq::socket_t &hub, int argc, char *argv[]) {
    dwork::QueryMsg query = loc_msg(dwork::QueryMsg::Exit, argc > 2 ? argv[2] : NULL);
    return sendrecv(hub, query);
}

int status(zmq::socket_t &hub, int argc, char *argv[]) {
    dwork::QueryMsg query;
    query.set_type(dwork::QueryMsg::OK);
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
    ,   {"addfile",  Command(addfile,  "<filename>")}
    ,   {"steal",    Command(steal,    "[-n <num>] [hostname]")}
    ,   {"complete", Command(complete, "task")}
    ,   {"exit",     Command(worker_exit, "[hostname]")}
    ,   {"status",   Command(status, "")}
    };

    zmq::context_t context (1);
    zmq::socket_t  hub(context, zmq::socket_type::req);
    const char local[] = "tcp://localhost:6125";
    const char *dhub = local;
    if(argc >= 3 && !std::string("-s").compare(argv[1])) {
        dhub = argv[2];
        argv[2] = argv[0];
        argc -= 2;
        argv += 2;
    }

    // No args at all
    if(argc < 2) {
        std::cout << "Usage: " << argv[0] << " [-s server] [-h] <cmd name>" << std::endl;
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

    hub.connect (dhub);
    Command &c = action[ argv[1] ];
    if(c.run == NULL) {
        std::cout << "Invalid command: " << argv[1] << std::endl;
        return 1;
    }
    int64_t t0 = time_in_us();
    int ret = c.run(hub, argc, argv);
    int64_t t1 = time_in_us();
    printf("query time (us) = %ld\n", t1-t0);
    return ret;
}

