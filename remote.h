//                  *****  Remote Class  *****

#ifndef REMOTE_H
#define REMOTE_H

#include "remotefile.h"
#include "jsonmap.h"
#include "web.h"

class Remote
{
private:
    RemoteFile                  rfile_;                 // Remote page definition file
    JSONMap                     icons_;                 // Icon list

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

    static Remote *singleton_;
    Remote() {}

    struct URLPROC
    {
        const char *url;
        bool (Remote::* get)(WEB *web, void *client, const HTTPRequest &rqst);
        bool (Remote::* post)(WEB *web, void *client, const HTTPRequest &rqst);
    };
    static struct URLPROC funcs[];   

public:
    static Remote *get() { if (!singleton_) singleton_ = new Remote(); return singleton_; }
    bool init();
};

#endif
