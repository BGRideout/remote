//                  ***** Remote class "backup" methods  *****

#include "remote.h"
#include "backup.h"
#include "command.h"
#include "config.h"
#include "menu.h"
#include "txt.h"
#include "web_files.h"

bool Remote::backup_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
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
        std::string val = rqst.cookie("msg");
        html.substitute("<?msg?>", val);
        val = rqst.cookie("msgcolor", "transparent");
        html.substitute("<?msgcolor?>", val);
        
        ret = send_http(web, client, html, close);
    }
    return ret;
}

bool Remote::backup_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    close = false;
    //rqst.printPostData();
    std::string msg("Success");
    std::string button = rqst.postValue("button");
    if (button == "upload")
    {
        ret = Backup::loadBackup(rqst, msg);
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

    std::string resp("HTTP/1.1 303 OK\r\nLocation: /backup\r\n"
                    "Set-Cookie: msg=<?msg?>; Max-Age=5\r\n"
                    "Set-Cookie: msgcolor=<?msgcolor?>; Max-Age=5\r\n"
                    "Connection: keep-alive\r\n\r\n");
    TXT::substitute(resp, "<?msg?>", msg);
    TXT::substitute(resp, "<?msgcolor?>", ret ? "green" : "red");
    web->send_data(client, resp.c_str(), resp.length());
    return ret;
}
