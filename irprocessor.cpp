//                  *****  IR_Processor class implementation  *****

#include "irprocessor.h"
#include "ir_led.h"
#include "menu.h"
#include "remote.h"
#include <stdio.h>
#include <pico/stdlib.h>

IR_Processor::IR_Processor(Remote *remote, int gpio_send, int gpio_receive)
     : remote_(remote), busy_(0), busy_cb_(nullptr)
{
    asy_ctx_ = cyw43_arch_async_context();
    ir_device_ = new IR_Device(gpio_send, gpio_receive, asy_ctx_);
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
    else if (cmd->action() == "test_send")
    {
        cancel_repeat();
        cmd->setReply(cmd->action());
        if (!send(cmd))
        {
            do_reply(cmd);
        }
    }
    else if (cmd->action() == "ir_get")
    {
        ir_device_->identify(identified, new std::pair<IR_Processor *, Command *>(this, cmd));
    }
    return true;
}

void IR_Processor::identified(const std::string &type, uint16_t address, uint16_t value, void *data)
{
    auto ptrs = static_cast<std::pair<IR_Processor *, Command *> *>(data);
    IR_Processor *self = ptrs->first;
    Command *cmd = ptrs->second;
    cmd->setStep(type, address, value);
    cmd->setReply("ir_resp");
    self->do_reply(cmd);
    delete ptrs;
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
        ret = param->start();
    }
    else
    {
        cmd->setReply("busy");
    }
    return ret;
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

    if (rcmd->repeat() > 0)
    {
        int repeat = rcmd->repeat();
        int delays = sparam->getTime();
        if (delays > repeat)
        {
            repeat = delays;
        }
        if (repeat < 20)
        {
            repeat = 20;
        }
        rparam->setInterval(repeat);
        ret = rparam->start();
    }
    return ret;
}

bool IR_Processor::cancel_repeat()
{
    RepeatWorker *param = repeat_worker_;
    return param->cancel();
}

void IR_Processor::add_to_busy(int add)
{
    int prev = busy_;
    busy_ += add;
    if (prev == 0 ^ busy_ == 0)
    {
        if (busy_cb_)
        {
            busy_cb_(busy_ != 0);
        }
    }
}

//  *****  SendWorker  *****

bool IR_Processor::SendWorker::start()
{
    irProcessor()->add_to_busy(1);
    send_step_ = 0;
    if (!repeated())
    {
        start_time_ = to_ms_since_boot(get_absolute_time());
    }
    return async_context_add_at_time_worker_in_ms(asy_ctx_, &time_worker_, 0);
}

void IR_Processor::SendWorker::time_work(async_context_t *context, async_at_time_worker_t *worker)
{
    SendWorker *param = sendWorker(worker);
    param->time_work();
}

void IR_Processor::SendWorker::time_work()
{
    bool more = false;
    int ii = sendStep();
    if (command() && ii < command()->steps().size())
    {
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time_;
        Command::Step step = getStep(ii, true);
        // printf("%5d Step %2d: '%s' %d %d %d repeat=%s\n",
        //        elapsed, ii, step.type().c_str(), step.address(), step.value(), step.delay(), repeated() ? "T" : "F");

        if (get_transmitter(step.type()))
        {
            bool repeat = repeated() ||
                         (last_step_.type() == step.type() &&
                          last_step_.address() == step.address() &&
                          last_step_.value() == step.value() &&
                          last_step_.delay() < ir_led_->repeatInterval() / 2);
            last_step_ = step;
            ir_led_->setMessageTimes(step.address(), step.value());
            if (!repeat)
            {
                ir_led_->transmit();
            }
            else
            {
                ir_led_->repeat();
            }
        }
        else if (getMenuSteps(step))
        {
            step = getStep(ii, true);
            // printf("%5d Step %2d: '%s' %d %d %d repeat=%s\n",
            //        elapsed, ii, step.type().c_str(), step.address(), step.value(), step.delay(), repeated() ? "T" : "F");
            if (get_transmitter(step.type()))
            {
                last_step_ = step;
                ir_led_->setMessageTimes(step.address(), step.value());
                if (!repeated())
                {
                    ir_led_->transmit();
                }
                else
                {
                    ir_led_->repeat();
                }
            }
            else
            {
                set_ir_complete(nullptr, this);
            }
        }
        else
        {
            set_ir_complete(nullptr, this);
        }
        more = true;
    }
    else if (command())
    {
        if (doReply())
        {
            irProcessor()->do_reply(command());
        }
    }

    if (!more)
    {
        if (repeat_worker_)
        {
            repeat_worker_->setIRComplete();
        }
        else
        {
            reset();
        }
    irProcessor()->add_to_busy(-1);
    }
}

