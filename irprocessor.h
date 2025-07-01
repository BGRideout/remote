//                  *****  IR_Processor class  *****

#ifndef IR_PROCESSOR_H
#define IR_PROCESSOR_H

#include "irdevice.h"
#include "command.h"
#include <vector>
#include <deque>
#include <map>
#include <pico/async_context.h>

class Remote;
class IR_LED;

class IR_Processor
{
private:
    class RepeatWorker;

    class SendWorker
    {
    private:
        IR_Processor                *irp_;              // Pointer to this object
        IR_LED                      *ir_led_;           // IR LED
        uint32_t                    start_time_;        // Command start time
        async_context_t             *asy_ctx_;          // Async context
        async_at_time_worker_t      time_worker_;       // Timing worker
        async_when_pending_worker_t ir_complete_;       // IR output complete worker
        Command                     *cmd_;              // Active command
        RepeatWorker                *repeat_worker_;    // Repeat worker
        int                         send_step_;         // Step in send operation
        std::deque<Command::Step>   menu_steps_;        // Menu steps
        Command::Step               last_step_;         // Last command step sent
        bool                        repeated_;          // Repeated operation flag
        bool                        do_reply_;          // Send reply when action complete

        IR_Processor *irProcessor() const { return irp_; }
        bool get_transmitter(const std::string &proto);
        bool scheduleNext();
        bool getMenuSteps(const Command::Step &step);

        int sendStep() const { return send_step_; }
        int nextStep() { return menu_steps_.size() == 0 ? send_step_++ : send_step_; }
        void setStep(int step) { send_step_ = step; }
        Command::Step getStep(int stepNo, bool peek = false);
        bool repeated() const { return repeated_; }
        void setIRComplete() { async_context_set_work_pending(asy_ctx_, &ir_complete_); }

        static void time_work(async_context_t *, async_at_time_worker_t *);
        void time_work();
        static void ir_complete(async_context_t *, async_when_pending_worker_t *);
        static void set_ir_complete(IR_LED *led, void *user_data);
        bool doReply() const { return do_reply_; }

    public:
        SendWorker(IR_Processor *parent, async_context_t *async)
         : irp_(parent), ir_led_(nullptr), start_time_(0), asy_ctx_(async),
           cmd_(nullptr), repeat_worker_(nullptr), send_step_(0), repeated_(false), do_reply_(false)
        {
            time_worker_ = { .do_work = time_work, .user_data = this };
            ir_complete_ = { .do_work = ir_complete, .user_data = this };
            async_context_add_when_pending_worker(asy_ctx_, &ir_complete_);
        }

        Command *command() const { return cmd_; }
        void setCommand(Command *cmd) { cmd_ = cmd; }
        void resetCommand() { if (cmd_) delete cmd_; cmd_ = nullptr; }

        bool start();
        const uint32_t &start_time() const {return start_time_;}
        uint32_t elapsed() const {return to_ms_since_boot(get_absolute_time()) - start_time_;}

        void setRepeatWorker(RepeatWorker *worker) { repeat_worker_ = worker; }
        void setRepeated(bool repeated = true) { repeated_ = repeated; }
        void setDoReply(bool doReply = true) { do_reply_ = doReply; }

        int getTime();

        void reset() { cmd_ = nullptr; repeat_worker_ = nullptr; send_step_ = 0;
                       menu_steps_.clear(), last_step_.setType(""), repeated_ = false; do_reply_ = false; }
    };

    static SendWorker *sendWorker(async_at_time_worker_t *worker) { return static_cast<SendWorker *>(worker->user_data); }
    static SendWorker *sendWorker(async_when_pending_worker_t *worker) { return static_cast<SendWorker *>(worker->user_data); }
    static SendWorker *sendWorker(void *user_data) { return static_cast<SendWorker *>(user_data); }

    class RepeatWorker
    {
    private:
        IR_Processor                *irp_;              // Pointer to this object
        async_context_t             *asy_ctx_;          // Async context
        SendWorker                  *send_;             // Send worker
        int                         count_;             // Limit counter / activity flag
        const int                   repeat_limit_ = 500;// Repeat limit (msec)
        absolute_time_t             repeat_until_;      // Repeat expiry time

        IR_Processor *irProcessor() const { return irp_; }

        void setActive() { count_ = 1; }
        bool reachedLimit()
            { return isActive() ? count_++ > 100 || absolute_time_diff_us(get_absolute_time(), repeat_until_) < 0 : false; }
        bool isIdle() const { return count_ == 0; }
        void finish();

        void repeat();
        void ir_complete();

    public:
        RepeatWorker(IR_Processor *parent, async_context_t *async, SendWorker *sendWorker)
         : irp_(parent), asy_ctx_(async), send_(sendWorker), count_(0)
        {
        }

        bool start();
        bool isActive() const { return count_ > 0; }
        void setIRComplete() { if (count_ != 0) ir_complete(); }

        bool cancel();
        void reset() { count_ = 0; }
        void continueRepeat() {repeat_until_ = delayed_by_ms(get_absolute_time(), repeat_limit_);}

        SendWorker *sendWorker() const { return send_; }
    };

    Remote                          *remote_;           // Remote object
    IR_Device                       *ir_device_;        // IR device object
    SendWorker                      *send_worker_;      // Send step worker
    RepeatWorker                    *repeat_worker_;    // Repeat worker
    async_context_t                 *asy_ctx_;          // Async context

    int                             busy_;              // Busy counter
    void add_to_busy(int add);
    bool isBusy() const { return busy_ != 0; }
    void (*busy_cb_)(bool busy, void *user_data);
    void *user_data_;

    bool do_command(Command *cmd);
    bool do_reply(Command *cmd);
    bool send(Command *cmd);
    bool do_repeat(Command *cmd);
    bool cancel_repeat();
    bool isRepeating(const Command *cmd) const
        {return repeat_worker_->isActive() && send_worker_->command() != nullptr && *send_worker_->command() == *cmd;}

    static void identified(const std::string &type, uint16_t address, uint16_t value, void *data);

public:
    IR_Processor(Remote*remote, int gpio_send, int gpio_receive);
    ~IR_Processor() { delete ir_device_; delete send_worker_; delete repeat_worker_; }

    void run();

    void setBusyCallback(void (*busy_cb)(bool busy, void *user_data), void *user_data) { busy_cb_ = busy_cb; user_data_ = user_data; }
};

#endif
