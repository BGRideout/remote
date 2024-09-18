//                  *****  Command class  *****

#ifndef COMMAND_H
#define COMMAND_H

#include "remotefile.h"
#include "jsonmap.h"
#include <string>
#include <vector>

class WEB;

class Command
{
private:
    class Step
    {
    private:
        std::string     type_;              // Command type
        int             address_;           // Address
        int             value_;             // Value
        int             delay_;             // Post action delay (msec)

    public:
        Step() : address_(0), value_(0), delay_(0) {}
        Step(const RemoteFile::Button::Action &act)
         : type_(act.type()), address_(act.address()), value_(act.value()), delay_(act.delay()) {}

        const std::string &type() const { return type_; }
        int address() const { return address_; }
        int value() const { return value_; }
        int delay() const { return delay_; }
    };

    WEB                 *web_;              // Pointer to web object
    void                *client_;           // Pointer to web client

    int                 button_;            // Button position
    std::string         action_;            // Command action
    std::string         url_;               // Original URL
    double              duration_;          // Button hold duration

    std::string         redirect_;          // Redirection after command
    int                 repeat_;            // Repeat interval
    std::vector<Step>   steps_;             // Command steps

    std::string         reply_;             // Reply string

    Command();
    Command(const Command &other);

public:
    Command(WEB *web, void *client, const JSONMap &msgmap, const RemoteFile::Button *button);
    ~Command() {}

    void execute();

    WEB *web() const { return web_; }
    void *client() const { return client_; }
    const std::string &reply() const { return reply_; }
};

#endif
