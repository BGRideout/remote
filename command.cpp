//                  ***** Command class implementation  *****

#include "command.h"
#include "irdevice.h"
#include <stdio.h>

int Command::count_ = 0;

Command::Command(WEB *web, ClientHandle client, const JSONMap &msgmap, const RemoteFile::Button *button)
    :web_(web), client_(client), button_(0), duration_(0.0), repeat_(0), row_(0)
{
    url_ = msgmap.strValue("path", "");
    const char *func = msgmap.strValue("func");
    if (func && strcmp(func, "btnVal") == 0)
    {
        action_ = msgmap.strValue("action", "");
        duration_ = msgmap.realValue("duration");

        if (button)
        {
            button_ = button->position();
            redirect_ = button->redirect();
            repeat_ = button->repeat();
            for (auto it = button->actions().cbegin(); it != button->actions().cend(); ++it)
            {
                steps_.emplace_back(*it);
            }
        }
    }
    else if (func && strcmp(func, "test_send") == 0)
    {
        action_ = "test_send";
        const char *type = msgmap.strValue("type");
        if (type && IR_Device::validProtocol(type))
        {
            steps_.emplace_back(type, msgmap.intValue("address"), msgmap.intValue("value"), 0);
        }
    }
    else if (func && strcmp(func, "ir_get") == 0)
    {
        action_ = "ir_get";
        row_ = msgmap.intValue("ir_get");
    }

    ++count_;
    //printf("Command count: %d (new)  %p\n", count_, this);
}

Command::Command(const Command &other)
{
    web_ = other.web_;
    client_ = other.client_;
    button_ = other.button_;
    action_ = other.action_;
    url_ = other.url_;
    duration_ = other.duration_;
    redirect_ = other.redirect_;
    repeat_ = other.repeat_;
    steps_ = other.steps_;
    row_ = other.row_;
    reply_ = other.reply_;
    ++count_;
    //printf("Command count: %d (copy) %p\n", count_, this);
}

Command::~Command()
{
    --count_;
    //printf("Command count: %d (des)  %p\n", count_, this);
}

void Command::setStep(const std::string &type, uint16_t address, uint16_t value)
{
    steps_.clear();
    steps_.emplace_back(type, address, value, 0);
}

void Command::setReply(const std::string &action)
{
    JSONMap::JMAP jmap;
    jmap["button"] = std::to_string(button_);
    jmap["action"] = action;
    jmap["url"] = url_;

    if (action_ == "ir_get")
    {
        jmap["func"] = "ir_resp";
        jmap["ir_resp"] = std::to_string(row_);
        if (steps_.size() == 1)
        {
            jmap["type"] = steps_.front().type();
            jmap["address"] = std::to_string(steps_.front().address());
            jmap["value"] = std::to_string(steps_.front().value());
            jmap["delay"] = std::to_string(steps_.front().delay());
        }
    }
    else if (action_ == "test_send")
    {
        jmap["func"] = "send_resp";
    }
    else
    {
        jmap["func"] = "btn_resp";
        jmap["redirect"] = make_redirect(url_, redirect_);
    }

    JSONMap::fromMap(jmap, reply_);
}

std::string Command::make_redirect(const std::string &base, const std::string &redirect)
{
    std::string ret;
    if (!redirect.empty())
    {
        if (redirect.at(0) != '/' && redirect.substr(0, 7) != "http://" && redirect.substr(0,8) != "https://")
        {
            if (redirect != "..")
            {
                if (base == "/")
                {
                    ret = base + redirect;
                }
                else
                {
                    ret = base + "/" + redirect;
                }
            }
            else
            {
                ret = base;
                std::size_t i1 = ret.rfind('/');
                if (i1 != std::string::npos)
                {
                    ret.erase(i1);
                    if (ret.empty())
                    {
                        ret = "/";
                    }
                }
            }
        }
        else
        {
            ret = redirect;
        }
    }
    return ret;
}