#include "remote.h"
#include "remotefile.h"
#include "irprocessor.h"
#include "command.h"
#include "txt.h"
#include "config.h"
#include "web.h"
#include "web_files.h"
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

#define ROOT_OFFSET 0x140000
#define ROOT_SIZE   0x060000
#define WEB_DEBUG   0

struct Remote::URLPROC Remote::funcs[] =
    {
        {std::regex("^/index(|\\.html)$", std::regex_constants::extended), &Remote::remote_get, nullptr},
        {std::regex("^/backup(|\\.html)$", std::regex_constants::extended), &Remote::backup_get, &Remote::backup_post},
        {std::regex("^(.*)/setup(|\\.html)$", std::regex_constants::extended), &Remote::setup_get, &Remote::setup_post},
        {std::regex("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended), &Remote::setup_btn_get, &Remote::setup_btn_post},
        {std::regex("^/editprompt(|\\.html)$", std::regex_constants::extended), &Remote::prompt_get, &Remote::prompt_post}
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
    web->setDebug(WEB_DEBUG);
    web->set_http_callback(http_message_);
    web->set_message_callback(ws_message_);
    web->set_notice_callback(web_state);
    bool ret = web->init();

    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("icons.json", data, datalen))
    {
        const char *start = strchr(data, '{');
        if (!start || !icons_.loadString(start))
        {
            printf("Failed to load icons.json\n");
            printf("ICONS[%d] :\n%s\n--------\n", datalen, data);
            ret = false;
        }
    }
    else
    {
        printf("Could not find load icons.json\n");
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

bool Remote::get_efile(const std::string &url, WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    rfile_.clear();
    if (!efile_.isModified() || RemoteFile::urlToAction(url) == efile_.filename())
    {
        ret = efile_.loadForURL(url);
    }
    else
    {
        std::string rurl = url;
        if (rurl.empty()) rurl = "/";
        std::string eurl = RemoteFile::actionToURL(efile_.filename());
        if (eurl.empty()) eurl = "/";
        printf("Requesting edit of '%s' over modified '%s'\n", url.c_str(), efile_.filename());
        std::string resp("HTTP/1.1 303 OK\r\nLocation: /editprompt?editurl=" + eurl + "&rqsturl=" + rurl + "\r\n"
                         "Connection: keep-alive\r\n\r\n");
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
        ret = true;
    }
    return ret;
}

bool Remote::http_message(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
 
    if (rqst.type() == "GET")
    {
        ret = http_get(web, client, rqst,close);
    }
    else if (rqst.type() == "POST")
    {
        ret = http_post(web, client, rqst, close);
    }
    return ret;
}

void Remote::ws_message(WEB *web, void *client, const std::string &msg)
{
    JSONMap msgmap(msg.c_str());
    if (msgmap.hasProperty("btnVal"))
    {
        remote_button(web, client, msgmap);
    }
    else if (msgmap.hasProperty("ir_get"))
    {
        setup_ir_get(web, client, msgmap);
    }
    else
    {
        printf("Message processor not found for %s\n", msg.c_str());
    }
}

bool Remote::http_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
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
            web->send_data(client, data, datalen, false);
            close = false;
            ret = true;
        }
        else
        {
            printf("Remote page %s\n", url.c_str());
            ret = remote_get(web, client, rqst, close);
        }
    }
    if (!ret)
    {
        printf("Returning false for GET of %s\n", url.c_str());
    }
    return ret;
}

bool Remote::http_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    printf("POST %s\n", rqst.url().c_str());
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
        printf("Returning false for POST of %s\n", url.c_str());
    }
    return ret;
}

