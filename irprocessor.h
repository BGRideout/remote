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
    class SendWorker
    {
    private:
        IR_Processor                *irp_;              // Pointer to this object
        async_at_time_worker_t      time_worker_;       // Timing worker
        async_when_pending_worker_t ir_complete_;       // IR output complete worker
        Command                     *cmd_;              // Active command
        int                         send_step_;         // Step in send operation
        bool                        repeated_;          // Repeated operation flag
        bool                        do_reply_;          // Send reply when action complete

    public:
        SendWorker(IR_Processor *parent,
                 void (*time_worker)(async_context_t *, async_at_time_worker_t *),
                 void (*ir_complete)(async_context_t *, async_when_pending_worker_t *))
         : irp_(parent), cmd_(nullptr), send_step_(0), repeated_(false), do_reply_(false)
        {
            time_worker_.do_work = time_worker; time_worker_.user_data = this;
            ir_complete_.do_work = ir_complete, ir_complete_.user_data = this;
        }

        IR_Processor *irProcessor() const { return irp_; }
        async_at_time_worker_t *timeWorker() { return &time_worker_; }
        async_when_pending_worker_t *irComplete() { return &ir_complete_; }

        Command *command() const { return cmd_; }
        void setCommand(Command *cmd) { cmd_ = cmd; }

        int sendStep() const { return send_step_; }
        int nextStep() { return send_step_++; }
        void setStep(int step) { send_step_ = step; }

        bool repeated() const { return repeated_; }
        void setRepeated(bool repeated = true) { repeated_ = repeated; }

        bool doReply() const { return do_reply_; }
        void setDoReply(bool doReply = true) { do_reply_ = doReply; }

        void reset() { cmd_ = nullptr; send_step_ = 0; repeated_ = false; do_reply_ = false; }
    };

    static SendWorker *sendWorker(async_at_time_worker_t *worker) { return static_cast<SendWorker *>(worker->user_data); }
    static SendWorker *sendWorker(async_when_pending_worker_t *worker) { return static_cast<SendWorker *>(worker->user_data); }
    static SendWorker *sendWorker(void *user_data) { return static_cast<SendWorker *>(user_data); }

    class RepeatWorker
    {
    private:
        IR_Processor                *irp_;              // Pointer to this object
        async_at_time_worker_t      worker_;            // Repeat time worker
        SendWorker                  *send_;             // Send worker
        int                         interval_;          // Repeat interval (ms)
        int                         count_;             // Limit counter / activity flag

    public:
        RepeatWorker(IR_Processor *parent,
                     SendWorker *sendWorker,
                     void (*do_work)(async_context_t *, struct async_work_on_timeout *))
         : irp_(parent), send_(sendWorker), interval_(0), count_(0)
        {
            worker_.do_work = do_work; worker_.user_data = this;
        }

        IR_Processor *irProcessor() const { return irp_; }
        SendWorker *sendWorker() const { return send_; }
        async_at_time_worker_t *worker() { return &worker_; }

        int interval() const { return interval_; }
        void setInterval(int interval) { interval_ = interval; }

        void setActive() { count_ = 1; }
        bool isActive() const { return count_ > 0; }
        bool reachedLimit() { return count_++ > 100; }

        void reset() { interval_ = 0; count_ = 0; }
    };

    static RepeatWorker *workerParam(async_at_time_worker_t *worker) { return static_cast<RepeatWorker *>(worker->user_data); }

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

    static void send_work(async_context_t *context, async_at_time_worker_t *worker);
    void send_work(SendWorker *param);
    static void ir_complete(async_context_t *context, async_when_pending_worker_t *worker);
    static void set_ir_complete(void *user_data);

    static void repeat_work(async_context_t *context, async_at_time_worker_t *worker);
    void repeat_work(RepeatWorker *param);

public:
    IR_Processor(Remote*remote, int gpio_send, int gpio_receive);

    void run();
};

#endif
