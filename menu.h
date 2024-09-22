//                  *****  Menu class  *****

#ifndef MENU_H
#define MENU_H

#include "command.h"
#include <map>
#include <string>
#include <vector>
#include <deque>
#include <tiny-json.h>
#include <ostream>

class Menu
{
private:
    std::string             name_;                  // Menu name
    std::map<std::string, Command::Step> commands_; // Commands (open, up, down, left, right, ok)
    std::vector<int>        rows_;                  // Rows per column
    std::vector<int>        colrow_;                // Current row by column
    int                     curcol_;                // Current column

    std::string             filename_;              // Filename
    char                    *data_;                 // File data
    size_t                  datasize_;              // Data block size

    bool load();
    bool loadJSON(const json_t *json);

    bool set_pos(const std::string &opt, int col, int row, std::deque<Command::Step> &steps);
    bool move_pos(int cols, int rows, std::deque<Command::Step> &steps);
    void add_step(const std::string &op, std::deque<Command::Step> &steps);

    static std::map<std::string, Menu *> menus_;    // Map of known menus

public:
    Menu() : curcol_(-1), data_(nullptr), datasize_(0) {}
    ~Menu() {}

    bool loadMenu(const char *name);
    bool loadFile(const char *filename);
    bool loadString(const std::string &data, const char *filename);
    bool loadJSON(const json_t *json, const char *filename);
    void outputJSON(std::ostream &strm) const;
    void clear() { name_.clear(), commands_.clear(); rows_.clear(), colrow_.clear(); }

    const std::string &name() const { return name_; }

    /**
     * @brief   Get the steps for a menu action
     * 
     * @param   step    Step containing menu call
     * @param   steps   deque to receive steps to accomplish menu action
     * 
     * @return  true if steps filled successfully
     */
    bool getSteps(const Command::Step &step, std::deque<Command::Step> &steps);
    static bool getMenuSteps(const Command::Step &step, std::deque<Command::Step> &steps);

    /**
     * @brief   Reset the position of the menu
     */
    void reset() { curcol_ = -1; colrow_.assign(rows_.size(), -1); }
};

#endif
