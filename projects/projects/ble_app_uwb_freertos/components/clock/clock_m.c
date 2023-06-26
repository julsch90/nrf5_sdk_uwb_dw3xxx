#include "clock_m.h"

#include "nrf_drv_clock.h"
#include "app_error.h"

void
clock_m_init(void) {

    /* initialize clock driver for better time accuracy in FREERTOS */
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    // // lfclk started already by sd or freertos
    // if (!nrf_sdh_is_enabled()) {
    //     nrf_drv_clock_lfclk_request(NULL);
    // }

    return;
}
