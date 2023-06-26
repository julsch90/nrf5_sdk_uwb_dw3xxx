#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>\n

#define NRF_LOG_ENABLED 1
#define NRF_LOG_DEFERRED 0
#define NRF_LOG_BACKEND_RTT_ENABLED 1

#define NRF_CLI_ENABLED 1
#define NRF_CLI_LOG_BACKEND 0

// <o> NRF_LOG_DEFAULT_LEVEL  - Default Severity level
// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug
#define NRF_LOG_DEFAULT_LEVEL 3

#define NRF_FPRINTF_FLAG_AUTOMATIC_CR_ON_LF_ENABLED 1

#define POWER_ENABLED 1

#define RTC_ENABLED 1
#define RTC2_ENABLED 1

#define TIMER_ENABLED 1
#define TIMER1_ENABLED 1
#define TIMER2_ENABLED 1
#define TIMER3_ENABLED 1

#define NRF_LIBUARTE_ASYNC_WITH_APP_TIMER 0

#define SPI_ENABLED 1
#define SPI0_ENABLED 0
#define SPI1_ENABLED 1
#define SPI1_USE_EASY_DMA 1
#define SPI2_ENABLED 1          // icm
#define SPI2_USE_EASY_DMA 1     // icm
#define SPI3_ENABLED 1
#define SPI3_USE_EASY_DMA 1

#define NRF_TWI_MNGR_ENABLED 1
// #define NRF_TWI_SENSOR_ENABLED 1
#define TWI_ENABLED 1
// #define TWI_DEFAULT_CONFIG_CLR_BUS_INIT 0
// #define TWI_DEFAULT_CONFIG_HOLD_BUS_UNINIT 0
#define TWI0_ENABLED 1
#define TWI0_USE_EASY_DMA 1

#define SEGGER_RTT_CONFIG_BUFFER_SIZE_DOWN 128

#define PWM_AUDIO_CONFIG_USE_SCHEDULER 1



// #define SAADC_ENABLED 0


#endif //APP_CONFIG_H
