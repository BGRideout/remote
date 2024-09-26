//                  *****  Remote Class  *****

#ifndef REMOTE_H
#define REMOTE_H

#include "remotefile.h"
#include "jsonmap.h"
#include "web.h"
#include "button.h"
#include "pico/cyw43_arch.h"
#include <pico/util/queue.h>
#include <pico/async_context.h>
#include <regex>

#define     IR_SEND_GPIO    17
#define     IR_RCV_GPIO     16
#define     INDICATOR_GPIO  18
#define     BUTTON_GPIO      3

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

    class Indicator
    {
    private:
        LED                     *led_;                  // LED object pointer
        bool                    ir_busy_;               // IR processor busy
        int                     web_state_;             // Web connection state
        bool                    ap_state_;              // AP active

        void update();

    public:
        Indicator(int led_gpio);
        ~Indicator();

        void setIRState(bool busy) { ir_busy_ = busy; update(); }
        void setWebState(int state);
    };
    Indicator                   *indicator_;            // Indicator LED object
    Button                      *button_;               // AP activation button

    bool http_message(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    static bool http_message_(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
     { return Remote::get()->http_message(web, client, rqst, close); }
    void ws_message(WEB *web, void *client, const std::string &msg);
    static void ws_message_(WEB *web, void *client, const std::string &msg) { Remote::get()->ws_message(web, client, msg); }

    bool http_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    bool http_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close);

    bool remote_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    bool remote_button(WEB *web, void *client, const JSONMap &msgmap);
    bool backup_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    bool backup_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    bool setup_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    bool setup_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    bool setup_btn_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    bool setup_btn_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close);

    std::string get_label(const RemoteFile::Button *button) const;
    bool get_label(std::string &label, const std::string &background, const std::string &color, const std::string &fill) const;

    bool queue_command(const Command *cmd);

    static void get_replies(async_context_t *context, async_when_pending_worker_t *worker);
    void get_replies();

    static Remote *singleton_;
    Remote() : indicator_(nullptr) {}

    struct URLPROC
    {
        std::regex url_match;
        bool (Remote::* get)(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
        bool (Remote::* post)(WEB *web, void *client, const HTTPRequest &rqst, bool &close);
    };
    static struct URLPROC funcs[];   

public:
    static Remote *get() { if (!singleton_) singleton_ = new Remote(); return singleton_; }
    ~Remote();
    bool init(int indicator_gpio, int button_gpio);

    Command *getNextCommand();
    void commandReply(Command *command);

    static void ir_busy(bool busy);
    static void web_state(int state);
    static void button_event(struct Button::ButtonEvent &ev);
};

#endif
