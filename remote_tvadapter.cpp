//                 ***** Remote class "tvadapter" methods  *****

#include "remote.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"
#include <stdio.h>

bool Remote::tvadapter_button(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    bool ret = false;
    const char *lbl = msgmap.strValue("button", "");
    const char *action = msgmap.strValue("func", "");
    if (strlen(action) > 7) action += 7;
    get_rfile("/tvadapter.html");
    int btnpos = rfile_.findButtonPosition(lbl);
    if (btnpos != -1)
    {
        log_->print_debug(1, "TV adapter button %s at position %d\n", lbl, btnpos);
        std::string json = std::string("{\"func\": \"btnVal\", \"btnVal\": \"" +
                                    std::to_string(btnpos)  + "\", \"action\": \"" +
                                    action + "\", \"path\": \"/tvadapter\" }");
        JSONMap jmap(json.c_str());
        ret = remote_button(web, client, jmap);
    }
    else
    {
        log_->print_error("Did not find TV adapter button '%s'\n", lbl);
    }
    return ret;
}

bool Remote::tvadapter_input(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    bool ret = false;
    int addr = msgmap.intValue("address", -1);
    if (addr >= 0 && addr < 15)
    {
        int addrs[2] = {addr, 0};
        for (int ii = 0; !ret && ii < 2; ii++)
        {
            char lbl[10];
            sprintf(lbl, "Input %d", addrs[ii]);
            int btnpos = rfile_.findButtonPosition(lbl);
            if (btnpos != -1)
            {
                log_->print_debug(1, "TV adapter button %s at position %d\n", lbl, btnpos);
                std::string json = std::string("{\"func\": \"btnVal\", \"btnVal\": \"" +
                                            std::to_string(btnpos)  + "\", \"action\": \"" +
                                            "click" + "\", \"path\": \"/tvadapter\" }");
                JSONMap jmap(json.c_str());
                ret = remote_button(web, client, jmap);
            }
        }
    }
    return ret;
}
