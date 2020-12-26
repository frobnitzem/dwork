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
#include <event.hh>
#include <omp.h>

#include <mpi.h>
#define MPICHECK(cmd) do {                            \
    int e = cmd;                                      \
    if( e != MPI_SUCCESS ) {                          \
      printf("Failed: MPI error %s:%d '%d'\n",        \
                             __FILE__,__LINE__, e);   \
      exit(EXIT_FAILURE);                             \
      }                                               \
  } while(0)

struct Kernel {
long iterations;
void *x;
    Kernel(long y) : iterations(y) {}
};
double execute_kernel_compute(const Kernel &k);

class Worker {
    zmq::context_t &context;
    zmq::socket_t hub;
    char hostname[256];
    const long iter;

    public:

    Worker(zmq::context_t &_context, const char *server, const long _iter)
        :  context(_context)
        ,  hub(context, zmq::socket_type::req)
        ,  iter(_iter)
    {
        hub.connect(server);
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
        long fin = 0;
        const int P = 2; // pipeline depth
        //worker_exit(); // re-queue is good practice, but requires unique hostnames (must add rank/CPU num, etc.)
        dwork::Event a[P], b[P]; // Events, named after recording process.
        std::vector<std::string> tasks[P];

        #pragma omp parallel num_threads(P)
        {
            int thread  = omp_get_thread_num();
            int threads = omp_get_num_threads();
            bool prev_done = false;
            for(long i=0; ; i++) {
                if(thread == 0) { // A
                    if(i >= P) {
                        b[i%P].wait(); // wait for i-resources (returned by B)
                        if(tasks[i%P].size() > 0)
                            complete(tasks[i%P]);
                    }
                    if(prev_done) break;

                    prev_done = steal(tasks[i%P]);
                    a[i%P].record(prev_done);
                }

                if(threads < 2 || thread == 1) { // B
                    bool done = a[i%P].wait();
                    if(done) {
                        b[i%P].record(done);
                        break;
                    }
                    for(auto &t : tasks[i%P]) { // execute tasks in serial
                        if(iter > 0) {
                            Kernel k(iter);
                            x += execute_kernel_compute(k);
                        }
                        fin++;
                    }
                    b[i%P].record(done);
                }
            }
        }
        printf("Completed %ld tasks\n", fin);
        return x;
    }
};

struct MPIH {
    int ranks, rank;
    MPI_Comm comm;

    MPIH(int *argc, char **argv[]) : comm(MPI_COMM_WORLD) {
        int provided;
        MPICHECK( MPI_Init_thread(argc, argv, MPI_THREAD_FUNNELED, &provided) );
        assert(provided >= MPI_THREAD_FUNNELED);
        MPICHECK( MPI_Comm_size( comm, &ranks) );
        MPICHECK( MPI_Comm_rank( comm, &rank ) );
    }
    ~MPIH() {
        MPI_Finalize();
    }
};

int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    const char localhost[] = "tcp://localhost:6125";
    const char *hub = localhost;
    long iter = 0;
    MPIH mpi(&argc, &argv);

    zmq::context_t context (1);
    if(argc >= 3 && !std::string("-s").compare(argv[1])) {
        hub = argv[2];
        argv[2] = argv[0];
        argc -= 2;
        argv += 2;
    }
    if(argc == 2)
        iter = atol(argv[1]);
    printf("Connecting to %s and running with iter = %ld\n", hub, iter);

    Worker w(context, hub, iter);

    MPI_Barrier(mpi.comm);
    int64_t t0 = time_in_us();
    int ret = w.run();

    MPI_Barrier(mpi.comm);
    int64_t t1 = time_in_us();
    if(mpi.rank == 0)
        printf("time (us) = %ld\n", t1-t0);

    return ret;
}
