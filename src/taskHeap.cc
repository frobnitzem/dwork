#include <taskHeap.hh>
#define INITIAL_TASKHEAP (512)

namespace dwork {

/**
* Record processor to lookup existing or set to n if not found.
*/
class GetOrSet final : public tkrzw::DBM::RecordProcessor {
  public:
    /**
     * Constructor.
     * @param n          The number to set if not found.
     * @param current    (output) value after lookup or set.
     */
    GetOrSet(int64_t n, int64_t* current = nullptr)
        : n_(n), current_(current) {}

    /**
     * Processes an existing record.
     */
    std::string_view ProcessFull(std::string_view key, std::string_view value) override {
        if (value.size() != 0) { // 0 if completed already. this will re-queue
            if (current_ != nullptr)
                *current_ = tkrzw::StrToIntBigEndian(value);
            return NOOP;
        }
        if (current_ != nullptr)
            *current_ = n_;
        new_value_.reserve( sizeof(int64_t) );
        new_value_.append(tkrzw::IntToStrBigEndian(n_));

        return new_value_;
    }

    /**
     * Processes an empty record space.
     */
    std::string_view ProcessEmpty(std::string_view key) override {
        if (current_ != nullptr)
            *current_ = n_;
        new_value_.reserve( sizeof(int64_t) );
        new_value_.append(tkrzw::IntToStrBigEndian(n_));

        return new_value_;
    }

   private:
    int64_t n_;
    int64_t *current_;
    std::string new_value_;
};

size_t TaskHeap::active() {
    return tasks.size()*TASK_BLK - nfree;
}

void TaskHeap::expand() { // caller must ensure only one thread calls this
    int sz = tasks.size();
    int end = sz == 0 ? INITIAL_TASKHEAP : sz*2;
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
TaskHeap::TaskHeap(size_t sz, std::string_view prefix, bool recover) : ids(sz), nfree(0) {
    const int32_t options = recover ? tkrzw::File::OPEN_DEFAULT : tkrzw::File::OPEN_TRUNCATE;
    expand();

    if(prefix.size() == 0) return;
    std::string fname{prefix};
    fname.append("_th.flat");

    tkrzw::Status status = ids.Open(fname, true, options);
    if(status != tkrzw::Status::SUCCESS) {
        throw std::runtime_error("Opening "+fname+" failed: "+status.GetMessage());
    }
    // TODO: use IsHealthy
    if(recover) {
        /* Read all task names */
        auto iter = ids.MakeIterator();
        std::string key, val;
        for(iter->First(); iter->Get(&key, &val) == tkrzw::Status::SUCCESS; iter->Next()) {
            int64_t n = to_int(val);
            while(nfree <= n) {
                expand();
            }
            int64_t end = INITIAL_TASKHEAP*TASK_BLK; // end of segment containing n
            while(n >= end) {
                end *= 2;
            }
            int64_t start = end == INITIAL_TASKHEAP*TASK_BLK ? 0 : end/2;
            // flist from start to end is stored reversed
            printf("n=%ld start=%ld end=%ld flist=%ld\n", n, start, end, flist[start + end-n-1]);
            flist[start + end-n-1] = -1;
            (*this)[n].name = key;
        }
        int64_t used = 0; // compress flist
        for(int64_t i = 0; i<flist.size(); i++) {
            if(flist[i] == -1) {
                used++;
                continue;
            }
            flist[i-used] = flist[i];
        }
        nfree -= used;
    }
}
TaskHeap::~TaskHeap() {
    shutdown();
}
void TaskHeap::shutdown() {
    if(ids.IsOpen()) {
        ids.Close();
    }
}
size_t TaskHeap::lookup(std::string_view name) {
    std::string n = ids.GetSimple(name, "");
    if(n.size() == 0)
        return -1;
    return tkrzw::StrToIntBigEndian(n);
}

size_t TaskHeap::new_task(std::string_view name) {
    int64_t n, val;
    {
        std::lock_guard<std::mutex> lck(mtx);
        if(nfree == 0) {
            expand();
        }
        n = flist[--nfree];
    }
    GetOrSet P(n, &val);
    ids.Process(name, &P, true);

    if( n == val ) { // success, store name
        (*this)[n].name = name;
    } else { // record exists, replace n
        std::lock_guard<std::mutex> lck(mtx);
        flist[nfree++] = n;
    }

    return val;
}

void TaskHeap::delete_task(std::string_view name) {
    size_t n = lookup(name);
    if(n == -1) return;
    ids.Remove(name);

    std::lock_guard<std::mutex> lck(mtx);
    flist[nfree++] = n;
}

}
