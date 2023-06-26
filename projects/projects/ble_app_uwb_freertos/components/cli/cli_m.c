#include "cli_m.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

#include "nrf.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "app_timer.h"
#include "fds.h"
#include "app_error.h"
#include "app_util.h"

#include "nrf_cli.h"
#include "nrf_cli_rtt.h"
#include "nrf_cli_types.h"

#include "boards.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_log_backend_flash.h"
#include "nrf_fstorage_nvmc.h"

// #include "nrf_mpu_lib.h"
// #include "nrf_stack_guard.h"

#include "FreeRTOS.h"
#include "task.h"


#define CLI_TASK_ENABLED 1

#if CLI_TASK_ENABLED
static TaskHandle_t m_cli_task_tandle;        /**< CLI stack thread. */
#endif /* CLI_TASK_ENABLED */


// #if defined(APP_USBD_ENABLED) && APP_USBD_ENABLED
// #define CLI_OVER_USB_CDC_ACM 1
// #else
#define CLI_OVER_USB_CDC_ACM 0
// #endif

#if CLI_OVER_USB_CDC_ACM
#include "nrf_cli_cdc_acm.h"
#include "nrf_drv_usbd.h"
#include "app_usbd_core.h"
#include "app_usbd.h"
#include "app_usbd_string_desc.h"
#include "app_usbd_cdc_acm.h"
#endif //CLI_OVER_USB_CDC_ACM

// #if defined(TX_PIN_NUMBER) && defined(RX_PIN_NUMBER)
// #define CLI_OVER_UART 1
// #else
#define CLI_OVER_UART 0
// #endif

#if CLI_OVER_UART
#include "nrf_cli_uart.h"
#endif


/* If enabled then CYCCNT (high resolution) timestamp is used for the logger. */
// #define USE_CYCCNT_TIMESTAMP_FOR_LOG 0


#if NRF_LOG_BACKEND_FLASHLOG_ENABLED
NRF_LOG_BACKEND_FLASHLOG_DEF(m_flash_log_backend);
#endif

#if NRF_LOG_BACKEND_CRASHLOG_ENABLED
NRF_LOG_BACKEND_CRASHLOG_DEF(m_crash_log_backend);
#endif


#if CLI_OVER_USB_CDC_ACM

/**
 * @brief Enable power USB detection
 *
 * Configure if example supports USB port connection
 */
#ifndef USBD_POWER_DETECTION
#define USBD_POWER_DETECTION true
#endif

// static TaskHandle_t m_usbd_thread;        /**< USB stack thread. */

// static bool m_usb_isr_event = false;


static void
usbd_user_ev_handler(app_usbd_event_type_t event) {

    switch (event)
    {
        // case APP_USBD_EVT_DRV_SUSPEND:
        //     break;
        // case APP_USBD_EVT_DRV_RESUME:
        //     break;
        // case APP_USBD_EVT_STARTED:
        //     break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            NRF_LOG_INFO("USB power detected");
            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            NRF_LOG_INFO("USB power removed");
            // m_usb_connected = false;
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            NRF_LOG_INFO("USB ready");
            // m_usb_connected = true;
            app_usbd_start();
            break;
        default:
            break;
    }
}

#endif //CLI_OVER_USB_CDC_ACM

/**
 * @brief Command line interface instance
 * */
#define CLI_EXAMPLE_LOG_QUEUE_SIZE  (4)

#if CLI_OVER_USB_CDC_ACM
NRF_CLI_CDC_ACM_DEF(m_cli_cdc_acm_transport);
NRF_CLI_DEF(m_cli_cdc_acm,
            "usb_cli:~$ ",
            &m_cli_cdc_acm_transport.transport,
            '\r',
            CLI_EXAMPLE_LOG_QUEUE_SIZE);
#endif //CLI_OVER_USB_CDC_ACM

#if CLI_OVER_UART
NRF_CLI_UART_DEF(m_cli_uart_transport, 0, 64, 16);
NRF_CLI_DEF(m_cli_uart,
            "uart_cli:~$ ",
            &m_cli_uart_transport.transport,
            '\r',
            CLI_EXAMPLE_LOG_QUEUE_SIZE);
#endif

NRF_CLI_RTT_DEF(m_cli_rtt_transport);
NRF_CLI_DEF(m_cli_rtt,
            "rtt_cli:~$ ",
            &m_cli_rtt_transport.transport,
            '\n',
            CLI_EXAMPLE_LOG_QUEUE_SIZE);

