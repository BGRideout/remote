//                  *****  RemoteFile Implementation  *****

#include "remotefile.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>


void RemoteFile::clear()
{
    filename_.clear();
    title_.clear();
    buttons_.clear();
    if (data_) delete [] data_;
    data_ = nullptr;
}

bool RemoteFile::loadFile(const char *filename)
{
    bool ret = false;
    clear();

    FILE *f = fopen(filename, "r");
    if (f)
    {
        filename_ = filename;
        struct stat sb;
        stat(filename, &sb);
        data_ = new char[sb.st_size + 1];
        fread(data_, sb.st_size, 1, f);
        data_[sb.st_size] = 0;
        fclose(f);

        json_t jbuf[sb.st_size / 4];
        json_t const* json = json_create(data_, jbuf, sizeof jbuf / sizeof *jbuf );
        if (json)
        {
            ret = true;
            const json_t *t = json_getProperty(json, "title");
            title_ = t;

            json_t const *buttons = json_getProperty(json, "buttons");
            if (buttons && json_getType(buttons) == JSON_ARRAY)
            {
                int nb = 0;
                for (json_t const *button = json_getChild(buttons); button != nullptr; button = json_getSibling(button))
                {
                    nb++;
                }
                buttons_.reserve(nb);
                for (json_t const *button = json_getChild(buttons); ret && button != nullptr; button = json_getSibling(button))
                {
                    json_t const *pos = json_getProperty(button, "pos");
                    if (pos)
                    {
                        int position = json_getInteger(pos);
                        if (position > 0 && !getButton(position))
                        {
                            auto i1 = std::lower_bound(buttons_.begin(), buttons_.end(), Button(position));
                            auto i2 = buttons_.emplace(i1, Button());
                            ret = i2->loadFromJSON(button);
                        }
                    }
                }
            }
        }
    }
    return ret;
}

void RemoteFile::outputJSON(std::ostream &strm) const
{
    strm << "{\"title\":\"" << title() << "\",\n"
         << "\"buttons:\":[";
    std::string sep("\n");
    for (auto it = buttons_.cbegin(); it != buttons_.cend(); ++it)
    {
        strm << sep;
        sep = ",\n\n";
        it->outputJSON(strm);
    }
    strm << "\n]}\n";
}

RemoteFile::Button *RemoteFile::getButton(int position)
{
    Button *ret = nullptr;
    auto it = std::find(buttons_.begin(), buttons_.end(), Button(position));
    if (it != buttons_.end())
    {
        ret = &(*it);
    }
    return ret;
}

RemoteFile::Button *RemoteFile::addButton(int position, const char *label, const char *color, const char *redirect, int repeat)
{
    Button *btn = nullptr;
    if (position > 0 && strlen(label) > 0)
    {
        btn = getButton(position);
        if (!btn)
        {
            if (buttons_.size() == buttons_.capacity())
            {
                buttons_.reserve(buttons_.size() + 16);
            }
            auto i1 = std::lower_bound(buttons_.begin(), buttons_.end(), Button(position));
            auto i2 = buttons_.emplace(i1, position, label, color, redirect, repeat);
            btn = &(*i2);
        }
        else
        {
            btn->setLabel(label);
            btn->setColor(color);
            btn->setRedirect(redirect);
            btn->setRepeat(repeat);
        }
    }
    return btn;
}

bool RemoteFile::deleteButton(int position)
{
    bool ret = false;
    auto it = std::find(buttons_.begin(), buttons_.end(), Button(position));
    if (it != buttons_.end())
    {
        buttons_.erase(it);
        ret = true;
    }
    return ret;
}

bool RemoteFile::changePosition(Button *button, int newpos)
{
    bool ret = false;
    if (button && newpos > 0)
    {
        Button *other = getButton(newpos);
        if (other)
        {
            other->setPosition(button->position());
        }
        button->setPosition(newpos);
        std::sort(buttons_.begin(), buttons_.end());
    }

    return ret;
}



//                  *****  RemoteFile::Button  *****

