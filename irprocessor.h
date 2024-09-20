//                  *****  IR_Processor class  *****

#ifndef IR_PROCESSOR_H
#define IR_PROCESSOR_H

#include "command.h"
#include <vector>
#include <pico/async_context.h>

class Remote;

class IR_Processor
{
private:
    class RepeatWorker;

    class SendWorker
    {
    private:
        IR_Processor                *irp_;              // Pointer to this object
        async_context_t             *asy_ctx_;          // Async context
        async_at_time_worker_t      time_worker_;       // Timing worker
        async_when_pending_worker_t ir_complete_;       // IR output complete worker
        Command                     *cmd_;              // Active command
        RepeatWorker                *repeat_worker_;    // Repeat worker
        int                         send_step_;         // Step in send operation
        bool                        repeated_;          // Repeated operation flag
        bool                        do_reply_;          // Send reply when action complete

        static void time_work(async_context_t *, async_at_time_worker_t *);
        static void ir_complete(async_context_t *, async_when_pending_worker_t *);

    public:
        SendWorker(IR_Processor *parent, async_context_t *async)
         : irp_(parent), asy_ctx_(async), cmd_(nullptr), repeat_worker_(nullptr), send_step_(0), repeated_(false), do_reply_(false)
        {
            time_worker_.do_work = time_work; time_worker_.user_data = this;
            ir_complete_.do_work = ir_complete, ir_complete_.user_data = this;
            async_context_add_when_pending_worker(asy_ctx_, &ir_complete_);
        }

        IR_Processor *irProcessor() const { return irp_; }

        Command *command() const { return cmd_; }
        void setCommand(Command *cmd) { cmd_ = cmd; }
        void resetCommand() { if (cmd_) delete cmd_; cmd_ = nullptr; }

        bool start() { send_step_ = 0; return async_context_add_at_time_worker_in_ms(asy_ctx_, &time_worker_, 0); }
        bool scheduleNext()
          {
            const Command::Step &step = cmd_->steps().at(nextStep());
            return async_context_add_at_time_worker_in_ms(asy_ctx_, &time_worker_, step.delay());
          }

        void setRepeatWorker(RepeatWorker *worker) { repeat_worker_ = worker; }

        int sendStep() const { return send_step_; }
        int nextStep() { return send_step_++; }
        void setStep(int step) { send_step_ = step; }

        bool repeated() const { return repeated_; }
        void setRepeated(bool repeated = true) { repeated_ = repeated; }

        bool doReply() const { return do_reply_; }
        void setDoReply(bool doReply = true) { do_reply_ = doReply; }

        void setIRComplete()
          {
            async_context_set_work_pending(asy_ctx_, &ir_complete_);
            if (repeat_worker_) repeat_worker_->setIRComplete();
          }

        void reset() { cmd_ = nullptr; repeat_worker_ = nullptr; send_step_ = 0; repeated_ = false; do_reply_ = false; }
    };

    static SendWorker *sendWorker(async_at_time_worker_t *worker) { return static_cast<SendWorker *>(worker->user_data); }
    static SendWorker *sendWorker(async_when_pending_worker_t *worker) { return static_cast<SendWorker *>(worker->user_data); }
    static SendWorker *sendWorker(void *user_data) { return static_cast<SendWorker *>(user_data); }

    class RepeatWorker
    {
    private:
        IR_Processor                *irp_;              // Pointer to this object
        async_context_t             *asy_ctx_;          // Async context
        async_at_time_worker_t      time_worker_;       // Repeat time worker
        async_when_pending_worker_t ir_complete_;       // IR output complete worker
        SendWorker                  *send_;             // Send worker
        int                         interval_;          // Repeat interval (ms)
        int                         count_;             // Limit counter / activity flag

        static void time_work(async_context_t *, async_at_time_worker_t *);
        static void ir_complete(async_context_t *, async_when_pending_worker_t *);

    public:
        RepeatWorker(IR_Processor *parent, async_context_t *async, SendWorker *sendWorker)
         : irp_(parent), asy_ctx_(async), send_(sendWorker), interval_(0), count_(0)
        {
            time_worker_.do_work = time_work; time_worker_.user_data = this;
            ir_complete_.do_work = ir_complete; ir_complete_.user_data = this;
            async_context_add_when_pending_worker(asy_ctx_, &ir_complete_);
        }

        IR_Processor *irProcessor() const { return irp_; }
        SendWorker *sendWorker() const { return send_; }

        int interval() const { return interval_; }
        void setInterval(int interval) { interval_ = interval; }

        void setActive() { count_ = 1; }
        bool isActive() const { return count_ > 0; }
        bool reachedLimit() { return isActive() ? count_++ > 100 : false; }

        bool start() { setActive(); time_worker_.next_time = get_absolute_time(); return send_->start(); }
        bool scheduleNext(int interval)
         {
            time_worker_.next_time = delayed_by_ms(time_worker_.next_time, interval);
            return async_context_add_at_time_worker(asy_ctx_, &time_worker_);
         }
        void setIRComplete() { if (isActive()) async_context_set_work_pending(asy_ctx_, &ir_complete_); }

        bool cancel();
        void finish();
        void reset() { interval_ = 0; count_ = 0; }
    };

    static RepeatWorker *repeatWorker(async_at_time_worker_t *worker) { return static_cast<RepeatWorker *>(worker->user_data); }
    static RepeatWorker *repeatWorker(async_when_pending_worker_t *worker) { return static_cast<RepeatWorker *>(worker->user_data); }

    Remote                          *remote_;           // Remote object
    int                             gpio_send_;         // GPIO to send IR data
    int                             gpio_receive_;      // GPIO to receive IR dat
    SendWorker                      *send_worker_;      // Send step worker
    RepeatWorker                    *repeat_worker_;    // Repeat worker
    async_context_t                 *asy_ctx_;          // Async context

    bool do_command(Command *cmd);
    bool do_reply(Command *cmd);
    bool do_reply(SendWorker *param);
    bool send(Command *cmd);
    bool send(SendWorker *param);
    bool do_repeat(Command *cmd);
    bool cancel_repeat();

    void send_work(SendWorker *param);
    static void set_ir_complete(void *user_data);

    void repeat_work(RepeatWorker *param);

public:
    IR_Processor(Remote*remote, int gpio_send, int gpio_receive);

    void run();
};

#endif