void IR_Processor::SendWorker::set_ir_complete(IR_LED *led, void *user_data)
{
    SendWorker *param = sendWorker(user_data);
    param->setIRComplete();
}

void IR_Processor::SendWorker::ir_complete(async_context_t *context, async_when_pending_worker_t *worker)
{
    SendWorker *param = sendWorker(worker);
    param->scheduleNext();
}

Command::Step IR_Processor::SendWorker::getStep(int stepNo, bool peek)
{
    Command::Step ret;
    if (menu_steps_.size() > 0)
    {
        ret = menu_steps_.front();
        if (!peek)
        {
            menu_steps_.pop_front();
            nextStep();
        }
    }
    else if (stepNo < cmd_->steps().size())
    {
        ret = cmd_->steps().at(stepNo);
    }
    return ret;
}

bool IR_Processor::SendWorker::getMenuSteps(const Command::Step &step)
{
    bool ret = Menu::getMenuSteps(step, menu_steps_);
    if (ret && step.delay() > 0)
    {
        menu_steps_.push_back(Command::Step("", 0, 0, step.delay()));
    }
    return ret;
}

int IR_Processor::SendWorker::getTime()
{
    int delays = 0;
    for (int ii = 0; ii < cmd_->steps().size(); ii++)
    {
        delays += cmd_->steps().at(ii).delay();
        if (get_transmitter(cmd_->steps().at(ii).type()))
        {
            delays += ir_led_->repeatInterval() * (1 /*+ ir_led_->minimum_repeats()*/);
        }
        else if (getMenuSteps(cmd_->steps().at(ii)))
        {
            while (menu_steps_.size() > 0)
            {
                delays += menu_steps_.front().delay();
                if (get_transmitter(menu_steps_.front().type()))
                {
                    delays += ir_led_->repeatInterval() * (1 + ir_led_->minimum_repeats());
                }
                menu_steps_.pop_front();
            }
        }
    }
    return delays;
}

bool IR_Processor::SendWorker::scheduleNext()
{
    int ns = nextStep();
    const Command::Step step = getStep(ns);
    int delay = step.delay();
    int rpt = 0;
    if (get_transmitter(step.type()))
    {
        rpt = ir_led_->repeatInterval();
    }
    absolute_time_t next1 = make_timeout_time_ms(delay);
    absolute_time_t next2 = delayed_by_ms(time_worker_.next_time, rpt);
    time_worker_.next_time = absolute_time_diff_us(next1, next2) > 0 ? next2 : next1;
    return async_context_add_at_time_worker(asy_ctx_, &time_worker_);
}

bool IR_Processor::SendWorker::get_transmitter(const std::string &proto)
{
    bool ret = irProcessor()->ir_device_->get_transmitter(proto, ir_led_);
    if (ret)
    {
        ir_led_->setDoneCallback(set_ir_complete, this);
    }
    return ret;
}


//  *****  RepeatWorker  *****

bool IR_Processor::RepeatWorker::start()
{
    irProcessor()->add_to_busy(1);
    setActive();
    time_worker_.next_time = get_absolute_time();
    return send_->start();
}

void IR_Processor::RepeatWorker::time_work(async_context_t *context, async_at_time_worker_t *worker)
{
    RepeatWorker *param = repeatWorker(worker);
    param->time_work();
}

void IR_Processor::RepeatWorker::time_work()
{
    if (isActive())
    {
        SendWorker *send_param = sendWorker();
        if (send_param->command() && interval() > 0)
        {
            if (!reachedLimit())
            {
                send_param->setRepeated();
                send_param->start();
            }
            else
            {
                printf("Stop repeating at limit\n");
                finish();
            }
        }
        else
        {
            finish();
        }
    }
}

void IR_Processor::RepeatWorker::ir_complete(async_context_t *context, async_when_pending_worker_t *worker)
{
    RepeatWorker *param = repeatWorker(worker);
    param->ir_complete();
}

void IR_Processor::RepeatWorker::ir_complete()
{
    if (isActive())
    {
        scheduleNext(interval());
    }
    else if (!isIdle())
    {
        finish();
    }
}

bool IR_Processor::RepeatWorker::scheduleNext(int interval)
{
    time_worker_.next_time = delayed_by_ms(time_worker_.next_time, interval);
    return async_context_add_at_time_worker(asy_ctx_, &time_worker_);
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
    irProcessor()->add_to_busy(-1);
}