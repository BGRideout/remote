//                  *****  Command class  *****

#ifndef COMMAND_H
#define COMMAND_H

#include "remotefile.h"
#include "jsonmap.h"
#include "web.h"
#include <string>
#include <vector>
#include <stdint.h>

class Command
{
public:
    class Step
    {
    private:
        std::string     type_;              // Command type
        uint16_t        address_;           // Address
        uint16_t        value_;             // Value
        uint16_t        delay_;             // Post action delay (msec)

    public:
        Step() : address_(0), value_(0), delay_(0) {}
        Step(const std::string &type, uint16_t address, uint16_t value, uint32_t delay)
         : type_(type), address_(address), value_(value), delay_(delay) {}
        Step(const RemoteFile::Button::Action &act)
         : type_(act.type()), address_(act.address()), value_(act.value()), delay_(act.delay()) {}

        const std::string &type() const { return type_; }
        uint16_t address() const { return address_; }
        uint16_t value() const { return value_; }
        uint16_t delay() const { return delay_; }

        void setType(const std::string &type) { type_ = type; }
        void setAddress(uint16_t address) { address_ = address; }
        void setValue(uint16_t value) { value_ = value; }
        void setDelay(uint16_t delay) { delay_ = delay; }
    };

private:
    WEB                 *web_;              // Pointer to web object
    ClientHandle        client_;            // Handle to web client

    int                 button_;            // Button position
    std::string         action_;            // Command action
    std::string         url_;               // Original URL
    double              duration_;          // Button hold duration

    std::string         redirect_;          // Redirection after command
    int                 repeat_;            // Repeat interval
    std::vector<Step>   steps_;             // Command steps

    int                 row_;               // Action row number

    std::string         reply_;             // Reply string

    static int          count_;             // Instance count
    
    Command();

public:
    Command(WEB *web, ClientHandle client, const JSONMap &msgmap, const RemoteFile::Button *button);
    Command(const Command &other);
    ~Command();

    WEB *web() const { return web_; }
    ClientHandle client() const { return client_; }
    int button() const { return button_; }
    const std::string &action() const { return action_; }
    const std::string &url() const { return url_; }
    const double duration() const { return duration_; }
    const std::string &redirect() const { return redirect_; }
    int repeat() const { return repeat_; }
    const std::vector<Step> &steps() { return steps_; }
    void setStep(const std::string &type, uint16_t address, uint16_t value);
    const std::string &reply() const { return reply_; }

    void setRepeat(int repeat) { repeat_ = repeat; }
    void setReply(const std::string &action);

    static std::string make_redirect(const std::string &base, const std::string &redirect);
};

#endif
