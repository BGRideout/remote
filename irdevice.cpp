//                  *****  IR_Device class implementation  *****

#include "irdevice.h"

#include "nec_transmitter.h"
#include "nec_receiver.h"
#include "sony_transmitter.h"

std::map<std::string, struct IR_Device::IRMap> IR_Device::irs_ =
            {
                {"NEC", {.tx=IR_Device::new_NEC_tx, .rx=IR_Device::new_NEC_rx, .decode=NEC_Receiver::decode}},
                {"Sony12", {.tx=IR_Device::new_Sony12_tx}},
                {"Sony15", {.tx=IR_Device::new_Sony15_tx}},
            };

IR_LED *IR_Device::new_NEC_tx(int gpio) { return new NEC_Transmitter(gpio); }
IR_LED *IR_Device::new_Sony12_tx(int gpio) { return new Sony12_Transmitter(gpio); }
IR_LED *IR_Device::new_Sony15_tx(int gpio) { return new Sony15_Transmitter(gpio); }

IR_Receiver *IR_Device::new_NEC_rx(int gpio) { return new NEC_Receiver(gpio); }


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
