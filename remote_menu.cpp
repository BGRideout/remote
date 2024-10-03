//                  ***** Remote class "setup" methods  *****

#include "remote.h"
#include "menu.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"

bool Remote::menu_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("menuedit.html", data, datalen))
    {
        std::string html(data, datalen);
        std::string menu_name = rqst.query("menu");
        std::string readonly;
        std::vector<std::string> names;
        Menu::menuNames(names);
        std::string data;
        for (auto it = names.cbegin(); it != names.cend(); ++it)
        {
            data += "<option value=\"" + *it + "\"" + (menu_name == *it ? " selected" : "") + ">" + *it + "</option>";
        }
        while(TXT::substitute(html, "<?menus?>", data));

        std::map<std::string, Command::Step> emptymap;
        std::map<std::string, Command::Step> &cmdmap = emptymap;
        std::string rowspercol;
        Menu *menu = Menu::getMenu(menu_name);
        if (menu)
        {
            menu_name = menu->name();
            readonly = " readonly";
            cmdmap = menu->commands();
            rowspercol = menu->rowsPerColumn();
        }
        else
        {
            menu_name = "";
            emptymap["open"] = Command::Step();
            emptymap["up"] = Command::Step();
            emptymap["down"] = Command::Step();
            emptymap["left"] = Command::Step();
            emptymap["right"] = Command::Step();
            emptymap["ok"] = Command::Step();
        }

        while(TXT::substitute(html, "<?menuname?>", menu_name));
        while(TXT::substitute(html, "<?readonly?>", readonly));
        for (auto it = cmdmap.cbegin(); it != cmdmap.cend(); ++it)
        {
            std::string key = it->first;
            const Command::Step &step = it->second;
            TXT::substitute(html, "<?" + key + "typ?>", step.type());
            TXT::substitute(html, "<?" + key + "add?>", std::to_string(step.address()));
            TXT::substitute(html, "<?" + key + "val?>", std::to_string(step.value()));
            TXT::substitute(html, "<?" + key + "dly?>", std::to_string(step.delay()));
        }
        TXT::substitute(html, "<?rowspercol?>", rowspercol);

        HTTPRequest::setHTMLLengthHeader(html);
        web->send_data(client, html.c_str(), html.length());
        close = false;
        ret = true;
    }
    return ret;
}

bool Remote::menu_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    rqst.printPostData();
    const char *sel = rqst.postValue("sel");
    if (sel)
    {
        std::string resp("HTTP/1.1 303 OK\r\nLocation: /menu");
        if (strlen(sel) > 0)
        {
            resp += "?menu=";
            resp += sel;
        }
        resp += "\r\nConnection: keep-alive\r\n\r\n";
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
        return true;
    }

    const char *name = rqst.postValue("name");
    const char *original = rqst.postValue("original");
    if (original)
    {
        Menu *menu = nullptr;
        if (strlen(original) == 0 && name && strlen(name) > 0)
        {
            // New menu
            menu = Menu::addMenu(name);
        }
        else
        {
            menu = Menu::getMenu(original);
        }

        if (menu)
        {
            const char *value = rqst.postValue("rows");
            if (value)
            {
                menu->setRowsPerColumn(value);
            }


            std::vector<const char *> typ;
            std::vector<const char *> add;
            std::vector<const char *> val;
            std::vector<const char *> dly;
            rqst.postArray("typ", typ);
            rqst.postArray("add", add);
            rqst.postArray("val", val);
            rqst.postArray("dly", dly);

            const char *act[] = {"open", "up", "down", "left", "right", "ok"};
            size_t nn = sizeof(act) / sizeof(*act);
            if (add.size() == nn && val.size() == nn && dly.size() == nn)
            {
                for (int ii = 0; ii < nn; ii++)
                {
                    if (typ[ii] && strlen(typ[ii]) > 0)
                    {
                        menu->setIRCode(act[ii], typ[ii], to_u16(add[ii]), to_u16(val[ii]), to_u16(val[ii]));
                    }
                    else
                    {
                        menu->setIRCode(act[ii], "", 0, 0, 0);
                    }
                }
            }

            menu->saveFile();
        }

        std::string resp("HTTP/1.1 303 OK\r\nLocation: /menu?menu=");
        resp += original;
        resp += "\r\nConnection: keep-alive\r\n\r\n";
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
        ret = true;
    }

    return ret;
}

bool Remote::menu_ir_get(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    bool ret = true;
    printf("ir_get = %d, path = %s\n", msgmap.intValue("ir_get"), msgmap.strValue("path"));

    int row = msgmap.intValue("ir_get");
    Command *cmd = new Command(web, client, msgmap, nullptr);
    queue_command(cmd);
    return ret;
}

