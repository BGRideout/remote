//                  ***** Command class implementation  *****

#include "command.h"
#include <stdio.h>
#include <pico/stdlib.h>

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
}

void Command::execute()
{

    if (action_ == "click")
    {
    }
    else if (action_ == "press")
    {
        if (true)
        {
            ;
        }
        else
        {
            ;
        }
    }
    else if (action_ == "release" || action_ == "cancel")
    {
        ;
    }

    for (auto it = steps_.cbegin(); it != steps_.cend(); ++it)
    {
        printf("Step: '%s' %d %d %d\n", it->type().c_str(), it->address(), it->value(), it->delay());
        sleep_ms(50 + it->delay());
    }

    JSONMap::JMAP jmap;
    jmap["button"] = std::to_string(button_);
    jmap["action"] = action_;
    jmap["url"] = url_;

    std::string red(redirect_);
    if (red.length() > 0 && red.at(0) != '/')
    {
        red.insert(0, "/");
    }
    jmap["redirect"] = red;

    JSONMap::fromMap(jmap, reply_);
    printf("response: %s\n", reply_.c_str());
}