#include "remote.h"
#include "remotefile.h"
#include "txt.h"
#include "config.h"
#include "web.h"
#include "web_files.h"
#include "jsonmap.h"
#include "backup.h"

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

struct Remote::URLPROC Remote::funcs[] =
    {
        {"/index", &Remote::remote_get, nullptr},
        {"/backup", &Remote::backup_get, &Remote::backup_post}
    };

bool Remote::init()
{
    WEB *web = WEB::get();
    web->set_http_callback(get()->http_message_);
    web->set_message_callback(get()->ws_message_);
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

bool Remote::http_message(WEB *web, void *client, const HTTPRequest &rqst)
{
    bool ret = false;
 
    if (rqst.type() == "GET")
    {
        ret = http_get(web, client, rqst);
    }
    else if (rqst.type() == "POST")
    {
        ret = http_post(web, client, rqst);
    }
    return ret;
}

void Remote::ws_message(WEB *web, void *client, const std::string &msg)
{
    printf("Message: %s\n", msg.c_str());
    JSONMap msgmap(msg.c_str());
    if (msgmap.hasProperty("btnVal"))
    {
        remote_button(web, client, msgmap);
    }
    else
    {
        printf("btnVal not found\n");
    }
}

bool Remote::http_get(WEB *web, void *client, const HTTPRequest &rqst)
{
    bool ret = false;
    std::string url = rqst.path();
    printf("GET %s\n", url.c_str());
    bool found = false;
    url = rqst.root();
    for (int ii = 0; ii < count_of(funcs); ii++)
    {
        if (url == funcs[ii].url && funcs[ii].get != nullptr)
        {
            ret = (this->*funcs[ii].get)(web, client, rqst);
            found = true;
            break;
        }
    }
    if (!found)
    {
        ret = false;
        url = rqst.path();
        const char *data;
        u16_t datalen;
        if (url.length() > 0 && WEB_FILES::get()->get_file(url.substr(1), data, datalen))
        {
            web->send_data(client, data, datalen, false);
            ret = true;
        }
        else
        {
            printf("Remote page %s\n", url.c_str());
            ret = remote_get(web, client, rqst);
        }
    }
    if (!ret)
    {
        printf("Returning false for GET of %s\n", url.c_str());
    }
    return ret;
}

bool Remote::http_post(WEB *web, void *client, const HTTPRequest &rqst)
{
    bool ret = false;
    std::string url = rqst.path();
    printf("POST %s\n", url.c_str());
    bool found = false;
    url = rqst.root();
    for (int ii = 0; ii < count_of(funcs); ii++)
    {
        if (url == funcs[ii].url && funcs[ii].post != nullptr)
        {
            ret = (this->*funcs[ii].post)(web, client, rqst);
            found = true;
            break;
        }
    }

    if (!found)
    {
        url = rqst.path();
    }

    if (!ret)
    {
        printf("Returning false for POST of %s\n", url.c_str());
    }
    return ret;
}

bool Remote::remote_get(WEB *web, void *client, const HTTPRequest &rqst)
{
    bool ret = rfile_.loadForURL(rqst.path());
    if (!ret)
    {
        printf("Error loading action file for %s\n", rqst.url().c_str());
        return false;
    }

    const char *data;
    u16_t datalen;
    WEB_FILES::get()->get_file("index.html", data, datalen);

    std::string html(data, datalen);

    while(TXT::substitute(html, "!!title!!", rfile_.title()));

    std::string backurl = rqst.path();
    std::size_t i1 = backurl.rfind('/');
    if (i1 != std::string::npos && backurl.length() > i1 + 1)
    {
        backurl = backurl.erase(i1 + 1);
    }
    if (strcmp(rfile_.filename(), "actions.json") != 0)
    {
        TXT::substitute(html, "!!backloc!!", backurl);
        TXT::substitute(html, "!!backvis!!", "visible");
    }
    else
    {
        TXT::substitute(html, "!!backloc!!", "/");
        TXT::substitute(html, "!!backvis!!", "hidden");
    }

    std::size_t bi = html.find("!!buttons!!");
    for (auto it = rfile_.buttons().cbegin(); it != rfile_.buttons().cend(); ++it)
    {
        int pos = it->position();
        int row = (pos - 1) / 5 + 1;
        int col = (pos - 1) % 5 + 1;

        std::string button;
        if (strlen(it->redirect()) > 0 || it->actions().size() > 0)
        {
            button = "<button type=\"submit\"!!class!! style=\"grid-row:!row!; grid-column:!col!; color: {0}; background: {1};\""
                " name=\"btnVal\" value=\"!pos!\">\n"
                "  !label!\n"
                "</button>\n";

            TXT::substitute(button, "!!class!!", strlen(it->redirect()) > 0 && it->actions().size() == 0 ? " class=\"redir\"" : "");
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

        std::string color("black");
        std::string background("#efefef");
        std::string fill("transparent");
        std::vector<std::string> colors;
        TXT::split(it->color(), "/", colors);
        if (colors.size() > 0 && !colors.at(0).empty())
        {
            background = colors.at(0);
        }
        if (colors.size() > 1 && !colors.at(1).empty())
        {
            color = colors.at(1);
        }
        if (colors.size() > 2 && !colors.at(2).empty())
        {
            fill = colors.at(2);
        }

        std::string label = it->label();
        if (label.length() > 1 && label.at(0) == '@')
        {
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
        TXT::substitute(button, "!label!", label);
        while (TXT::substitute(button, "{0}", color));
        while (TXT::substitute(button, "{1}", background));
        while (TXT::substitute(button, "{2}", fill));
        html.insert(bi, button);
        bi += button.length();
    }

    TXT::substitute(html, "!!buttons!!", "");
    web->send_data(client, html.c_str(), html.length());

    return ret;
}

bool Remote::remote_button(WEB *web, void *client, const JSONMap &msgmap)
{
    printf("btnVal = %d, action = %s path = %s duration = %f\n",
        msgmap.intValue("btnVal"), msgmap.strValue("action"), msgmap.strValue("path"), msgmap.realValue("duration"));

    int button = msgmap.intValue("btnVal");
    std::string action = msgmap.strValue("action", "");
    std::string url = msgmap.strValue("path");
    double duration = msgmap.realValue("duration");

    JSONMap::JMAP jmap;
    jmap["button"] = std::to_string(button);
    jmap["action"] = action;
    jmap["url"] = url;
    jmap["redirect"] = "";

    rfile_.loadForURL(url);
    RemoteFile::Button *btn = rfile_.getButton(button);
    printf("button %p\n", btn);
    if (btn)
    {
        std::string red(btn->redirect());
        if (red.length() > 0 && red.at(0) != '/')
        {
            red.insert(0, "/");
        }
        jmap["redirect"] = red;
    }

    if (action == "click")
    {
        ;
    }
    else if (action == "press")
    {
        if (true)
        {
            ;
        }
        else
        {
            jmap["action"] = "no-repeat";
        }
    }
    else if (action == "release" || action == "cancel")
    {
        ;
    }
    std::string resp;
    JSONMap::fromMap(jmap, resp);
    printf("response: %s\n", resp.c_str());
    web->send_message(client, resp);
    return true;
}

bool Remote::backup_get(WEB *web, void *client, const HTTPRequest &rqst)
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
        TXT::substitute(html, "!!files!!", files);
        std::string val = rqst.cookie("msg");
        TXT::substitute(html, "!!msg!!", val);
        val = rqst.cookie("msgcolor", "transparent");
        TXT::substitute(html, "!!msgcolor!!", val);
        web->send_data(client, html.c_str(), html.length());
        ret = true;
    }
    return ret;
}

bool Remote::backup_post(WEB *web, void *client, const HTTPRequest &rqst)
{
    bool ret = false;
    const HTTPRequest::PostData &data = rqst.postData();
    printf("Post data[%d]\n", data.size());
    for (auto it = data.cbegin(); it != data.cend(); ++it)
    {
        printf("%s=%s\n", it->first.c_str(), it->second);
    }

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

    std::string resp("HTTP/1.0 303 OK\r\nLocation: /backup\r\n"
                     "Set-Cookie: msg=!!msg!!; Max-Age=5\r\n"
                     "Set-Cookie: msgcolor=!!msgcolor!!; Max-Age=5\r\n\r\n");
    TXT::substitute(resp, "!!msg!!", msg);
    TXT::substitute(resp, "!!msgcolor!!", ret ? "green" : "red");
    web->send_data(client, resp.c_str(), resp.length());
    ret = true;
    return ret;
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

    Remote::get()->init();

    // RemoteFile rfile;
    // if (rfile.loadFile("actions.json"))
    // {
    //     printf("%s : %s\n", rfile.filename(), rfile.title());
    //     const RemoteFile::ButtonList &btns = rfile.buttons();
    //     for (auto it = btns.cbegin(); it != btns.cend(); ++it)
    //     {
    //         const RemoteFile::Button &btn = *it;
    //         printf("%d %s %s %s %d\n", btn.position(), btn.label(), btn.color(), btn.redirect(), btn.repeat());
    //         const RemoteFile::Button::ActionList &acts = btn.actions();
    //         for (auto i2 = acts.cbegin(); i2 != acts.cend(); ++i2)
    //         {
    //             printf("  %s %d %d %d\n", i2->type(), i2->address(), i2->value(), i2->delay());
    //             sleep_ms(10);
    //         }
    //     }

        
    //     RemoteFile::Button *btn = rfile.addButton(30, "Cast", "LightMagenta", "", 0);;
    //     if (btn)
    //     {
    //         btn->clearActions();
    //         btn->addAction("bogus", 1, 2, 3);
    //     }

    //     rfile.deleteButton(31);

    //     rfile.changePosition(rfile.getButton(18), 19);

    //     std::stringstream out;
    //     rfile.outputJSON(out);
    //     printf("%s", out.str().c_str());
    // }

    while (true)
    {
        ;
    }

    return 0;
}
