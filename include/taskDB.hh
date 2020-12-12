#ifndef _TASK_DB_HH
#define _TASK_DB_HH

#include <taskHeap.hh>
#include <tkrzw_cmd_util.h>
#include <tkrzw_str_util.h>

#include <assert.h>

namespace dwork {
using TaskID = uint64_t;

// Task that can be serialized and deserialized from hash-tbl
struct TaskT {
    bool complete; ///< Is this task already completed?
    int64_t joins; ///< Number of pending joins
    std::vector<TaskID> succ; /// List of successors to notify on completion

    TaskT();
    TaskT(int j);
    TaskT(std::string_view x);
    std::string toStr();
};

/**
 * The TaskDB implements a lookup table from names to TaskID-s.
 *
 * TaskID-s are assigned sequentially.
 *
 * It is an error to delete a task which may still have references,
 * or to reference a task which has already been deleted.
 *
 */
class TaskDB {
  public:
    TaskDB(size_t sz);
    TaskID new_task(std::string_view name, std::vector<std::string_view> deps,
                     void (*enque)(std::string_view, void *), void *);
    int64_t complete_task(std::string_view name,
                     void (*enque)(std::string_view, void *), void *);

    /** return a copy of the task DB state - for debugging */
    TaskT inspect(std::string_view name);

    /** Returns number of dependencies with newly installed successor hooks. */
    int64_t add_deps(TaskID n, std::vector<std::string_view> deps);

    TaskHeap       TH;
  protected:
    tkrzw::TinyDBM tasks;
};

}
#endif
