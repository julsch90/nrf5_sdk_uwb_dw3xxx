#include <stdint.h>
#include <string.h>

#include "ble_m.h"

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"

#include "ble.h"
#include "ble_hci.h"
#include "ble_db_discovery.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
// #include "ble_bas.h"
// #include "ble_hrs.h"
#include "ble_dis.h"
#include "ble_conn_params.h"
#include "FreeRTOS.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_freertos.h"
#include "app_timer.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "bsp_btn_ble.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "fds.h"
#include "nrf_fstorage.h"
#include "ble_conn_state.h"
#include "nrf_drv_clock.h"
#include "ble_nus_c.h"
#include "ble_nus.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_ble_scan.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"




#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */
#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */
#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */

// static bool erase_bonds = false;

static uint16_t m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - OPCODE_LENGTH - HANDLE_LENGTH; /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */


/* server / peripheral */

#define DEVICE_NAME                     "uwb-ble"                                   /**< Name of device. Will be included in the advertising data. */

#define APP_ADV_FAST_ENABLED            true                                        /**< The advertising fast enable/disable */
#define APP_ADV_FAST_INTERVAL           MSEC_TO_UNITS(500, UNIT_0_625_MS)           /**< The advertising interval (in units of 0.625 ms. This value corresponds to 500 ms). */
#define APP_ADV_FAST_DURATION           MSEC_TO_UNITS(180000, UNIT_10_MS)           /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define APP_ADV_SLOW_ENABLED            false                                       /**< The advertising slow enable/disable */
#define APP_ADV_SLOW_INTERVAL           MSEC_TO_UNITS(2000, UNIT_0_625_MS)          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 2000 ms). */
#define APP_ADV_SLOW_DURATION           MSEC_TO_UNITS(0, UNIT_10_MS)                /**< The advertising duration (inf seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define APP_ADV_DISABLE_ON_DISCONNECT   true                                        /**< Disabling advertising after disconnect  */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */


BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
// NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle_nus      = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

static bool m_scanning = false;                             /**< Variable that informs about the ongoing scanning. True when scan is ON. */
static bool m_adv_active = false;                           /**< Variable that informs about the ongoing adv. True when adv is ON. */

static ble_m_rx_evt_handler_t m_nus_peripheral_rx_evt_handler;

/* client / central */

BLE_NUS_C_DEF(m_ble_nus_c);                                             /**< BLE Nordic UART Service (NUS) client instance. */
// NRF_BLE_GATT_DEF(m_gatt);                                               /**< GATT module instance. */
BLE_DB_DISCOVERY_DEF(m_db_disc);                                        /**< Database discovery module instance. */
NRF_BLE_SCAN_DEF(m_scan);                                               /**< Scanning Module instance. */
NRF_BLE_GQ_DEF(m_ble_gatt_queue,                                        /**< BLE GATT Queue instance. */
               NRF_SDH_BLE_CENTRAL_LINK_COUNT,
               NRF_BLE_GQ_QUEUE_SIZE);

static uint16_t m_conn_handle_nus_c    = BLE_CONN_HANDLE_INVALID;       /**< Handle of the current connection. */

/**@brief NUS UUID. */
static ble_uuid_t const m_nus_uuid =
{
    .uuid = BLE_UUID_NUS_SERVICE,
    .type = NUS_SERVICE_UUID_TYPE
};

static ble_m_rx_evt_handler_t m_nus_central_rx_evt_handler;


/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{
    // uint32_t err_code;

    switch (p_evt->type)
    {
        case BLE_NUS_EVT_RX_DATA:
        {
            // NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
            // NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
            // NRF_LOG_INFO("BLE_NUS_EVT_RX_DATA");

            if (m_nus_peripheral_rx_evt_handler != NULL)
            {
                // m_nus_peripheral_rx_evt_handler((uint8_t*)p_evt->params.rx_data.p_data, (uint16_t*)p_evt->params.rx_data.length);
                m_nus_peripheral_rx_evt_handler((uint8_t*)p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
            }
        }   break;


        case BLE_NUS_EVT_TX_RDY:
            // NRF_LOG_INFO("BLE_NUS_EVT_TX_RDY")
            break;

        case BLE_NUS_EVT_COMM_STARTED:
            // NRF_LOG_INFO("BLE_NUS_EVT_COMM_STARTED")
            break;

        case BLE_NUS_EVT_COMM_STOPPED:
            // NRF_LOG_INFO("BLE_NUS_EVT_COMM_STOPPED")
            break;

        default:
            break;
    }

}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle_nus, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}



