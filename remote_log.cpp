//                 ***** Remote class "test" methods  *****

#include "remote.h"
#include "file_logger.h"
#include "txt.h"
#include "web_files.h"
#include <stdio.h>

bool Remote::log_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("log.html", data, datalen))
    {
        int32_t endl = log_->line_count();
        try
        {
            endl = std::stol(rqst.query("endl"));
        }
        catch(const std::exception& e)
        {
            ;
        }
        endl = endl > log_->line_count() ? log_->line_count() : endl;
        int32_t bgnl = endl - 50;
        if (bgnl < 0)
        {
            bgnl = 0;
            endl = 50 > log_->line_count() ? log_->line_count() : 50;
        }
        
        long bgnp = log_->find_line(bgnl);
        long endp = log_->find_line(endl - bgnl, bgnp);

        TXT html(data, datalen, datalen + endp - bgnp + 128);

        html.substitute("<?from?>", bgnl);
        html.substitute("<?to?>", endl);
        html.substitute("<?dbglvl?>", debug_level_);

        uint32_t pos = html.find("<?lines?>");
        html.replace(pos, 9, "");

        char linebuf[133];
        FILE *f = log_->open();
        log_->position(f, bgnp);
        while (bgnl++ < endl && log_->read(f, linebuf, sizeof(linebuf)))
        {
            pos = html.insert(pos, linebuf);
        }
        log_->close(f);

        ret = send_http(web, client, html, close);
    }
    return ret;
}


bool Remote::log_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    rqst.printPostData();
    const char *dbg = rqst.postValue("dbg");
    if (dbg)
    {
        debug_level_ = atoi(dbg);
        WEB::get()->setDebug(debug_level_);
    }

    const char *btn = rqst.postValue("btn");
    if (btn && strcmp(btn, "download") == 0)
    {
        uint32_t fsz = log_->file_size();
        char hdr[] = "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=log.txt\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: ";
        TXT html(hdr, sizeof(hdr) - 1, sizeof(hdr) + fsz + 32);
        html += std::to_string(fsz).c_str();
        html += "\r\n\r\n";
        web->send_data(client, html.data(), html.datasize(), WEB::PREALL);
        html.release();

        char buf[4096];
        FILE *f = log_->open();
        while (fsz > 0)
        {
            uint32_t nn = fsz > sizeof(buf) ? sizeof(buf) : fsz;
            uint32_t nw = fread(buf, 1, nn, f);
            if (nn == nw)
            {
                web->send_data(client, buf, nw);
                fsz -= nw;
            }
            else
            {
                printf("Log read error\n");
                return false;
            }
        }
        log_->close(f);
        return true;
    }

    std::string resp("HTTP/1.1 303 OK\r\nLocation: /log");
    const char *to = rqst.postValue("to");
    if (to)
    {
        uint32_t endl = atol(to);
        if (btn)
        {
            if (strcmp(btn, "up") == 0)
            {
                endl -= 50;
            }
            else if (strcmp(btn, "down") == 0)
            {
                endl += 50;
            }
            else if (strcmp(btn, "down") == 0)
            {
                endl += 50;
            }
            else if (strcmp(btn, "top") == 0)
            {
                endl = 0;
            }
            else if (strcmp(btn, "bottom") == 0)
            {
                endl = log_->max_line_count() + 50;
            }
        }
        resp += "?endl=" + std::to_string(endl);
    }
    resp += "\r\nConnection: keep-alive\r\n\r\n";
    close = false;
    return web->send_data(client, resp.c_str(), resp.length());
}
