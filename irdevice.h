//                  *****  IR_Device class  *****

#ifndef IR_DEVICE_H
#define IR_DEVICE_H

#include "ir_led.h"
#include "ir_receiver.h"
#include <map>
#include <string>

class IR_Device
{
private:
    int             tx_gpio_;               // GPIO for transmit
    int             rx_gpio_;               // GPIO for receive
    IR_LED          *tx_ir_led_;            // Transmit device
    IR_LED          *rx_ir_led_;            // Receive device

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

public:
    IR_Device(int tx_gpio, int rx_gpio) : tx_gpio_(tx_gpio), rx_gpio_(rx_gpio), tx_ir_led_(nullptr), rx_ir_led_(nullptr) {}
    ~IR_Device() { release_tx(); release_rx(); }

    bool get_transmitter(const std::string &proto, IR_LED * &ir_led);

    bool validProtocol(const std::string &proto) const { return irs_.find(proto) != irs_.cend(); }

    void release_tx() { if (tx_ir_led_) delete tx_ir_led_; tx_ir_led_ = nullptr; }
    void release_rx() { if (rx_ir_led_) delete rx_ir_led_; rx_ir_led_ = nullptr; }
};

#endif
