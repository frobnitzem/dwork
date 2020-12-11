#include <iostream>
#include <fstream>
#include <string>

#include "TaskMsg.pb.h"
using namespace std;

// Iterates though all people in the AddressBook and prints info about them.
void Show(const dwork::TaskMsg& task) {
  cout << "Task: " << task.name() << endl;
  cout << "  Origin: " << task.origin() << endl;
  if (task.has_location()) {
      cout << "  Location: " << task.location() << endl;
  }

  for (int j = 0; j < task.log_size(); j++) {
      const dwork::TaskMsg::LogMsg& log = task.log(j);

      switch (log.state()) {
        case dwork::TaskMsg::Pending:
          cout << "  Pending: ";
          break;
        case dwork::TaskMsg::Stolen:
          cout << "  Stolen: ";
          break;
        case dwork::TaskMsg::Waiting:
          cout << "  Waiting: ";
          break;
        case dwork::TaskMsg::Copying:
          cout << "  Copying: ";
          break;
        case dwork::TaskMsg::Ready:
          cout << "  Ready: ";
          break;
        case dwork::TaskMsg::Started:
          cout << "  Started: ";
          break;
        case dwork::TaskMsg::Done:
          cout << "  Done: ";
          break;
      }
      cout << log.time() << endl;
    }
}

// Main function:  Reads the entire address book from a file and prints all
//   the information inside.
int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " <TaskMsg file>" << endl;
    return -1;
  }

  dwork::TaskMsg task;

  {
    // Read the existing address book.
    fstream input(argv[1], ios::in | ios::binary);
    if (!task.ParseFromIstream(&input)) {
      cerr << "Failed to parse TaskMsg." << endl;
      return -1;
    }
  }

  std::cout << task.DebugString() << std::endl;
  Show(task);

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  return 0;
}

