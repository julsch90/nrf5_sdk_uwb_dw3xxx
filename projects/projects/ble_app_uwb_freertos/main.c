#include "main.h"

#include "nordic_common.h"
#include "boards.h"
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_drv_power.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_rtc.h"
#include "nrf_drv_ppi.h"
#include "app_scheduler.h"
#include "app_error.h"
#include "app_timer.h"

/* freertos */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* components / modules */
#include "log_m.h"
#include "clock_m.h"
#include "bsp_m.h"
#include "ble_m.h"
#include "wdt_m.h"
#include "usb_m.h"
#include "cli_m.h"


// Scheduler settings
#define SCHED_MAX_EVENT_DATA_SIZE   sizeof(APP_TIMER_SCHED_EVENT_DATA_SIZE)
#define SCHED_QUEUE_SIZE            10

#define DEAD_BEEF   0xDEADBEEF              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

extern void app_main();

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void
assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name) {
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/* Task to be created. */
void
main_task( void * pvParameters ) {

    /* The parameter value is expected to be 1 as 1 is passed in the
    pvParameters value in the call to xTaskCreate() below. */
    configASSERT( ( ( uint32_t ) pvParameters ) == 1 );

    NRF_LOG_INFO("main_task started.");

#if USB_M_ENABLED
    usb_m_init();
#endif

#if CLI_M_ENABLED
    cli_m_init();
#endif


    vTaskDelay(10);

    for( ;; ) {


        app_main();

    }

    vTaskDelete( NULL );
}



/**
 * @brief Function for application main entry.
 *
 */
int
main(void) {

    // int err_code;

    /* initialize modules */
    clock_m_init();
    nrf_drv_power_init(NULL);
    log_m_init();

    /* activate deep sleep mode. */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    // TODO: move to where is needed (!nrf_drv_gpiote_is_init())
    // err_code = nrf_drv_ppi_init();
    // APP_ERROR_CHECK(err_code);

    /* init app timer */
    app_timer_init();

#if WDT_M_ENABLED
    wdt_m_init();
#endif /* WDT_M_ENABLED */

    /* configure board. */
    bsp_m_init();

    /* app scheduler init */
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);

    /* initialize ble */
    // ble_m_init();

    /* create main task */
    if (pdPASS != xTaskCreate( main_task, "main", 1024, ( void * ) 1, tskIDLE_PRIORITY, NULL )) {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }

    // Do not start any interrupt that uses system functions before system initialisation.
    // The best solution is to start the OS before any other initalisation.

    // Start FreeRTOS scheduler.
    vTaskStartScheduler();

    for ( ;; ) {
        APP_ERROR_HANDLER(NRF_ERROR_FORBIDDEN);
    }
}



// To Test Low power mode - Set configUSE_IDLE_HOOK as '1' in FreeRTOSConfig.h
void
vApplicationIdleHook( void ) {

#if USB_M_ENABLED
    usb_m_process();
#endif

#if CLI_M_ENABLED
    cli_m_process();
#endif

#if NRF_MODULE_ENABLED(APP_SCHEDULER)
    app_sched_execute();
#endif

    if ( xPortGetFreeHeapSize() <= 2048 ) {
        NRF_LOG_WARNING("free heap: %i (min: %i)", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize() );
    }
}

void
vApplicationStackOverflowHook( TaskHandle_t xTask, signed char *pcTaskName ) {
    NRF_LOG_ERROR("STACK OVERFLOW: %s", pcTaskName);
}


void
Error_Handler(void) {
    // app_error_handler((NRF_ERROR_FORBIDDEN), __LINE__, (uint8_t*) __FILE__); // TODO
    APP_ERROR_HANDLER(NRF_ERROR_FORBIDDEN);
}


/**
 * @brief Assert callback.
 *
 * @param[in] id    Fault identifier. See @ref NRF_FAULT_IDS.
 * @param[in] pc    The program counter of the instruction that triggered the fault, or 0 if
 *                  unavailable.
 * @param[in] info  Optional additional information regarding the fault. Refer to each fault
 *                  identifier for details.
 */
void
app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info) {

    bsp_board_leds_off();

    NRF_LOG_ERROR("app_error_fault_handler(id=%i,pc=%i,info=%i)", id, pc, info);

    while (1);
}