#include "remote.h"
#include "remotefile.h"
#include "irprocessor.h"
#include "command.h"
#include "txt.h"
#include "config.h"
#include "web.h"
#include "web_files.h"
#include "web_set_time.h"
#include "jsonmap.h"
#include "backup.h"
#include "led.h"

#include <stdio.h>
#include <stdlib.h>
#include <pico/stdlib.h>
#include "pico/cyw43_arch.h"
#include <pfs.h>
#include <lfs.h>
#include <sys/stat.h>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>

Remote *Remote::singleton_ = nullptr;
int Remote::debug_level_ = 0;

#define ROOT_OFFSET 0x110000
#define ROOT_SIZE   0x0EF000

struct Remote::URLPROC Remote::funcs[] =
    {
        {std::regex("^/index(|\\.html)$", std::regex_constants::extended), &Remote::remote_get, nullptr},
        {std::regex("^/config(|\\.html)$", std::regex_constants::extended), &Remote::config_get, nullptr},
        {std::regex("^/backup(|\\.html)$", std::regex_constants::extended), &Remote::backup_get, &Remote::backup_post},
        {std::regex("^/menu(|\\.html)$", std::regex_constants::extended), &Remote::menu_get, &Remote::menu_post},
        {std::regex("^(.*)/setup(|\\.html)$", std::regex_constants::extended), &Remote::setup_get, &Remote::setup_post},
        {std::regex("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended), &Remote::setup_btn_get, &Remote::setup_btn_post},
        {std::regex("^/editprompt(|\\.html)$", std::regex_constants::extended), &Remote::prompt_get, &Remote::prompt_post},
        {std::regex("^/test(|\\.html)$", std::regex_constants::extended), &Remote::test_get, nullptr},
        {std::regex("^/log(|\\.html)$", std::regex_constants::extended), &Remote::log_get, &Remote::log_post},
    };

struct Remote::WSPROC Remote::wsproc[] =
    {
        {"btnVal", std::regex(".*", std::regex_constants::extended), &Remote::remote_button},
        {"ir_get", std::regex("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended), &Remote::setup_ir_get},
        {"ir_get", std::regex("^/menu.*", std::regex_constants::extended), &Remote::menu_ir_get},
        {"ir_get", std::regex("^/test.*", std::regex_constants::extended), &Remote::test_ir_get},
        {"config_update", std::regex("^/config.*", std::regex_constants::extended), &Remote::config_update},
        {"get_wifi", std::regex("^/config.*", std::regex_constants::extended), &Remote::config_get_wifi},
        {"scan_wifi", std::regex("^/config.*", std::regex_constants::extended), &Remote::config_scan_wifi},
        {"test_send", std::regex("^/test.*", std::regex_constants::extended), &Remote::test_send},
    };

bool Remote::init(int indicator_gpio, int button_gpio)
{
    indicator_ = new Indicator(indicator_gpio);
    button_ = new Button(0, button_gpio);
    button_->setEventCallback(button_event);

    queue_init(&exec_queue_, sizeof(Command *), 8);
    queue_init(&resp_queue_, sizeof(Command *), 8);

    worker_ = { .do_work = get_replies, .user_data = this };
    async_context_add_when_pending_worker(cyw43_arch_async_context(), &worker_);
    
    WEB *web = WEB::get();
    web->setLogger(log_);
    web->set_http_callback(http_message_);
    web->set_message_callback(ws_message_);
    web->set_notice_callback(web_state);
    bool ret = web->init();
    if (ret)
    {
        CONFIG *cfg = CONFIG::get();
        ret = web->connect_to_wifi(cfg->hostname(), cfg->ssid(), cfg->password());
    }

    set_time_set_cb(time_callback_s);

    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("icons.json", data, datalen))
    {
        const char *start = strchr(data, '{');
        if (!start || !icons_.loadString(start))
        {
            log_->print("Failed to load icons.json\n");
            log_->print("ICONS[%d] :\n%s\n--------\n", datalen, data);
            ret = false;
        }
    }
    else
    {
        log_->print("Could not find load icons.json\n");
        ret = false;
    }

    return ret;
}

Remote::~Remote()
{
    async_context_remove_when_pending_worker(cyw43_arch_async_context(), &worker_);

    Command *cmdptr;
    while (queue_try_remove(&exec_queue_, &cmdptr))
    {
        delete cmdptr;
    }
    queue_free(&exec_queue_);

    while (queue_try_remove(&resp_queue_, &cmdptr))
    {
        delete cmdptr;
    }
    queue_free(&resp_queue_);
}

bool Remote::get_rfile(const std::string &url)
{
    if (!efile_.isModified())
    {
        efile_.clear();
    }
    return rfile_.loadForURL(url);
}

bool Remote::get_efile(const std::string &url)
{
    return efile_.loadForURL(url);
}

bool Remote::get_efile(const std::string &url, WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    rfile_.clear();
    if (!efile_.isModified() || RemoteFile::urlToAction(url) == efile_.filename())
    {
        ret = efile_.loadForURL(url);
        if (!ret)
        {
            web->send_data(client, "HTTP/1.0 404 NOT_FOUND\r\n\r\n", 26);
        }
    }
    else
    {
        std::string rurl = url;
        if (rurl.empty()) rurl = "/";
        std::string eurl = RemoteFile::actionToURL(efile_.filename());
        if (eurl.empty()) eurl = "/";
        log_->print("Requesting edit of '%s' over modified '%s'\n", url.c_str(), efile_.filename());
        std::string resp("HTTP/1.1 303 OK\r\nLocation: /editprompt?editurl=" + eurl + "&rqsturl=" + rurl + "\r\n"
                         "Connection: keep-alive\r\n\r\n");
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
    }
    return ret;
}

bool Remote::http_message(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;

    if (isDebug(1)) log_->print("%d HTTP %s %s\n", client, rqst.type().c_str(), rqst.url().c_str());
    if (rqst.type() == "GET")
    {
        ret = http_get(web, client, rqst, close);
    }
    else if (rqst.type() == "POST")
    {
        ret = http_post(web, client, rqst, close);
    }
    return ret;
}

void Remote::ws_message(WEB *web, ClientHandle client, const std::string &msg)
{
    JSONMap msgmap(msg.c_str());
    const char *func = msgmap.strValue("func");
    const char *path = msgmap.strValue("path");
    if (func && path)
    {
        if (isDebug(1)) log_->print("%d WS func=%s, path=%s\n", client, func, path);
        bool found = false;
        for (int ii = 0; ii < count_of(wsproc); ii++)
        {
            if (func == wsproc[ii].func && std::regex_match(path, wsproc[ii].path_match))
            {
                (this->*wsproc[ii].cb)(web, client, msgmap);
                found = true;
                break;
            }
        }
        if (!found)
        {
            log_->print("Message processor not found for func: '%s', path: '%s' <- '%s'\n", func, path, msg.c_str());
        }
    }
    else
    {
        log_->print("No function or path defined for %s\n", msg.c_str());
    }
}

bool Remote::send_http(WEB *web, ClientHandle client, TXT &html, bool &close)
{
    HTTPRequest::setHTMLLengthHeader(html);
    bool ret = web->send_data(client, html.data(), html.datasize(), WEB::PREALL);
    html.release();
    close = !ret;
    return ret;
}

bool Remote::http_get(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string url = rqst.path();
    bool found = false;
    for (int ii = 0; ii < count_of(funcs); ii++)
    {
        if (std::regex_match(url, funcs[ii].url_match) && funcs[ii].get != nullptr)
        {
            ret = (this->*funcs[ii].get)(web, client, rqst, close);
            found = true;
            break;
        }
    }
    if (!found)
    {
        ret = false;
        const char *data;
        u16_t datalen;
        if (url.length() > 0 && WEB_FILES::get()->get_file(url.substr(1), data, datalen))
        {
            web->send_data(client, data, datalen, WEB::STAT);
            close = false;
            ret = true;
        }
        else
        {
            ret = remote_get(web, client, rqst, close);
        }
    }
    if (!ret)
    {
        log_->print("Returning false for GET of %s\n", url.c_str());
    }
    return ret;
}

bool Remote::http_post(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    bool found = false;
    std::string url = rqst.path();
    for (int ii = 0; ii < count_of(funcs); ii++)
    {
        if (std::regex_match(url, funcs[ii].url_match) && funcs[ii].post != nullptr)
        {
            ret = (this->*funcs[ii].post)(web, client, rqst, close);
            found = true;
            break;
        }
    }

    if (!ret)
    {
        log_->print("Returning false for POST of %s\n", url.c_str());
    }
    return ret;
}

std::string Remote::get_label(const RemoteFile::Button *button) const
{
    std::string background;
    std::string color;
    std::string fill;
    button->getColors(background, color, fill);
    std::string label = button->label();
    get_label(label, background, color, fill);
    return label;
}

bool Remote::get_label(std::string &label, const std::string &background, const std::string &color, const std::string &fill) const
{
    bool ret = false;
    if (label.length() > 1 && label.at(0) == '@')
    {
        ret = true;
        label = label.substr(1);
        std::string key = label;
        for (auto it = key.begin(); it!= key.end(); ++it)
        {
            *it = std::tolower(*it);
        }
        std::string icon = icons_.strValue(key.c_str(), "");
        if (!icon.empty())
        {
            label = icon;
        }
    }
    while (TXT::substitute(label, "{0}", color));
    while (TXT::substitute(label, "{1}", background));
    while (TXT::substitute(label, "{2}", fill));
    return ret;
}

bool Remote::queue_command(const Command *cmd)
{
    bool ret = queue_try_add(&exec_queue_, &cmd);
    return ret;
}

Command *Remote::getNextCommand()
{
    Command *ret = nullptr;
    if (!queue_try_remove(&exec_queue_, &ret))
    {
        ret = nullptr;
    }
    return ret;
}

void Remote::commandReply(Command *command)
{
    queue_try_add(&resp_queue_, &command);
    async_context_set_work_pending(cyw43_arch_async_context(), &worker_);
}

void Remote::get_replies(async_context_t *context, async_when_pending_worker_t *worker)
{
    Remote *self = static_cast<Remote *>(worker->user_data);
    self->get_replies();
}

void Remote::get_replies()
{
    Command *cmd = nullptr;
    while (queue_try_remove(&resp_queue_, &cmd))
    {
        if (isDebug(1)) log_->print("Reply: %s\n", cmd->reply().c_str());
        cmd->web()->send_message(cmd->client(), cmd->reply());
        delete cmd;
    }
}

void Remote::ir_busy(bool busy)
{
    get()->indicator_->setIRState(busy);
}

void Remote::web_state(int state)
{
    get()->indicator_->setWebState(state);
    if (state == WEB::STA_CONNECTED)
    {
        WEB *web = WEB::get();
        std::string resp;
        config_wifi_message(web, resp);
        get()->log_->print("WiFi connect message: %s\n", resp.c_str());
        web->broadcast_websocket(resp);
    }
}

void Remote::button_event(struct Button::ButtonEvent &ev)
{
    if (ev.action == Button::Button_Clicked)
    {
        get()->log_->print("Start WiFi AP fo 30 minutes\n");
        WEB::get()->enable_ap(30, "webremote");
    }
}

uint16_t Remote::to_u16(const std::string &str)
{
    uint16_t ret = 0;
    if (!str.empty())
    {
        try
        {
            uint32_t val = std::stoul(str);
            if (val <= 0xffff)
            {
                ret = static_cast<uint16_t>(val);
            }
            else
            {
                throw std::out_of_range("too big for uint16_t");
            }
        }
        catch(const std::exception& e)
        {
            log_->print("Error %s converting %s to uint16_t\n", e.what(), str.c_str());
        }
    }
    return ret;
}

void Remote::time_callback()
{
    if (!time_initialized_)
    {
        time_initialized_ = true;
        log_->initialize_timestamps();
    }
}

void Remote::setDebug(int level)
{
    debug_level_ = level;
    WEB::get()->setDebug(debug_level_);
    CONFIG::get()->set_debug(debug_level_);
}

//      *****  Indicator  *****

Remote::Indicator::Indicator(int led_gpio) : ir_busy_(false), web_state_(0), ap_state_(false)
{
    led_ = new LED(led_gpio);
     update();
}

Remote::Indicator::~Indicator()
{
    delete led_;
}

void Remote::Indicator::setWebState(int state)
{
    switch (state)
    {
    case WEB::STA_INITIALIZING:
    case WEB::STA_CONNECTED:
    case WEB::STA_DISCONNECTED:
        web_state_ = state;
        break;

    case WEB::AP_ACTIVE:
        ap_state_ = true;
        break;

    case WEB::AP_INACTIVE:
        ap_state_ = false;
        break;
    }
    update();
}

void Remote::Indicator::update()
{
    uint32_t    pattern = ir_busy_ ? 0xffffffff : 0;
    switch (web_state_)
    {
    case WEB::STA_INITIALIZING:
        pattern |= 0xcccccccc;
        break;

    case WEB::STA_CONNECTED:
        break;

    case WEB::STA_DISCONNECTED:
        pattern |= 0x00ff00ff;
        break;

    default:
        pattern = 0xcccccccc;
    }

    if (ap_state_)
    {
        pattern |= 0x0000ffff;
    }

    led_->setFlashPeriod(1000);
    led_->setFlashPattern(pattern, 32);
}

int main ()
{
    stdio_init_all();
    sleep_ms(1000);

    struct pfs_pfs *pfs;
    struct lfs_config cfg;
    ffs_pico_createcfg (&cfg, ROOT_OFFSET, ROOT_SIZE);
    pfs = pfs_ffs_create (&cfg);
    pfs_mount (pfs, "/");

    printf("Filesystem mounted\n");

    CONFIG::get()->init();
    setenv("TZ", CONFIG::get()->timezone(), 1);

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }

    Remote::get()->setDebug(CONFIG::get()->debug());
    Remote::get()->cleanupFiles();
    Remote::get()->init(INDICATOR_GPIO, BUTTON_GPIO);

    IR_Processor *ir = new IR_Processor(Remote::get(), IR_SEND_GPIO, IR_RCV_GPIO);
    ir->setBusyCallback(Remote::ir_busy);
    ir->run();

    return 0;
}
