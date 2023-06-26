#include <stdint.h>
#include <string.h>

#include "main.h"

#include "wdt_m.h"

#if WDT_M_ENABLED

#include "nrf_drv_wdt.h"
#include "bsp.h"
#include "app_timer.h"
#include "nrf_log.h"


#define WDT_RELOAD_VALUE_MS                     ( 5000 )
#define WDT_APP_TIMER_CHECK_INTERVAL_TIME_MS    ( 2000 )

static nrf_drv_wdt_channel_id   m_channel_id;

// APP_TIMER_DEF(m_wdt_tmr);


// void
// wdt_app_timer_handler(void * p_context) {

//     /* checking modules */

//     // TODO: FEED WDT ONLY IF MODULES ARE WORKING

//     wdt_m_feed();
// }


/**
 * @brief WDT events handler.
 */
static void
wdt_event_handler(void) {

    NRF_LOG_ERROR("wdt occured");

    bsp_board_leds_off();

    //NOTE: The max amount of time we can spend in WDT interrupt is two cycles of 32768[Hz] clock - after that, reset occurs

    // TODO: do something before reset?
}


void
wdt_m_feed(void) {
    nrf_drv_wdt_channel_feed(m_channel_id);
}

void
wdt_m_init() {

    int err_code;

    nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
    config.reload_value = WDT_RELOAD_VALUE_MS;
    err_code = nrf_drv_wdt_init(&config, wdt_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_wdt_channel_alloc(&m_channel_id);
    APP_ERROR_CHECK(err_code);
    nrf_drv_wdt_enable();

    // /* enable an app timer instance for checking flags (checking healthy application/module states) */
    // err_code = app_timer_create(&m_wdt_tmr, APP_TIMER_MODE_REPEATED, wdt_app_timer_handler);
    // APP_ERROR_CHECK(err_code);
    // err_code = app_timer_start(m_wdt_tmr, APP_TIMER_TICKS(WDT_APP_TIMER_CHECK_INTERVAL_TIME_MS), NULL);
    // APP_ERROR_CHECK(err_code);
}

#endif /* WDT_M_ENABLED */