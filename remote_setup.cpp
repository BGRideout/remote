//                  ***** Remote class "setup" methods  *****

#include "remote.h"
#include "command.h"
#include "txt.h"
#include "web_files.h"

bool Remote::setup_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        if (base_url.empty()) base_url = "/";

        std::string done = rqst.query("done");
        if (done == "true")
        {
            rfile_.clear();
            efile_.clear();
            std::string resp("HTTP/1.1 303 OK\r\nLocation: " + base_url + "\r\n"
                            "Connection: keep-alive\r\n\r\n");
            web->send_data(client, resp.c_str(), resp.length());
            close = false;
            return true;
        }

        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for GET %s\n", rqst.url().c_str());
            //  Return true as get_efile has issued response
            return true;
        }

        const char *data;
        u16_t datalen;
        if (WEB_FILES::get()->get_file("setup.html", data, datalen))
        {
            std::string html(data, datalen);

            while(TXT::substitute(html, "<?title?>", efile_.title()));

            bool modified = efile_.isModified();
            TXT::substitute(html, "<?modified?>", modified ? "unsaved" : "saved");

            std::size_t bi = html.find("<?buttons?>");
            TXT::substitute(html, "<?buttons?>", "");
            int nb = efile_.maxButtonPosition();
            nb = (nb + 9) / 5 * 5 + 1;
            std::string button;
            for (int pos = 1; pos < nb; pos++)
            {
                RemoteFile::Button *btn = efile_.getButton(pos);
                std::string background;
                std::string color;
                std::string fill;
                RemoteFile::Button::getColors(btn, background, color, fill);

                button = "<button type=\"button\" style=\"";
                int row = (pos - 1) / 5 + 1;
                int col = (pos - 1) % 5 + 1;
                button += "grid-row:" + std::to_string(row) + "; grid-column:" + std::to_string(col);
                button += "; color: " + color + "; background: " + background + ";\" ";
                button += "onclick=\"btnAction(" + std::to_string(pos) + ")\">";
                if (btn)
                {
                    std::string label = btn->label();
                    get_label(label, background, color, fill);
                    button += label;
                }
                button += "</button>";
                html.insert(bi, button);
                bi += button.length();
            }

            HTTPRequest::setHTMLLengthHeader(html);
            web->send_data(client, html.c_str(), html.length());
            close = false;
            ret = true;
        }
    }
    return ret;
}

bool Remote::setup_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    //rqst.printPostData();
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for POST %s\n", rqst.url().c_str());
            //  Return true as get_efile has sent response
            return true;
        }

        const char *title = rqst.postValue("title");
        if (title)
        {
            efile_.setTitle(title);
        }

        const char *save = rqst.postValue("save");
        if (efile_.isModified() && save && strcmp(save, "true") == 0)
        {
            if (efile_.saveFile())
            {
                efile_.clearModified();
                rfile_.clear();
            }
        }

        std::string resp("HTTP/1.1 303 OK\r\nLocation: " + url + "\r\n"
                         "Connection: keep-alive\r\n\r\n");
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
        ret = true;
    }

    return ret;
}

