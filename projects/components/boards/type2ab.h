#ifndef __TYPE2AB_H__
#define __TYPE2AB_H__


#include "nrf_gpio.h"


#ifdef __cplusplus
extern "C" {
#endif /* end of __cplusplus */


/**
 * +---------------------------------------------+ *
 * |     Pins connected to DW3000                | *
 * +=============================================+ *
 * |  port&pin      |            Type            | *
 * +---------------------------------------------+ *
 * |  P0.20         |           SPI_CS           | *
 * +---------------------------------------------+ *
 * |  P0.16         |           SPI_CLK          | *
 * +---------------------------------------------+ *
 * |  P0.17         |           SPI_MOSI         | *
 * +---------------------------------------------+ *
 * |  P0.23         |           SPI_MISO         | *
 * +---------------------------------------------+ *
 * |  P0.15         |           RESET            | *
 * +---------------------------------------------+ *
 * |  P0.25         |           IRQ              | *
 * +---------------------------------------------+ *
 * |  P1.02         |           WAKEUP           | *
 * +---------------------------------------------+ *
 **/
#define DW3000_CS_Pin      NRF_GPIO_PIN_MAP(0, 20)
#define DW3000_CLK_Pin     NRF_GPIO_PIN_MAP(0, 16)
#define DW3000_MOSI_Pin    NRF_GPIO_PIN_MAP(0, 17)
#define DW3000_MISO_Pin    NRF_GPIO_PIN_MAP(0, 23)
#define DW3000_RESET_Pin   NRF_GPIO_PIN_MAP(0, 15)
#define DW3000_IRQn_Pin    NRF_GPIO_PIN_MAP(0, 25)
#define DW3000_WAKEUP_Pin  NRF_GPIO_PIN_MAP(1, 2)

/**
 * +---------------------------------------------+ *
 * |     Pins connected to LIS2DW12              | *
 * +=============================================+ *
 * |  port&pin      |            Type            | *
 * +---------------------------------------------+ *
 * |  P0.19         |           SCL/SPC          | *
 * +---------------------------------------------+ *
 * |  P0.22         |           SDA/SDI/SDO      | *
 * +---------------------------------------------+ *
 * |  P1.1          |           INT1             | *
 * +---------------------------------------------+ *
 * |  P1.4          |           INT2             | *
 * +---------------------------------------------+ *
 * |  P1.3          |           CS               | *
 * +---------------------------------------------+ *
 * |  P1.0          |           SDO/SA0          | *
 * +---------------------------------------------+ *
 **/
#define LIS2DW12_IIC_SCL     NRF_GPIO_PIN_MAP(0, 19)
#define LIS2DW12_IIC_SDA     NRF_GPIO_PIN_MAP(0, 22)
#define LIS2DW12_INT1        NRF_GPIO_PIN_MAP(1, 1)
#define LIS2DW12_INT2        NRF_GPIO_PIN_MAP(1, 4)
#define LIS2DW12_SPI_CS      NRF_GPIO_PIN_MAP(1, 3)
#define LIS2DW12_SPI_MISO    NRF_GPIO_PIN_MAP(1, 0)
#define LIS2DW12_SPI_SCLK    NRF_GPIO_PIN_MAP(0, 19)
#define LIS2DW12_SPI_MOSI    NRF_GPIO_PIN_MAP(0, 22)

/**
 * +---------------------------------------------+ *
 * |     Pins connected to crystal oscillator    | *
 * +=============================================+ *
 * |  port&pin      |            Type            | *
 * +---------------------------------------------+ *
 * |  XC1           |           32 MHz           | *
 * +---------------------------------------------+ *
 * |  XC2           |           32 MHz           | *
 * +---------------------------------------------+ *
 **/
#define EXTERNAL_32MHZ_CRYSTAL 1


#ifdef __cplusplus
}
#endif /* end of __cplusplus */


#endif /* end of __TYPE2AB_H__ */