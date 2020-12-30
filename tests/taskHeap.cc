#include <stdio.h>
#include <taskHeap.hh>
#include <unistd.h>

#include "test.h"

// offset between names to lookup and sequential task numbers
const size_t offset = 5;

std::string task(size_t n) {
    return tkrzw::IntToStrBigEndian(n+offset);
}

// match (no offset)
int match(std::string_view x, size_t n) {
    return tkrzw::StrToIntBigEndian(x) == n;
}

int main(int argc, char *argv[]) {
    int test_num = 0;
    int errors = 0;
    size_t ntasks = 0;
    std::string fname;

    int opt;
    while ( (opt = getopt(argc, argv, "f:")) != -1) {
        switch (opt) {
        case 'f':
            fname = optarg;
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-f dbfile]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (argc - optind != 0) {
        fprintf(stderr, "Expected no arguments\n");
        return EXIT_FAILURE;
    }

    dwork::TaskHeap H(1<<20, "local");
    std::vector<size_t> alloc(1<<20);

    for(int i=0; i<(1<<19); i++) {
        alloc[i] = H.new_task( task(i) );
        ntasks++;
    }
    check("new active tasks", H.active() != (1<<19));

    for(int i=0; i < (1<<19); i+=10) {
        H.delete_task( H[alloc[i]].name );
        ntasks--;
    }
    printf("%ld active tasks (expect %ld).\n", H.active(), ntasks);

    check("deleted tasks removed from active", H.active() != ntasks);
    check("H[49].name", !match(H[alloc[49]].name, 49+offset));
    check("lookup(49)", H.lookup(task(49)) != 49);

    for(int i=(1<<19); i< (1<<20); i++) {
        alloc[i] = H.new_task( task(i) );
        ntasks++;
    }
    printf("%ld active tasks.\n", H.active());
    check("active tasks increase on add", H.active() != ntasks);

    for(int i=1; i < (1<<20); i+=20) {
        H.delete_task( H[alloc[i]].name );
        ntasks--;
    }
    printf("%ld active tasks (expect %ld).\n", H.active(), ntasks);
    check("deleted tasks removed from active", H.active() != ntasks);

    check("H[6512].name", !match(H[alloc[6512]].name, 6512+offset));
    check("lookup(6512)", H.lookup(task(6512)) != 6512);

    return errors;
}
