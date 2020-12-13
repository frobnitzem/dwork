
../include/TaskMsg.pb.h: TaskMsg.pb.cc
	cp TaskMsg.pb.h $@

TaskMsg.pb.cc: TaskMsg.proto
	protoc -I=. --cpp_out=. $^

