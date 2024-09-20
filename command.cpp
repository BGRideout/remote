//                  ***** Command class implementation  *****

#include "command.h"
#include <stdio.h>

int Command::count_ = 0;

Command::Command(WEB *web, void *client, const JSONMap &msgmap, const RemoteFile::Button *button)
    :web_(web), client_(client)
{
    button_ = msgmap.intValue("btnVal");
    action_ = msgmap.strValue("action", "");
    url_ = msgmap.strValue("path");
    duration_ = msgmap.realValue("duration");

    redirect_ = button->redirect();
    repeat_ = button->repeat();
    for (auto it = button->actions().cbegin(); it != button->actions().cend(); ++it)
    {
        steps_.emplace_back(*it);
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
    reply_ = other.reply_;
    ++count_;
    //printf("Command count: %d (copy) %p\n", count_, this);
}

Command::~Command()
{
    --count_;
    //printf("Command count: %d (des)  %p\n", count_, this);
}

void Command::setReply(const std::string &action)
{
    JSONMap::JMAP jmap;
    jmap["button"] = std::to_string(button_);
    jmap["action"] = action;
    jmap["url"] = url_;

    std::string red(redirect_);
    if (red.length() > 0 && red.at(0) != '/')
    {
        red.insert(0, "/");
    }
    jmap["redirect"] = red;

    JSONMap::fromMap(jmap, reply_);
}