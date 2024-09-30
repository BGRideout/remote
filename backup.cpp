//                  *****  Backup class implementation  *****

#include "backup.h"
#include "remotefile.h"
#include "menu.h"
#include "txt.h"
#include "web.h"
#include "config.h"
#include <tiny-json.h>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <vector>
#include <sys/stat.h>

const char *Backup::hdrs[] =
    {
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=",

        "\r\n"
        "Set-Cookie: msg=Success; Max-Age=5\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: ",

        "\r\n\r\n",

        "{\"backup\":\n[\n",

        "\n{\"file\": \"",

        "\", \"data\": ",

        "}\n",

        "]}\n"
    };

#define HTTP_HDR_1      0
#define HTTP_HDR_2      1
#define HTTP_HDR_3      2
#define BACKUP_HDR      3
#define FILE_HDR_1      4
#define FILE_HDR_2      5
#define FILE_TRAILER    6
#define BACKUP_TRAILER  7

#define SEGSize(idx) strlen(hdrs[idx])

bool Backup::loadBackup(const HTTPRequest &post, std::string &msg)
{
    bool ret = false;
    std::string filename = post.postValue("actfile.filename");
    const char *actfile = post.postValue("actfile");
    size_t la = strlen(actfile);
    char *data = new char[la + 1];
    memcpy(data, actfile, la + 1);
    data[la] = 0;
    json_t jbuf[la / 4];

    json_t const* json = json_create(data, jbuf, sizeof jbuf / sizeof *jbuf );
    if (json)
    {
        const json_t *prop = json_getProperty(json, "backup");
        if (prop)
        {
            if (json_getType(prop) == JSON_ARRAY)
            {
                for (const json_t *act = json_getChild(prop); act != nullptr; act = json_getSibling(act))
                {
                    filename = json_getPropertyValue(act, "file");
                    const json_t *actions = json_getProperty(act, "data");
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
    bool ret = true;
    std::string msg("Success");
    std::vector<std::string> files;
    const char *savefile = rqst.postValue("savefile");
    if (!savefile) savefile = "";
    std::string downloadFile = savefile;
    if (downloadFile == "all")
    {
        // Full backup
        downloadFile = CONFIG::get()->hostname();
        downloadFile += ".json";
        RemoteFile::actionFiles(files);
        Menu::menuFiles(files);
    }
    else
    {
        // Single file backup
        if (!downloadFile.empty())
        {
            files.push_back(downloadFile);
        }
    }

    // Compute content length
    uint32_t cl = SEGSize(BACKUP_HDR) + SEGSize(BACKUP_TRAILER);
    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        cl += fileSegmentSize(it->c_str());
    }

    // Send header and backup header
    std::string resp(hdrs[HTTP_HDR_1]);
    resp += downloadFile;
    resp += hdrs[HTTP_HDR_2];
    resp += std::to_string(cl);
    resp += hdrs[HTTP_HDR_3];

    resp += hdrs[BACKUP_HDR];
    web->send_data(client, resp.c_str(), resp.length());

    // Send file segments
    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        resp = hdrs[FILE_HDR_1] + *it + hdrs[FILE_HDR_2];
        web->send_data(client, resp.c_str(), resp.length());

        char *data = fileData(it->c_str());
        if (data)
        {
            web->send_data(client, data, fileSize(it->c_str()));
            delete [] data;
        }
        web->send_data(client, hdrs[FILE_TRAILER], SEGSize(FILE_TRAILER));
    }

    // Send backup trailer
    web->send_data(client, hdrs[BACKUP_TRAILER], SEGSize(BACKUP_TRAILER));

    return ret;
}

uint32_t Backup::fileSize(const char *filename)
{
    uint32_t ret = 0;
    struct stat sb;
    int sts = stat(filename, &sb);
    if (sts == 0)
    {
        ret = sb.st_size;
    }
    return ret;
}

uint32_t Backup::fileSegmentSize(const char *filename)
{
    uint32_t ret = fileSize(filename);
    ret += SEGSize(FILE_HDR_1) + strlen(filename) + SEGSize(FILE_HDR_2) + SEGSize(FILE_TRAILER);
    return ret;
}

char *Backup::fileData(const char *filename)
{
    char *ret = nullptr;
    uint32_t sz = fileSize(filename);
    if (sz > 0)
    {
        FILE *f = fopen(filename, "r");
        if (f)
        {
            ret = new char[sz + 1];
            int nn = fread(ret, 1, sz, f);
            if (nn == sz)
            {
                ret[sz] = 0;
            }
            else
            {
                delete [] ret;
                ret = nullptr;
            }
            fclose(f);
        }
    }
    return ret;
}