bool RemoteFile::Button::loadFromJSON(const json_t *json)
{
    bool ret = true;
    json_t const *prop = json_getProperty(json, "pos");
    if (prop)
    {
        int pos = json_getInteger(prop);
        if (pos > 0 && pos <= 100)
        {
            position_ = pos;
        }
        else
        {
            position_ = 0;;
            label_.clear();
            ret = false;
        }
    }

    if (ret)
    {
        prop = json_getProperty(json, "lbl");
        if (prop)
        {
            label_ = prop;
        }
        else
        {
            position_ = 0;;
            label_.clear();
            ret = false;
        }
    }

    if (ret)
    {
        color_ = json_getProperty(json, "bck");
        redirect_ = json_getProperty(json, "red");
        prop = json_getProperty(json, "rpt");
        if (prop)
        {
            repeat_ = json_getInteger(prop);
        }
        else
        {
            repeat_ = 0;
        }
    }

    actions_.clear();
    if (ret)
    {
        prop = json_getProperty(json, "action");
        if (prop)
        {
            int na = 0;
            for (json_t const *aprop = json_getChild(prop); ret && aprop != nullptr; aprop = json_getSibling(aprop))
            {
                na++;
            }
            actions_.reserve(na);
            for (json_t const *aprop = json_getChild(prop); ret && aprop != nullptr; aprop = json_getSibling(aprop))
            {
                actions_.emplace_back();
                ret = actions_.back().loadFromJSON(aprop);
            }
            if (!ret)
            {
                actions_.clear();
            }
        }
    }

    return ret;
}
void RemoteFile::Button::outputJSON(std::ostream &strm) const
{
    strm << "{\"pos\":" << position() << ","
         << "\"lbl\":\"" << label() << "\","
         << "\"bck\":\"" << color() << "\","
         << "\"red\":\"" << redirect() << "\","
         << "\"rpt\":" << repeat() << ",\n"
         << "\"action\":[";

    std::string sep("\n");
    for (auto it = actions_.cbegin(); it != actions_.cend(); ++it)
    {
        strm << sep;
        sep = ",\n";
        it->outputJSON(strm);
    }
    strm << "]}";
}

void RemoteFile::Button::clear()
{
    label_.clear();
    color_.clear();
    redirect_.clear();
    repeat_ = 0;
    actions_.clear();
}

void RemoteFile::Button::clearActions()
{
    actions_.clear();
}

void RemoteFile::Button::addAction(const char *type, int address, int value, int delay)
{
    if (actions_.size() == actions_.capacity())
    {
        actions_.reserve(actions_.size() + 16);
    }
    actions_.emplace_back(type, address, value, delay);
}

bool operator < (const RemoteFile::Button &lhs, const RemoteFile::Button &rhs) {return lhs.position() < rhs.position(); }
bool operator == (const RemoteFile::Button &lhs, const RemoteFile::Button &rhs) {return lhs.position() == rhs.position(); }


//                  *****  RemoteFile::Button::Action  *****

bool RemoteFile::Button::Action::loadFromJSON(const json_t *json)
{
    bool ret = true;
    json_t const *prop = json_getProperty(json, "typ");
    if (prop)
    {
        type_ = prop;
    }
    else
    {
        type_.clear();
        ret = false;
    }

    if (ret)
    {
        prop = json_getProperty(json, "add");
        if (prop)
        {
            address_ = json_getInteger(prop);
        }
        else
        {
            address_ = 0;
        }

        prop = json_getProperty(json, "val");
        if (prop)
        {
            value_ = json_getInteger(prop);
        }
        else
        {
            value_ = 0;
        }

        prop = json_getProperty(json, "dly");
        if (prop)
        {
            delay_ = json_getInteger(prop);
        }
        else
        {
            delay_ = 0;
        }
    }

    return ret;
}

void RemoteFile::Button::Action::outputJSON(std::ostream &strm) const
{
    strm << "{\"typ\":\"" << type() << "\""
         << ",\"add\":" << address()
         << ",\"val\":" << value()
         << ",\"dly\":" << delay() << "}";
}

void RemoteFile::Button::Action::clear()
{
    type_.clear();
    address_ = 0;
    value_ = 0;
    delay_ = 0;
}
