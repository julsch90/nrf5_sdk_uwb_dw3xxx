/**
 * @file rtc_m.c
 * @author julian schmidt
 * @brief
 * @version 0.1
 * @date 2024-05-13
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "stdint.h"

#include "rtc_m.h"

#include "nrf_drv_rtc.h"
#include "nrf_log.h"

const nrf_drv_rtc_t rtc = NRF_DRV_RTC_INSTANCE(2); /**< Declaring an instance of nrf_drv_rtc for RTC2. */

static uint8_t m_rtc_m_initialized;
static void (*m_rtc_evt_handler)(uint32_t expired_timestamp);

// static uint64_t m_rtc_counter_monotonic;
static uint32_t m_rtc_overflow_counter;

/** @brief: Function for handling the RTC2 interrupts.
 * Triggered on TICK, COMPARE0, COMPARE1 match.
 */
static void
rtc_handler(nrf_drv_rtc_int_type_t int_type) {

    // uint32_t timestamp;
    uint32_t timestamp_compare = 0;

    // timestamp = nrf_drv_rtc_counter_get(&rtc);

    switch (int_type) {
        case NRF_DRV_RTC_INT_COMPARE0:

            // timestamp = nrf_drv_rtc_counter_get(&rtc);
            timestamp_compare = nrf_rtc_cc_get(rtc.p_reg, 0);

            nrf_drv_rtc_cc_disable(&rtc, 0);

            // NRF_LOG_INFO("NRF_DRV_RTC_INT: ts:%u cc0:%u", timestamp, timestamp_compare);

            if(m_rtc_evt_handler != NULL) {
                // nrf_gpio_pin_toggle(LED_1);
                m_rtc_evt_handler(timestamp_compare); // TODO: add current counter???
            }

            break;

        case NRF_DRV_RTC_INT_COMPARE1:
            NRF_LOG_ERROR("NRF_DRV_RTC_INT_COMPARE1");
            break;

        case NRF_DRV_RTC_INT_TICK:
            NRF_LOG_ERROR("NRF_DRV_RTC_INT_TICK");
            break;

        case NRF_DRV_RTC_INT_OVERFLOW:
            // NRF_LOG_INFO("NRF_DRV_RTC_INT_OVERFLOW");
            m_rtc_overflow_counter++;
            // m_rtc_counter_monotonic += (uint64_t)(1 << 24) + nrf_drv_rtc_counter_get(&rtc);
            break;

        default:
            NRF_LOG_ERROR("NRF_DRV_RTC_INT_UNKNOWN: %u", int_type);
            break;
    }

}

void
rtc_m_init(void) {

    int err_code;

    if (m_rtc_m_initialized) {
        return;
    }

    // m_rtc_counter_monotonic = 0;
    m_rtc_overflow_counter = 0;

    nrf_drv_rtc_config_t rtc_config = NRF_DRV_RTC_DEFAULT_CONFIG;
    // rtc_config.prescaler = 0;
    err_code = nrf_drv_rtc_init(&rtc, &rtc_config, rtc_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_rtc_overflow_enable(&rtc,true);

    nrf_drv_rtc_enable(&rtc);

    m_rtc_m_initialized = 1;
}

void
rtc_m_deinit() {

    if (!m_rtc_m_initialized) {
        return;
    }

    rtc_m_register_timer_event_callback(NULL);

    // nrf_drv_rtc_disable(&rtc);
    nrf_drv_rtc_uninit(&rtc);

    m_rtc_m_initialized = 0;
}

void
rtc_m_timer_set(uint32_t timestamp) {

    int err_code;

    // nrf_drv_timer_us_to_ticks

    timestamp &= rtc_m_max_tick_value();

    err_code = nrf_drv_rtc_cc_set(&rtc, 0, timestamp, true);
    APP_ERROR_CHECK(err_code);
}

void
rtc_m_register_timer_event_callback(void (*p_rtc_m_event_callback)(uint32_t expired_timestamp)) {
    m_rtc_evt_handler = p_rtc_m_event_callback;
}

uint32_t
rtc_m_get_timestamp_capture(void) {
    // [INFO] no timestamp capture available with rtc
    return rtc_m_get_timestamp_current();
}

uint32_t
rtc_m_get_timestamp_current(void) {
    return nrf_drv_rtc_counter_get(&rtc);
}

uint64_t
rtc_m_get_time_us() {

    uint64_t rtc_counter_monotonic;
    uint64_t timestamp_us;

    rtc_counter_monotonic = ((uint64_t)(m_rtc_overflow_counter << 24) + nrf_drv_rtc_counter_get(&rtc));

    /* calc monotonic counter to timestamp in us (increment by using hours) */
    timestamp_us  =  ( rtc_counter_monotonic / (RTC_DEFAULT_CONFIG_FREQUENCY*60*60) ) * (1000000ULL*60*60);                         /* integer part hours in us */
    timestamp_us += (( rtc_counter_monotonic % (RTC_DEFAULT_CONFIG_FREQUENCY*60*60) ) * 1000000ULL) / RTC_DEFAULT_CONFIG_FREQUENCY; /* decimal part hours in us */
    // NRF_LOG_INFO(" %d us | %d ms | %d s | %d h", (uint32_t)timestamp_us, (uint32_t)(timestamp_us/1000), (uint32_t)(timestamp_us/1000/1000), (uint32_t)(timestamp_us/1000/1000/60/60) );

    return timestamp_us;
}

uint64_t
rtc_m_get_time_ms() {
    return (rtc_m_get_time_us() / 1000UL);
}

uint32_t
rtc_m_max_tick_value(void) {
    return nrf_drv_rtc_max_ticks_get(&rtc);
}

uint32_t
rtc_m_ticks_diff_compute(uint32_t ticks_to, uint32_t ticks_from) {
    return ((ticks_to - ticks_from) & rtc_m_max_tick_value());
}

uint32_t
rtc_m_us_to_ticks(uint32_t time_us) {
    return RTC_US_TO_TICKS((uint64_t)time_us, RTC_DEFAULT_CONFIG_FREQUENCY);
}

uint32_t
rtc_m_ticks_to_us(uint32_t ticks) {

    uint32_t time_us;

    time_us = (uint32_t) (ticks * (1000000.0 / RTC_DEFAULT_CONFIG_FREQUENCY) );

    return time_us;
}
