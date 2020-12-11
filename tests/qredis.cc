#include <stdio.h>
#include <stdlib.h>

#include <qredis.hh>

/*
void evalMulti(char *argv[]) {
    std::string command =
        "local link_id = redis.call(\"INCR\", KEYS[1])"
        "redis.call(\"HSET\", KEYS[2], link_id, ARGV[1])"
        "local data = redis.call(\"HGETALL\", KEYS[2])"
        "return data";
    command.append( multiCmd );

    redisReply *reply = (redisReply *)redisCommand(ctxt,
            "EVAL %s %d %s %s %s", command.c_str(), 2, argv[1], argv[2], argv[3]);

}*/

int main() {
    Redis r("127.0.0.1", 6379);
    //std::cout << "PING: "
    //          << r.cmd({"PING"}) << std::endl;

    // Set a key
    std::cout << "SET: "
              << r.set("foo", "hello world")
              << std::endl;

    // Try a GET and two INCR
    std::cout << "GET foo: "
              << r.get("foo").value_or("nil")
              << std::endl;

    for(int i=0; i<2; i++) {
      std::cout << "INCR counter: "
                << r.incr("counter")
                << std::endl;
    }

    // Create a list of numbers, from 0 to 9 
    r.del("mylist");

    for(int j = 0; j < 10; j++) {
        char buf[64];

        snprintf(buf,64,"element %u",j);
        r.lpush("mylist", buf);
    }

    // Let's check what we have inside the list
    for(int j = 0; j <= 10; j++) {
        auto ret = r.rpop("mylist");
        if(!ret.has_value()) {
            break;
        }
        std::cout << j << ": " << ret.value() << std::endl;
    }
    /*
    reply = redisCommand(c,"LRANGE mylist 0 -1");
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (j = 0; j < reply->elements; j++) {
            printf("%u) %s\n", j, reply->element[j]->str);
        }
    }
    freeReplyObject(reply);
    */

    return 0;
}
