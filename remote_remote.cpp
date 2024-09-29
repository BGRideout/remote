//                  ***** Remote class "remote" methods  *****

#include "remote.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"

bool Remote::remote_get(WEB *web, void *client, const HTTPRequest &rqst, bool &close)
{
    bool ret = get_rfile(rqst.root());
    if (!ret)
    {
        printf("Error loading action file for %s\n", rqst.url().c_str());
        return false;
    }

    const char *data;
    u16_t datalen;
    WEB_FILES::get()->get_file("index.html", data, datalen);

    std::string html(data, datalen);

    while(TXT::substitute(html, "<?title?>", rfile_.title()));

    std::string backurl = rqst.root();
    std::size_t i1 = backurl.rfind('/');
    if (i1 != std::string::npos && backurl.length() > i1 + 1)
    {
        backurl = backurl.erase(i1 + 1);
    }
    if (strcmp(rfile_.filename(), "actions.json") != 0)
    {
        TXT::substitute(html, "<?backloc?>", backurl);
        TXT::substitute(html, "<?backvis?>", "visible");
    }
    else
    {
        TXT::substitute(html, "<?backloc?>", "/");
        TXT::substitute(html, "<?backvis?>", "hidden");
    }

    std::size_t bi = html.find("<?buttons?>");
    TXT::substitute(html, "<?buttons?>", "");
    std::string button;
    for (auto it = rfile_.buttons().cbegin(); it != rfile_.buttons().cend(); ++it)
    {
        int pos = it->position();
        int row = (pos - 1) / 5 + 1;
        int col = (pos - 1) % 5 + 1;

        if (strlen(it->redirect()) > 0 || it->actions().size() > 0)
        {
            button = "<button type=\"submit\"<?class?> style=\"grid-row:!row!; grid-column:!col!; color: {0}; background: {1};\""
                " name=\"btnVal\" value=\"!pos!\">\n"
                "  !label!\n"
                "</button>\n";

            TXT::substitute(button, "<?class?>", strlen(it->redirect()) > 0 && it->actions().size() == 0 ? " class=\"redir\"" : "");
        }
        else
        {
            button = "<span style=\"grid-row:!row!; grid-column:!col!; color: {0}; background: transparent; font-size: 24px; font-weight: bold;\">\n"
                "  !label!\n"
                "</span>\n";
        }
        TXT::substitute(button, "!row!", std::to_string(row));
        TXT::substitute(button, "!col!", std::to_string(col));
        TXT::substitute(button, "!pos!", std::to_string(pos));

        std::string background;
        std::string color;
        std::string fill;
        it->getColors(background, color, fill);

        std::string label = it->label();
        get_label(label, background, color, fill);
        TXT::substitute(button, "!label!", label);
        while (TXT::substitute(button, "{0}", color));
        while (TXT::substitute(button, "{1}", background));
        while (TXT::substitute(button, "{2}", fill));
        html.insert(bi, button);
        bi += button.length();
    }

    HTTPRequest::setHTMLLengthHeader(html);
    web->send_data(client, html.c_str(), html.length());
    close = false;
    
    return ret;
}

bool Remote::remote_button(WEB *web, void *client, const JSONMap &msgmap)
{
    bool ret = false;
    printf("btnVal = %d, action = %s path = %s duration = %f\n",
        msgmap.intValue("btnVal"), msgmap.strValue("action"), msgmap.strValue("path"), msgmap.realValue("duration"));

    int button = msgmap.intValue("btnVal");
    std::string url = msgmap.strValue("path");
    get_rfile(url);
    RemoteFile::Button *btn = rfile_.getButton(button);
    if (btn)
    {
        ret = true;
        Command *cmd = new Command(web, client, msgmap, btn);
        queue_command(cmd);
    }
    return ret;
}
