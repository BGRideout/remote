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

    bool http_message(WEB *web, void *client, HTTPRequest &rqst);
    static bool http_message_(WEB *web, void *client, HTTPRequest &rqst) { return Remote::get()->http_message(web, client, rqst); }
    void ws_message(WEB *web, void *client, const std::string &msg);
    static void ws_message_(WEB *web, void *client, const std::string &msg) { Remote::get()->ws_message(web, client, msg); }

    bool http_get(WEB *web, void *client, HTTPRequest &rqst);
    bool remote_page(WEB *web, void *client, HTTPRequest &rqst);

    static Remote *singleton_;
    Remote() {}

public:
    static Remote *get() { if (!singleton_) singleton_ = new Remote(); return singleton_; }
    bool init();
};

#endif
