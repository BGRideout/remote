//                 ***** Remote class "watchdog" methods  *****

#include "remote.h"
#include "hardware/watchdog.h"
#include "hardware/exception.h"
#include "pico/stdio_usb.h"
#include "cyw43_locker.h"

#include <signal.h>


#define WATCHDOG_TIMEOUT_MSEC   8000

bool                    Remote::watchdog_active_ = false;
async_at_time_worker_t  Remote::watchdog_worker_ = {.do_work = watchdog_periodic};

static void init_signal_handlers();
static void signal_handler(int sig);
static void exception_handler();

void Remote::watchdog_init()
{
    init_signal_handlers();
    watchdog_periodic(cyw43_arch_async_context(), nullptr);
    printf("watchdog initialized\n");
}

void Remote::watchdog_periodic(async_context_t *ctx, async_at_time_worker_t *param)
{
    if (watchdog_active_)
    {
        if (!stdio_usb_connected())
        {
            CYW43Locker lock;
            watchdog_update();
        }
        else
        {
            watchdog_disable();
            watchdog_active_ = false;
            printf("watchdog disabled\n");
        }
    }
    else
    {
        if (!stdio_usb_connected())
        {
            watchdog_enable(WATCHDOG_TIMEOUT_MSEC, true);
            watchdog_active_ = true;
            printf("watchdog enabled\n");
        }
    }

    async_context_add_at_time_worker_in_ms(ctx, &watchdog_worker_, WATCHDOG_TIMEOUT_MSEC / 2);
}

#ifdef PICO_PANIC_FUNCTION
extern "C" void PICO_PANIC_FUNCTION(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    Remote::get()->logger()->print_timestamp();
    Remote::get()->logger()->print_error("\n*** PANIC! ***\n");
    Remote::get()->logger()->print_error(fmt, ap);

    va_end(ap);

    while (true);
}
#endif

static void init_signal_handlers()
{
    int sigs[] = {SIGFPE, SIGILL, SIGSEGV, SIGTRAP};
    //struct sigaction sa = {.sa_handler = signal_handler};
    for (int ii = 0; ii < count_of(sigs); ii++)
    {
        //sigaction(ii, &sa, nullptr);
        signal(ii, signal_handler);
    }
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, exception_handler);
}

static void signal_handler(int sig)
{
    Remote::get()->logger()->print_timestamp();
    Remote::get()->logger()->print_error("\n*** Signal %d ***\n", sig);

    while (true);
}

static void exception_handler()
{
    Remote::get()->logger()->print_timestamp();
    Remote::get()->logger()->print_error("\n*** HARDFAULT_EXCEPTION %d ***\n");

    while (true);
}