bool Remote::remote_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = get_rfile(rqst.root());
    if (!ret)
    {
        printf("Error loading action file for %s\n", rqst.url().c_str());
        return false;
    }

    const char *data;
    u16_t datalen;
    WEB_FILES::get()->get_file("index.html", data, datalen);

    std::string html(data, datalen);

    while(TXT::substitute(html, "<?title?>", rfile_.title()));

    std::string backurl = rqst.root();
    std::size_t i1 = backurl.rfind('/');
    if (i1 != std::string::npos && backurl.length() > i1 + 1)
    {
        backurl = backurl.erase(i1 + 1);
    }
    if (strcmp(rfile_.filename(), "actions.json") != 0)
    {
        TXT::substitute(html, "<?backloc?>", backurl);
        TXT::substitute(html, "<?backvis?>", "visible");
    }
    else
    {
        TXT::substitute(html, "<?backloc?>", "/");
        TXT::substitute(html, "<?backvis?>", "hidden");
    }

    std::size_t bi = html.find("<?buttons?>");
    TXT::substitute(html, "<?buttons?>", "");
    std::string button;
    for (auto it = rfile_.buttons().cbegin(); it != rfile_.buttons().cend(); ++it)
    {
        int pos = it->position();
        int row = (pos - 1) / 5 + 1;
        int col = (pos - 1) % 5 + 1;

        if (strlen(it->redirect()) > 0 || it->actions().size() > 0)
        {
            button = "<button type=\"submit\"<?class!! style=\"grid-row:!row!; grid-column:!col!; color: {0}; background: {1};\""
                " name=\"btnVal\" value=\"!pos!\">\n"
                "  !label!\n"
                "</button>\n";

            TXT::substitute(button, "<?class?>", strlen(it->redirect()) > 0 && it->actions().size() == 0 ? " class=\"redir\"" : "");
        }
        else
        {
            button = "<span style=\"grid-row:!row!; grid-column:!col!; color: {0}; background: transparent; font-size: 24px; font-weight: bold;\">\n"
                "  !label!\n"
                "</span>\n";
        }
        TXT::substitute(button, "!row!", std::to_string(row));
        TXT::substitute(button, "!col!", std::to_string(col));
        TXT::substitute(button, "!pos!", std::to_string(pos));

        std::string background;
        std::string color;
        std::string fill;
        it->getColors(background, color, fill);

        std::string label = it->label();
        get_label(label, background, color, fill);
        TXT::substitute(button, "!label!", label);
        while (TXT::substitute(button, "{0}", color));
        while (TXT::substitute(button, "{1}", background));
        while (TXT::substitute(button, "{2}", fill));
        html.insert(bi, button);
        bi += button.length();
    }

    HTTPRequest::setHTMLLengthHeader(html);
    web->send_data(client, html.c_str(), html.length());
    close = false;
    
    return ret;
}

bool Remote::remote_button(WEB *web, void *client, const JSONMap &msgmap)
{
    bool ret = false;
    printf("btnVal = %d, action = %s path = %s duration = %f\n",
        msgmap.intValue("btnVal"), msgmap.strValue("action"), msgmap.strValue("path"), msgmap.realValue("duration"));

    int button = msgmap.intValue("btnVal");
    std::string url = msgmap.strValue("path");
    get_rfile(url);
    RemoteFile::Button *btn = rfile_.getButton(button);
    if (btn)
    {
        ret = true;
        Command *cmd = new Command(web, client, msgmap, btn);
        queue_command(cmd);
    }
    return ret;
}

bool Remote::backup_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("backup.html", data, datalen))
    {
        std::string files = "<option value='all'>All (backup: ";
        files += CONFIG::get()->hostname();
        files += ".json)</option>\n";

        std::vector<std::string> action_files;
        RemoteFile::actionFiles(action_files);
        for (auto it = action_files.cbegin(); it != action_files.cend(); ++it)
        {
            files += "<option value='" + *it + "'>";
            if (it->at(0) == 'a')
            {
                files += "Action: " + RemoteFile::actionToURL(*it) + "</option>";
            }
            else
            {
                files += "Menu: " + it->substr(5, it->length() - 10) + "</option>";
            }
        }

        std::string html(data, datalen);
        HTTPRequest::replaceHeader(html);
        TXT::substitute(html, "<?files?>", files);
        std::string val = rqst.cookie("msg");
        TXT::substitute(html, "<?msg?>", val);
        val = rqst.cookie("msgcolor", "transparent");
        TXT::substitute(html, "<?msgcolor?>", val);
        HTTPRequest::setHTMLLengthHeader(html);
        web->send_data(client, html.c_str(), html.length());
        close = false;
        ret = true;
    }
    return ret;
}

