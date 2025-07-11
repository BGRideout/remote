//                  *****  Menu class implementation  *****

#include "menu.h"
#include "txt.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sstream>

std::map<std::string, Menu *> Menu::menus_;

Menu::Menu() : curcol_(-1), data_(nullptr), datasize_(0)
{
    const char *cmds[] = {"open", "up", "down", "left", "right", "ok"};
    for (int ii = 0; ii < 6; ii++)
    {
        commands_[cmds[ii]] = Command::Step("", 0, 0, 0);
    }
}

Menu::Menu(const std::string &name) : curcol_(-1), data_(nullptr), datasize_(0)
{
    setName(name);
    const char *cmds[] = {"open", "up", "down", "left", "right", "ok"};
    for (int ii = 0; ii < 6; ii++)
    {
        commands_[cmds[ii]] = Command::Step("", 0, 0, 0);
    }
}

bool Menu::loadMenu(const char *name)
{
    return loadFile(menuFile(name).c_str());
}

bool Menu::loadFile(const char *filename)
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

bool Menu::loadString(const std::string &data, const char *filename)
{
    clear();
    filename_ = filename;
    datasize_ = data.length();
    data_ = new char[datasize_ + 1];
    memcpy(data_, data.c_str(), datasize_ + 1);
    return load();
}

bool Menu::loadJSON(const json_t *json, const char *filename)
{
    clear();
    filename_ = filename;
    return loadJSON(json);
}

bool Menu::load()
{
    bool ret = false;
    int nj = JSONMap::itemCount(data_);
    json_t jbuf[nj];
    json_t const* json = json_create(data_, jbuf, nj);
    if (json)
    {
        ret = loadJSON(json);
    }
    else
    {
        printf("Failed to load menu JSON\n");
    }
    return ret;
}

bool Menu::loadJSON(const json_t *json)
{
    bool ret = true;
    const json_t *prop = json_getProperty(json, "name");
    if (prop)
    {
        setName(json_getValue(prop));
    }
    else
    {
        std::size_t i1 = filename_.rfind('_');
        if (i1 != std::string::npos)
        {
            i1 += 1;
            std::size_t i2 = filename_.find('.', i1);
            setName(filename_.substr(i1, i2 - i1));
        }
        else
        {
            ret = false;
        }
    }

    if (ret)
    {
        ret = !name_.empty();
    }

    if (ret)
    {
        ret = false;
        prop = json_getProperty(json, "rows");
        if (prop && json_getType(prop) == JSON_ARRAY)
        {
            for (const json_t *val = json_getChild(prop); val != nullptr; val = json_getSibling(val))
            {
                rows_.push_back(json_getInteger(val));
                ret = true;
            }
        }
    }

    if (ret)
    {
        const char *cmds[] = {"open", "up", "down", "left", "right", "ok"};
        for (int ii = 0; ii < 6; ii++)
        {
            std::string type;
            uint16_t addr = 0;
            uint16_t value = 0;
            uint32_t delay = 0;

            prop = json_getProperty(json, cmds[ii]);
            if (prop && json_getType(prop) == JSON_ARRAY)
            {
                const json_t *val = json_getChild(prop);
                if (val)
                {
                    type = json_getValue(val);
                }
                if (val && (val = json_getSibling(val)))
                {
                    addr = json_getInteger(val);
                }
                if (val && (val = json_getSibling(val)))
                {
                    value = json_getInteger(val);
                }
                if (val && (val = json_getSibling(val)))
                {
                    delay = json_getInteger(val);
                }

                commands_[cmds[ii]] = Command::Step(type, addr, value, delay);
            }
        }

        ret = commands_.size() > 0;
    }

    if (ret)
    {
        reset();
    }
    else
    {
        clear();
    }

    delete [] data_;
    datasize_ = 0;
    return ret;
}

void Menu::outputJSON(std::ostream &strm) const
{
    strm << "{\n\"file\": \"" << menuFile(name_) << "\",\n"
         << "\"name\": \"" << name_ << "\",\n";

    for (auto it = commands_.cbegin(); it != commands_.cend(); ++it)
    {
        strm << "\"" << it->first << "\": [\"" << it->second.type() << "\", "
             << it->second.address() << ", " << it->second.value() << ", " << it->second.delay() << "],\n";
    }

    strm << "\"rows\": [";
    std::string sep;
    for (auto it = rows_.cbegin(); it != rows_.cend(); ++it)
    {
        strm << sep << *it;
        sep = ", ";
    }
    strm << "]\n}\n";
}

