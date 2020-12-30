#ifndef _TASK_DB_HH
#define _TASK_DB_HH

#include <taskHeap.hh>
#include <tkrzw_cmd_util.h>
#include <tkrzw_str_util.h>

#include <assert.h>

namespace dwork {
using TaskID = uint64_t;

/** Task that can be serialized and deserialized from hash-tbl */
struct TaskT {
    bool complete; ///< Is this task already completed?
    int64_t joins; ///< Number of pending joins
    std::vector<TaskID> succ; /// List of successors to notify on completion

    TaskT(); ///< Creates a completed task.
    TaskT(int j); ///< Creates a task with 'j joins
    TaskT(std::string_view x); ///< Creates a task from a DB-stored string
    std::string toStr();       ///< serialize to a string
};

/**
 * The TaskDB implements a lookup table from names to TaskID-s.
 *
 * The names are user-defined, but should not overlap or else
 * their tasks will not be distinct.
 *
 * Internally, the TaskDB assigns tasks a TaskID by which
 * it will look up the Task's TaskT record.
 *
 * TaskT records have a join counter and enumerate the task's successors.
 *
 */
class TaskDB {
  public:
    /** Create a TaskDB with hash-table sizes sz */
    TaskDB(size_t sz, std::string_view prefix = "",
           void (*enque)(std::string_view, void *) = NULL, void *info = NULL);
    ~TaskDB();
    void shutdown(); ///< save and close DB file

    /** Add a new task to the DB.
     *
     * @param name   unique name by which the TaskDB will identify the task  
     * @param deps   list of dependencies which must complete before this one
     * @param enque  callback to enqueue successors (if ready)
     * @param info   extra info passed through to enque
     */
    TaskID new_task(std::string_view name, std::vector<std::string_view> deps,
                     void (*enque)(std::string_view, void *), void *);

    /** Mark the task as completed.
     *
     * This deletes the task from the TaskDB,
     * Returns number of dependencies with newly installed successor hooks.
     *
     * It is an error to complete a task which is a successor of another task.
     * Luckily, this is impossible as long as the task was started after
     * its join counter reached zero.
     *
     * This implementation assumes all non-existing tasks are completed.
     *
     * @param name    task name
     * @param enque   callback to enqueue successors (if ready)
     * @param info    extra info passed through to enque
     */
    int64_t complete_task(std::string_view name,
                     void (*enque)(std::string_view, void *), void *info);

    /** return a copy of the task DB state - for debugging */
    TaskT inspect(std::string_view name);

    /** Returns number of dependencies with newly installed successor hooks. */
    int64_t add_deps(TaskID n, std::vector<std::string_view> deps);

    /** Heap associating names to TaskID-s */
    TaskHeap       TH;

  protected:
    tkrzw::TinyDBM tasks;
};

}
#endif