bool Remote::backup_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string msg("Success");
    std::string button = rqst.postValue("button");
    if (button == "upload")
    {
        ret = Backup::loadBackup(rqst);
    }
    else if (button == "download")
    {
        ret = true;
    }
    else
    {
        msg = "Invalid button";
    }

    std::string resp("HTTP/1.1 303 OK\r\nLocation: /backup\r\n"
                     "Set-Cookie: msg=!!msg!!; Max-Age=5\r\n"
                     "Set-Cookie: msgcolor=!!msgcolor!!; Max-Age=5\r\n"
                     "Connection: keep-alive\r\n\r\n");
    TXT::substitute(resp, "<?msg?>", msg);
    TXT::substitute(resp, "<?msgcolor?>", ret ? "green" : "red");
    web->send_data(client, resp.c_str(), resp.length());
    close = false;
    ret = true;
    return ret;
}

bool Remote::setup_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        if (base_url.empty()) base_url = "/";

        std::string done = rqst.query("done");
        if (done == "true")
        {
            rfile_.clear();
            efile_.clear();
            std::string resp("HTTP/1.1 303 OK\r\nLocation: " + base_url + "\r\n"
                            "Connection: keep-alive\r\n\r\n");
            web->send_data(client, resp.c_str(), resp.length());
            close = false;
            return true;
        }

        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for GET %s\n", rqst.url().c_str());
            //  Return true as get_efile has issued response
            return true;
        }

        const char *data;
        u16_t datalen;
        if (WEB_FILES::get()->get_file("setup.html", data, datalen))
        {
            std::string html(data, datalen);

            while(TXT::substitute(html, "<?title?>", efile_.title()));

            bool modified = efile_.isModified();
            TXT::substitute(html, "<?modified?>", modified ? "unsaved" : "saved");

            std::size_t bi = html.find("<?buttons?>");
            TXT::substitute(html, "<?buttons?>", "");
            int nb = efile_.maxButtonPosition();
            nb = (nb + 9) / 5 * 5 + 1;
            std::string button;
            for (int pos = 1; pos < nb; pos++)
            {
                RemoteFile::Button *btn = efile_.getButton(pos);
                std::string background;
                std::string color;
                std::string fill;
                RemoteFile::Button::getColors(btn, background, color, fill);

                button = "<button type=\"button\" style=\"";
                int row = (pos - 1) / 5 + 1;
                int col = (pos - 1) % 5 + 1;
                button += "grid-row:" + std::to_string(row) + "; grid-column:" + std::to_string(col);
                button += "; color: " + color + "; background: " + background + ";\" ";
                button += "onclick=\"btnAction(" + std::to_string(pos) + ")\">";
                if (btn)
                {
                    std::string label = btn->label();
                    get_label(label, background, color, fill);
                    button += label;
                }
                button += "</button>";
                html.insert(bi, button);
                bi += button.length();
            }

            HTTPRequest::setHTMLLengthHeader(html);
            web->send_data(client, html.c_str(), html.length());
            close = false;
            ret = true;
        }
    }
    return ret;
}

bool Remote::setup_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    rqst.printPostData();
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for POST %s\n", rqst.url().c_str());
            //  Return true as get_efile has sent response
            return true;
        }

        const char *title = rqst.postValue("title");
        if (title)
        {
            efile_.setTitle(title);
        }

        const char *save = rqst.postValue("save");
        if (efile_.isModified() && save && strcmp(save, "true") == 0)
        {
            if (efile_.saveFile())
            {
                efile_.clearModified();
                rfile_.clear();
            }
        }

        std::string resp("HTTP/1.1 303 OK\r\nLocation: " + url + "\r\n"
                         "Connection: keep-alive\r\n\r\n");
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
        ret = true;
    }

    return ret;
}

