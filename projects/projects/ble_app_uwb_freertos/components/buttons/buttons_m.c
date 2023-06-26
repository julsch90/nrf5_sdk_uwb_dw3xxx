#include "buttons_m.h"

#include "boards.h"
#include "nrf_log.h"
#include "app_button.h"
#include "app_timer.h"
#include "app_error.h"
#include "bsp.h"

#include "FreeRTOS.h"
#include "task.h"


#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50) /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */
#define BUTTON_STATE_POLL_INTERVAL_MS   200UL
#define BUTTON_LONG_PRESS_TIME_MS       2000UL
#define BUTTON_LONG_PRESS(MS)           (uint32_t)( (MS) / (BUTTON_STATE_POLL_INTERVAL_MS) )

#define BUTTON_VIBRATION_TIME_MS        60UL

extern void bsp_event_handler(bsp_event_t event);



APP_TIMER_DEF(m_button_vibration_tmr);
static void button_vibration_timer_handler(void * p_context);

static uint32_t m_button_vibration_is_running;

static void
button_vibration_timer_handler(void * p_context) {

#ifdef __BOARD_NCSW01_DWM3001C_H__
    nrf_gpio_pin_clear(MOTOR_VIBRATOR_PIN);
#endif /* __BOARD_NCSW01_DWM3001C_H__ */

    m_button_vibration_is_running = false;
}



#if (BUTTONS_NUMBER > 0)
#ifndef BSP_SIMPLE
// static bsp_event_callback_t   m_registered_callback         = NULL;
// static bsp_button_event_cfg_t m_events_list[BUTTONS_NUMBER] = {{BSP_EVENT_NOTHING, BSP_EVENT_NOTHING}}; // [DONE]
APP_TIMER_DEF(m_button_tmr);
static void button_event_handler(uint8_t pin_no, uint8_t button_action);
static void button_timer_handler(void * p_context);
#endif /* BSP_SIMPLE */

