//                  *****  Menu class implementation  *****

#include "menu.h"
#include "txt.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

std::map<std::string, Menu *> Menu::menus_;

bool Menu::loadMenu(const char *name)
{
    std::string filename("menu_");
    filename += name;
    filename += ".json";
    return loadFile(filename.c_str());
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
    json_t jbuf[datasize_ / 4];
    json_t const* json = json_create(data_, jbuf, sizeof jbuf / sizeof *jbuf );
    if (json)
    {
        ret = loadJSON(json);
    }
    return ret;
}

bool Menu::loadJSON(const json_t *json)
{
    bool ret = true;
    const json_t *prop = json_getProperty(json, "name");
    if (prop)
    {
        name_ = json_getValue(prop);
    }
    else
    {
        std::size_t i1 = filename_.rfind('_');
        if (i1 != std::string::npos)
        {
            i1 += 1;
            std::size_t i2 = filename_.find('.', i1);
            name_ = filename_.substr(i1, i2 - i1);
        }
        else
        {
            ret = false;
        }
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
    strm << "{\n\"file\": \"menu_" << name_ << ".json\",\n"
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

bool Menu::getSteps(const Command::Step &step, std::deque<Command::Step> &steps)
{
    bool ret = false;
    steps.clear();
    std::size_t i1 = step.type().find('.');
    std::string name = step.type().substr(0, i1);
    if (name == name_)
    {
        std::string func("set");
        std::string opt;
        if (i1 != std::string::npos && i1 < step.type().length() - 1)
        {
            func = step.type().substr(i1 + 1);
            std::size_t i2 = func.find('(');
            if (i2 != std::string::npos && i2 < step.type().length() - 2)
            {
                std::size_t i3 = func.find(')', i2);
                if (i3 != std::string::npos)
                {
                    opt = func.substr(i2 + 1, i3 - i2 - 2);
                    func = func.erase(i1);
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
    std::size_t i1 = step.type().find('.');
    std::string name = step.type().substr(0, i1);
    auto it = menus_.find(name);
    if (it == menus_.end())
    {
        std::pair<std::map<std::string, Menu *>::iterator, bool> ins = menus_.emplace(name, new Menu());
        it = ins.first;
        Menu *menu = it->second;
        if (!menu->loadMenu(name.c_str()))
        {
            delete menu;
            menus_[name] = nullptr;
            printf("'%s' is not a menu\n", name.c_str());
        }
    }

    Menu *menu = it->second;
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
            rowcount = colrow_.at(curcol_);
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
