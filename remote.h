//                  *****  Remote Class  *****

#ifndef REMOTE_H
#define REMOTE_H

#include "remotefile.h"
#include "jsonmap.h"
#include "web.h"
#include "txt.h"
#include "button.h"
#include "file_logger.h"
#include "pico/cyw43_arch.h"
#include <pico/util/queue.h>
#include <pico/async_context.h>
#include <regex>
#include <set>
#include <string>

#define     IR_SEND_GPIO    17
#define     IR_RCV_GPIO     16
#define     INDICATOR_GPIO  18
#define     BUTTON_GPIO      3

#define     LOG_FILE        "log_file.txt"

class Command;
class LED;

class Remote
{
private:
    RemoteFile                  rfile_;                 // Remote page definition file
    RemoteFile                  efile_;                 // Definition file for editing
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
    FileLogger                  *log_;                  // Logger
    static int                  debug_level_;           // Debug level

    bool get_rfile(const std::string &url);
    bool get_efile(const std::string &url);
    bool get_efile(const std::string &url, WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);

    bool http_message(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    static bool http_message_(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
     { return Remote::get()->http_message(web, client, rqst, close); }
    void ws_message(WEB *web, ClientHandle client, const std::string &msg);
    static void ws_message_(WEB *web, ClientHandle client, const std::string &msg) { Remote::get()->ws_message(web, client, msg); }

    bool send_http(WEB *web, ClientHandle client, TXT &html, bool &close);

    bool http_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool http_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);

    bool remote_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool remote_button(WEB *web, ClientHandle client, const JSONMap &msgmap);
    bool backup_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool backup_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool setup_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool setup_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool setup_btn_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool setup_btn_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool setup_ir_get(WEB *web, ClientHandle client, const JSONMap &msgmap);
    bool menu_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool menu_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool menu_ir_get(WEB *web, ClientHandle client, const JSONMap &msgmap);
    bool config_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool config_update(WEB *web, ClientHandle client, const JSONMap &msgmap);
    bool config_get_wifi(WEB *web, ClientHandle client, const JSONMap &msgmap);
    bool config_scan_wifi(WEB *web, ClientHandle client, const JSONMap &msgmap);
    static bool config_scan_complete(WEB *web, ClientHandle client, const WiFiScanData &data, void *user_data);
    static void config_wifi_message(WEB *web, std::string &message);
    bool test_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool test_send(WEB *web, ClientHandle client, const JSONMap &msgmap);
    bool test_ir_get(WEB *web, ClientHandle client, const JSONMap &msgmap);
    bool log_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool log_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool prompt_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    bool prompt_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);

    void list_files();
    void get_references(std::set<std::string> &files, std::set<std::string> &references);
    int  add_missing_actions();
    int  remove_excess_actions();

    std::string get_label(const RemoteFile::Button *button) const;
    bool get_label(std::string &label, const std::string &background, const std::string &color, const std::string &fill) const;

    bool queue_command(const Command *cmd);

    static void get_replies(async_context_t *context, async_when_pending_worker_t *worker);
    void get_replies();

    uint16_t to_u16(const std::string &str);

    bool time_initialized_;         // Time initilized by NTP
    void time_callback();
    static void time_callback_s() { get()->time_callback(); }

    static Remote *singleton_;
    Remote() : indicator_(nullptr), log_(new FileLogger(LOG_FILE)), time_initialized_(false) {}

    struct URLPROC
    {
        std::regex url_match;
        bool (Remote::* get)(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
        bool (Remote::* post)(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close);
    };
    static struct URLPROC funcs[];

    struct WSPROC
    {
        std::string func;
        std::regex  path_match;
        bool (Remote::* cb)(WEB *web, ClientHandle client, const JSONMap &msgmap);
    };
    static struct WSPROC wsproc[];

public:
    static Remote *get() { if (!singleton_) singleton_ = new Remote(); return singleton_; }
    ~Remote();
    bool init(int indicator_gpio, int button_gpio);

    Command *getNextCommand();
    void commandReply(Command *command);

    static void ir_busy(bool busy);
    static void web_state(int state);
    static void button_event(struct Button::ButtonEvent &ev);

    /**
     * @brief   Cleanup files
     * 
     * @details Remove any action files not referenced by redirects
     *          Remove menu files not referenced by commands
     *          Add missing action or menu files
     *          Recreate files that will not load
     *          Remove unknown files
     */
    void cleanupFiles();

    void setDebug(int level);
    static bool isDebug(int level = 1) { return level <= debug_level_; }
    static FileLogger *logger() { return get()->log_; }
};

#endif
