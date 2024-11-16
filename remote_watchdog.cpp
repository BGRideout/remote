//                 ***** Remote class "watchdog" methods  *****

#include "remote.h"
#include "hardware/watchdog.h"
#include "pico/stdio_usb.h"

#define WATCHDOG_TIMEOUT_MSEC   8000

void Remote::watchdog_init()
{
    if (!stdio_usb_connected())
    {
        watchdog_enable(WATCHDOG_TIMEOUT_MSEC, true);
        watchdog_active_ = true;
    }
}

void Remote::watchdog_update()
{
    if (watchdog_active_)
    {
        if (watchdog_get_time_remaining_ms() < WATCHDOG_TIMEOUT_MSEC * 1000 / 2)
        {
            if (!stdio_usb_connected())
            {
                ::watchdog_update();
            }
            else
            {
                watchdog_disable();
                watchdog_active_ = false;
            }
        }
    }
    else
    {
        watchdog_init();
    }
}