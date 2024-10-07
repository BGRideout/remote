//                  ***** Remote class "remote" methods  *****

#include "remote.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"

bool Remote::remote_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
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

    TXT html(data, datalen, 16384);

    while(html.substitute("<?title?>", rfile_.title()));

    std::string backurl = rqst.root();
    std::size_t i1 = backurl.rfind('/');
    if (i1 != std::string::npos)
    {
        backurl = backurl.erase(i1);
    }
    if (backurl.empty()) backurl = "/";
    if (strcmp(rfile_.filename(), "actions.json") != 0)
    {
        html.substitute("<?backloc?>", backurl.c_str());
        html.substitute("<?backvis?>", "visible");
    }
    else
    {
        html.substitute("<?backloc?>", "/");
        html.substitute("<?backvis?>", "hidden");
    }

    std::size_t bi = html.find("<?buttons?>");
    html.substitute("<?buttons?>", "");
    TXT button(640);
    std::string background;
    std::string color;
    std::string fill;
    std::string label;

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

            button.substitute("<?class?>", strlen(it->redirect()) > 0 && it->actions().size() == 0 ? " class=\"redir\"" : "");
        }
        else
        {
            button = "<span style=\"grid-row:!row!; grid-column:!col!; color: {0}; background: transparent; font-size: 24px; font-weight: bold;\">\n"
                "  !label!\n"
                "</span>\n";
        }
        button.substitute("!row!", row);
        button.substitute("!col!", col);
        button.substitute("!pos!", pos);

        it->getColors(background, color, fill);

        label = it->label();
        get_label(label, background, color, fill);
        button.substitute("!label!", label);
        while (button.substitute("{0}", color));
        while (button.substitute("{1}", background));
        while (button.substitute("{2}", fill));
        html.insert(bi, button.data());
        bi += button.datasize();
    }

    ret = send_http(web, client, html, close);
   
    return ret;
}

bool Remote::remote_button(WEB *web, ClientHandle client, const JSONMap &msgmap)
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
