//                  *****  IR_Device class implementation  *****

#include "irdevice.h"

#include "nec_transmitter.h"
#include "nec_receiver.h"
#include "samsung_transmitter.h"
#include "samsung_receiver.h"
#include "sony_transmitter.h"
#include "sony_receiver.h"
#include "raw_receiver.h"
#include "logger.h"
#include <stdio.h>

#define IR_DEVICE_SAMPLES   256             // Maximum samples to read
#define IR_DEVICE_TIMEOUT   10000           // Read timeout (msec)
#define IR_DEVICE_BITTMO    500             // Bit timeout

std::map<std::string, struct IR_Device::IRMap> IR_Device::irs_ =
            {
                {"NEC", {.tx=IR_Device::new_NEC_tx, .decode=NEC_Receiver::decode}},
                {"Sam", {.tx=IR_Device::new_SAM_tx, .decode=SAMSUNG_Receiver::decode}},
                {"Sony12", {.tx=IR_Device::new_Sony12_tx, .decode=Sony12_Receiver::decode}},
                {"Sony15", {.tx=IR_Device::new_Sony15_tx, .decode=Sony15_Receiver::decode}},
            };

IR_LED *IR_Device::new_NEC_tx(int gpio) { return new NEC_Transmitter(gpio); }
IR_LED *IR_Device::new_SAM_tx(int gpio) { return new SAMSUNG_Transmitter(gpio); }
IR_LED *IR_Device::new_Sony12_tx(int gpio) { return new Sony12_Transmitter(gpio); }
IR_LED *IR_Device::new_Sony15_tx(int gpio) { return new Sony15_Transmitter(gpio); }

IR_Device::IR_Device(int tx_gpio, int rx_gpio, async_context_t *asy_ctx)
     : tx_gpio_(tx_gpio), rx_gpio_(rx_gpio), tx_ir_led_(nullptr), rx_ir_led_(nullptr),
       asy_ctx_(asy_ctx), raw_(nullptr), times_(nullptr), n_times_(0), log_(nullptr), cb_(nullptr), user_data_(nullptr)
{
    read_complete_ = {.do_work = read_done, .user_data = this};
    async_context_add_when_pending_worker(asy_ctx, &read_complete_);
}

bool IR_Device::get_transmitter(const std::string &proto, IR_LED * &ir_led)
{
    bool ret = true;
    if (!ir_led || ir_led->protocol() != proto)
    {
        ret = false;
        auto it = irs_.find(proto);
        if (it != irs_.end())
        {
            if (ir_led)
            {
                delete ir_led;
                ir_led = nullptr;
            }
            ir_led = it->second.tx(tx_gpio_);

            if (ir_led)
            {
                ret = true;
            }
        }
    }

    return ret;
}

int IR_Device::protocols(std::vector<std::string> &protolist)
{
    protolist.clear();
    for (auto it = irs_.cbegin(); it != irs_.cend(); ++it)
    {
        protolist.emplace_back(it->first);
    }
    return protolist.size();
}

void IR_Device::identify(void (*cb)(const std::string &type, uint16_t address, uint16_t value, void *data), void *data)
{
    cb_ = cb;
    user_data_ = data;
    times_ = new uint32_t[IR_DEVICE_SAMPLES];
    n_times_ = 0;
    if (raw_ == nullptr)
    {
        raw_ = new RAW_Receiver(rx_gpio_, IR_DEVICE_SAMPLES);
    }
    raw_->set_times(times_, IR_DEVICE_SAMPLES, &n_times_);
    raw_->set_user_data(this);
    raw_->set_message_timeout(IR_DEVICE_TIMEOUT);
    raw_->set_bit_timeout(IR_DEVICE_BITTMO);
    raw_->set_rcv_callback(ir_rcv);
    raw_->set_tmo_callback(ir_tmo);
    raw_->start_message_timeout();
}

void IR_Device::ir_rcv(uint64_t timestamp, uint16_t address, uint16_t value, IR_Receiver *obj)
{
    IR_Device *self = static_cast<IR_Device *>(obj->user_data());
    async_context_set_work_pending(self->asy_ctx_, &self->read_complete_);
}

bool IR_Device::ir_tmo(bool msg, uint32_t n_pulse, uint32_t const *pulses, IR_Receiver *obj)
{
    IR_Device *self = static_cast<IR_Device *>(obj->user_data());
    if (self->log_) self->log_->print("identify: %s timeout. Read %d pulses\n", msg ? "message" : "pulse", n_pulse);
    async_context_set_work_pending(self->asy_ctx_, &self->read_complete_);
    return false;
}

void IR_Device::read_done(async_context_t *ctx, async_when_pending_worker_t *worker)
{
    IR_Device *self = static_cast<IR_Device *>(worker->user_data);
    self->read_done();
}

void IR_Device::read_done()
{
    if (log_)
    {
        log_->print_debug(1, "Read %d times:", n_times_);
        for (uint32_t ii = 0; ii < n_times_; ii++)
        {
            if ((ii % 10) == 0) log_->print_debug(1, "\n");
            log_->print_debug(1, " %d", times_[ii]);
        }
        log_->print_debug(1, "\n");
    }
    if (raw_)
    {
        delete raw_;
        raw_ = nullptr;
    }

    std::string type;
    uint16_t address = 0;
    uint16_t value = 0;
    for (auto it = irs_.cbegin(); it != irs_.cend(); ++it)
    {
        if (it->second.decode)
        {
            if (it->second.decode(times_, n_times_, address, value, 0xffff))
            {
                type = it->first;
                break;
            }
        }
    }

    if (log_) log_->print("identify: result: '%s' %d %d\n", type.c_str(), address, value);

    if (cb_)
    {
        cb_(type, address, value, user_data_);
    }
    cb_ = nullptr;
    user_data_ = nullptr;
}