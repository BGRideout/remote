//                  *****  Backup class implementation  *****

#include "backup.h"
#include "remotefile.h"
#include "menu.h"
#include <tiny-json.h>
#include <string.h>
#include <stdio.h>
#include <sstream>

bool Backup::loadBackup(const HTTPRequest &post)
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
                filename = json_getPropertyValue(prop, "file");
                const json_t *actions = json_getProperty(prop, "data");
                ret = loadBackupFile(actions, filename.c_str());
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
