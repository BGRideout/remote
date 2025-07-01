//                  *****  RemoteFile *****

#ifndef REMOTFILE_H
#define REMOTFILE_H

#include "jsonstring.h"
#include <string>
#include <string.h>
#include <list>
#include <set>
#include <vector>
#include <tiny-json.h>
#include <ostream>

#define MAX_REMOTE_BUTTONS  100

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
            bool            modified_;          // Modified flag

        public:
            Action() : address_(0), value_(0), delay_(0), modified_(false) {}
            Action(const char *type, int address, int value, int delay)
             : type_(type), address_(address), value_(value), delay_(delay), modified_(false) {}

            const char *type() const { return type_.str(); }
            void setType(const char *type) { modified_ |= strcmp(type_.str(), type) != 0; type_ = type; }

            int address() const { return address_; }
            void setAddress(int address) { modified_ |= address_ != address; address_ = address; }

            int value() const { return value_; }
            void setValue(int value) { modified_ |= value_ != value; value_ = value; }
            
            int delay() const { return delay_; }
            void setDelay(int delay) { modified_ |= delay_ != delay; delay_ = delay; }

            bool loadFromJSON(const json_t *json);
            void outputJSON(std::ostream &strm) const;

            bool isModified() const { return modified_; }
            void setModified() { modified_ = true; }
            void clearModified() { modified_ = false; }

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
        bool                modified_;          // Modified flag

        void setPosition(int position) { modified_ = position_ != position; position_ = position; }

        Button(const Button &);
        Button &operator =(const Button &);

    public:
        Button() : repeat_(0), position_(0), modified_(false) {}
        Button(int position) : repeat_(0), position_(position), modified_(false) {}
        Button(int position, const char *label, const char *color, const char *redirect, int repeat)
            : label_(label), color_(color), redirect_(redirect), repeat_(repeat), position_(position), modified_(false) {}

        const char *label() const { return label_.str(); }
        void setLabel(const char *label) { modified_ |= strcmp(label_.str(), label) != 0; label_ = label; }

        const char *color() const { return color_.str(); }
        void setColor(const char *color) { modified_ |= strcmp(color_.str(), color) != 0; color_ = color; }
        void getColors(std::string &background, std::string &stroke, std::string &fill) const;
        static void getColors(const Button *button, std::string &background, std::string &stroke, std::string &fill);

        const char *redirect() const { return redirect_.str(); }
        void setRedirect(const char *redirect) { modified_ |= strcmp(redirect_.str(), redirect) != 0; redirect_ = redirect; }
            
        int repeat() const { return repeat_; }
        void setRepeat(int repeat) { modified_ |= repeat_ != repeat; repeat_ = repeat; }
            
        int position() const { return position_; }

        const ActionList &actions() const { return actions_; }
        Action *action(int seqno) { return (seqno >= 0 && seqno < actions_.size()) ? &actions_[seqno] : nullptr; }

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

        /**
         * @brief   Insert an action before the indicated position
         * 
         * @param   pos     Position in list to insert
         * @param   type    IR protocol or other type string
         * @param   address Address
         * @param   value   Value
         * @param   delay   Post command delay in msec
         * 
         * @return  true if insertd, false if bad position
         */
        bool insertAction(int pos, const char *type=nullptr, int address=0, int value=0, int delay=0);

        /**
         * @brief   Remove an action
         * 
         * @param   seqno   Sequence position of action
         * 
         * @return  true if action deleted
         */
        bool deleteAction(int seqno);

        bool isModified() const;
        void setModified() { modified_ = true; }
        void clearModified();
    };

    typedef std::list<Button> ButtonList;

private:
    JSONString              filename_;          // Loaded file name
    JSONString              title_;             // Page title
    ButtonList              buttons_;           // Page buttons
    char                    *data_;             // File data
    size_t                  datasize_;          // Data block size
    bool                    modified_;          // Modified flag

    bool load();
    bool loadJSON(const json_t *json);

    RemoteFile(const RemoteFile &);
    RemoteFile &operator =(const RemoteFile &);

public:
    RemoteFile() : data_(nullptr), datasize_(0), modified_(false) {}
    ~RemoteFile() { clear(); }

    const char *filename() const { return filename_.str(); }

    const char *title() const { return title_.str(); }
    void setTitle(const char *title) { modified_ |= strcmp(title_.str(), title) != 0; title_ = title; }

    /**
     * @brief   Access the list of buttons
     * 
     * @return  Reference to the list of buttons
     */
    const ButtonList &buttons() const { return buttons_; }

    /**
     * @brief   Get maimum button position
     * 
     * @return  Largest position value for buttons
     */
    int maxButtonPosition() const;

    /**
     * @brief   Get pointer to button at specified position
     * 
     * @param   position    Position identifier of button
     * 
     * @return  Pointer to Button or null if not defined
     */
    Button *getButton(int position);

    /**
     * @brief   Find a button with a given label
     * 
     * @param   label   Label to find (leading @ ignored)
     * 
     * @return  button position if found or -1 if not
     */
    int findButtonPosition(const char *label) const;

    /**
     * @brief   Add or update a button
     * 
     * @param   position    Position identifier of button
     * @param   label       Label string
     * @param   color       Color string
     * @param   redirect    Redirect string
     * @param   repeat      Repeat interval (msec)
     * 
     * @return  Pointer to button if added or updated, null if invalid position
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
    bool saveFile() const;

    bool isModified() const;
    void setModified() { modified_ = true; }
    void clearModified();

    /**
     * @brief   Enumerate the action files
     * 
     * @details Action files start with "action" and end with ".json"
     * 
     * @param   files       Set to receive file names (not cleared first)
     * 
     * @return  Count of files found
     */
    static int actionFiles(std::set<std::string> &files);

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
