//                  *****  RemoteFile *****

#ifndef REMOTFILE_H
#define REMOTFILE_H

#include "jsonstring.h"
#include <map>
#include <vector>
#include <tiny-json.h>
#include <sstream>

class RemoteFile
{
public:
    class Button
    {
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
            //Action(const char *type, int address, int value, int delay) : type_(type), address_(address), value_(value), delay_(delay) {}

            const char *type() const { return type_.str(); }
            //void setType(const char *type) { type_ = type; }

            int address() const { return address_; }
            //void setAddress(int address) { address_ = address; }

            int value() const { return value_; }
            //void setValue(int value) { value_ = value; }
            
            int delay() const { return delay_; }
            //void setDelay(int delay) { delay_ = delay; }

            bool loadFromJSON(const json_t *json);
            void outputJSON(std::stringstream &strm) const;
        };

        typedef std::vector<Action> ActionList;

    private:
        JSONString          label_;             // Button label
        JSONString          color_;             // Button color string
        JSONString          redirect_;          // Redirect string
        int                 repeat_;            // Repeat interval (msec)
        int                 position_;          // Position index
        ActionList          actions_;           // Actions to be performed

    public:
        Button() : repeat_(0), position_(0) {}

        const char *label() const { return label_.str(); }
        //void setLabel(const char *label) { label_ = label; }

        const char *color() const { return color_.str(); }
        //void setColor(const char *color) { color_ = color; }

        const char *redirect() const { return redirect_.str(); }
        //void setRedirect(const char *redirect) { redirect_ = redirect; }
            
        int repeat() const { return repeat_; }
        //void setRepeat(int repeat) { repeat_ = repeat; }
            
        int position() const { return position_; }
        //void setPosition(int position) { position_ = position; }

        const ActionList &actions() const { return actions_; }

        bool loadFromJSON(const json_t *json);
        void outputJSON(std::stringstream &strm) const;
    };

    typedef std::map<int, Button> ButtonMap;

private:
    JSONString              filename_;          // Loaded file name
    JSONString              title_;             // Page title
    ButtonMap               buttons_;           // Page buttons
    char                    *data_;             // File data

public:
    RemoteFile() : data_(nullptr) {}
    ~RemoteFile() { clear(); }

    const char *filename() const { return filename_.str(); }
    const char *title() const { return title_.str(); }
    const ButtonMap &buttons() const { return buttons_; }

    void clear();
    bool loadFile(const char *filename);
    void outputJSON(std::stringstream &strm) const;
};

#endif
