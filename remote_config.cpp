//                  ***** Remote class "config" methods  *****

#include "remote.h"
#include "config.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"
#include <stdio.h>
#include <sys/stat.h>

#define CERTFILENAME    "cert.pem"
#define KEYFILENAME     "key.pem"
#define PASSFILENAME    "pass.txt"


bool Remote::config_get(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
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

bool Remote::config_post(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    CONFIG *cfg = CONFIG::get();
    const char *hostname = rqst.postValue("hostname");
    const char *ssid = rqst.postValue("ssid");
    const char *pwd = rqst.postValue("pwd");
    const char *timezone = rqst.postValue("timezone");

    if (hostname && ssid && pwd && timezone)
    {
        if (strlen(pwd) == 0) pwd = cfg->password();
        bool wifichange = false;
        if (strcmp(hostname, cfg->hostname()) != 0)
        {
            cfg->set_hostname(hostname);
            wifichange = true;
        }

        if (strcmp(ssid, cfg->ssid()) != 0 ||
            strcmp(pwd, cfg->password()) != 0)
        {
            cfg->set_wifi_credentials(ssid, pwd);
            wifichange = true;
        }

        if (strcmp(timezone, cfg->timezone()) != 0)
        {
            cfg->set_timezone(timezone);
            setenv("TZ", timezone, 1);
            tzset();
        }

        if (wifichange)
        {
            web->update_wifi(hostname, ssid, pwd);
        }
    }

    const char *certfile = rqst.postValue("cert.filename");
    const char *keyfile = rqst.postValue("key.filename");
    const char *pass = rqst.postValue("pass");
    if (certfile && keyfile && pass)
    {
        bool certchange = false;
        if (strlen(certfile) != 0)
        {
            FILE *fd = fopen(CERTFILENAME, "w");
            fputs(rqst.postValue("cert"), fd);
            fclose(fd);
            certchange = true;
        }

        if (strlen(keyfile) != 0)
        {
            FILE *fd = fopen(KEYFILENAME, "w");
            fputs(rqst.postValue("key"), fd);
            fclose(fd);
            certchange = true;
        }

        if (strlen(pass) > 0)
        {
            FILE *fd = fopen(PASSFILENAME, "w");
            fputs(pass, fd);
            fclose(fd);
            certchange = true;
        }

#ifdef USE_HTTPS
        if (certchange)
        {
            bool ret = config_get(web, client, rqst, close);
            close = true;
            log_->print("Changing TLS parameters\n");
            if (web->is_https_listening())
            {
                web->stop_https();
            }
            if (web->start_https())
            {
                log_->print("Started HTTPS\n");
                if (!web->is_ap_active())
                {
                    web->stop_http();
                }
            }
            else
            {
                log_->print("Failed to start HTTPS\n");
                web->start_http();
            }
            return ret;
        }
#endif
    }
    return config_get(web, client, rqst, close);
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
              "\", \"ssid\": \"" + web->wifi_ssid() + "\", \"ip\": \"" + web->ip_addr() +
              "\", \"timezone\": \"" + CONFIG::get()->timezone() +
              "\", \"http\": \"" + std::string(web->is_http_listening() ? "true" : "false") +
              "\", \"https\": \"" + std::string(web->is_https_listening() ? "true" : "false") +
#ifdef USE_HTTPS
              "\", \"https_ena\": \"true\"" + 
#else
              "\", \"https_ena\": \"false\"" + 
#endif
              "}";
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
    get()->log_->print_debug(1, "WiFi scan response: %s\n", resp.c_str());
    return web->send_message(client, resp.c_str());
}

bool Remote::tls_callback(WEB *web, std::string &cert, std::string &pkey, std::string &pkpass)
{
#ifdef USE_HTTPS
    bool ret = true;
    char    line[128];

    cert.clear();
    pkey.clear();
    pkpass.clear();

    struct stat sb;
    if (stat(CERTFILENAME, &sb) != 0 || sb.st_size < 64)
    {
        // No valid certificate file. Quietly return false
        return false;
    }

    FILE *fd = fopen(CERTFILENAME, "r");
    if (fd)
    {
        while (fgets(line,sizeof(line), fd))
        {
            cert += line;
        }
        fclose(fd);
    }
    else
    {
        printf("Failed to open certificate file\n");
        ret = false;
    }

    fd = fopen(KEYFILENAME, "r");
    if (fd)
    {
        while (fgets(line,sizeof(line), fd))
        {
            pkey += line;
        }
        fclose(fd);
    }
    else
    {
        printf("Failed to open private key file\n");
        ret = false;
    }

    fd = fopen(PASSFILENAME, "r");
    if (fd)
    {
        if (fgets(line,sizeof(line), fd))
        {
            pkpass += line;
        }
        fclose(fd);
    }
    else
    {
        printf("Failed to open passphrase file\n");
        ret = false;
    }
#else
    bool ret = false;
#endif
    return ret;
}