bool Menu::saveFile() const
{
    bool ret = false;
    if (name_.length() > 0)
    {
        std::string filename = menuFile(name_);
        FILE *f = fopen(filename.c_str(), "w");
        if (f)
        {
            ret = true;
            std::ostringstream out;
            outputJSON(out);
            size_t n = fwrite(out.str().c_str(), out.str().size(), 1, f);
            int sts = fclose(f);
            if (n != 1 || sts != 0)
            {
                printf("Failed to write file %s: n=%d sts=%d\n", filename.c_str(), n, sts);
                ret = false;
            }
        }
        else
        {
            printf("Failed to open '%s'. err=%d : %s\n", filename.c_str(), errno, strerror(errno));
        }
    }
    return ret;
}

Menu *Menu::getMenu(const std::string &name)
{
    Menu *ret = nullptr;
    if (!name.empty())
    {
        auto it = menus_.find(name);
        if (it != menus_.end())
        {
            ret = it->second;
        }
        else
        {
            ret = new Menu();
            if (!ret->loadMenu(name.c_str()))
            {
                delete ret;
                ret = nullptr;
                menus_.erase(name);
                printf("'%s' is not a menu\n", name.c_str());
            }
        }
    }
    return ret;
}

Menu *Menu::addMenu(const std::string &name)
{
    Menu *ret = getMenu(name);
    if (!ret)
    {
        ret = new Menu(name);
    }
    return ret;
}

bool Menu::deleteMenu(const char *name)
{
    bool ret = false;
    std::string filename = menuFile(name);
    if (unlink(filename.c_str()) == 0)
    {
        ret = true;
    }
    auto it = menus_.find(name);
    if (it != menus_.end())
    {
        delete it->second;
        menus_.erase(name);
    }
    return ret;
}

bool Menu::setName(const std::string &name)
{
    bool ret = true;
    if (name != name_ && !name.empty())
    {
        if (!name_.empty())
        {
            auto it = menus_.find(name_);
            if (it != menus_.end())
            {
                if (it->second != this)
                {
                    delete it->second;
                }
                menus_.erase(name_);
            }
        }
        name_ = name;
        menus_[name_] = this;
    }
    return ret;
}

bool Menu::rename(const std::string &name)
{
    bool ret = true;
    if (name != name_ && !name.empty())
    {
        std::string newname = name;
        std::string newfile = menuFile(name);
        struct stat sb;
        int seq = 0;
        while (stat(newfile.c_str(), &sb) == 0 && seq < 99)
        {
            ++seq;
            newname = name + "_" + std::to_string(seq);
            newfile = menuFile(newname);
        }
        if (!name_.empty())
        {
            std::string oldfile = menuFile(name_);
            ::rename(oldfile.c_str(), newfile.c_str());
        }
        setName(newname);
    }
    return ret;
}

void Menu::setIRCode(const std::string &op, const std::string &type, uint16_t address, uint16_t value, uint16_t delay)
{
    auto it = commands_.find(op);
    if (it != commands_.end())
    {
        Command::Step &step = it->second;
        step.setType(type);
        step.setAddress(address);
        step.setValue(value);
        step.setDelay(delay);
    }
}

std::string Menu::rowsPerColumn() const
{
    std::string ret;
    std::string sep;
    for (auto it = rows_.cbegin(); it != rows_.cend(); ++it)
    {
        ret += sep + std::to_string(*it);
        sep = ",";
    }
    return ret;
}

bool Menu::setRowsPerColumn(const std::string &rowspercol)
{
    bool ret = false;
    std::vector<std::string> srows;
    TXT::split(rowspercol, ",", srows);
    std::vector<int> rows;
    try
    {
        for (auto it = srows.cbegin(); it != srows.cend(); ++it)
        {
            rows.push_back(std::stoul(it->c_str()));
        }

        if (rows != rows_)
        {
            rows_ = rows;
            reset();
        }
        ret = true;
    }
    catch(const std::exception& e)
    {
        printf("Error %s converting rows per column %s\n", e.what(), rowspercol.c_str());
    }
    return ret;
}

bool Menu::getSteps(const Command::Step &step, std::deque<Command::Step> &steps)
{
    bool ret = false;
    steps.clear();
    std::size_t i1 = step.type().find_first_of(".(");
    std::string name = step.type().substr(0, i1);
    if (name == name_)
    {
        std::string func("set");
        std::string opt;
        if (i1 != std::string::npos && i1 < step.type().length() - 1)
        {
            std::size_t i2 = step.type().find('(');
            if (i1 < i2)
            {
                ++i1;
                func = step.type().substr(i1, i2 - i1);
            }
            if (i2 != std::string::npos && i2 < step.type().length() - 2)
            {
                std::size_t i3 = step.type().find(')', i2);
                if (i3 != std::string::npos)
                {
                    opt = step.type().substr(i2 + 1, i3 - i2 - 1);
                }
            }
        }

        int p1 = step.address();
        int p2 = step.value();

        if (func == "set")
        {
            ret = set_pos(opt, p1, p2, steps);
        }
        else if (func == "move")
        {
            ret = move_pos(p1, p2, steps);
        }
        else if (func == "up")
        {
            ret = move_pos(0, -1, steps);
        }
        else if (func == "down")
        {
            ret = move_pos(0, 1, steps);
        }
        else if (func == "left")
        {
            ret = move_pos(-1, 0, steps);
        }
        else if (func == "right")
        {
            ret = move_pos(1, 0, steps);
        }
        else if (func == "ok")
        {
            add_step("ok", steps);
        }
        else if (func == "clear")
        {
            reset();
            ret = true;
        }
    }
    return ret;
}