bool Remote::setup_btn_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        if (base_url.empty()) base_url = "/";
        int pos = to_u16(match[3].str());
        printf("GET '%s' button at %d\n", base_url.c_str(), pos);

        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for GET %s\n", rqst.url().c_str());
            //  Return true as get_efile has sent response
            return true;
        }

        RemoteFile::Button newbtn(pos);
        RemoteFile::Button *button = efile_.getButton(pos);
        int nb = efile_.maxButtonPosition();
        nb = (nb + 9) / 5 * 5 + 1;
        if (pos > 0 && pos <= nb)
        {            
            if (!button)
            {
                button = &newbtn;
            }
        }
        else
        {
            printf("Invalid button %d for %s\n", pos, rqst.url().c_str());
            return false;
        }

        const char *data;
        u16_t datalen;
        if (WEB_FILES::get()->get_file("setupbtn.html", data, datalen))
        {
            std::string html(data, datalen);
            TXT::substitute(html, "<?label?>", button->label());
            TXT::substitute(html, "<?color?>", button->color());
            TXT::substitute(html, "<?redirect?>", button->redirect());
            TXT::substitute(html, "<?repeat?>", std::to_string(button->repeat()));
            TXT::substitute(html, "<?swap?>", std::to_string(button->position()));

            TXT::substitute(html, "<?btn?>", std::to_string(pos));
            while(TXT::substitute(html, "<?path?>", base_url));
            TXT::substitute(html, "<?btncount?>", std::to_string(button->actions().size()));

            std::size_t bi = html.find("<?steps?>");
            TXT::substitute(html, "<?steps?>", "");
            std::string action;
            int row = 0;
            for (auto it = button->actions().cbegin(); it != button->actions().cend(); ++it, ++row)
            {
                action = "<tr>";
                action += "<td><input type=\"text\" name=\"typ\" value=\"" + std::string(it->type()) + "\" /></td>";
                action += "<td><input type=\"number\" name=\"add\" value=\"" + std::to_string(it->address()) + "\" /></td>";
                action += "<td><input type=\"number\" name=\"val\" value=\"" + std::to_string(it->value()) + "\" /></td>";
                action += "<td><input type=\"number\" name=\"dly\" value=\"" + std::to_string(it->delay()) + "\" /></td>";
                action += "<td><button type=\"submit\" name=\"add_row\" value=\"" + std::to_string(row) + "\">+</button></td>";
                action += "<td><button type=\"button\" onclick=\"load_ir(" + std::to_string(row) + ");\">&lt;-IR</button></td>";
                action += "</tr>";
                html.insert(bi, action);
                bi += action.length();
            }

            HTTPRequest::setHTMLLengthHeader(html);
            web->send_data(client, html.c_str(), html.length());
            close = false;
            ret = true;
        }
    }
    return ret;
}

bool Remote::setup_btn_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    //rqst.printPostData();
    bool ret = false;
    std::string url = rqst.root();
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        int pos = to_u16(match[3].str());
        printf("POST '%s' button at %d\n", base_url.c_str(), pos);

        ret = get_efile(base_url, web, client, rqst, close);
        if (!ret)
        {
            printf("Error loading action file for POST %s\n", rqst.url().c_str());
            //  Return true as get_efile has sent response
            return true;
        }

        const char *value = rqst.postValue("lbl");
        if (!value) value = "";
        RemoteFile::Button *button = efile_.getButton(pos);
        if (button)
        {
            if (*value == 0)
            {
                if (efile_.deleteButton(pos))
                {
                    button = nullptr;
                }
            }
        }
        else
        {
            if (*value != 0)
            {
                button = efile_.addButton(pos, value, "", "", 0);
            }
        }
        if (button)
        {
            if (value) button->setLabel(value);

            value = rqst.postValue("bck");
            if (value) button->setColor(value);

            value = rqst.postValue("red");
            if (value) button->setRedirect(value);

            value = rqst.postValue("repeat");
            if (value) button->setRepeat(to_u16(value));

            value = rqst.postValue("swap");
            if (value)
            {
                int newpos = to_u16(value);
                if (newpos != pos && newpos > 0 && newpos <= 100)
                {
                    efile_.changePosition(button, newpos);
                    pos = newpos;
                    url = base_url + "/setup/" + std::to_string(pos);
                }
            }

            std::vector<const char *> typ;
            std::vector<const char *> add;
            std::vector<const char *> val;
            std::vector<const char *> dly;
            rqst.postArray("typ", typ);
            rqst.postArray("add", add);
            rqst.postArray("val", val);
            rqst.postArray("dly", dly);

            int nn = typ.size();
            if (add.size() == nn && val.size() == nn && dly.size() == nn)
            {
                int actno = 0;
                for (int seqno = 0; seqno < nn; seqno++)
                {
                    const char *type = typ.at(seqno);
                    int address = to_u16(add.at(seqno));
                    int value = to_u16(val.at(seqno));
                    int delay = to_u16(dly.at(seqno));

                    if (!type || *type == 0)
                    {
                        button->deleteAction(actno);
                    }
                    else
                    {
                        if (actno < button->actions().size())
                        {
                            RemoteFile::Button::Action *action = button->action(actno);
                            action->setType(type);
                            action->setAddress(address);
                            action->setValue(value);
                            action->setDelay(delay);
                        }
                        else
                        {
                            button->addAction(type, address, value, delay);
                        }
                        actno += 1;
                    }
                }

                while (button->actions().size() > nn)
                {
                    button->deleteAction(nn);
                }
            }
            else
            {
                printf("POST array sizes do not match\n");
            }

            const char *add_row = rqst.postValue("add_row");
            if (add_row)
            {
                int before = to_u16(add_row);
                if (before >= 0 && before < button->actions().size())
                {
                    button->insertAction(before);
                }
            }
        }

        const char *btn = rqst.postValue("button");
        if (btn && strcmp(btn, "done") == 0)
        {
            url = base_url + "/setup";
        }

        std::string resp("HTTP/1.1 303 OK\r\nLocation: " + url + "\r\n"
                         "Connection: keep-alive\r\n\r\n");
        web->send_data(client, resp.c_str(), resp.length());
        close = false;
        ret = true;
    }
    return ret;
}