bool Remote::setup_btn_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        if (base_url.empty()) base_url = "/";
        int pos = std::atoi(match[3].str().c_str());
        printf("GET '%s' button at %d\n", base_url.c_str(), pos);

        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for GET %s\n", rqst.url().c_str());
            //  Return true as get_efile has sent response
            return true;
        }

        RemoteFile::Button newbtn(pos);
        RemoteFile::Button *button = efile_.getButton(pos);
        int nb = efile_.maxButtonPosition();
        nb = (nb + 9) / 5 * 5 + 1;
        if (pos > 0 && pos <= nb)
        {            
            if (!button)
            {
                button = &newbtn;
            }
        }
        else
        {
            printf("Invalid button %d for %s\n", pos, rqst.url().c_str());
            return false;
        }

        const char *data;
        u16_t datalen;
        if (WEB_FILES::get()->get_file("setupbtn.html", data, datalen))
        {
            std::string html(data, datalen);
            TXT::substitute(html, "<?label?>", button->label());
            TXT::substitute(html, "<?color?>", button->color());
            TXT::substitute(html, "<?redirect?>", button->redirect());
            TXT::substitute(html, "<?repeat?>", std::to_string(button->repeat()));
            TXT::substitute(html, "<?swap?>", std::to_string(button->position()));

            TXT::substitute(html, "<?btn?>", std::to_string(pos));
            while(TXT::substitute(html, "<?path?>", base_url));
            TXT::substitute(html, "<?btncount?>", std::to_string(button->actions().size()));

            std::size_t bi = html.find("<?steps?>");
            TXT::substitute(html, "<?steps?>", "");
            std::string action;
            int row = 0;
            for (auto it = button->actions().cbegin(); it != button->actions().cend(); ++it, ++row)
            {
                action = "<tr>";
                action += "<td><input type=\"text\" name=\"typ\" value=\"" + std::string(it->type()) + "\" /></td>";
                action += "<td><input type=\"text\" name=\"add\" value=\"" + std::to_string(it->address()) + "\" /></td>";
                action += "<td><input type=\"text\" name=\"val\" value=\"" + std::to_string(it->value()) + "\" /></td>";
                action += "<td><input type=\"text\" name=\"dly\" value=\"" + std::to_string(it->delay()) + "\" /></td>";
                action += "<td><button type=\"submit\" name=\"add_row\" value=\"" + std::to_string(row) + "\">+</button></td>";
                action += "<td><button type=\"button\" onclick=\"load_ir(" + std::to_string(row) + ");\">&lt;-IR</button></td>";
                action += "</tr>";
                html.insert(bi, action);
                bi += action.length();
            }

            HTTPRequest::setHTMLLengthHeader(html);
            web->send_data(client, html.c_str(), html.length());
            close = false;
            ret = true;
        }
    }
    return ret;
}

bool Remote::setup_btn_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    rqst.printPostData();
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        int pos = std::atoi(match[3].str().c_str());
        printf("POST '%s' button at %d\n", base_url.c_str(), pos);

        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for POST %s\n", rqst.url().c_str());
            //  Return true as get_efile has sent response
            return true;
        }

        const char *value = rqst.postValue("lbl");
        if (!value) value = "";
        RemoteFile::Button *button = efile_.getButton(pos);
        if (button)
        {
            if (*value == 0)
            {
                if (efile_.deleteButton(pos))
                {
                    button = nullptr;
                }
            }
        }
        else
        {
            if (*value != 0)
            {
                button = efile_.addButton(pos, value, "", "", 0);
            }
        }
        if (button)
        {
            if (value) button->setLabel(value);

            value = rqst.postValue("bck");
            if (value) button->setColor(value);

            value = rqst.postValue("red");
            if (value) button->setRedirect(value);

            value = rqst.postValue("repeat");
            if (value) button->setRepeat(atoi(value));

            value = rqst.postValue("swap");
            if (value)
            {
                int newpos = atoi(value);
                if (newpos != pos && newpos > 0 && newpos <= 100)
                {
                    efile_.changePosition(button, newpos);
                    pos = newpos;
                    url = base_url + "/setup/" + std::to_string(pos);
                }
            }

            std::vector<const char *> typ;
            std::vector<const char *> add;
            std::vector<const char *> val;
            std::vector<const char *> dly;
            rqst.postArray("typ", typ);
            rqst.postArray("add", add);
            rqst.postArray("val", val);
            rqst.postArray("dly", dly);

            int nn = typ.size();
            if (add.size() == nn && val.size() == nn && dly.size() == nn)
            {
                int actno = 0;
                for (int seqno = 0; seqno < nn; seqno++)
                {
                    const char *type = typ.at(seqno);
                    int address = atoi(add.at(seqno));
                    int value = atoi(val.at(seqno));
                    int delay = atoi(dly.at(seqno));

                    if (!type || *type == 0)
                    {
                        button->deleteAction(actno);
                    }
                    else
                    {
                        if (actno < button->actions().size())
                        {
                            RemoteFile::Button::Action *action = button->action(actno);
                            action->setType(type);
                            action->setAddress(address);
                            action->setValue(value);
                            action->setDelay(delay);
                        }
                        else
                        {
                            button->addAction(type, address, value, delay);
                        }
                        actno += 1;
                    }
                }

                while (button->actions().size() > nn)
                {
                    button->deleteAction(nn);
                }
            }
            else
            {
                printf("POST array sizes do not match\n");
            }

            const char *add_row = rqst.postValue("add_row");
            if (add_row)
            {
                int before = atoi(add_row);
                if (before >= 0 && before < button->actions().size())
                {
                    button->insertAction(before);
                }
            }

            const char *btn = rqst.postValue("button");
            if (btn && strcmp(btn, "done") == 0)
            {
                url = base_url + "/setup";
            }
        }

        std::string resp("HTTP/1.1 303 OK\r\nLocation: " + url + "\r\n"
                         "Connection: keep-alive\r\n\r\n");
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
        ret = true;
    }
    return ret;
}

