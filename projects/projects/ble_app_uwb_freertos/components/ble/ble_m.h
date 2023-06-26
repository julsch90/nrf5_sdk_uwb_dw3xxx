#ifndef __BLE_M_H
#define __BLE_M_H


#ifdef __cplusplus
extern "C" {
#endif


typedef void (*ble_m_rx_evt_handler_t)(uint8_t *p_data, uint16_t length);

void ble_m_scan_start(void);
void ble_m_scan_stop(void);

void ble_m_adv_start(void);
void ble_m_adv_stop(void);

void ble_m_peripheral_set_rx_evt_handler(ble_m_rx_evt_handler_t rx_evt_handler);
void ble_m_central_set_rx_evt_handler(ble_m_rx_evt_handler_t rx_evt_handler);

int ble_m_peripheral_send(uint8_t *p_data, uint16_t length);
int ble_m_central_send(uint8_t *p_data, uint16_t length);

int ble_m_peripheral_is_connected();
int ble_m_central_is_connected();

void ble_m_peripheral_disconnect();
void ble_m_central_disconnect();

void ble_m_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __BLE_M_H */
