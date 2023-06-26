#ifndef __USB_M_H
#define __USB_M_H


#ifdef __cplusplus
extern "C" {
#endif


int  usb_m_write(uint8_t *p_data, uint16_t length);
void usb_m_process(void);
void usb_m_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __USB_M_H */
