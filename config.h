#ifndef CONFIG_H
#define CONFIG_H

#define HOSTNAME "webremote"
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

class CONFIG
{
private:
    struct _config
    {
        char    hostname[32];
        char    ssid[64];
        char    password[64];
        char    title[16];
        char    timezone[64];
        int     debug;
    } cfgdata;

    CONFIG();
    bool read_config();
    bool write_config();

public:
    static CONFIG *get() {static CONFIG *singleton = nullptr; if (!singleton) singleton = new CONFIG(); return singleton;}
    bool init();

    const char *hostname() const { return cfgdata.hostname; }
    const char *ssid() const { return cfgdata.ssid; }
    const char *password() const { return cfgdata.password; }
    const char *title() const { return cfgdata.title; }
    const char *timezone() const { return cfgdata.timezone; }
    int         debug() const { return cfgdata.debug; }

    bool set_hostname(const char *hostname);
    bool set_wifi_credentials(const char *ssid, const char *password);
    bool set_title(const char *title);
    bool set_timezone(const char *timezone);
    bool set_debug(int debug);
};

#endif