/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    // uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            // m_adv_active
            NRF_LOG_INFO("BLE_ADV_EVT_FAST");

            // err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            // APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            NRF_LOG_INFO("BLE_ADV_EVT_IDLE");
            // advertising_start(NULL);
            // sleep_mode_enter();
            break;
        default:
            break;
    }
}

/**@brief   Function for handling BLE events from the central application.
 *
 * @details This function parses scanning reports and initiates a connection to peripherals when a
 *          target UUID is found. It updates the status of LEDs used to report the central application
 *          activity.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_central_evt(ble_evt_t const * p_ble_evt)
{
    ret_code_t            err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
        // Upon connection, check which peripheral is connected (HR or RSC), initiate DB
        // discovery, update LEDs status, and resume scanning, if necessary.
        case BLE_GAP_EVT_CONNECTED:
        {
            err_code = ble_nus_c_handles_assign(&m_ble_nus_c, p_ble_evt->evt.gap_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);

            // err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            // APP_ERROR_CHECK(err_code);

            // start discovery of services. The NUS Client waits for a discovery result
            err_code = ble_db_discovery_start(&m_db_disc, p_ble_evt->evt.gap_evt.conn_handle);
            APP_ERROR_CHECK(err_code);
        } break; // BLE_GAP_EVT_CONNECTED

        // Upon disconnection, reset the connection handle of the peer that disconnected,
        // update the LEDs status and start scanning again.
        case BLE_GAP_EVT_DISCONNECTED:
        {
            if (p_gap_evt->conn_handle == m_conn_handle_nus_c)
            {
                NRF_LOG_INFO("NUS central disconnected (reason: %d)",
                             p_gap_evt->params.disconnected.reason);

                m_conn_handle_nus_c = BLE_CONN_HANDLE_INVALID;
            }

            if (   (m_conn_handle_nus_c == BLE_CONN_HANDLE_INVALID)
                // || (m_conn_handle_nus_c  == BLE_CONN_HANDLE_INVALID) // may use for other services
                )
            {
                // Start scanning.
                ble_m_scan_start();

                // Update LEDs status.
                // bsp_board_led_on(CENTRAL_SCANNING_LED);
            }

            // if (ble_conn_state_central_conn_count() == 0)
            // {
            //     bsp_board_led_off(CENTRAL_CONNECTED_LED);
            // }
        } break; // BLE_GAP_EVT_DISCONNECTED

        case BLE_GAP_EVT_TIMEOUT:
        {
            // No timeout for scanning is specified, so only connection attemps can timeout.
            if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN)
            {
                NRF_LOG_INFO("Connection Request timed out.");
            }
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        {
            // Pairing not supported.
            err_code = sd_ble_gap_sec_params_reply(p_ble_evt->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            // Accept parameters requested by peer.
            err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle,
                                                    &p_gap_evt->params.conn_param_update_request.conn_params);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            NRF_LOG_DEBUG("BLE_GATTS_EVT_SYS_ATTR_MISSING");
            // No system attributes have been stored.
            // err_code = sd_ble_gatts_sys_attr_set(m_conn_handle_nus, NULL, 0, 0);
            // APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief   Function for handling BLE events from peripheral applications.
 * @details Updates the status LEDs used to report the activity of the peripheral applications.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_peripheral_evt(ble_evt_t const * p_ble_evt)
{
    ret_code_t            err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Peripheral connected");
            // err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            // APP_ERROR_CHECK(err_code);

            m_conn_handle_nus = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle_nus);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Peripheral disconnected. conn_handle: 0x%x, reason: 0x%x",
                         p_gap_evt->conn_handle,
                         p_gap_evt->params.disconnected.reason);

            m_conn_handle_nus = BLE_CONN_HANDLE_INVALID;

            // LED indication will be changed when advertising starts.
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }

}

/**@brief Function for checking whether a bluetooth stack event is an advertising timeout.
 *
 * @param[in] p_ble_evt Bluetooth stack event.
 */
