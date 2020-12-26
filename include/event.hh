#include <unistd.h>
#include <stdio.h>
#include <mutex>

namespace dwork {
struct Event {
    Event() : val(0) {
        sync.lock();
    }
    Event(const Event &) = delete;
    Event &operator=(Event &) = delete;

    void record(int v) {
        val = v;
        sync.unlock();
    }
    int wait() {
        sync.lock(); // will not succeed until record call has completed
        return val;
    }

    int val; // passed from sender to receiver
    std::mutex sync;
};
}
