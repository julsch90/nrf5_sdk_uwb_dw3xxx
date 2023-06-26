#ifndef __BOARD_DWM3001CDK_H__
#define __BOARD_DWM3001CDK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"
#include "dwm3001c.h"

// #define BOARD_DWM3001CDK
// #define BOARD_DWM3001C

#define LEDS_NUMBER             4

#define LED_1                   NRF_GPIO_PIN_MAP(0,5)  // D10 (blue) on the schematics
#define LED_2                   NRF_GPIO_PIN_MAP(0,4)  // D9 (green) on the schematics
#define LED_3                   NRF_GPIO_PIN_MAP(0,22) // D11 (red) on the schematics
#define LED_4                   NRF_GPIO_PIN_MAP(0,14) // D12 (red) on the schematics

#define LED_RED                 LED_3
#define LED_GREEN               LED_1
#define LED_BLUE                LED_2

#define LED_START               LED_1
#define LED_STOP                LED_4
#define LED_ERROR               LED_RED

#define LEDS_ACTIVE_STATE       0

#define LEDS_LIST               { LED_1, LED_2, LED_3, LED_4 }

#define LEDS_INV_MASK           LEDS_MASK

#define BSP_LED_0               LED_1
#define BSP_LED_1               LED_2
#define BSP_LED_2               LED_3
#define BSP_LED_3               LED_4

#define BSP_LED_RED             LED_RED
#define BSP_LED_GREEN           LED_GREEN
#define BSP_LED_BLUE            LED_BLUE

#define BSP_LED_ERROR           LED_RED


#define BUTTONS_NUMBER          1

#define BUTTON_1                NRF_GPIO_PIN_MAP(0,2)
#define BUTTON_PULL             NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE    0

#define BUTTONS_LIST            { BUTTON_1 }

#define BSP_BUTTON_0            BUTTON_1

#define RX_PIN_NUMBER           15
#define TX_PIN_NUMBER           19
#define CTS_PIN_NUMBER          (-1)
#define RTS_PIN_NUMBER          (-1)
#define HWFC                    false

// // Arduino board mappings
// #define ARDUINO_13_PIN          NRF_GPIO_PIN_MAP(0,  3)  // used as DW3000_CLK_Pin
// #define ARDUINO_12_PIN          NRF_GPIO_PIN_MAP(0, 29)  // used as DW3000_MISO_Pin
// #define ARDUINO_11_PIN          NRF_GPIO_PIN_MAP(0,  8)  // used as DW3000_MOSI_Pin
// #define ARDUINO_10_PIN          NRF_GPIO_PIN_MAP(1,  6)  // used as DW3000_CS_Pin
// #define ARDUINO_9_PIN           NRF_GPIO_PIN_MAP(1, 19)  // used as DW3000_WKUP_Pin
// #define ARDUINO_8_PIN           NRF_GPIO_PIN_MAP(1,  2)  // used as DW3000_IRQn_Pin
// #define ARDUINO_7_PIN           NRF_GPIO_PIN_MAP(0, 25)  // used as DW3000_RESET_Pin

// #define DW3000_RESET_Pin        ARDUINO_7_PIN
// #define DW3000_IRQn_Pin         ARDUINO_8_PIN
// // #define DW3000_WAKEUP_Pin       ARDUINO_9_PIN           // not available at this board

// // SPI defs
// #define DW3000_CS_Pin           ARDUINO_10_PIN
// #define DW3000_CLK_Pin          ARDUINO_13_PIN          // DWM3000 shield SPIM1 sck connected to DW1000
// #define DW3000_MOSI_Pin         ARDUINO_11_PIN          // DWM3000 shield SPIM1 mosi connected to DW1000
// #define DW3000_MISO_Pin         ARDUINO_12_PIN          // DWM3000 shield SPIM1 miso connected to DW1000
// #define DW3000_SPI_IRQ_PRIORITY APP_IRQ_PRIORITY_LOW    //

// UART symbolic constants
#define UART_0_TX_PIN           TX_PIN_NUMBER           // DWM1001 module pin 20, DEV board name RXD
#define UART_0_RX_PIN           RX_PIN_NUMBER           // DWM1001 module pin 18, DEV board name TXD
#define DW3000_RTS_PIN_NUM      UART_PIN_DISCONNECTED
#define DW3000_CTS_PIN_NUM      UART_PIN_DISCONNECTED

// #define LED_ERROR               BSP_LED_0
// #define DW3000_MAX_SPI_FREQ     NRF_SPIM_FREQ_32M

// #define I2C_SCL_PIN             NRF_GPIO_PIN_MAP(1, 4)
// #define I2C_SDA_PIN             NRF_GPIO_PIN_MAP(0, 24)

// #define LIS2DH12_SCL_PIN        NRF_GPIO_PIN_MAP(1, 4)
// #define LIS2DH12_SDA_PIN        NRF_GPIO_PIN_MAP(0, 24)
// #define LIS2DH12_INT1_PIN       NRF_GPIO_PIN_MAP(0, 16)

// #define ACC_IRQ                 LIS2DH12_INT1


#ifdef __cplusplus
}
#endif

#endif /* end of __BOARD_DWM3001CDK_H__ */
