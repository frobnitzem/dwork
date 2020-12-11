
../include/TaskMsg.pb.h: TaskMsg.pb.cc
	cp TaskMsg.pb.h $@

TaskMsg.pb.cc: TaskMsg.proto
	protoc -I=./proto --cpp_out=proto $^

