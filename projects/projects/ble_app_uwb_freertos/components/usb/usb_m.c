#include <stdint.h>
#include <string.h>

#include "main.h"

#include "usb_m.h"

#if USB_M_ENABLED

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
// #include "ble_hci.h"
// #include "ble_advdata.h"
// #include "ble_advertising.h"
// #include "ble_conn_params.h"
#include "nrf_sdh.h"
// #include "nrf_sdh_soc.h"
// #include "nrf_sdh_ble.h"
// #include "nrf_sdh_freertos.h"
// #include "nrf_ble_gatt.h"
#include "app_timer.h"
// #include "ble_nus.h"
// #include "app_uart.h"
#include "app_util_platform.h"
// #include "bsp_btn_ble.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_drv_usbd.h"
// #include "nrf_drv_clock.h"
// #include "nrf_gpio.h"
// #include "nrf_delay.h"
// #include "nrf_drv_power.h"

#include "app_error.h"
#include "app_util.h"
#include "app_usbd_core.h"
#include "app_usbd.h"
#include "app_usbd_string_desc.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"

/* FreeRTOS related */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// USB DEFINES START

/* option to use own usb task (or usb queue processed by idle task) */
#define USB_TASK_ENABLED        1

static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                    app_usbd_cdc_acm_user_event_t event);

#define CDC_ACM_COMM_INTERFACE  0
#define CDC_ACM_COMM_EPIN       NRF_DRV_USBD_EPIN2

#define CDC_ACM_DATA_INTERFACE  1
#define CDC_ACM_DATA_EPIN       NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT      NRF_DRV_USBD_EPOUT1

#define CDC_ACM_MAX_DATA_LEN    256


static uint8_t m_cdc_data_array[CDC_ACM_MAX_DATA_LEN];

/** @brief CDC_ACM class instance. */
APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                            cdc_acm_user_ev_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_AT_V250);

// USB DEFINES END

#if USB_TASK_ENABLED
static TaskHandle_t m_usb_task_tandle;        /**< USB stack thread. */
#endif /* USB_TASK_ENABLED */

static QueueHandle_t m_usb_evt_tx_done_queue_handle;




// USB CODE START
static bool m_usb_connected = false;
static bool m_usb_port_opened = false;
// static bool m_usb_tx_done = false;
// static bool m_usb_isr_event = false;


/** @brief User event handler @ref app_usbd_cdc_acm_user_ev_handler_t */
static void
cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst, app_usbd_cdc_acm_user_event_t event) {

    app_usbd_cdc_acm_t const * p_cdc_acm = app_usbd_cdc_acm_class_get(p_inst);
    BaseType_t higher_priority_task_woken = pdFALSE;
    void* d = (void *)1;

    switch (event)
    {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
        {
            /*Set up the first transfer*/
            ret_code_t ret = app_usbd_cdc_acm_read(&m_app_cdc_acm,
                                                   m_cdc_data_array,
                                                   1);
            UNUSED_VARIABLE(ret);

            // ret = app_timer_stop(m_blink_cdc);
            // APP_ERROR_CHECK(ret);
            // bsp_board_led_on(LED_CDC_ACM_CONN);

            NRF_LOG_INFO("CDC ACM port opened");
            m_usb_port_opened = true;
            break;
        }

        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            NRF_LOG_INFO("CDC ACM port closed");
            m_usb_port_opened = false;
            if (m_usb_connected)
            {
                // ret_code_t ret = app_timer_start(m_blink_cdc,
                //                                  APP_TIMER_TICKS(LED_BLINK_INTERVAL),
                //                                  (void *) LED_CDC_ACM_CONN);
                // APP_ERROR_CHECK(ret);
            }
            break;

        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            // NRF_LOG_INFO(" (%i) APP_USBD_CDC_ACM_USER_EVT_TX_DONE", xTaskGetTickCountFromISR());
            // m_usb_tx_done = true;

            if ( m_usb_evt_tx_done_queue_handle != NULL ) {
                xQueueSendFromISR( m_usb_evt_tx_done_queue_handle, &d, &higher_priority_task_woken );
                portYIELD_FROM_ISR( higher_priority_task_woken );
            }

            break;

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
        {
            ret_code_t ret;
            static uint8_t index = 0;
            index++;

            // size_t size = app_usbd_cdc_acm_bytes_stored(p_cdc_acm);
            // NRF_LOG_DEBUG("Bytes waiting: %d", size);

            do
            {
                /* get amount of data transferred */
                size_t size = app_usbd_cdc_acm_rx_size(p_cdc_acm);
                NRF_LOG_DEBUG("RX: size: %lu value: %02X", size, m_cdc_data_array[(uint8_t)(index - 1)]);

                /* GET RXed BYTE FROM USB */
                // -> m_cdc_data_array[(uint8_t)(index - 1)]

                /* fetch data until internal buffer is empty */
                ret = app_usbd_cdc_acm_read(&m_app_cdc_acm,
                                            &m_cdc_data_array[index],
                                            1);
                if (ret == NRF_SUCCESS)
                {
                    index++;
                }
            }
            while (ret == NRF_SUCCESS);

            break;
        }
        default:
            break;
    }
}


