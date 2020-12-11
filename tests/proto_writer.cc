#include <iostream>
#include <fstream>
#include <string>
#include "TaskMsg.pb.h"

using namespace std;

// This function fills in a TaskMsg
void FillTask(dwork::TaskMsg *task) {
  task->set_name("f 1 2 3");
  task->set_origin("hub");

  dwork::TaskMsg::LogMsg *log = task->add_log();
  log->set_state(dwork::TaskMsg::Started);
  log->set_time(12);
}

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " <TaskMsg file>" << endl;
    return -1;
  }

  dwork::TaskMsg task;

  FillTask(&task);

  { // Write to disk.
    fstream output(argv[1], ios::out | ios::trunc | ios::binary);
    if (!task.SerializeToOstream(&output)) {
      cerr << "Failed to write TaskMsg." << endl;
      return -1;
    }
  }

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  return 0;
}

/*
int readTask() {
    fstream input(argv[1], ios::in | ios::binary);
    if (!input) {
      cout << argv[1] << ": File not found.  Creating a new file." << endl;
    } else if (!task.ParseFromIstream(&input)) {
      cerr << "Failed to parse TaskMsg file." << endl;
      return -1;
    }
}*/