static bool ble_evt_is_advertising_timeout(ble_evt_t const * p_ble_evt)
{
    return (p_ble_evt->header.evt_id == BLE_GAP_EVT_ADV_SET_TERMINATED);
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    // uint32_t err_code;

    uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    uint16_t role = ble_conn_state_role(conn_handle);
    // NRF_LOG_INFO("role: 0x%X",role);

    // Based on the role this device plays in the connection, dispatch to the right handler.
    if (role == BLE_GAP_ROLE_PERIPH || ble_evt_is_advertising_timeout(p_ble_evt))
    {
        // ble_hrs_on_ble_evt(p_ble_evt, &m_hrs);
        // ble_rscs_on_ble_evt(p_ble_evt, &m_rscs);
        on_ble_peripheral_evt(p_ble_evt);
    }
    else if ((role == BLE_GAP_ROLE_CENTRAL) || (p_ble_evt->header.evt_id == BLE_GAP_EVT_ADV_REPORT))
    {
        // ble_hrs_c_on_ble_evt(p_ble_evt, &m_hrs_c);
        // ble_rscs_c_on_ble_evt(p_ble_evt, &m_rscs_c);
        on_ble_central_evt(p_ble_evt);
    }
}

/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    /* enable soft device */
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);
    ASSERT(nrf_sdh_is_enabled());

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

