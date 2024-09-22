//                  *****  IR_Processor class implementation  *****

#include "irprocessor.h"
#include "menu.h"
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
    start_time_ = to_ms_since_boot(get_absolute_time());
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

bool IR_Processor::send_work(SendWorker *param)
{
    bool ret = false;
    int ii = param->sendStep();
    if (param->command() && ii < param->command()->steps().size())
    {
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time_;
        Command::Step step = param->getStep(ii);
        printf("%5d Step: '%s' %d %d %d repeat=%s\n",
         elapsed, step.type().c_str(), step.address(), step.value(), step.delay(), param->repeated() ? "T" : "F");

        if (get_transmitter(step.type(), param))
        {
            ir_led_->setMessageTimes(step.address(), step.value());
            if (!param->repeated())
            {
                ir_led_->transmit();
            }
            else
            {
                ir_led_->repeat();
            }
        }
        else if (param->getMenuSteps(step))
        {
            step = param->getStep(ii);
            printf("%5d Step: '%s' %d %d %d repeat=%s\n",
            elapsed, step.type().c_str(), step.address(), step.value(), step.delay(), param->repeated() ? "T" : "F");
            if (get_transmitter(step.type(), param))
            {
                ir_led_->setMessageTimes(step.address(), step.value());
                if (!param->repeated())
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
                set_ir_complete(nullptr, param);
            }
        }
        else
        {
            set_ir_complete(nullptr, param);
        }
        ret = true;
    }
    else if (param->command())
    {
        if (param->doReply())
        {
            do_reply(param);
        }
    }
    return ret;
}

bool IR_Processor::get_transmitter(const std::string &proto, SendWorker *param)
{
    if (!ir_led_ || ir_led_->protocol() != proto)
    {
        if (ir_led_)
        {
             delete ir_led_;
             ir_led_ = nullptr;
        }
        if (proto == "NEC")
        {
            ir_led_ = new NEC_Transmitter(gpio_send_);
        }

        if (ir_led_)
        {
            ir_led_->setDoneCallback(set_ir_complete, param);
        }
    }

    return ir_led_ != nullptr;
}

void IR_Processor::set_ir_complete(IR_LED *led, void *user_data)
{
    SendWorker *param = sendWorker(user_data);
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

    if (rcmd->repeat() > 0)
    {
        int repeat = rcmd->repeat();
        int delays = 0;
        for (int ii = 0; ii < cmd->steps().size(); ii++)
        {
            if (get_transmitter(cmd->steps().at(ii).type(), sparam))
            {
                delays += ir_led_->repeatInterval();
            }
            else if (sparam->getMenuSteps(cmd->steps().at(ii)))
            {
                delays += sparam->getMenuDelay();
            }
            delays += cmd->steps().at(ii).delay();
        }
        if (delays > repeat)
        {
            repeat = delays;
        }
        if (repeat < 20)
        {
            repeat = 20;
        }
        rparam->setInterval(repeat);
        printf("Repeating at %d msec intervals\n", repeat);
        start_time_ = to_ms_since_boot(get_absolute_time());
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
    if (param->irProcessor()->send_work(param))
    {
        if (param->repeat_worker_)
        {
            param->repeat_worker_->setIRComplete();
        }
    }
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
    // printf("Menu returned %d steps. status = %d\n", menu_steps_.size(), ret);
    // for (auto it = menu_steps_.cbegin(); it != menu_steps_.cend(); ++it)
    // {
    //     printf("%s %d %d %d\n", it->type().c_str(), it->address(), it->value(), it->delay());
    // }
    return ret;
}

int IR_Processor::SendWorker::getMenuDelay()
{
    int ret = 0;
    while (menu_steps_.size() > 0)
    {
        ret += menu_steps_.front().delay();
        menu_steps_.pop_front();
    }
    return ret;
}

bool IR_Processor::SendWorker::scheduleNext()
{
    const Command::Step step = getStep(nextStep(), true);
    int delay = step.delay();
    int rpt = 0;
    if (irProcessor()->get_transmitter(step.type(), this))
    {
        rpt = irProcessor()->ir_led_->repeatInterval();
    }
    absolute_time_t next1 = make_timeout_time_ms(delay);
    absolute_time_t next2 = delayed_by_ms(time_worker_.next_time, rpt);
    time_worker_.next_time = absolute_time_diff_us(next1, next2) > 0 ? next2 : next1;
    return async_context_add_at_time_worker(asy_ctx_, &time_worker_);
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