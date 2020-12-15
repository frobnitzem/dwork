/** Debug utilities for printing
 *  QueryMsg-s, TaskMsg-s, and LogMsg-s
 */
#include <proto.hh>

namespace dwork {
std::ostream &operator<<(std::ostream &os, const TaskMsg::State &s) {
    switch(s) {
    case TaskMsg::Pending:
        return os << "Pending";
    case TaskMsg::Stolen:
        return os << "Stolen";
    case TaskMsg::Waiting:
        return os << "Waiting";
    case TaskMsg::Copying:
        return os << "Copying";
    case TaskMsg::Ready:
        return os << "Ready";
    case TaskMsg::Started:
        return os << "Started";
    case TaskMsg::Done:
        return os << "Done";
    case TaskMsg::Recorded:
        return os << "Recorded";
    default:
        break;
    }
    return os << "Unknown";
}

std::ostream &operator<<(std::ostream &os, const TaskMsg::Dep &d) {
    return os << d.name() << " @ " << d.location();
}

std::ostream &operator<<(std::ostream &os, const TaskMsg::LogMsg &l) {
    return os << l.state() << " @ " << l.time();
}

std::ostream &operator<<(std::ostream &os, const TaskMsg &t) {
    os << "TaskMsg " << t.name() << " from " << t.origin() << ":" << std::endl;
    for(const auto p : t.pred()) {
        os << "  pred: " << p << std::endl;
    }
    for(const auto s : t.succ()) {
        os << "  succ: " << s << std::endl;
    }
    for(const auto l : t.log()) {
        os << "  log: " << l << std::endl;
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, const QueryMsg::Type &s) {
    switch(s) {
    case QueryMsg::Create:
        return os << "Create";
    case QueryMsg::Steal:
        return os << "Steal";
    case QueryMsg::Complete:
        return os << "Complete";
    case QueryMsg::Transfer:
        return os << "Transfer";
    case QueryMsg::Lookup:
        return os << "Lookup";
    case QueryMsg::NotFound:
        return os << "NotFound";
    case QueryMsg::OK:
        return os << "OK";
    case QueryMsg::Exit:
        return os << "Exit";
    case QueryMsg::Error:
        return os << "Error";
    default:
        break;
    }
    return os << "Unknown";
}

std::ostream &operator<<(std::ostream &os, const QueryMsg &q) {
    os << "QueryMsg " << q.type() << ":" << std::endl;
    if(q.has_name()) {
        os << "  name: " << q.name() << std::endl;
    }
    if(q.has_location()) {
        os << "  location: " << q.location() << std::endl;
    }
    if(q.has_n()) {
        os << "  n: " << q.n() << std::endl;
    }
    if(q.task_size() == 0)
        return os;
    os << "---------------------------" << std::endl;
    for(const auto t : q.task()) {
        os << t;
    }
    os << "---------------------------" << std::endl;
    return os;
}

}
