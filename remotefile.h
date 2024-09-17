//                  *****  RemoteFile *****

#ifndef REMOTFILE_H
#define REMOTFILE_H

#include "jsonstring.h"
#include <vector>
#include <tiny-json.h>
#include <ostream>

class RemoteFile
{
public:
    class Button
    {
        friend class RemoteFile;

    public:
        class Action
        {
        private:
            JSONString      type_;              // Action type (IR protocol)
            int             address_;           // Address
            int             value_;             // Value
            int             delay_;             // Post action delay (msec)

        public:
            Action() : address_(0), value_(0), delay_(0) {}
            Action(const char *type, int address, int value, int delay) : type_(type), address_(address), value_(value), delay_(delay) {}

            const char *type() const { return type_.str(); }
            void setType(const char *type) { type_ = type; }

            int address() const { return address_; }
            void setAddress(int address) { address_ = address; }

            int value() const { return value_; }
            void setValue(int value) { value_ = value; }
            
            int delay() const { return delay_; }
            void setDelay(int delay) { delay_ = delay; }

            bool loadFromJSON(const json_t *json);
            void outputJSON(std::ostream &strm) const;

            void clear();
        };

        typedef std::vector<Action> ActionList;

    private:
        JSONString          label_;             // Button label
        JSONString          color_;             // Button color string
        JSONString          redirect_;          // Redirect string
        int                 repeat_;            // Repeat interval (msec)
        int                 position_;          // Position index
        ActionList          actions_;           // Actions to be performed

        void setPosition(int position) { position_ = position; }

    public:
        Button() : repeat_(0), position_(0) {}
        Button(int position) : repeat_(0), position_(position) {}
        Button(int position, const char *label, const char *color, const char *redirect, int repeat)
            : label_(label), color_(color), redirect_(redirect), repeat_(repeat), position_(position) {}

        const char *label() const { return label_.str(); }
        void setLabel(const char *label) { label_ = label; }

        const char *color() const { return color_.str(); }
        void setColor(const char *color) { color_ = color; }

        const char *redirect() const { return redirect_.str(); }
        void setRedirect(const char *redirect) { redirect_ = redirect; }
            
        int repeat() const { return repeat_; }
        void setRepeat(int repeat) { repeat_ = repeat; }
            
        int position() const { return position_; }

        const ActionList &actions() const { return actions_; }

        bool loadFromJSON(const json_t *json);
        void outputJSON(std::ostream &strm) const;

        /**
         * @brief   Clear all data except for position
         */
        void clear();

        /**
         * @brief   Clear all actions
         */
        void clearActions();

        /**
         * @brief   Add an action to the end of the action list
         * 
         * @param   type    IR protocol or other type string
         * @param   address Address
         * @param   value   Value
         * @param   delay   Post command delay in msec
         */
        void addAction(const char *type, int address, int value, int delay);
    };

    typedef std::vector<Button> ButtonList;

private:
    JSONString              filename_;          // Loaded file name
    JSONString              title_;             // Page title
    ButtonList              buttons_;           // Page buttons
    char                    *data_;             // File data
    size_t                  datasize_;          // Data block size

    bool load();
    bool loadJSON(const json_t *json);

public:
    RemoteFile() : data_(nullptr), datasize_(0) {}
    ~RemoteFile() { clear(); }

    const char *filename() const { return filename_.str(); }

    const char *title() const { return title_.str(); }
    void setTitle(const char *title) { title_ = title; }

    /**
     * @brief   Access the map of buttons
     * 
     * @return  Reference to the list of buttons
     */
    const ButtonList &buttons() const { return buttons_; }

    /**
     * @brief   Get pointer to button at specified position
     * 
     * @param   position    Position identifier of button
     * 
     * @return  Pointer to Button or null if not defined
     */
    Button *getButton(int position);

    /**
     * @brief   Add or update a button
     * 
     * @param   position    Position identifier of button
     * @param   label       Label string (cannot be blank)
     * @param   color       Color string
     * @param   redirect    Redirect string
     * @param   repeat      Repeat interval (msec)
     * 
     * @return  Pointer to button if added or updated, null if invalid position or label
     */
    Button *addButton(int position, const char *label, const char *color, const char *redirect, int repeat);

    /**
     * @brief   Remove a button
     * 
     * @param   position    Position identifier of button
     * 
     * @return  true if button deleted
     */
    bool deleteButton(int position);

    /**
     * @brief   Change a button's position.
     * 
     * @details If a button already has the new position, it is moved to the position
     *          of the specified button.
     * 
     * @param   button      Button to be moved
     * @param   newpos      New position for the button
     * 
     * @return  true if move as successful
     */
    bool changePosition(Button *button, int newpos);

    void clear();
    bool loadForURL(const std::string &url);
    bool loadFile(const char *filename);
    bool loadString(const std::string &data, const char *filename);
    bool loadJSON(const json_t *json, const char *filename);
    void outputJSON(std::ostream &strm) const;

    /**
     * @brief   Enumerate the action files
     * 
     * @details Action files start with "action" or "menu_" and end with ".json"
     * 
     * @param   files       Vector to receive file names
     * 
     * @return  Count of files found
     */
    static int actionFiles(std::vector<std::string> &files);

    /**
     * @brief   Convert URL path to action file
     * 
     * @param   path        Path portion of URL
     * 
     * @return  action file name
     */
    static std::string urlToAction(const std::string &path);

    /**
     * @brief   Convert action file to URL
     * 
     * @param   file        File name
     * 
     * @return  URL path string (blank if invalid file name)
     */
    static std::string actionToURL(const std::string &file);
};

#endif