static void
usbd_user_ev_handler(app_usbd_event_type_t event) {

    switch (event)
    {
        case APP_USBD_EVT_DRV_SUSPEND:
            break;

        case APP_USBD_EVT_DRV_RESUME:
            break;

        case APP_USBD_EVT_STARTED:
            break;

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
        {
            NRF_LOG_INFO("USB power removed");
            // ret_code_t err_code = app_timer_stop(m_blink_cdc);
            // APP_ERROR_CHECK(err_code);
            // bsp_board_led_off(LED_CDC_ACM_CONN);
            m_usb_connected = false;
            app_usbd_stop();
        }
            break;

        case APP_USBD_EVT_POWER_READY:
        {
            NRF_LOG_INFO("USB ready");
            // ret_code_t err_code = app_timer_start(m_blink_cdc,
            //                                       APP_TIMER_TICKS(LED_BLINK_INTERVAL),
            //                                       (void *) LED_CDC_ACM_CONN);
            // APP_ERROR_CHECK(err_code);
            m_usb_connected = true;
            app_usbd_start();
        }
            break;

        default:
            break;
    }
}

void
usb_new_event_isr_handler(app_usbd_internal_evt_t const * const p_event, bool queued) {

    UNUSED_PARAMETER(p_event);
    UNUSED_PARAMETER(queued);

    // m_usb_isr_event = true;

#if USB_TASK_ENABLED
    ASSERT(m_usb_task_tandle != NULL);

    /* release the semaphore */
    if ( m_usb_task_tandle != NULL ) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        vTaskNotifyGiveFromISR( m_usb_task_tandle, &higher_priority_task_woken );
        portYIELD_FROM_ISR( higher_priority_task_woken );
    }
#endif /* USB_TASK_ENABLED */
}

static void
usb_init(void) {

    ret_code_t ret;
    static const app_usbd_config_t usbd_config = {
#if USB_TASK_ENABLED
        .ev_isr_handler = usb_new_event_isr_handler,
#else /* USB_TASK_ENABLED */
        .ev_isr_handler = NULL,
#endif /* !USB_TASK_ENABLED */
        .ev_state_proc  = usbd_user_ev_handler
    };

    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);
    app_usbd_class_inst_t const * class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);
    ret = app_usbd_power_events_enable();
    APP_ERROR_CHECK(ret);
}

int
usb_m_write(uint8_t *p_data, uint16_t length) {

    int ret;
    void* d;

    if (!m_usb_connected) {
        return -1;
    }

    if (!m_usb_port_opened) {
        return -1;
    }

    /* clear tx done event */
    xQueueReceive(m_usb_evt_tx_done_queue_handle, &d, 0);

    ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, p_data, length);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("CDC ACM write error: %i",ret);
        return -1;
    }

    // NRF_LOG_INFO("TX OK: %i", xTaskGetTickCount());

    xQueueReceive( m_usb_evt_tx_done_queue_handle, &d, pdMS_TO_TICKS( 200 ) );
    // while(!m_usb_tx_done)
    // {
    //     /* wait */
    // }
    // m_usb_tx_done = false;

    // NRF_LOG_INFO("TX OK: %i", xTaskGetTickCount());

    return 0;
}

static void
usb_process(void) {

#if APP_USBD_CONFIG_EVENT_QUEUE_ENABLE
    while (app_usbd_event_queue_process())
    {
        /* Nothing to do */
    }
#else
#error "APP_USBD_CONFIG_EVENT_QUEUE_ENABLE NOT SET"
#endif
}

#if USB_TASK_ENABLED
static void
usb_task(void *arg) {

    usb_init();

    // vTaskDelay(10);
    /* set the first event to make sure that USB queue is processed after it is started */
    UNUSED_RETURN_VALUE(xTaskNotifyGive(xTaskGetCurrentTaskHandle()));

    for ( ;; )
    {
        /* waiting for event */
        // UNUSED_RETURN_VALUE(ulTaskNotifyTake(pdTRUE, portMAX_DELAY));
        UNUSED_RETURN_VALUE(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10)));

        usb_process();
    }

}
#endif /* USB_TASK_ENABLED */

void
usb_m_process(void) {

#if !USB_TASK_ENABLED
    usb_process();
#endif /* !USB_TASK_ENABLED */

}

void
usb_m_init(void) {

    // semaphore_mutex_handle = xSemaphoreCreateMutex();
    // configASSERT( semaphore_mutex_handle );
    m_usb_evt_tx_done_queue_handle = xQueueCreate(1, sizeof(void*));
    configASSERT( m_usb_evt_tx_done_queue_handle );

#if USB_TASK_ENABLED
    if (pdPASS != xTaskCreate(usb_task, "USB", 512, NULL, tskIDLE_PRIORITY, &m_usb_task_tandle)) {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
#else /* USB_TASK_ENABLED */
    usb_init();
#endif /* !USB_TASK_ENABLED */

}

#endif /* USB_M_ENABLED */
