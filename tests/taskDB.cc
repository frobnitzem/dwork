#include <iostream>
#include <taskDB.hh>
#include "test.h"

void enque(std::string_view name, void *info) {
    auto start = (std::vector<std::string> *)info;
    //start->emplace_back(name.data(), name.size());
    start->emplace_back(name);
}

void add_leaf(dwork::TaskDB &db, std::string_view x, void *info) {
    std::vector<std::string_view> deps;
    db.new_task(x, deps, enque, info);
}

void add_ternary(dwork::TaskDB &db, std::string_view x, std::string_view y, std::string_view z, void *info) {
    std::vector<std::string_view> deps;
    deps.push_back(y);
    deps.push_back(z);

    db.new_task(x, deps, enque, info);
}

void print_TaskT(const dwork::TaskT &t) {
    std::cout << "TaskT, complete = " << t.complete << std::endl;
    std::cout << "    joins = " << t.joins << std::endl;
    for(auto s : t.succ) {
        std::cout << "    succ = " << s << std::endl;
    }
}

int main(int argc, char *argv[]) {
    int test_num = 0;
    int errors = 0;

    std::vector<std::string> start;
    
    dwork::TaskDB db(1<<10);

    add_leaf(db, "B", &start);
    check("B is present", db.TH.lookup("B") == -1);

    add_leaf(db, "C", &start);
    add_ternary(db, "A", "B", "C", &start);
    add_ternary(db, "X", "A", "C", &start);

    print_TaskT( db.inspect("A") );
    print_TaskT( db.inspect("B") );
    print_TaskT( db.inspect("C") );

    db.complete_task("C", enque, &start);
    db.complete_task("B", enque, &start);
    check("A is still present", db.TH.lookup("A") == -1);

    db.complete_task("A", enque, &start);
    check("A is no longer present", db.TH.lookup("A") != -1);

    check("B starts", start.size() < 1 || start[0].compare("B"));
    check("C starts", start.size() < 2 || start[1].compare("C"));
    check("A starts", start.size() < 3 || start[2].compare("A"));
    check("X starts", start.size() < 4 || start[3].compare("X"));
    check("All tasks started", start.size() != 4);

    check("one active task", db.TH.active() != 1);

    return errors;
}