bool Remote::setup_ir_get(WEB *web, ClientHandle client, const JSONMap &msgmap)
{
    bool ret = false;
    printf("ir_get = %d, path = %s\n",
        msgmap.intValue("ir_get"), msgmap.strValue("path"));

    int row = msgmap.intValue("ir_get");
    std::string url = msgmap.strValue("path");
    std::smatch match;
    static std::regex reg("^(.*)/setup(|\\.html)/([0-9]+)$", std::regex_constants::extended);
    if (std::regex_match(url, match, reg))
    {
        std::string base_url = match[1].str();
        int pos = to_u16(match[3].str().c_str());
        printf("IR_Get '%s' button %d row %d\n", base_url.c_str(), pos, row);

        ret = get_efile(base_url);
        RemoteFile::Button *btn = efile_.getButton(pos);
        if (ret)
        {
            Command *cmd = new Command(web, client, msgmap, btn);
            queue_command(cmd);
        }
    }
    return ret;
}

bool Remote::prompt_get(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string editurl = rqst.query("editurl");
    std::string rqsturl = rqst.query("rqsturl");
    if (editurl.empty() || rqsturl.empty())
    {
        web->send_data(client, "HTTP/1.1 400 Bad request\r\n\r\n", 28);
        close = true;
        return true;
    }

    const char *data;
    u16_t datalen;
    if (WEB_FILES::get()->get_file("editprompt.html", data, datalen))
    {
        std::string html(data, datalen);
        TXT::substitute(html, "<?editurl?>", editurl);
        TXT::substitute(html, "<?rqsturl?>", rqsturl);
        HTTPRequest::setHTMLLengthHeader(html);
        web->send_data(client, html.c_str(), html.length());
        close = false;
        ret = true;
    }
    return ret;
}

bool Remote::prompt_post(WEB *web, ClientHandle client, const HTTPRequest &rqst, bool &close)
{
    //rqst.printPostData();
    const char *editurl = rqst.postValue("editurl");
    const char *rqsturl = rqst.postValue("rqsturl");
    const char *choice = rqst.postValue("choice");
    if (!editurl || !rqsturl || !choice)
    {
        web->send_data(client, "HTTP/1.1 400 Bad request\r\n\r\n", 28);
        close = true;
        return true;
    }

    std::string url(rqsturl);
    if (strcmp(choice, "yes") == 0)
    {
        url += "/setup";
        efile_.clear();
    }
    std::string resp("HTTP/1.1 303 OK\r\nLocation: " + url + "\r\n"
                        "Connection: keep-alive\r\n\r\n");
    web->send_data(client, resp.c_str(), resp.length());
    close = false;
    return true;
}
