#include <taskHeap.hh>

namespace dwork {

size_t TaskHeap::active() {
    return tasks.size()*TASK_BLK - nfree;
}

void TaskHeap::expand() { // caller must ensure only one thread calls this
    int sz = tasks.size();
    int end = sz == 0 ? 512 : sz*2;
    tasks.resize(end);
    flist.resize(end*TASK_BLK);

    size_t k = end*TASK_BLK-1; // first index to push onto flist
    for(size_t i=sz; i<end; i++) {
        for(int j=0; j<TASK_BLK; j++) {
            flist[nfree+j] = k-j;
        }
        nfree += TASK_BLK;
        k -= TASK_BLK;
    }
}
TaskHeap::TaskHeap(size_t sz) : ids(sz), nfree(0) {
    expand();
}
size_t TaskHeap::lookup(std::string_view name) {
    std::string n = ids.GetSimple(name, "");
    if(n.size() == 0)
        return -1;
    return tkrzw::StrToIntBigEndian(n);
}

size_t TaskHeap::new_task(std::string_view name) {
    size_t n;
    {
        std::lock_guard<std::mutex> lck(mtx);
        if(nfree == 0) {
            expand();
        }
        n = flist[--nfree];
    }
    std::string val = tkrzw::IntToStrBigEndian(n, sizeof(uint64_t));
    if( !ids.Set(name, val, false).IsOK() ) {
        // FIXME: record exists - race condition.
        std::lock_guard<std::mutex> lck(mtx);
        flist[nfree++] = n;
        return lookup(name);
    }
    (*this)[n].name = name;

    return n;
}

void TaskHeap::delete_task(std::string_view name) {
    size_t n = lookup(name);
    if(n == -1) return;
    ids.Remove(name);

    std::lock_guard<std::mutex> lck(mtx);
    flist[nfree++] = n;
}

}