// TODO: WHAT ABOUT IF BUTTONS_ACTIVE_STATE DIFFER ON EACH BUTTON?
#ifndef BSP_SIMPLE
static const app_button_cfg_t buttons[BUTTONS_NUMBER] =
{
    #ifdef BSP_BUTTON_0
    {BSP_BUTTON_0, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_0

    #ifdef BSP_BUTTON_1
    {BSP_BUTTON_1, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_1

    #ifdef BSP_BUTTON_2
    {BSP_BUTTON_2, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_2

    #ifdef BSP_BUTTON_3
    {BSP_BUTTON_3, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_3

    #ifdef BSP_BUTTON_4
    {BSP_BUTTON_4, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_4

    #ifdef BSP_BUTTON_5
    {BSP_BUTTON_5, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_5

    #ifdef BSP_BUTTON_6
    {BSP_BUTTON_6, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_6

    #ifdef BSP_BUTTON_7
    {BSP_BUTTON_7, BUTTONS_ACTIVE_STATE, BUTTON_PULL, button_event_handler},
    #endif // BUTTON_7

};

static bsp_button_event_cfg_t m_events_list[BUTTONS_NUMBER] =
{
    #ifdef BSP_BUTTON_0
    {BSP_EVENT_KEY_0, BSP_EVENT_KEY_1},
    #endif // BUTTON_0

    #ifdef BSP_BUTTON_1
    {BSP_EVENT_NOTHING, BSP_EVENT_NOTHING},
    #endif // BUTTON_1

    #ifdef BSP_BUTTON_2
    {BSP_EVENT_NOTHING, BSP_EVENT_NOTHING},
    #endif // BUTTON_2

    #ifdef BSP_BUTTON_3
    {BSP_EVENT_NOTHING, BSP_EVENT_NOTHING},
    #endif // BUTTON_3

};
#endif /* BSP_SIMPLE */
#endif /* (BUTTONS_NUMBER > 0) */


// #if (BUTTONS_NUMBER > 0)
// bool button_is_pressed(uint32_t button)
// {
//     if (button < BUTTONS_NUMBER)
//     {
//         return bsp_board_button_state_get(button);
//     }
//     else
//     {
//         //If button is not present always return false
//         return false;
//     }
// }
// #endif /* (BUTTONS_NUMBER > 0) */



#if (BUTTONS_NUMBER > 0) && !(defined BSP_SIMPLE)

static void
button_event_handler(uint8_t pin_no, uint8_t button_action) {

    // bsp_event_t        event  = BSP_EVENT_NOTHING;
    uint32_t           button = 0;
    uint32_t           err_code;
    // static uint8_t     current_long_push_pin_no;              /**< Pin number of a currently pushed button, that could become a long push if held long enough. */
    static uint8_t     current_long_push_button;              /**< Pin number of a currently pushed button, that could become a long push if held long enough. */
    // static bsp_event_t release_event_at_push[BUTTONS_NUMBER]; /**< Array of what the release event of each button was last time it was pushed, so that no release event is sent if the event was bound after the push of the button. */

    button = bsp_board_pin_to_button_idx(pin_no);

    // NRF_LOG_INFO("button: %i, pin_no: %i, button_action: %i", button, pin_no, button_action);

    if (button < BUTTONS_NUMBER)
    {
        switch (button_action)
        {
            case APP_BUTTON_PUSH:
                // event = m_events_list[button].push_event;
                // if (m_events_list[button].long_push_event != BSP_EVENT_NOTHING)
                // {
                    err_code = app_timer_start(m_button_tmr, APP_TIMER_TICKS(BUTTON_STATE_POLL_INTERVAL_MS), (void*)&current_long_push_button);
                    if (err_code == NRF_SUCCESS)
                    {
                        current_long_push_button = button;
                        // current_long_push_button = pin_no;

                        if ( !m_button_vibration_is_running ) {
                            err_code = app_timer_start(m_button_vibration_tmr, APP_TIMER_TICKS(BUTTON_VIBRATION_TIME_MS), NULL);
                            if (err_code == NRF_SUCCESS) {

#ifdef __BOARD_NCSW01_DWM3001C_H__
                                nrf_gpio_pin_set(MOTOR_VIBRATOR_PIN);
#endif /* __BOARD_NCSW01_DWM3001C_H__ */

                                m_button_vibration_is_running = true;
                            }
                        }

                    }
                // }
                // release_event_at_push[button] = m_events_list[button].release_event;
                break;

            // TODO: USE TIMESTAMP ? ()
            // case APP_BUTTON_RELEASE:
            //     (void)app_timer_stop(m_button_tmr);
            //     button_timer_handler((void*)&current_long_push_button); // TODO: NOT A GOOD IDEA (called short press again)
            //     // if (release_event_at_push[button] == m_events_list[button].release_event)
            //     // {
            //     //     event = m_events_list[button].release_event;
            //     // }
            //     break;
            // case BSP_BUTTON_ACTION_LONG_PUSH:
            //     event = m_events_list[button].long_push_event;
        }
    }

    // if ((event != BSP_EVENT_NOTHING) && (m_registered_callback != NULL))
    // {
    //     m_registered_callback(event);
    // }


    // if ((pin_no == BUTTON_1) && (button_action == APP_BUTTON_PUSH)) {

    //     err_code = app_timer_start(m_button_tmr, APP_TIMER_TICKS(BUTTON_STATE_POLL_INTERVAL_MS), NULL);
    //     APP_ERROR_CHECK(err_code);
    // }


    // TODO: MAYBE HANDLE LIKE THIS:
    // if (button_action == APP_BUTTON_PUSH)
    // {
    //     switch (pin_no)
    //     {
    //         case BSP_BUTTON_0:
    //         {
    //             if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
    //             {
    //                 err_code = sd_ble_gap_connect(&m_peer_addr,
    //                                               &m_scan_param,
    //                                               &m_connection_param,
    //                                               APP_IPSP_TAG);

    //                 if (err_code != NRF_SUCCESS)
    //                 {
    //                     APPL_LOG("Connection Request Failed, reason 0x%08lX", err_code);
    //                 }
    //                 APP_ERROR_CHECK(err_code);
    //             }
    //             else
    //             {
    //                  APPL_LOG("Connection exists with peer");
    //             }

    //             break;
    //         }
    //         case BSP_BUTTON_1:
    //         {
    //             if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    //             {
    //                 ble_ipsp_handle_t ipsp_handle;
    //                 ipsp_handle.conn_handle = m_conn_handle;
    //                 err_code = ble_ipsp_connect(&ipsp_handle);
    //                 APP_ERROR_CHECK_BOOL((err_code == NRF_SUCCESS) ||
    //                                      (err_code == NRF_ERROR_BLE_IPSP_CHANNEL_ALREADY_EXISTS));
    //             }
    //             else
    //             {
    //                  APPL_LOG("No physical link exists with peer");
    //             }
    //             break;
    //         }
    //         case BSP_BUTTON_2:
    //         {
    //             err_code = ble_ipsp_disconnect(&m_handle);
    //             APPL_LOG("ble_ipsp_disconnect result %08lX", err_code);
    //             break;
    //         }
    //         case BSP_BUTTON_3:
    //         {
    //             if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    //             {
    //                 err_code = sd_ble_gap_disconnect(m_conn_handle, 0x13);
    //                 APPL_LOG("sd_ble_gap_disconnect result %08lX", err_code);
    //             }
    //             else
    //             {
    //                 APPL_LOG("No physical link exists with peer");
    //             }
    //             break;
    //         }
    //         default:
    //             break;
    //     }
    // }
}

static void
button_timer_handler(void * p_context) {

    uint32_t err_code;
    static uint32_t cnt;
    uint8_t button;

    button = *(uint8_t*)p_context;

    // NRF_LOG_INFO("button_timer_handler: button: %i (%i)", button, app_button_is_pushed(button) );

    if (app_button_is_pushed(button)) {

        cnt++;
        if ( cnt >= BUTTON_LONG_PRESS(BUTTON_LONG_PRESS_TIME_MS) ) {
            cnt = 0;
            // NRF_LOG_INFO("Long Button press");
            // bsp_event_handler(BSP_EVENT_DFU);    // TODO: ADD OWN EVENT DEFINES:  BSP_EVENT_KEY_0_SHORT_PRESS, BSP_EVENT_KEY_0_LONG_PRESS, BSP_EVENT_KEY_0_2X_PRESS, BSP_EVENT_KEY_0_4X_PRESS
            bsp_event_handler(m_events_list[button].long_push_event);

            if ( !m_button_vibration_is_running ) {
                err_code = app_timer_start(m_button_vibration_tmr, APP_TIMER_TICKS(BUTTON_VIBRATION_TIME_MS), NULL);
                if (err_code == NRF_SUCCESS) {

#ifdef __BOARD_NCSW01_DWM3001C_H__
                    nrf_gpio_pin_set(MOTOR_VIBRATOR_PIN);
#endif /* __BOARD_NCSW01_DWM3001C_H__ */

                    m_button_vibration_is_running = true;
                }
            }


        } else {
            err_code = app_timer_start(m_button_tmr, APP_TIMER_TICKS(BUTTON_STATE_POLL_INTERVAL_MS), p_context);
            APP_ERROR_CHECK(err_code);
        }

    } else {
        cnt = 0; // reset counter variable
        // NRF_LOG_INFO("Short button press");
        // bsp_event_handler(BSP_EVENT_KEY_0); // TODO: ADD OWN EVENT DEFINES:  BSP_EVENT_KEY_0_SHORT_PRESS, BSP_EVENT_KEY_0_LONG_PRESS, BSP_EVENT_KEY_0_2X_PRESS, BSP_EVENT_KEY_0_4X_PRESS
        bsp_event_handler(m_events_list[button].push_event);
    }
}
#endif /* (BUTTONS_NUMBER > 0) && !(defined BSP_SIMPLE) */


void
buttons_m_init(void) {

    uint32_t err_code;

    // TODO: check irq gpiote irq priority

    err_code = app_button_init(buttons, ARRAY_SIZE(buttons), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);

    /* enable an app timer instance to detect (long, extra long, multiple) button press */
    err_code = app_timer_create(&m_button_tmr, APP_TIMER_MODE_SINGLE_SHOT, button_timer_handler);
    APP_ERROR_CHECK(err_code);

    /* enable an app timer instance for vibration on button press/events */
    err_code = app_timer_create(&m_button_vibration_tmr, APP_TIMER_MODE_SINGLE_SHOT, button_vibration_timer_handler);
    APP_ERROR_CHECK(err_code);
}