/**@brief Function for handling events from the GATT library. */
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)
    {
        NRF_LOG_INFO("ATT MTU exchange completed.");

        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Ble NUS max data length set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }

    if ((m_conn_handle_nus == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}

/**@brief Function for initializing the GATT library. */
static void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_central_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = APP_ADV_FAST_ENABLED;
    init.config.ble_adv_fast_interval = APP_ADV_FAST_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_FAST_DURATION;

    init.config.ble_adv_slow_enabled  = APP_ADV_SLOW_ENABLED;
    init.config.ble_adv_slow_interval = APP_ADV_SLOW_INTERVAL;
    init.config.ble_adv_slow_timeout  = APP_ADV_SLOW_DURATION;

    init.config.ble_adv_on_disconnect_disabled  = APP_ADV_DISABLE_ON_DISCONNECT;

    // init.config.ble_adv_whitelist_enabled  = 0;

    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


// /**@brief Function for starting advertising.
//  */
// static void advertising_start(void * p_erase_bonds)
// {
//     uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
//     APP_ERROR_CHECK(err_code);
// }

/**@brief Function for handling the Nordic UART Service Client errors.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nus_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling Scanning Module events.
 */
static void scan_evt_handler(scan_evt_t const * p_scan_evt)
{
    ret_code_t err_code;

    switch(p_scan_evt->scan_evt_id)
    {
        case NRF_BLE_SCAN_EVT_CONNECTING_ERROR:
        {
            err_code = p_scan_evt->params.connecting_err.err_code;
            APP_ERROR_CHECK(err_code);
        } break;

        case NRF_BLE_SCAN_EVT_CONNECTED:
        {
            ble_gap_evt_connected_t const * p_connected =
                            p_scan_evt->params.connected.p_connected;
            // Scan is automatically stopped by the connection.
            NRF_LOG_INFO("Connecting to target %02x%02x%02x%02x%02x%02x",
                    p_connected->peer_addr.addr[0],
                    p_connected->peer_addr.addr[1],
                    p_connected->peer_addr.addr[2],
                    p_connected->peer_addr.addr[3],
                    p_connected->peer_addr.addr[4],
                    p_connected->peer_addr.addr[5]
                    );
        } break;

        case NRF_BLE_SCAN_EVT_SCAN_TIMEOUT:
        {
            NRF_LOG_INFO("Scan timed out.");
            ble_m_scan_start();
        } break;

        default:
            break;
    }
}


/**@brief Function for initializing the scanning and setting the filters.
 */
static void scan_init(void)
{
    ret_code_t          err_code;
    nrf_ble_scan_init_t init_scan;

    memset(&init_scan, 0, sizeof(init_scan));

    init_scan.connect_if_match = true;
    init_scan.conn_cfg_tag     = APP_BLE_CONN_CFG_TAG;

    err_code = nrf_ble_scan_init(&m_scan, &init_scan, scan_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_filter_set(&m_scan, SCAN_UUID_FILTER, &m_nus_uuid);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_filters_enable(&m_scan, NRF_BLE_SCAN_UUID_FILTER, false);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling database discovery events.
 *
 * @details This function is a callback function to handle events from the database discovery module.
 *          Depending on the UUIDs that are discovered, this function forwards the events
 *          to their respective services.
 *
 * @param[in] p_event  Pointer to the database discovery event.
 */
static void db_disc_handler(ble_db_discovery_evt_t * p_evt)
{
    ble_nus_c_on_db_disc_evt(&m_ble_nus_c, p_evt);
}


/**@brief Callback handling Nordic UART Service (NUS) client events.
 *
 * @details This function is called to notify the application of NUS client events.
 *
 * @param[in]   p_ble_nus_c   NUS client handle. This identifies the NUS client.
 * @param[in]   p_ble_nus_evt Pointer to the NUS client event.
 */

/**@snippet [Handling events from the ble_nus_c module] */
static void ble_nus_c_evt_handler(ble_nus_c_t * p_ble_nus_c, ble_nus_c_evt_t const * p_ble_nus_evt)
{
    ret_code_t err_code;

    switch (p_ble_nus_evt->evt_type)
    {
        case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:

            NRF_LOG_INFO("Discovery complete.");
            if (m_conn_handle_nus_c == BLE_CONN_HANDLE_INVALID)
            {
                m_conn_handle_nus_c = p_ble_nus_evt->conn_handle;

                err_code = ble_nus_c_handles_assign(p_ble_nus_c, p_ble_nus_evt->conn_handle, &p_ble_nus_evt->handles);
                APP_ERROR_CHECK(err_code);

                err_code = ble_nus_c_tx_notif_enable(p_ble_nus_c);
                APP_ERROR_CHECK(err_code);
                NRF_LOG_INFO("Connected to device with Nordic UART Service.");
            }
            break;

        case BLE_NUS_C_EVT_NUS_TX_EVT:
            // NRF_LOG_INFO("[BLE_NUS_C_EVT_NUS_TX_EVT] TODO: handle rx event ");
            // ble_nus_chars_received_uart_print(p_ble_nus_evt->p_data, p_ble_nus_evt->data_len);
            if (m_nus_central_rx_evt_handler != NULL)
            {
                m_nus_central_rx_evt_handler(p_ble_nus_evt->p_data, p_ble_nus_evt->data_len);
            }
            break;

        case BLE_NUS_C_EVT_DISCONNECTED:
            // NRF_LOG_INFO("Nordic UART Service Disconnected.");
            break;
    }
}
/**@snippet [Handling events from the ble_nus_c module] */


/**@brief Function for initializing the Nordic UART Service (NUS) client. */
static void nus_c_init(void)
{
    ret_code_t       err_code;
    ble_nus_c_init_t init;

    init.evt_handler   = ble_nus_c_evt_handler;
    init.error_handler = nus_error_handler;
    init.p_gatt_queue  = &m_ble_gatt_queue;

    err_code = ble_nus_c_init(&m_ble_nus_c, &init);
    APP_ERROR_CHECK(err_code);
}


/** @brief Function for initializing the database discovery module. */
static void db_discovery_init(void)
{
    ble_db_discovery_init_t db_init;

    memset(&db_init, 0, sizeof(ble_db_discovery_init_t));

    db_init.evt_handler  = db_disc_handler;
    db_init.p_gatt_queue = &m_ble_gatt_queue;

    ret_code_t err_code = ble_db_discovery_init(&db_init);
    APP_ERROR_CHECK(err_code);
}


void
ble_m_scan_start(void) {

    ret_code_t ret;

    NRF_LOG_DEBUG("ble scan start");

    ret = nrf_ble_scan_start(&m_scan);
    APP_ERROR_CHECK(ret);

    // // LED INDICATION
    // ret = bsp_indication_set(BSP_INDICATE_SCANNING);
    // APP_ERROR_CHECK(ret);
    // bsp_board_led_on(CENTRAL_SCANNING_LED);

    m_scanning = true;
}

void
ble_m_scan_stop(void) {

    NRF_LOG_DEBUG("ble scan stop");

    nrf_ble_scan_stop();

    // // LED INDICATION
    // bsp_board_led_off(CENTRAL_SCANNING_LED);

    m_scanning = false;
}


void
ble_m_adv_start(void) {

    NRF_LOG_DEBUG("ble adv start");

    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
    m_adv_active = true;
}

void
ble_m_adv_stop(void) {

    ret_code_t err_code;

    NRF_LOG_DEBUG("ble adv stop");

    // [INFO] ILDE NOT WORKING TO STOP ADV
    // err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_IDLE);
    // APP_ERROR_CHECK(err_code);

    // Turn off the LED to signal advertising.
    // bsp_board_led_off(PERIPHERAL_ADVERTISING_LED);
    err_code = sd_ble_gap_adv_stop(m_advertising.adv_handle);
    if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_INVALID_STATE) && (err_code != BLE_ERROR_INVALID_ADV_HANDLE))
    {
        APP_ERROR_CHECK(err_code);
    }

    m_adv_active = false;
}

void
ble_m_peripheral_set_rx_evt_handler(ble_m_rx_evt_handler_t rx_evt_handler) {
    m_nus_peripheral_rx_evt_handler = rx_evt_handler;
}

void
ble_m_central_set_rx_evt_handler(ble_m_rx_evt_handler_t rx_evt_handler) {
    m_nus_central_rx_evt_handler = rx_evt_handler;
}

int
ble_m_peripheral_send(uint8_t *p_data, uint16_t length) {

    int ret = -1;

    // NRF_LOG_INFO("m_conn_handle_nus: %i",m_conn_handle_nus);
    if(m_conn_handle_nus == BLE_CONN_HANDLE_INVALID)
    {
        return ret;
    }

    do
    {
        ret = ble_nus_data_send(&m_nus, p_data, &length, m_conn_handle_nus);
        if (    (ret != NRF_ERROR_INVALID_STATE)
            &&  (ret != NRF_ERROR_RESOURCES)
            &&  (ret != NRF_ERROR_NOT_FOUND))
        {
            // APP_ERROR_CHECK(ret); // TODO: handle error?
        }
    } while (ret == NRF_ERROR_RESOURCES);


    return ret;
}

int
ble_m_central_send(uint8_t *p_data, uint16_t length) {

    int ret = -1;

    if (m_ble_nus_c.conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return ret;
    }

    do
    {
        ret = ble_nus_c_string_send(&m_ble_nus_c, p_data, length);
        if ( (ret != NRF_ERROR_INVALID_STATE) && (ret != NRF_ERROR_RESOURCES) )
        {
            break;
            APP_ERROR_CHECK(ret);
        }
    } while (ret == NRF_ERROR_RESOURCES);


    return ret;
}

int
ble_m_peripheral_is_connected() {

    if (m_conn_handle_nus == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }

    return 1;
}

int
ble_m_central_is_connected() {

    if (m_ble_nus_c.conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }

    return 1;
}

void
ble_m_peripheral_disconnect() {

    uint32_t err_code;

    if (m_conn_handle_nus == BLE_CONN_HANDLE_INVALID)
    {
        return;
    }

    /* Disconnect from a device. */
    err_code = sd_ble_gap_disconnect(m_conn_handle_nus, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
}

void
ble_m_central_disconnect() {

    uint32_t err_code;

    if (m_ble_nus_c.conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return;
    }

    err_code = sd_ble_gap_disconnect(m_ble_nus_c.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);

}

void
ble_m_init(void) {

    /* configure and initialize the BLE stack. */
    ble_stack_init();
    scan_init();
    gap_params_init();
    gatt_init();
    conn_params_init();
    db_discovery_init();
    nus_c_init();
    services_init();
    advertising_init();

    nrf_sdh_freertos_init(NULL, NULL);
}
