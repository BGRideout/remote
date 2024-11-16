//                  ***** Remote class "backup" methods  *****

#include "remote.h"
#include "backup.h"
#include "command.h"
#include "config.h"
#include "menu.h"
#include "txt.h"
#include "web_files.h"

bool Remote::backup_get(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("backup.html", data, datalen))
    {
        std::string files = "<option value='all'>All (backup: ";
        files += CONFIG::get()->hostname();
        files += ".json)</option>\n";

        std::set<std::string> action_files;
        RemoteFile::actionFiles(action_files);
        Menu::menuFiles(action_files);
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

        TXT html(data, datalen, 2048);
        html.substitute("<?files?>", files);

        std::vector<std::string> msg_color;
        TXT::split(rqst.userData(), "|", msg_color);
        if (msg_color.size() == 2)
        {
            html.substitute("<?msg?>", msg_color.at(0));
            html.substitute("<?msgcolor?>", msg_color.at(1));
        }
        else
        {
            html.substitute("<?msg?>", "");
            html.substitute("<?msgcolor?>", "transparent");
        }
        
        ret = send_http(web, client, html, close);
    }
    return ret;
}

bool Remote::backup_post(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    close = false;
    //rqst.printPostData();
    std::string msg("Success");
    std::string button = rqst.postValue("button");
    if (button == "upload")
    {
        rfile_.clear();
        efile_.clear();
        ret = Backup::loadBackup(rqst, msg);
        if (!ret)
        {
            msg = "Load failed";
        }
    }
    else if (button == "download")
    {
        ret = Backup::saveBackup(web, client, rqst);
        return ret;
    }
    else
    {
        msg = "Invalid button";
    }

    msg += ret ? "|green" : "|red";
    rqst.setUserData(msg);
    return backup_get(web, client, rqst, close);
}
