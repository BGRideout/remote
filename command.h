//                  *****  Command class  *****

#ifndef COMMAND_H
#define COMMAND_H

#include "remotefile.h"
#include "jsonmap.h"
#include <string>
#include <vector>
#include <stdint.h>

class WEB;

class Command
{
public:
    class Step
    {
    private:
        std::string     type_;              // Command type
        uint16_t        address_;           // Address
        uint16_t        value_;             // Value
        uint32_t        delay_;             // Post action delay (msec)

        Step() : address_(0), value_(0), delay_(0) {}

    public:
        Step(const RemoteFile::Button::Action &act)
         : type_(act.type()), address_(act.address()), value_(act.value()), delay_(act.delay()) {}

        const std::string &type() const { return type_; }
        uint16_t address() const { return address_; }
        uint16_t value() const { return value_; }
        uint32_t delay() const { return delay_; }
    };

private:
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

    static int          count_;             // Instance count

    Command();

public:
    Command(WEB *web, void *client, const JSONMap &msgmap, const RemoteFile::Button *button);
    Command(const Command &other);
    ~Command();

    WEB *web() const { return web_; }
    void *client() const { return client_; }
    int button() const { return button_; }
    const std::string &action() const { return action_; }
    const std::string &url() const { return url_; }
    const double duration() const { return duration_; }
    const std::string &redirect() const { return redirect_; }
    int repeat() const { return repeat_; }
    const std::vector<Step> &steps() { return steps_; }
    const std::string &reply() const { return reply_; }

    void setRepeat(int repeat) { repeat_ = repeat; }
    void setReply(const std::string &action);
};

#endif
