//                  *****  RemoteFile Implementation  *****

#include "remotefile.h"
#include "txt.h"
#include <algorithm>
#include <dirent.h>
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
    datasize_ = 0;
    modified_ = false;
}

bool RemoteFile::loadForURL(const std::string &url)
{
    std::string actfile = urlToAction(url);
    bool ret = actfile == filename_.str();
    if (!ret)
    {
        ret = loadFile(actfile.c_str());
    }
    return ret;
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
        datasize_ = sb.st_size;
        data_ = new char[datasize_ + 1];
        fread(data_, datasize_, 1, f);
        data_[datasize_] = 0;
        fclose(f);
        ret = load();
    }
    return ret;
}

bool RemoteFile::loadString(const std::string &data, const char *filename)
{
    clear();
    filename_ = filename;
    datasize_ = data.length();
    data_ = new char[datasize_ + 1];
    memcpy(data_, data.c_str(), datasize_ + 1);
    return load();
}

bool RemoteFile::loadJSON(const json_t *json, const char *filename)
{
    clear();
    filename_ = filename;
    return loadJSON(json);
}

bool RemoteFile::load()
{
    bool ret = false;
    json_t jbuf[datasize_ / 4];
    json_t const* json = json_create(data_, jbuf, sizeof jbuf / sizeof *jbuf );
    if (json)
    {
        ret = loadJSON(json);
    }
    return ret;
}

bool RemoteFile::loadJSON(const json_t *json)
{
    bool ret = true;
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
    return ret;
}

void RemoteFile::outputJSON(std::ostream &strm) const
{
    strm << "{\"title\":\"" << title() << "\",\n"
         << "\"buttons\":[";
    std::string sep("\n");
    for (auto it = buttons_.cbegin(); it != buttons_.cend(); ++it)
    {
        strm << sep;
        sep = ",\n\n";
        it->outputJSON(strm);
    }
    strm << "\n]}\n";
}

int RemoteFile::maxButtonPosition() const
{
    int ret = 0;
    for (auto it = buttons_.cbegin(); it != buttons_.cend(); ++it)
    {
        if (it->position() > ret)
        {
            ret = it->position();
        }
    }
    return ret;
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
            btn->setModified();
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
        modified_ = true;
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

bool RemoteFile::isModified() const
{
    bool modified = modified_;
    for (auto it = buttons_.cbegin(); !modified && it != buttons_.cend(); ++it)
    {
        modified |= it->isModified();
    }
    return modified;
}

void RemoteFile::clearModified()
{
    modified_ = false;
    for (auto it = buttons_.begin(); it != buttons_.end(); ++it)
    {
        it->clearModified();
    }
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
    modified_ = false;

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
    modified_ = true;
}

void RemoteFile::Button::addAction(const char *type, int address, int value, int delay)
{
    if (actions_.size() == actions_.capacity())
    {
        actions_.reserve(actions_.size() + 16);
    }
    actions_.emplace_back(type, address, value, delay);
    modified_ = true;
}

bool RemoteFile::Button::insertAction(int pos, const char *type, int address, int value, int delay)
{
    bool ret = false;
    if (pos >= 0 && pos < actions_.size())
    {
        if (actions_.size() == actions_.capacity())
        {
            actions_.reserve(actions_.size() + 16);
        }
        actions_.emplace(actions_.begin() + pos, type, address, value, delay);
        modified_ = true;
        ret = true;
    }
    return ret;
}

bool RemoteFile::Button::deleteAction(int seqno)
{
    bool ret = false;
    if (seqno >= 0 && seqno < actions_.size())
    {
        actions_.erase(actions_.begin() + seqno);
        modified_ = true;
    }
    return ret;
}

void RemoteFile::Button::getColors(std::string &background, std::string &stroke, std::string &fill) const
{
    getColors(this, background, stroke, fill);
}

void RemoteFile::Button::getColors(const Button *button, std::string &background, std::string &stroke, std::string &fill)
{
    background = "#efefef";
    stroke = "black";
    fill = "transparent";
    if (button)
    {
        std::vector<std::string> colors;
        TXT::split(button->color(), "/", colors);
        if (colors.size() > 0 && !colors.at(0).empty())
        {
            background = colors.at(0);
        }
        if (colors.size() > 1 && !colors.at(1).empty())
        {
            stroke = colors.at(1);
        }
        if (colors.size() > 2 && !colors.at(2).empty())
        {
            fill = colors.at(2);
        }
    }
}

bool RemoteFile::Button::isModified() const
{
    bool modified = modified_;
    for (auto it = actions_.cbegin(); !modified && it != actions_.cend(); ++it)
    {
        modified |= it->isModified();
    }
    return modified;
}

void RemoteFile::Button::clearModified()
{
    modified_ = false;
    for (auto it = actions_.begin(); it != actions_.end(); ++it)
    {
        it->clearModified();
    }
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
        modified_ = false;
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
    modified_ = false;
}


//                  ***** RemoteFile Static Members  *****

int RemoteFile::actionFiles(std::vector<std::string> &files)
{
    files.clear();
    DIR *dir = opendir("/");
    if (dir)
    {
        std::string file;
        struct dirent *ent = readdir(dir);
        while (ent)
        {
            file = ent->d_name;
            if (file.length() >= 10 &&
                (file.substr(0, 7) == "actions" || file.substr(0, 5) == "menu_") &&
                file.substr(file.length() - 5) == ".json")
            {
                files.push_back(file);
            }
            ent = readdir(dir);
        }
    }
    return files.size();
}

std::string RemoteFile::urlToAction(const std::string &path)
{
    std::string ret("actions");
    std::string lpath(path);
    if (lpath.length() > 0 && lpath.at(0) == '/') lpath.erase(0, 1);
    std::size_t i1 = lpath.rfind('.');
    if (i1 != std::string::npos) lpath.erase(i1);
    bool index = lpath.empty() || lpath == "index";
    if (!index)
    {
        while(TXT::substitute(lpath, "/", "_"));
        ret += "_" + lpath;
    }
    ret += ".json";
    return ret;
}

std::string RemoteFile::actionToURL(const std::string &file)
{
    std::size_t i1 = file.find("actions");
    std::size_t i2 = file.rfind(".json");
    if (i1 != std::string::npos && i2 != std::string::npos && i1 + 7 <= i2)
    {
        i1 += 7;
        std::vector<std::string> tokens;
        TXT::split(file.substr(i1, i2 - i1), "_", tokens);
        return "/" + TXT::join(tokens, "/");
    }
    return std::string();
}
