#ifndef __BOARD_TYPE2AB_EVB_H__
#define __BOARD_TYPE2AB_EVB_H__


#include "nrf_gpio.h"
#include "type2ab.h"


#ifdef __cplusplus
extern "C" {
#endif /* end of __cplusplus */

// /* ENABLE_USB_PRINT Macro is uncommented then Segger RTT Print will be enabled*/
// #define ENABLE_USB_PRINT

// /* Enable IMU */
// #define CONFIG_IMU_ENABLE 1

/**
 * +---------------------------------------------+ *
 * |     LEDs definitions for Type2AB EVB        | *
 * +=============================================+ *
 * |  port&pin      |            Type            | *
 * +---------------------------------------------+ *
 * |  P0.28         |      LED_1/BSP_LED_0       | *
 * +---------------------------------------------+ *
 * |  P0.21         |      LED_2/BSP_LED_1       | *
 * +---------------------------------------------+ *
 **/
#define LEDS_NUMBER             2
#define LED_1                   NRF_GPIO_PIN_MAP(0, 28)
#define LED_2                   NRF_GPIO_PIN_MAP(0, 21)
#define LED_START               LED_1
#define LED_STOP                LED_2
#define BSP_LED_0               LED_1
#define BSP_LED_1               LED_2
#define LEDS_LIST               {LED_1, LED_2}
#define LEDS_INV_MASK           LEDS_MASK
#define LEDS_ACTIVE_STATE       1

/**
 * +---------------------------------------------+ *
 * |    BUTTONs definitions for Type2AB EVB      | *
 * +=============================================+ *
 * |  port&pin            |         Type         | *
 * +---------------------------------------------+ *
 * |  P0.30               |         BUTTON_1     | *
 * +---------------------------------------------+ *
 **/

#define BUTTONS_NUMBER          1
#define BUTTON_1                NRF_GPIO_PIN_MAP(0,30)
#define BUTTON_PULL             NRF_GPIO_PIN_PULLUP
#define BUTTONS_LIST            {BUTTON_1}
#define BUTTONS_ACTIVE_STATE    0
#define BSP_BUTTON_0            BUTTON_1

/**
 * +---------------------------------------------+ *
 * |    UART definitions for Type2AB EVB         | *
 * +=============================================+ *
 * |  port&pin      |            Type            | *
 * +---------------------------------------------+ *
 * |  P0.07         |           UART_TX          | *
 * +---------------------------------------------+ *
 * |  P0.08         |           UART_RX          | *
 * +---------------------------------------------+ *
 * |  disable       |           UART_RTS         | *
 * +---------------------------------------------+ *
 * |  disable       |           UART_CTS         | *
 * +---------------------------------------------+ *
 **/
#define UART_0_TX_PIN           NRF_GPIO_PIN_MAP(0, 7)
#define UART_0_RX_PIN           NRF_GPIO_PIN_MAP(0, 8)
#define CTS_PIN_NUMBER          UART_PIN_DISCONNECTED
#define RTS_PIN_NUMBER          UART_PIN_DISCONNECTED


#ifdef __cplusplus
}
#endif /* end of __cplusplus */


#endif /* end of __BOARD_TYPE2AB_EVB_H__ */