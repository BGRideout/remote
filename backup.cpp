//                  *****  Backup class implementation  *****

#include "backup.h"
#include "remotefile.h"
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
    printf("Filename: %s\n", filename);

    if (strncmp(filename, "actions", 7) == 0)
    {
        ret = rfile.loadJSON(json, filename);
        if (ret)
        {
            std::ostringstream out;
            rfile.outputJSON(out);
            printf("File data:\n%s\n", out.str().c_str());
            FILE *f = fopen(filename, "w");
            if (f)
            {
                size_t n = fwrite(out.str().c_str(), out.str().size() + 1, 1, f);
                int sts = fclose(f);
                if (n != 1 || sts != 0)
                {
                    printf("Failed to write file %s: n=%d sts=%d\n", filename, n, sts);
                    ret = false;
                }
            }
        }
    }
    else if (strncmp(filename, "menu_", 5) == 0)
    {
        printf("menu file\n");
        ret = true;
    }

    return ret;
}
