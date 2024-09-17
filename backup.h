//                  *****  Backup Class  *****

#ifndef BACKUP_H
#define BACKUP_H

#include "httprequest.h"
#include <tiny-json.h>

class Backup
{
private:

    static bool loadBackupFile(const json_t *json, const char *filename);

public:

    /**
     * @brief   Load files from upload POST message
     * 
     * @param   post        Backup POST request
     * 
     * @return  true if successful
     */
    static bool loadBackup(const HTTPRequest &post);

};

#endif
