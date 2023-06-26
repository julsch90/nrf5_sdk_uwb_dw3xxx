#include "log_m.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_error.h"

#if NRF_LOG_ENABLED && NRF_LOG_DEFERRED
#include "FreeRTOS.h"
#include "task.h"
#endif //NRF_LOG_ENABLED && NRF_LOG_DEFERRED


#if NRF_LOG_ENABLED && NRF_LOG_DEFERRED
static TaskHandle_t m_logger_task_handle;    /**< Definition of Logger thread. */
#endif //NRF_LOG_ENABLED && NRF_LOG_DEFERRED

#if NRF_LOG_ENABLED && NRF_LOG_DEFERRED
/**@brief Thread/task for handling the logger.
 *
 * @details This thread is responsible for processing log entries if logs are deferred.
 *          Thread flushes all log entries and suspends. It is resumed by idle task hook.
 *
 * @param[in]   arg   Pointer used for passing some arbitrary information (context) from the
 *                    osThreadCreate() call to the thread.
 */
static void
logger_task(void * arg) {

    UNUSED_PARAMETER(arg);

    for ( ;; ) {

        if (!NRF_LOG_PROCESS()) {
            xTaskNotifyWait(0, UINT32_MAX, NULL, portMAX_DELAY);
        }
    }
}
#endif //NRF_LOG_ENABLED && NRF_LOG_DEFERRED

#if NRF_LOG_ENABLED && NRF_LOG_DEFERRED
void
log_pending_hook( void ) {

    BaseType_t result = pdFAIL;

    if ( __get_IPSR() != 0 ) {

        BaseType_t higher_priority_task_woken = pdFALSE;
        result = xTaskNotifyFromISR( m_logger_task_handle, 0, eSetValueWithoutOverwrite, &higher_priority_task_woken );

        if ( pdFAIL != result ) {
            portYIELD_FROM_ISR( higher_priority_task_woken );
        }

    } else {
        UNUSED_RETURN_VALUE(xTaskNotify( m_logger_task_handle, 0, eSetValueWithoutOverwrite ));
    }
}
#endif //NRF_LOG_ENABLED && NRF_LOG_DEFERRED

void
log_m_init(void) {

    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();

#if NRF_LOG_ENABLED && NRF_LOG_DEFERRED
    if (pdPASS != xTaskCreate(logger_task, "LOG", 256, NULL, tskIDLE_PRIORITY, &m_logger_task_handle)) {
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }
#endif //NRF_LOG_ENABLED && NRF_LOG_DEFERRED

    return;
}
