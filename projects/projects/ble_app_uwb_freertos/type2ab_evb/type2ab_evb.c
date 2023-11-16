// #include "app_error.h"
// #include "thisBoard.h"
// #include "boards.h"
// #include "nrf_delay.h"
// #include "nrf_drv_clock.h"
// #include "nrf_log_default_backends.h"
// #include "nrf_log_ctrl.h"
// #include "nrf_log.h"
// #include "HAL_rtc.h"
// #include "HAL_watchdog.h"
// #include "HAL_DW3000.h"
// #include "deca_interface.h"

// __WEAK void init_timer(void) {}

// /* @fn  peripherals_init
//  *
//  * @param[in] void
//  * */
// void peripherals_init(void)
// {
//     ret_code_t ret;
//     ret_code_t err_code;

//    /* With this change, Reset After Power Cycle is not required */
//     nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);

//     if(!nrf_drv_clock_init_check() )
//     {
//         err_code = nrf_drv_clock_init();
//         APP_ERROR_CHECK(err_code);
//     }

//     // Request HF clock for precise block timing
//     nrf_drv_clock_hfclk_request(NULL);
//     while (!nrf_drv_clock_hfclk_is_running())
//     {
//         // spin lock
//     }

//     nrf_drv_clock_lfclk_request(NULL);

//     init_timer();

//     /* Watchdog 60sec */
//     wdt_init(60000);
// }

// void BoardInit(void)
// {
//     bsp_board_init(BSP_INIT_LEDS|BSP_INIT_BUTTONS);
//     peripherals_init();

//     for(int i=0; i<6; i++)
//     {
//         bsp_board_led_invert(BSP_BOARD_LED_0);
//         bsp_board_led_invert(BSP_BOARD_LED_1);
//         nrf_delay_ms(250);
//     }
// }

// int UwbInit(void)
// {
//     port_init_dw_chip(3);

//     init_dw3000_irq();
//     disable_dw3000_irq();

//     reset_DW3000();

//     return dwt_probe((struct dwt_probe_s*)&dw3000_probe_interf);
// }