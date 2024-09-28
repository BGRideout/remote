//                  ***** Command class implementation  *****

#include "command.h"
#include <stdio.h>

int Command::count_ = 0;

Command::Command(WEB *web, void *client, const JSONMap &msgmap, const RemoteFile::Button *button)
    :web_(web), client_(client), duration_(0.0), repeat_(0), row_(0)
{
    button_ = button->position();
    url_ = msgmap.strValue("path", "");
    if (msgmap.hasProperty("btnVal"))
    {
        action_ = msgmap.strValue("action", "");
        duration_ = msgmap.realValue("duration");

        redirect_ = button->redirect();
        repeat_ = button->repeat();
        for (auto it = button->actions().cbegin(); it != button->actions().cend(); ++it)
        {
            steps_.emplace_back(*it);
        }
    }
    else if (msgmap.hasProperty("ir_get"))
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

    if (action_ != "ir_get")
    {
        std::string red(redirect_);
        if (red.length() > 0 && red.at(0) != '/')
        {
            red.insert(0, "/");
        }
        jmap["redirect"] = red;
    }
    else
    {
        jmap["ir_resp"] = std::to_string(row_);
        if (steps_.size() == 1)
        {
            jmap["type"] = steps_.front().type();
            jmap["address"] = std::to_string(steps_.front().address());
            jmap["value"] = std::to_string(steps_.front().value());
            jmap["delay"] = std::to_string(steps_.front().delay());
        }
    }

    JSONMap::fromMap(jmap, reply_);
}