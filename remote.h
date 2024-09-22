//                  *****  Remote Class  *****

#ifndef REMOTE_H
#define REMOTE_H

#include "remotefile.h"
#include "jsonmap.h"
#include "web.h"
#include "pico/cyw43_arch.h"
#include <pico/util/queue.h>
#include <pico/async_context.h>

#define     IR_SEND_GPIO    17
#define     IR_RCV_GPIO     16
#define     INDICATOR_GPIO  18

class Command;
class LED;

class Remote
{
private:
    RemoteFile                  rfile_;                 // Remote page definition file
    JSONMap                     icons_;                 // Icon list
    queue_t                     exec_queue_;            // Command queue
    queue_t                     resp_queue_;            // Response queue
    async_when_pending_worker_t worker_;                // Response notice worker
    LED                         *indicator_;            // Indicator LED

    bool http_message(WEB *web, void *client, const HTTPRequest &rqst);
    static bool http_message_(WEB *web, void *client, const HTTPRequest &rqst) { return Remote::get()->http_message(web, client, rqst); }
    void ws_message(WEB *web, void *client, const std::string &msg);
    static void ws_message_(WEB *web, void *client, const std::string &msg) { Remote::get()->ws_message(web, client, msg); }

    bool http_get(WEB *web, void *client, const HTTPRequest &rqst);
    bool http_post(WEB *web, void *client, const HTTPRequest &rqst);

    bool remote_get(WEB *web, void *client, const HTTPRequest &rqst);
    bool remote_button(WEB *web, void *client, const JSONMap &msgmap);
    bool backup_get(WEB *web, void *client, const HTTPRequest &rqst);
    bool backup_post(WEB *web, void *client, const HTTPRequest &rqst);

    bool queue_command(const Command *cmd);

    static void get_replies(async_context_t *context, async_when_pending_worker_t *worker);
    void get_replies();

    static Remote *singleton_;
    Remote() : indicator_(nullptr) {}

    struct URLPROC
    {
        const char *url;
        bool (Remote::* get)(WEB *web, void *client, const HTTPRequest &rqst);
        bool (Remote::* post)(WEB *web, void *client, const HTTPRequest &rqst);
    };
    static struct URLPROC funcs[];   

public:
    static Remote *get() { if (!singleton_) singleton_ = new Remote(); return singleton_; }
    ~Remote();
    bool init(int indicator_gpio);

    Command *getNextCommand();
    void commandReply(Command *command);
};

#endif
