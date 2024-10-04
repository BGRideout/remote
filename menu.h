//                  *****  Menu class  *****

#ifndef MENU_H
#define MENU_H

#include "command.h"
#include <map>
#include <set>
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
    bool saveFile() const;
    void clear() { name_.clear(), commands_.clear(); rows_.clear(), colrow_.clear(); }

    const std::string &name() const { return name_; }
    void setName(const std::string &name) { name_ = name; }

    /**
     * @brief   Add a new menu
     * 
     * @param   name    Name of new menu
     * 
     * @return  Pointer to new menu or existing if name already defined
     */
    static Menu *addMenu(const std::string &name);

    /**
     * @brief   Get a menu by name
     * 
     * @param   name    Name of the menu
     * 
     * @return  Pointer to menu or null if name not valid
     */
    static Menu *getMenu(const std::string &name);

    /**
     * @brief   Get the menu's command map
     * 
     * @return  Map of step by operation (open, up, down, left, right, ok)
     */
    const std::map<std::string, Command::Step> &commands() const { return commands_; }

    /**
     * @brief   Set the IR code for an operation
     * 
     * @param   op      Operation (open, up, down, left, right, ok)
     * @param   type    IR type
     * @param   address IR address
     * @param   value   IR command value
     * @param   delay   Delay in msec following IR code send
     */
    void setIRCode(const std::string &op, const std::string &type, uint16_t address, uint16_t value, uint16_t delay);

    /**
     * @brief   Get the rows per column as a comma separated string
     * 
     * @return  comma separated string
     */
    std::string rowsPerColumn() const;

    /**
     * @brief   Set the rows per column from comma separated string
     * 
     * @param   rowspercol  String containing rows per column separated by commas
     * 
     * @return  true if data set, false if badly formatted string
     */
    bool setRowsPerColumn(const std::string &rowspercol);

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

    /**
     * @brief   Enumerate the menu files
     * 
     * @details Action files start with "menu_" and end with ".json"
     * 
     * @param   files       Set to receive file names (not cleared first)
     * 
     * @return  Count of files found
     */
    static int menuFiles(std::set<std::string> &files);

    /**
     * @brief   Enumerate the menu names
     * 
     * @param   names       Set to receive menu names
     * 
     * @return  Count of menus found
     */
    static int menuNames(std::set<std::string> &names);
};

#endif
