#include <iostream>
#include <string>
#include <list>
#include <optional>
#include <string_view>
#include <sys/time.h>

extern "C" {
#include <hiredis/hiredis.h>
}

/* Class for making blocking calls to redis. */
class Redis {
    public:
        Redis(const char *hostname, int port) {
            struct timeval timeout = { 1, 500000 }; // 1.5 seconds
            c = redisConnectWithTimeout(hostname, port, timeout);
            state = handle_err();
        }

        Redis(const char *fifo) {
            struct timeval timeout = { 1, 500000 }; // 1.5 seconds
            c = redisConnectUnixWithTimeout(fifo, timeout);
            state = handle_err();
        }

        ~Redis() {
            down();
        }

        std::optional<std::string> get(std::string_view arg) {
            return optstr_cmd({"GET", arg});
        }
        int set(std::string_view arg1, std::string_view arg2) {
            return int_cmd({"SET", arg1, arg2});
        }
        int del(std::string_view arg) {
            return int_cmd({"DEL", arg});
        }

        std::optional<std::string> rpop(std::string_view lst) {
            return optstr_cmd({"RPOP", lst});
        }
        int lpush(std::string_view lst, std::string_view arg) {
            return int_cmd({"LPUSH", lst, arg});
        }

        int incr(std::string_view counter) {
            return int_cmd({"INCR", counter});
        }

        int sadd(std::string_view set, std::string_view key) {
            return int_cmd({"SADD", set, key});
        }
        int srem(std::string_view set, std::string_view key) {
            return int_cmd({"SREM", set, key});
        }

        int get_state() {
            return state;
        }

    private:
        redisContext *c = NULL;
        int state;

        int handle_err() {
            if (c == NULL || c->err) {
                if (c) {
                    std::cout << "Connection error: " << c->errstr << std::endl;
                    redisFree(c);
                } else {
                    std::cout << "Connection error: can't allocate redis context" << std::endl;
                }
                return 1; // error state
            }
            return 0;
        }

        void down() {
            if(state == 0) {
                state = 1;
                redisFree(c);
            }
        }

        redisReply *run(const std::list<std::string_view> &args) {
            if(state != 0) {
                return NULL;
            }
            char const *argv[args.size()];
            size_t argvlen[args.size()];

            int count = 0;
            for(auto t : args) {
                argv[count] = &t.front();
                argvlen[count] = t.size();
                count++;
            }
            auto reply = (redisReply *)redisCommandArgv(c, count, argv, argvlen);
            if(reply == NULL) down();
            return reply;
        }

        std::optional<std::string> optstr_cmd(const std::list<std::string_view> &args) {
            redisReply *reply = run(args);
            if(reply == NULL || reply->type == REDIS_REPLY_NIL) {
                freeReplyObject(reply);
                return {};
            }
            std::string ret(reply->str, reply->len); // copy out the reply
            freeReplyObject(reply);
            return ret;
        }

        int int_cmd(const std::list<std::string_view> &args) {
            auto reply = run(args);
            if(reply == NULL) {
                return 0;
            }
            int ret = reply->integer;
            freeReplyObject(reply);
            return ret;
        }

};
