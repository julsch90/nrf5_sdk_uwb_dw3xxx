/**
 * @file rtc_m.h
 * @author julian schmidt (julian.schmidt@nc-systems.de)
 * @brief
 * @version 0.1
 * @date 2024-05-13
 *
 * @copyright Copyright (c) 2022
 *
 */


#ifndef __RTC_M_H
#define __RTC_M_H

#ifdef __cplusplus
extern "C" {
#endif

void rtc_m_init(void);
void rtc_m_deinit(void);

void rtc_m_timer_set(uint32_t timestamp);

void rtc_m_register_timer_event_callback(void (*p_rtc_m_event_callback)(uint32_t expired_timestamp));

uint32_t rtc_m_get_timestamp_capture(void);
uint32_t rtc_m_get_timestamp_current(void);
uint64_t rtc_m_get_time_us();
uint64_t rtc_m_get_time_ms();

uint32_t rtc_m_max_tick_value(void);

uint32_t rtc_m_ticks_diff_compute(uint32_t ticks_to, uint32_t ticks_from);
uint32_t rtc_m_us_to_ticks(uint32_t time_us);
uint32_t rtc_m_ticks_to_us(uint32_t ticks);






#ifdef __cplusplus
}
#endif

#endif /* __RTC_M_H */
