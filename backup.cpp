//                  *****  Backup class implementation  *****

#include "backup.h"
#include "remotefile.h"
#include "menu.h"
#include "txt.h"
#include "web.h"
#include <tiny-json.h>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <sys/stat.h>

bool Backup::loadBackup(const HTTPRequest &post, std::string &msg)
{
    bool ret = false;
    std::string filename = post.postValue("actfile.filename");
    const char *actfile = post.postValue("actfile");
    size_t la = strlen(actfile);
    char *data = new char[la + 1];
    memcpy(data, actfile, la + 1);
    json_t jbuf[la / 4];

    json_t const* json = json_create(data, jbuf, sizeof jbuf / sizeof *jbuf );
    if (json)
    {
        const json_t *prop = json_getProperty(json, "backup");
        if (prop)
        {
            printf("loading full backup\n");
            if (json_getType(prop) == JSON_ARRAY)
            {
                for (const json_t *act = json_getChild(prop); act != nullptr; act = json_getSibling(act))
                {
                    filename = json_getPropertyValue(act, "file");
                    const json_t *actions = json_getProperty(act, "data");
                    printf("loading actions for %s\n", filename.c_str());
                    ret = loadBackupFile(actions, filename.c_str());
                }
            }
        }
        else
        {
            prop = json_getProperty(json, "data");
            if (prop)
            {
                filename = json_getPropertyValue(json, "file");
                ret = loadBackupFile(prop, filename.c_str());
            }
            else
            {
                ret = loadBackupFile(json, filename.c_str());
            }
        }
    }

    delete [] data;
    
    return ret;
}

bool Backup::loadBackupFile(const json_t *json, const char *filename)
{
    bool ret = false;
    RemoteFile rfile;
    Menu       menu;
    printf("Filename: %s\n", filename);

    if (strncmp(filename, "actions", 7) == 0)
    {
        ret = rfile.loadJSON(json, filename);
        if (ret)
        {
            ret = rfile.saveFile();
        }
    }
    else if (strncmp(filename, "menu_", 5) == 0)
    {
        ret = menu.loadJSON(json, filename);
        if (ret)
        {
            ret = menu.saveFile();
        }
    }

    return ret;
}

bool Backup::saveBackup(WEB *web, void *client, const HTTPRequest &rqst)
{
    bool ret = false;
    int err = 0;
    std::string msg("Success");
    const char *savefile = rqst.postValue("savefile");
    if (!savefile) savefile = "";
    if (savefile == "all")
    {
        // Full backup
    }
    else
    {
        // Single file backup
        struct stat sb;
        int sts = stat(savefile, &sb);
        err = errno;
        if (sts == 0)
        {
            FILE *f = fopen(savefile, "r");
            err = errno;
            if (f)
            {
                char *data = new char[sb.st_size];
                sts = fread(data, sizeof(char), sb.st_size, f);
                err = errno;
                if (sts == sb.st_size)
                {
                    if (data[sb.st_size - 1] == 0)
                    {
                        sb.st_size -= 1;
                    }
                    
                    std::string resp("HTTP/1.1 200 OK\r\n"
                                     "Content-Type: application/octet-stream\r\n"
                                     "Content-Disposition: attachment; filename=");
                    resp += savefile;
                    resp += "\r\n"
                                     "Set-Cookie: msg=Success; Max-Age=5\r\n"
                                     "Connection: keep-alive\r\n"
                                     "Content-Length: ";

                    std::string start("{\"file\": \"");
                    start += savefile;
                    start += "\", \"data\": ";

                    std::string finish("}");
                    uint32_t ll = start.length() + sb.st_size + finish.length();

                    resp += std::to_string(ll) + "\r\n\r\n" + start;
                    web->send_data(client, resp.c_str(), resp.length());
                    web->send_data(client, data, sb.st_size);
                    web->send_data(client, finish.c_str(), finish.length());
                    ret = true;
                }
                delete [] data;
                fclose(f);
            }
        }
    }

    if (!ret)
    {
        msg = strerror(err);
        std::string resp("HTTP/1.1 303 OK\r\nLocation: /backup\r\n"
                        "Set-Cookie: msg<?msg?>; Max-Age=5\r\n"
                        "Set-Cookie: msgcolor=<?msgcolor?>; Max-Age=5\r\n"
                        "Connection: keep-alive\r\n\r\n");
        TXT::substitute(resp, "<?msg?>", msg);
        TXT::substitute(resp, "<?msgcolor?>", ret ? "green" : "red");
        web->send_data(client, resp.c_str(), resp.length());
    }
    return ret;
}