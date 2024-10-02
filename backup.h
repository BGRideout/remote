//                  *****  Backup Class  *****

#ifndef BACKUP_H
#define BACKUP_H

#include "web.h"
#include "httprequest.h"
#include <stdint.h>
#include <tiny-json.h>

class Backup
{
private:
    static const char *hdrs[];        // Header segments

    static bool loadBackupFile(const json_t *json, const char *filename);

    static uint32_t fileSize(const char *filename);
    static uint32_t fileSegmentSize(const char *filename);
    static char *fileData(const char *filename);

public:

    /**
     * @brief   Load files from upload POST message
     * 
     * @param   post        Backup POST request
     * @param   msg         Message return string
     * 
     * @return  true if successful
     */
    static bool loadBackup(const HTTPRequest &post, std::string &msg);

    /**
     * @brief   Download backup of a file
     * 
     * @param   web         Pointer to web object
     * @param   client      Client handle
     * @param   rqst        POST request
     * 
     */
    static bool saveBackup(WEB *web, ClientHandle client, const HTTPRequest &rqst);
};

#endif
