#include "bsp_m.h"

#include "main.h"

#include "boards.h"
#include "nrf_sdh.h"
#include "nrf_bootloader_info.h"
#include "nrf_drv_power.h"
#include "nrf_drv_gpiote.h"
#include "nrf_log.h"
#include "app_button.h"
#include "app_timer.h"
#include "app_error.h"
#include "bsp.h"

#include "buttons_m.h"

#include "FreeRTOS.h"
#include "task.h"


#define configUSE_BSP_BUTTON    0
#define configUSE_BSP_LED       1

// TODO: MODE TO HEADER?
// TODO: ADD OWN EVENT DEFINES:  BSP_EVENT_BUTTON_0_SHORT_PRESS, BSP_EVENT_KEY_0_SHORT_PRESS, BSP_EVENT_KEY_0_LONG_PRESS, BSP_EVENT_KEY_0_2X_PRESS, BSP_EVENT_KEY_0_4X_PRESS
/* set bsp buttons to bsp events */
#define BSP_BOARD_BUTTON_0_SHORT_PUSH  BSP_EVENT_KEY_0
#define BSP_BOARD_BUTTON_0_LONG_PUSH   BSP_EVENT_KEY_1
#define BSP_BOARD_BUTTON_0_RELEASE     BSP_EVENT_KEY_2


// static bool m_erase_bonds;  /**< Bool to determine if bonds should be erased before scanning starts. Based on button push upon startup. */


/**
 * @brief Function for handling events from the BSP module.
 *
 * @param event
 */
void
bsp_event_handler(bsp_event_t event) {

    // uint32_t err_code;

    switch (event) {

        case BSP_EVENT_KEY_0:
            NRF_LOG_INFO("BSP_EVENT_KEY_0");

            break;

        case BSP_EVENT_KEY_1:
            NRF_LOG_INFO("BSP_EVENT_KEY_1");
            // NRF_LOG_INFO("button long press");
            break;

        case BSP_EVENT_DFU:
            NRF_LOG_INFO("BSP_EVENT_DFU");

            if (nrf_sdh_is_enabled()) {
                sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
                sd_nvic_SystemReset();
            } else {
                nrf_power_gpregret_set(BOOTLOADER_DFU_START);
                NVIC_SystemReset();
            }
            break;

        default:
            NRF_LOG_INFO("unknown event 0x%02X", event);
            break;
    }
}

void
bsp_m_init(void) {

    uint32_t err_code;
    uint32_t type = BSP_INIT_NONE;


    // TODO: check irq gpiote irq priority

    /* basic init */
    bsp_board_init(BSP_INIT_LEDS|BSP_INIT_BUTTONS);


#if (configUSE_BSP_BUTTON == 1)
    type |= BSP_INIT_BUTTONS;
#endif
#if (configUSE_BSP_LED == 1)
    type |= BSP_INIT_LEDS;
#endif

    err_code = bsp_init(type, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

#if (configUSE_BSP_BUTTON == 1)

    err_code = bsp_event_to_button_action_assign(BSP_BOARD_BUTTON_0, BSP_BUTTON_ACTION_PUSH, BSP_BOARD_BUTTON_0_SHORT_PUSH);
    APP_ERROR_CHECK(err_code);

    // err_code = bsp_event_to_button_action_assign(BSP_BOARD_BUTTON_0, BSP_BUTTON_ACTION_LONG_PUSH, BSP_BOARD_BUTTON_0_LONG_PUSH);
    // APP_ERROR_CHECK(err_code);

    // err_code = bsp_event_to_button_action_assign(BSP_BOARD_BUTTON_0, BSP_BUTTON_ACTION_RELEASE, BSP_BOARD_BUTTON_0_RELEASE);
    // APP_ERROR_CHECK(err_code);

    // TODO: NEED TO SET WHEN USING configUSE_BSP_BUTTON
    // /* enable an app timer instance to detect long button press */
    // err_code = app_timer_create(&m_button_action, APP_TIMER_MODE_SINGLE_SHOT, button_timeout_handler);
    // APP_ERROR_CHECK(err_code);
#endif

    // TODO: BONDS? necessary in project?
    // bsp_event_t startup_event;
    // err_code = bsp_btn_ble_init(NULL, &startup_event);
    // APP_ERROR_CHECK(err_code);
    // m_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);

#if (configUSE_BSP_BUTTON == 0)
    buttons_m_init();
#endif

}