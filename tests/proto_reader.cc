#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <TaskMsg.pb.h>
using namespace std;

void Show(const dwork::TaskMsg& task) {
  cout << "Task: " << task.name() << endl;
  if (task.has_origin()) {
      cout << "  Origin: " << task.origin() << endl;
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

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 2) {
    cerr << "Usage:  " << argv[0] << " <TaskMsg file>" << endl;
    return -1;
  }

  dwork::TaskMsg task;

  {
    fstream input(argv[1], ios::in | ios::binary);
    ostringstream ss;
    ss << input.rdbuf();
    std::string str = ss.str();

    if (!task.ParseFromString(str)) {
      cerr << "Failed to parse TaskMsg." << endl;
      return -1;
    }
  }

  //std::cout << task.DebugString() << std::endl;
  Show(task);

  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}

