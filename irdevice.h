//                  *****  IR_Device class  *****

#ifndef IR_DEVICE_H
#define IR_DEVICE_H

#include "ir_led.h"
#include "ir_receiver.h"
#include <map>
#include <string>
#include <pico/async_context.h>

class RAW_Receiver;

class IR_Device
{
private:
    int             tx_gpio_;                   // GPIO for transmit
    int             rx_gpio_;                   // GPIO for receive
    IR_LED          *tx_ir_led_;                // Transmit device
    IR_LED          *rx_ir_led_;                // Receive device
    async_context_t *asy_ctx_;                  // Async context
    async_when_pending_worker_t read_complete_; // IR output complete worker
    RAW_Receiver    *raw_;                      // Raw IR data rreceiver
    uint32_t        *times_;                    // Read times
    uint32_t        n_times_;                   // Number of read times

    //  *****  Protocol mapping  *****
    struct IRMap
    {
        IR_LED *(*tx)(int gpio);
        IR_Receiver *(*rx)(int gpio);
        bool (*decode)(uint32_t const *pulses, uint32_t n_pulse, uint16_t &addr, uint16_t &func, uint16_t address);
    };

    static std::map<std::string, struct IRMap> irs_;
    static IR_LED *new_NEC_tx(int gpio);
    static IR_Receiver *new_NEC_rx(int gpio);
    static IR_LED *new_Sony12_tx(int gpio);
    static IR_LED *new_Sony15_tx(int gpio);

    static void ir_rcv(uint64_t timestamp, uint16_t address, uint16_t value, IR_Receiver *obj);
    static bool ir_tmo(bool msg, uint32_t n_pulse, uint32_t const *pulses, IR_Receiver *obj);
    static void read_done(async_context_t *ctx, async_when_pending_worker_t *worker);
    void read_done();
    void (*cb_)(const std::string &type, uint16_t address, uint16_t value, void *data);
    void *user_data_;

public:
    IR_Device(int tx_gpio, int rx_gpio, async_context_t *asy_ctx);
    ~IR_Device() { release_tx(); release_rx(); }

    bool get_transmitter(const std::string &proto, IR_LED * &ir_led);

    bool validProtocol(const std::string &proto) const { return irs_.find(proto) != irs_.cend(); }

    void release_tx() { if (tx_ir_led_) delete tx_ir_led_; tx_ir_led_ = nullptr; }
    void release_rx() { if (rx_ir_led_) delete rx_ir_led_; rx_ir_led_ = nullptr; }

    void identify(void (*cb)(const std::string &type, uint16_t address, uint16_t value, void *data), void *data);
};

#endif
