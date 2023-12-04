/* Create a tree of local task-list servers that forward
 * requests to the root.
 */
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <map>
#include <system_error>
#include <stdint.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

struct Topology {
    char hostname[HOST_NAME_MAX];
    bool is_root, is_forwarder;
    char parent[HOST_NAME_MAX];
    int rank, ranks;

    Topology(MPI_Comm comm) : is_root(false), is_forwarder(false) {
        int ret;

        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &ranks);
        char *hosts = (char *)malloc(HOST_NAME_MAX*ranks);
        if( (ret = gethostname(hostname, HOST_NAME_MAX)) != 0) {
            //throw std::system_error(ret); //, "unable to gethostname");
            throw ret;
        }
        MPI_Allgather(hostname, HOST_NAME_MAX, MPI_BYTE,
            hosts, HOST_NAME_MAX, MPI_BYTE, comm);

        gather_topol(hosts);

        free(hosts);
    }

    void gather_topol(char *hosts) {
        // map from rack-ID to rank of first host with that ID
        std::map<uint32_t, int> lead;
        for(int i=0; i<ranks; i++) {
            uint32_t k = to_rack(hosts+i*HOST_NAME_MAX);
            lead.emplace(k, i);
        }

        /* print
        for (const auto& [key, value] : lead) {
            std::cout << key << " = " << value << "; ";
        }
        std::cout << "\n"; */

        // Determine self-role:
        uint32_t k = to_rack(hosts + rank*HOST_NAME_MAX);
        int uplev = lead[k]; // which rank is lead for my rack?
        if(uplev == rank) { // me! => forwarder
            is_forwarder = true;
            memcpy(parent, hosts, HOST_NAME_MAX); // parent = rank 0
        } else {
            memcpy(parent, hosts + uplev*HOST_NAME_MAX, HOST_NAME_MAX);
        }

        if(rank == 0) {
            is_root = true;
            is_forwarder = false;
        }
    }

    // convert from a hostname to its rack-id (use first 3 chars)
    uint32_t to_rack(char *host) {
        const int N = 3;
        uint32_t x = 0;
        for(int i=N-1; i>=0; i--) {
            x = (x<<8) + (unsigned int)host[i];
        }
        return x;
    }
};

/*
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    Topology top(MPI_COMM_WORLD);

    if(top.is_root) {
        std::cout << "Root node: ";
    }
    if(top.is_forwarder) {
        std::cout << "Forwarding node: ";
    }
    std::cout << top.rank << ": " << top.hostname << " ~> " << top.parent << std::endl;

    MPI_Finalize();
}*/