bool Remote::setup_ir_get(WEB *web, void *client, const JSONMap &msgmap)
{
    bool ret = false;
    printf("ir_get = %d, path = %s\n",
        msgmap.intValue("ir_get"), msgmap.strValue("path"));

    int row = msgmap.intValue("ir_get");
    std::string url = msgmap.strValue("path");
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        int pos = std::atoi(match[3].str().c_str());
        printf("IR_Get '%s' button %d row %d\n", base_url.c_str(), pos, row);

        ret = get_efile(base_url);
        RemoteFile::Button *btn = efile_.getButton(pos);
        if (ret && btn)
        {
            Command *cmd = new Command(web, client, msgmap, btn);
            queue_command(cmd);
        }
    }
    return ret;
}

bool Remote::prompt_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string editurl = rqst.query("editurl");
    std::string rqsturl = rqst.query("rqsturl");
    if (editurl.empty() || rqsturl.empty())
    {
        web->send_data(client, "HTTP/1.1 400 Bad request\r\n\r\n", 28);
        close = true;
        return true;
    }

    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("editprompt.html", data, datalen))
    {
        std::string html(data, datalen);
        TXT::substitute(html, "<?editurl?>", editurl);
        TXT::substitute(html, "<?rqsturl?>", rqsturl);
        HTTPRequest::setHTMLLengthHeader(html);
        web->send_data(client, html.c_str(), html.length());
        close = false;
        ret = true;
    }
    return ret;
}

bool Remote::prompt_post(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    rqst.printPostData();
    const char *editurl = rqst.postValue("editurl");
    const char *rqsturl = rqst.postValue("rqsturl");
    const char *choice = rqst.postValue("choice");
    if (!editurl || !rqsturl || !choice)
    {
        web->send_data(client, "HTTP/1.1 400 Bad request\r\n\r\n", 28);
        close = true;
        return true;
    }

    std::string url(rqsturl);
    if (strcmp(choice, "yes") == 0)
    {
        url += "/setup";
        efile_.clear();
    }
    std::string resp("HTTP/1.1 303 OK\r\nLocation: " + url + "\r\n"
                        "Connection: keep-alive\r\n\r\n");
    web->send_data(client, resp.c_str(), resp.length());
    close = false;
    return true;
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
        printf("Reply: %s\n", cmd->reply().c_str());
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
}

void Remote::button_event(struct Button::ButtonEvent &ev)
{
    if (ev.action == Button::Button_Clicked)
    {
        printf("Start WiFi AP fo 30 minutes\n");
        WEB::get()->enable_ap(30, "webremote");
    }
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

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }

    Remote::get()->init(INDICATOR_GPIO, BUTTON_GPIO);

    IR_Processor *ir = new IR_Processor(Remote::get(), IR_SEND_GPIO, IR_RCV_GPIO);
    ir->setBusyCallback(Remote::ir_busy);
    ir->run();

    return 0;
}