static void
cli_start(void) {

    ret_code_t ret;

#if CLI_OVER_USB_CDC_ACM
    ret = nrf_cli_start(&m_cli_cdc_acm);
    APP_ERROR_CHECK(ret);
#endif

#if CLI_OVER_UART
    ret = nrf_cli_start(&m_cli_uart);
    APP_ERROR_CHECK(ret);
#endif

    ret = nrf_cli_start(&m_cli_rtt);
    APP_ERROR_CHECK(ret);
}

static void
cli_init(void) {

    ret_code_t ret;

#if CLI_OVER_USB_CDC_ACM
    ret = nrf_cli_init(&m_cli_cdc_acm, NULL, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
#endif

#if CLI_OVER_UART
    nrf_drv_uart_config_t uart_config = NRF_DRV_UART_DEFAULT_CONFIG;
    uart_config.pseltxd = TX_PIN_NUMBER;
    uart_config.pselrxd = RX_PIN_NUMBER;
    uart_config.hwfc    = NRF_UART_HWFC_DISABLED;
    ret = nrf_cli_init(&m_cli_uart, &uart_config, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
#endif

    ret = nrf_cli_init(&m_cli_rtt, NULL, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
}


#if CLI_OVER_USB_CDC_ACM
void
usb_new_event_isr_handler(app_usbd_internal_evt_t const * const p_event, bool queued) {

    // BaseType_t higher_priority_task_woken = pdFALSE;
    UNUSED_PARAMETER(p_event);
    UNUSED_PARAMETER(queued);

    // m_usb_isr_event = true;

    // ASSERT(m_usbd_thread != NULL);
    // /* Release the semaphore */
    // vTaskNotifyGiveFromISR(m_usbd_thread, &higher_priority_task_woken);
    // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void
usbd_init(void) {

    ret_code_t ret;
    static const app_usbd_config_t usbd_config = {
        // .ev_handler = app_usbd_event_execute,
        .ev_isr_handler = usb_new_event_isr_handler,
        .ev_state_proc = usbd_user_ev_handler
    };
    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    app_usbd_class_inst_t const * class_cdc_acm =
            app_usbd_cdc_acm_class_inst_get(&nrf_cli_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    if (USBD_POWER_DETECTION)
    {
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    }
    else
    {
        NRF_LOG_INFO("No USB power detection enabled\nStarting USB now");

        app_usbd_enable();
        app_usbd_start();
    }

    /* Give some time for the host to enumerate and connect to the USB CDC port */
    nrf_delay_ms(1000);
}
#endif


static void
cli_process(void) {

#if CLI_OVER_USB_CDC_ACM
    nrf_cli_process(&m_cli_cdc_acm);
#endif

#if CLI_OVER_UART
    nrf_cli_process(&m_cli_uart);
#endif

    nrf_cli_process(&m_cli_rtt);
}


void
cli_m_process(void) {

#if !CLI_TASK_ENABLED
#if CLI_OVER_USB_CDC_ACM && APP_USBD_CONFIG_EVENT_QUEUE_ENABLE
    while (app_usbd_event_queue_process())
    {
        /* Nothing to do */
    }
#endif
    cli_process();
#endif /* !CLI_TASK_ENABLED */

}


#if CLI_TASK_ENABLED
static void
cli_task(void *arg) {

    cli_init();
#if CLI_OVER_USB_CDC_ACM
    usbd_init();
#endif
    cli_start();


    for ( ;; )
    {

#if CLI_OVER_USB_CDC_ACM && APP_USBD_CONFIG_EVENT_QUEUE_ENABLE
        while (app_usbd_event_queue_process())
        {
            /* Nothing to do */
        }
#endif

        cli_process();

        UNUSED_RETURN_VALUE(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)));
    }

}
#endif /* CLI_TASK_ENABLED */


void
cli_m_init(void) {

#if CLI_TASK_ENABLED
    if (pdPASS != xTaskCreate(cli_task, "CLI", 512, NULL, tskIDLE_PRIORITY, &m_cli_task_tandle)) {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
#else /* CLI_TASK_ENABLED */
    cli_init();
#if CLI_OVER_USB_CDC_ACM
    usbd_init();
#endif
    cli_start();
#endif /* !CLI_TASK_ENABLED */
}



