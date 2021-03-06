#ifndef _TASK_HEAP_HH
#define _TASK_HEAP_HH

#include <vector>
#include <tkrzw_dbm_tiny.h>
#include <stdexcept>

#include <mutex>

namespace dwork {
#define TASK_BLK 16

inline int64_t to_int(std::string_view x) { return (int64_t)tkrzw::StrToIntBigEndian(x); }

struct TaskInfo {
    std::string name;
};
/** This double-indirection is to prevent moving
 *  the underlying tasks when expand() is called on the TaskHeap.
 */
struct TaskBlk {
    std::vector<TaskInfo> grp;
    TaskBlk() : grp(TASK_BLK) {};
};

/** A thread-safe heap of tasks.
 */
class TaskHeap {
  public:
    /** Can throw std::runtime_error on file open errors (only if prefix != "") */
    TaskHeap(size_t sz, std::string_view prefix="", bool recover=false);
    ~TaskHeap();
    void shutdown(); ///< save and close DB file

    ///< lookup task number from a name (returns -1 on error)
    size_t lookup(std::string_view name);

    ///< lookup taskInfo from a number
    TaskInfo &operator[](size_t n) {
        return tasks[n/TASK_BLK].grp[n%TASK_BLK];
    }
    const TaskInfo &operator[](size_t n) const {
        return tasks[n/TASK_BLK].grp[n%TASK_BLK];
    }

    size_t new_task(std::string_view name);  ///< assign this task a number (or retrieve if existing)
    void delete_task(std::string_view name); ///< caller must ensure no refs to this number remain
    size_t active(); ///< returns number of active tasks

  protected:
    void expand(); ///< caller must ensure only one thread calls this

    std::mutex mtx;
    std::vector<TaskBlk> tasks;
    std::vector<size_t>  flist;
    tkrzw::TinyDBM ids;
    size_t               nfree;
};

}
#endif

