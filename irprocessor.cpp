//                  *****  IR_Processor class implementation  *****

#include "irprocessor.h"
#include "remote.h"
#include <stdio.h>
#include <pico/stdlib.h>

#include "nec_transmitter.h"

IR_Processor::IR_Processor(Remote *remote, int gpio_send, int gpio_receive)
     : remote_(remote), gpio_send_(gpio_send), gpio_receive_(gpio_receive), ir_led_(nullptr)
{
    asy_ctx_ = cyw43_arch_async_context();
    printf("IR_Processor core %d\n", async_context_core_num(asy_ctx_));
    send_worker_ = new SendWorker(this, asy_ctx_);
    repeat_worker_ = new RepeatWorker(this, asy_ctx_, send_worker_);
}

void IR_Processor::run()
{
    while (true)
    {
        Command *cmd = remote_->getNextCommand();
        if (cmd)
        {
            do_command(cmd);
        }
    }
}

bool IR_Processor::do_reply(Command *cmd)
{
    remote_->commandReply(cmd);
    return true;
}

bool IR_Processor::do_reply(SendWorker *param)
{
    bool ret = do_reply(param->command());
    param->reset();
    return ret;
}

bool IR_Processor::do_command(Command *cmd)
{
    if (cmd->action() == "click")
    {
        cancel_repeat();
        cmd->setReply(cmd->action());
        if (!send(cmd))
        {
            do_reply(cmd);
        }
    }
    else if (cmd->action() == "press")
    {
        bool cancelled = cancel_repeat();
        if (cmd->repeat() > 0 && cmd->redirect().empty())
        {
            if (!cancelled && send_worker_->command() == nullptr)
            {
                cmd->setReply(cmd->action());
                do_repeat(cmd);
            }
            else
            {
                cmd->setReply("busy");
            }
            do_reply(cmd);
        }
        else
        {
            cmd->setReply("no-repeat");
            if (!send(cmd))
            {
                do_reply(cmd);
            }
        }
    }
    else if (cmd->action() == "release" || cmd->action()== "cancel")
    {
        cancel_repeat();
        cmd->setReply(cmd->action());
        do_reply(cmd);
    }
    return true;
}

bool IR_Processor::send(Command *cmd)
{
    bool ret = false;
    SendWorker *param = send_worker_;
    if (param->command() == nullptr)
    {
        param->reset();
        param->setCommand(cmd);
        param->setDoReply();
        ret = send(param);
    }
    else
    {
        cmd->setReply("busy");
    }
    return ret;
}

bool IR_Processor::send(SendWorker *param)
{
    param->setStep(0);
    return param->start();
}    

void IR_Processor::send_work(SendWorker *param)
{
    int ret = 0;
    int ii = param->sendStep();
    if (ii < param->command()->steps().size())
    {
        const Command::Step &step = param->command()->steps().at(ii);
        printf("Step: '%s' %d %d %d repeat=%s\n", step.type().c_str(), step.address(), step.value(), step.delay(), param->repeated() ? "T" : "F");

        async_context_acquire_lock_blocking(asy_ctx_);
#if 1
        if (!ir_led_)
        {
            NEC_Transmitter *led = new NEC_Transmitter(gpio_send_);
            ir_led_ = led;
            ir_led_->setDoneCallback(set_ir_complete, param);
        }
        ir_led_->setMessageTimes(step.address(), step.value());
        if (!param->repeated())
        {
            ir_led_->transmit();
        }
        else
        {
            ir_led_->repeat();
        }
#else
        set_ir_complete(nullptr, param);
#endif
    }
    else
    {
        if (param->doReply())
        {
            do_reply(param);
        }
    }
}

void IR_Processor::set_ir_complete(IR_LED *led, void *user_data)
{
    SendWorker *param = sendWorker(user_data);
    async_context_release_lock(param->irProcessor()->asy_ctx_);
    param->setIRComplete();
}

bool IR_Processor::do_repeat(Command *cmd)
{
    bool ret = false;
    Command *rcmd = new Command(*cmd);
    RepeatWorker *rparam = repeat_worker_;
    rparam->reset();
    SendWorker *sparam = rparam->sendWorker();
    sparam->reset();
    sparam->setCommand(rcmd);
    sparam->setRepeatWorker(rparam);

    int repeat = rcmd->repeat();
    if (rcmd->repeat() > 0)
    {
        int delays = 0;
        for (auto it = cmd->steps().cbegin(); it != cmd->steps().cend(); ++it)
        {
            delays += it->delay();
        }
        if (delays > repeat)
        {
            repeat = delays + 10;
        }
        if (repeat < 100)
        {
            repeat = 100;
        }
        rparam->setInterval(repeat);
        printf("Repeating at %d msec intervals\n", repeat);
        ret = rparam->start();
    }
    return ret;
}

void IR_Processor::repeat_work(RepeatWorker *param)
{
    SendWorker *send_param = param->sendWorker();
    if (send_param->command() && param->interval() > 0)
    {
        if (!param->reachedLimit())
        {
            send_param->setRepeated();
            send(send_param);
        }
        else
        {
            printf("Stop repeating at limit\n");
            param->finish();
        }
    }
}

bool IR_Processor::cancel_repeat()
{
    RepeatWorker *param = repeat_worker_;
    return param->cancel();
}


void IR_Processor::SendWorker::time_work(async_context_t *context, async_at_time_worker_t *worker)
{
    SendWorker *param = sendWorker(worker);
    param->irProcessor()->send_work(param);
}

void IR_Processor::SendWorker::ir_complete(async_context_t *context, async_when_pending_worker_t *worker)
{
    SendWorker *param = sendWorker(worker);
    param->scheduleNext();
}

void IR_Processor::RepeatWorker::time_work(async_context_t *context, async_at_time_worker_t *worker)
{
    RepeatWorker *param = repeatWorker(worker);
    if (param->isActive())
    {
        param->irProcessor()->repeat_work(param);
    }
    else if (!param->isIdle())
    {
        param->finish();
    }
}

void IR_Processor::RepeatWorker::ir_complete(async_context_t *context, async_when_pending_worker_t *worker)
{
    RepeatWorker *param = repeatWorker(worker);
    if (param->isActive())
    {
        param->scheduleNext(param->interval());
    }
    else if (!param->isIdle())
    {
        param->finish();
    }
}

bool IR_Processor::RepeatWorker::cancel()
{
    bool ret = false;
    if (!isIdle())
    {
        ret = true;
        count_ = -1;
    }
    return ret;
}

void IR_Processor::RepeatWorker::finish()
{
    async_context_remove_at_time_worker(asy_ctx_, &time_worker_);
    send_->resetCommand();
    send_->reset();
    reset();
}