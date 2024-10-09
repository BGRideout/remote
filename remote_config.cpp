//                  ***** Remote class "config" methods  *****

#include "remote.h"
#include "config.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"
#include <stdio.h>

bool Remote::config_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("config.html", data, datalen))
    {
        ret = web->send_data(client, data, datalen, WEB::STAT);
    }
    return ret;
}

bool Remote::config_get_wifi(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    std::string resp;
    config_wifi_message(web, resp);
    return web->send_message(client, resp.c_str());
}

void Remote::config_wifi_message(WEB *web, std::string &message)
{
    message = "{\"func\": \"wifi_resp\", \"host\": \"" + web->hostname() +
              "\", \"ssid\": \"" + web->wifi_ssid() + "\", \"ip\": \"" + web->ip_addr() + "\"}";
}

bool Remote::config_update(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    CONFIG *cfg = CONFIG::get();
    const char *hostname = msgmap.strValue("hostname");
    const char *ssid = msgmap.strValue("ssid");
    const char *pwd = msgmap.strValue("pwd");
    if (hostname && ssid && pwd)
    {
        if (strlen(pwd) == 0) pwd = cfg->password();
        if (strcmp(hostname, cfg->hostname()) != 0 ||
            strcmp(ssid, cfg->ssid()) != 0 ||
            strcmp(pwd, cfg->password()) != 0)
        {
            cfg->set_hostname(hostname);
            cfg->set_wifi_credentials(ssid, pwd);
            web->update_wifi(hostname, ssid, pwd);
        }
        else
        {
            std::string resp;
            config_wifi_message(web, resp);
            if (isDebug(1)) log_->print("WiFi update response: %s\n", resp.c_str());
            web->send_message(client, resp.c_str());
        }
    }
    return true;
}

bool Remote::config_scan_wifi(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    web->scan_wifi(client, config_scan_complete, this);
    return true;
}

bool Remote::config_scan_complete(WEB *web, ClientHandle client, const WiFiScanData &data, void *user_data)
{
    std::string resp("{\"func\": \"wifi-ssids\", \"ssids\": [");
    std::string sep("{\"name\": \"");
    for (auto it = data.cbegin(); it != data.cend(); ++it)
    {
        if (!it->first.empty())
        {
            resp += sep + it->first;
            sep = "\"}, {\"name\": \"";
        }
    }
    resp += "\"}]}";
    if (isDebug(1)) get()->log_->print("WiFi scan response: %s\n", resp.c_str());
    return web->send_message(client, resp.c_str());
}