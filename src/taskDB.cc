#include <stdio.h>
#include <stdlib.h>

#include <taskDB.hh>

namespace dwork {

TaskT::TaskT()      : complete(true), joins(0) {}
TaskT::TaskT(int j) : complete(false), joins(j) {}

TaskT::TaskT(std::string_view x) :
            complete(x.size() == 0),
            joins(complete ? 0 : (int64_t)tkrzw::StrToIntBigEndian(x)),
            succ(complete ? 0 : x.size()/sizeof(TaskID)-1) {
        const size_t n = succ.size();
        for(size_t i=0; i<n; i++) {
            succ[i] = tkrzw::StrToIntBigEndian( x.substr((i+1)*sizeof(TaskID)) );
        }
}

std::string TaskT::toStr() {
    std::string x;
    if(complete) return x;

    x.reserve(sizeof(TaskID)*(succ.size()+1));
    x.append(tkrzw::IntToStrBigEndian((uint64_t)joins, sizeof(TaskID)));
    const size_t n = succ.size();
    for(size_t i=0; i<succ.size(); i++) {
        x.append(tkrzw::IntToStrBigEndian(succ[i], sizeof(TaskID)));
    }
    return x;
}

/**
* Record processor to add/incr task join counter
*/
class IncrJoin final : public tkrzw::DBM::RecordProcessor {
  public:
    /**
     * Constructor.
     * @param incr       The increment.
     * @param current    (output) value after incrementing
     */
    IncrJoin(int64_t incr, int64_t* current = nullptr)
        : incr_(incr), current_(current) {}

    /**
     * Processes an existing record.
     */
    std::string_view ProcessFull(std::string_view key, std::string_view value) override {
        int64_t num = incr_;
        if (value.size() != 0) { // 0 if completed already. this will re-queue
            num += tkrzw::StrToIntBigEndian(value);
        }
        if (current_ != nullptr) *current_ = num;

        new_value_.resize(0);
        new_value_.reserve( value.size() < sizeof(int64_t) ?
                            sizeof(int64_t) : value.size() );
        new_value_.append(tkrzw::IntToStrBigEndian(num));
        if(value.size() > sizeof(int64_t)) {
            new_value_.append(value.substr(sizeof(int64_t)));
        }
        return new_value_;
    }

    /**
     * Processes an empty record space.
     */
    std::string_view ProcessEmpty(std::string_view key) override {
        if(current_ != nullptr) *current_ = incr_;
        new_value_ = tkrzw::IntToStrBigEndian(incr_); // re-queue
        return new_value_;
    }

   private:
    int64_t incr_;
    int64_t *current_;
    std::string new_value_;
};

/**
* Record processor to add successor
*/
class AddSucc final : public tkrzw::DBM::RecordProcessor {
  public:
    /**
     * Constructor.
     * @param succ    The successor to enable.
     * @param join    Incremented for every successful addition.
     */
    AddSucc(std::string_view succ, int64_t *join) : succ_(succ), join_(join) {}

    /**
     * Processes an existing record.
     */
    std::string_view ProcessFull(std::string_view key, std::string_view value) override {
        if (value.size() == 0) { // completed already
            return NOOP;
        }
        *join_ += 1;
        new_value_.resize(0);
        new_value_.reserve(value.size() + succ_.size());
        new_value_.append(value);
        new_value_.append(succ_);
        return new_value_;
    }

    /**
     * Processes an empty record space.
     */
    std::string_view ProcessEmpty(std::string_view key) override {
        return NOOP;
    }

   private:
    /** The successor id. */
    std::string_view succ_;
    std::string new_value_;
    int64_t *join_;
};

/**
 * Record processor to retire a task.
 */
class RetireTask final : public tkrzw::DBM::RecordProcessor {
public:
    /**
     * Constructor.
     * @param task  Task to receive the result via std::move
     */
    explicit RetireTask(TaskT &task) : task_(task) {}

    /**
     * Processes an existing record.
     */
    std::string_view ProcessFull(std::string_view key, std::string_view value) override {
      task_ = TaskT(value);
      assert(task_.joins == 0); // Task was somehow enqueued during execution...
      return REMOVE;
    }

    /**
     * Processes an empty record space.
     */
    std::string_view ProcessEmpty(std::string_view key) override {
      task_ = TaskT("");
      return NOOP;
    }

   private:
    /** Status to report. */
    TaskT &task_;
};

TaskDB::TaskDB(size_t sz) : TH(sz), tasks(sz) { }

// FIXME: deal with the case where an identical task was detected in the DB
TaskID TaskDB::new_task(std::string_view name, std::vector<std::string_view> deps,
                         void (*enque)(std::string_view, void *), void *info) {
    // lookup to find existing key
    TaskID n = TH.lookup(name); // FIXME: replace with new_task, but allow lookup too
    if(n == -1) { // add new task
        n = TH.new_task(name);
    }
    std::string key = tkrzw::IntToStrBigEndian(n);

    IncrJoin P(deps.size());
    tasks.Process(key, &P, true);

    int64_t fixup = add_deps(n, deps) - deps.size();
    int64_t remain = deps.size();
    if(fixup != 0) { // remove refs to account for unsuccessful add_deps
            IncrJoin Pf(fixup, &remain);
            tasks.Process(key, &Pf, true);
    }
    if(remain == 0) { // task has been made newly ready
        enque(name, info);
    }
    return n;
}

/** Returns number of dependencies with newly installed successor hooks. */
int64_t TaskDB::add_deps(TaskID n, std::vector<std::string_view> deps) {
    std::string key = tkrzw::IntToStrBigEndian(n);
    int64_t join = 0;
    AddSucc P(key, &join);

    for(auto dep : deps) {
        TaskID d = TH.lookup(dep);
        if(d == -1) {
            continue;
        }
        tasks.Process(tkrzw::IntToStrBigEndian(d), &P, true);
    }
    return join;
}

/** return a copy of the task DB state - for debugging */
TaskT TaskDB::inspect(std::string_view name) {
    TaskID n = TH.lookup(name);
    if(n == -1) { // lookup error
        return TaskT{};
    }
    std::string key = tkrzw::IntToStrBigEndian(n);

    std::string val;
    if(!tasks.Get(key, &val).IsOK()) {
        return TaskT{};
    }
    return TaskT(val);
}

int64_t TaskDB::complete_task(std::string_view name,
                              void (*enque)(std::string_view, void *), void *info) {
    TaskID n = TH.lookup(name);
    if(n == -1) { // lookup error
        return -1;
    }
    std::string key = tkrzw::IntToStrBigEndian(n);
    TaskT task{};
    RetireTask R(task);
    tasks.Process(key, &R, true);

    TH.delete_task(name);

    int64_t remain;
    IncrJoin P(-1, &remain);
    // send a join notice to ea. successor
    for(auto succ : task.succ) {
        std::string s = tkrzw::IntToStrBigEndian(succ);
        tasks.Process(s, &P, true);
        if(remain == 0) {
            enque( TH[succ].name, info );
        }
    }

    return 0;
}

}

/*
int main() {
    int A = 12;
    char B[4];
    TaskDB db = TaskDB();
    db.set("A", &A);
    db.set("B", B);
    int *C = (int *)db.get("A");
    char *D = (char *)db.get("B");
    printf("%d %d\n", &A == C, B == D);
}*/