bool Menu::getMenuSteps(const Command::Step &step, std::deque<Command::Step> &steps)
{
    bool ret = false;
    std::size_t i1 = step.type().find_first_of(".(");
    std::string name = step.type().substr(0, i1);

    Menu *menu = getMenu(name);
    if (menu)
    {
        ret = menu->getSteps(step, steps);
    }

    return ret;
}

bool Menu::set_pos(const std::string &opt, int col, int row, std::deque<Command::Step> &steps)
{
    col -= 1;
    row -= 1;
    int colcount = rows_.size();
    bool ret = col >= 0 && col < colcount && row >= 0 && row < rows_.at(col);
    if (ret)
    {
        
        if (opt.find("-open") == std::string::npos)
        {
            add_step("open", steps);
        }

        if (curcol_ == -1)
        {
            int mid = colcount / 2;
            if (col < mid)
            {
                move_pos(-(colcount - 1), 0, steps);
                move_pos(col, 0, steps);
            }
            else
            {
                move_pos(colcount - 1, 0, steps);
                move_pos(-(colcount - col - 1), 0, steps);
            }
        }
        else
        {
            move_pos(col - curcol_, 0, steps);
        }

        int rowcount = rows_.at(col);
        int currow = colrow_.at(col);
        if (currow == -1)
        {
            int mid = rowcount / 2;
            if (row < mid)
            {
                move_pos(0, -(rowcount - 1), steps);
                move_pos(0, row, steps);
            }
            else
            {
                move_pos(0, rowcount - 1, steps);
                move_pos(0, -(rowcount - row - 1), steps);
            }
        }
        else
        {
            move_pos(0, row - currow, steps);
        }

        curcol_ = col;
        colrow_[col] = row;
        
        if (opt.find("-ok") == std::string::npos)
        {
            add_step("ok", steps);
        }
    }
    return ret;
}

bool Menu::move_pos(int cols, int rows, std::deque<Command::Step> &steps)
{
    int colcount = rows_.size();
    bool ret = abs(cols) < colcount;
    if (ret)
    {
        int rowcount = 0;
        if (curcol_ != -1)
        {
            rowcount = rows_.at(curcol_);
        }
        else
        {
            for (int ii = 0; ii < colcount; ii++)
            {
                int rows = rows_.at(ii);
                if (rows > rowcount)
                {
                    rowcount = rows;
                }
            }
        }
        ret = abs(rows) < rowcount;
        if (ret)
        {
            std::string op = "left";
            if (cols > 0)
            {
                op = "right";
            }
            int nn = abs(cols);
            for (int ii = 0; ii < nn; ii++)
            {
                add_step(op, steps);
            }
            if (curcol_ != -1)
            {
                curcol_ += cols;
            }

            op = "down";
            if (rows < 0)
            {
                op = "up";
            }
            nn = abs(rows);
            for (int ii = 0; ii < nn; ii++)
            {
                add_step(op, steps);
            }
            if (curcol_ != -1 && colrow_.at(curcol_) != -1)
            {
                colrow_[curcol_] += rows;
            }
        }
    }

    return ret;
}

void Menu::add_step(const std::string &op, std::deque<Command::Step> &steps)
{
    const auto it = commands_.find(op);
    if (it != commands_.cend() && !it->second.type().empty())
    {
        steps.push_back(it->second);
    }
}

int Menu::menuFiles(std::set<std::string> &files)
{
    int nf = files.size();
    DIR *dir = opendir("/");
    if (dir)
    {
        std::string file;
        struct dirent *ent = readdir(dir);
        while (ent)
        {
            file = ent->d_name;
            if (file.length() >= 10 &&
                (file.substr(0, 5) == "menu_") &&
                file.substr(file.length() - 5) == ".json")
            {
                files.insert(file);
            }
            ent = readdir(dir);
        }
	closedir(dir);
    }
    return files.size() - nf;
}

int Menu::menuNames(std::set<std::string> &names)
{
    names.clear();
    std::set<std::string> files;
    menuFiles(files);
    for (auto it = files.begin(); it != files.end(); ++it)
    {
        names.insert(it->substr(5, it->length() - 10));
    }
    return names.size();
}
