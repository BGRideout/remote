#include "remote.h"
#include "remotefile.h"
#include "txt.h"
#include "config.h"
#include "web.h"
#include "web_files.h"
#include "jsonmap.h"

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

bool Remote::http_message(WEB *web, void *client, HTTPRequest &rqst)
{
    bool ret = false;
 
    if (rqst.type() == "GET")
    {
        ret = http_get(web, client, rqst);
    }
    return ret;
}

void Remote::ws_message(WEB *web, void *client, const std::string &msg)
{
    printf("Message: %s\n", msg.c_str());
    JSONMap msgmap(msg.c_str());
    if (msgmap.hasProperty("btnVal"))
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

        if (action == "press")
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
    }
    else
    {
        printf("btnVal not found\n");
    }
 }

bool Remote::http_get(WEB *web, void *client, HTTPRequest &rqst)
{
    bool ret = false;
    std::string url = rqst.path();
    printf("GET %s\n", url.c_str());
    const char *data;
    u16_t datalen;
    if (url.length() > 0 && WEB_FILES::get()->get_file(url.substr(1), data, datalen))
    {
        web->send_data(client, data, datalen, false);
        ret = true;
    }
    else
    {
        printf("Special URL %s\n", url.c_str());
        ret = remote_page(web, client, rqst);
    }
    return ret;
}

bool Remote::remote_page(WEB *web, void *client, HTTPRequest &rqst)
{
    bool ret = false;
    std::string action_file = "actions.json";
    if (rfile_.filename() != action_file)
    {
        rfile_.loadFile(action_file.c_str());
    }

    std::string html = "<!DOCTYPE html>"
        "<html>\n"
        " <head>\n"
        "  <title id=\"title1\">!!title!!</title>\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "  <link rel=\"stylesheet\" type=\"text/css\" href=\"/webremote.css\" />\n"
        "  <script type=\"text/javascript\" src=\"/websocket.js\"></script>\n"
        "  <script type=\"text/javascript\" src=\"/webremote.js\"></script>\n"
        " </head>\n"
        " <body>\n"
        "   <span id=\"led\" class=\"led\"></span>\n"
        "   <h1 id=\"title\">!!title!!</h1>\n"
        "   <form id=\"btnForm\" method=\"post\">\n"
        "    <div id=\"btndiv\" class=\"buttons\">\n"
        "       !!buttons!!\n"
        "    </div>\n"
        "  </form>\n"
        " </body>\n";

    while(TXT::substitute(html, "!!title!!", rfile_.title()));

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
            button = "<span style=\"grid-row:!row!; grid-column:!col!; color: {0}; background: {1}; font-size: 24px; font-weight: bold;\">\n"
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
