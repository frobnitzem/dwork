#include <stdio.h>
#include <stdlib.h>
#include <tkrzw_cmd_util.h>
#include <tkrzw_dbm_tiny.h>

class TaskDB {
  public:
    TaskDB() : dbm(1024) {};
    void set(std::string_view key, void *loc) {
        char *y;
        if(asprintf(&y, "%llx", (long long)loc) <= 0) {
            exit(1);
        }
        dbm.Set(key, y);
        free(y);
    }
    void *get(std::string_view key) {
        std::string y = dbm.GetSimple(key, "");
        if(y.size() == 0) {
            return NULL;
        }
        return (void *)strtoull(&y[0], NULL, 16);
    }

  protected:
    tkrzw::TinyDBM dbm;
};

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
