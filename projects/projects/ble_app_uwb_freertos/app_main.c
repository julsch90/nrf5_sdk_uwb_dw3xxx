
#include "string.h"
#include "stdlib.h"

#include "main.h"

/* nrf */
#include "nrf_log.h"

/* freertos */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* modules */
#include "usb_m.h"
#include "rtc_m.h"

/* uwb */
#include <deca_probe_interface.h>
#include <deca_device_api.h>
#include <deca_spi.h>
#include <deca_port.h>
#include <deca_shared_defines.h>
#include <deca_shared_functions.h>
#include "deca_config_options.h"

/* choose rx, tx or disturb device for build */
#define TX_DEVICE_ENABLED           0
#define RX_DEVICE_ENABLED           1
#define DISTURB_DEVICE_ENABLED      0

#define WRITE_DIAG_TO_USB_ON_EVERY_RX_STATUS_EVENT_ENABLED  0   /* otherwise only on rx_ok and magic number match */

#if ((TX_DEVICE_ENABLED + RX_DEVICE_ENABLED + DISTURB_DEVICE_ENABLED) != 1)
#error "choose only one"
#endif


#define DEVICE_PAN_ID                   0xBEEF
#define DEVICE_SHORT_ADDR_TX            0x0001
#define DEVICE_SHORT_ADDR_TX_DISTURB    0x0003
#define DEVICE_SHORT_ADDR_RX            0x0002


#define TX_DELAY_MS                     20      /* Inter-frame delay period, in milliseconds. */
#define RX_TIMEOUT_UUS                  100000  /* Receive response timeout, expressed in UWB microseconds (UUS, 1 uus = 512/499.2 us) */

#define MAGIC_ID                        0x01
#define MAGIC_NUMBER                    0x12345678
#define MAGIC_NUMBER_DISTURB            0x87654321

#define USE_ISR_ENABLED                 0
#define USE_FRAME_FILTERING_ENABLED     0       /* necessary for using auto acknowledge */

/* cir */
#define CIR_LEN                         300
#define CIR_FP_IDX_OFFSET               700                         // [600:end] here is the fp inside the cir
#define CIR_BUFFER_LEN                  ( CIR_LEN * (3+3) + 1 )     // 3 byte real + 3 byte imag and 1 byte garbadge (first byte)

typedef struct __attribute__((__packed__)) {
    uint8_t frame_ctrl[2];
    uint8_t seq_num;
    uint16_t dst_pan_id;
    uint16_t dst_addr;
    uint16_t src_addr;
} mhr_802_15_4_intra_pan_short_addr_t; /* 9 bytes */

typedef struct __attribute__((__packed__)) {
    uint8_t id;
    uint32_t magic;
    uint32_t idx;
    uint32_t tx_timestamp_ms;
} frame_magic_data_t;

typedef struct __attribute__((__packed__)) {
    uint32_t rx_timestamp_ms;
    uint32_t status_reg;
    uint16_t frame_len;
    uint8_t frame_buffer[FRAME_LEN_MAX];
    dwt_deviceentcnts_t event_cnt;          // 24 bytes
    dwt_rxdiag_t rxdiag;                    // 112 bytes
    uint8_t dgc;
    int8_t rssi;
    uint16_t cir_start_idx;
    uint16_t cir_len;
    uint8_t cir_buffer[CIR_BUFFER_LEN];
} diag_frame_t;

typedef struct __attribute__((__packed__)) {
    uint32_t header;
    uint16_t data_length;
    uint8_t data[sizeof(diag_frame_t)];
    uint32_t checksum;
} data_packet_t;

static data_packet_t data_packet;
static diag_frame_t *p_diag_frame = (diag_frame_t*)(data_packet.data);

/* Channel 5, PRF 64M, Preamble Length 128, PAC 8, Preamble code 9, Data Rate 6.8M, No STS
 */
dwt_config_t config_option = {
    5,                 /* Channel number. */
    DWT_PLEN_128,      /* Preamble length. Used in TX only. */
    DWT_PAC8,          /* Preamble acquisition chunk size. Used in RX only. */
    9,                 /* TX preamble code. Used in TX only. */
    9,                 /* RX preamble code. Used in RX only. */
    3,                 /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,        /* Data rate. */
    DWT_PHRMODE_STD,   /* PHY header mode. */
    DWT_PHRRATE_STD,   /* PHY header rate. */
    (128 + 1 + 8 - 8), /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF,  /* STS Off */
    DWT_STS_LEN_64,    /* Ignore value when STS is disabled */
    DWT_PDOA_M0        /* PDOA mode off */
};

/*
 * TX Power Configuration Settings
 */
/* Values for the PG_DELAY and TX_POWER registers reflect the bandwidth and power of the spectrum at the current
 * temperature. These values can be calibrated prior to taking reference measurements. */
dwt_txconfig_t txconfig_option = {
    0x34,       /* PG delay. */
    0xfdfdfdfd, /* TX power. */
    0x0         /* PG count */
};

dwt_txconfig_t txconfig_option_ch9 = {
    0x34,       /* PG delay. */
    0xfefefefe, /* TX power. */
    0x0         /* PG count */
};


static int  uwb_init();
// static void rx_ok_cb(const dwt_cb_data_t *cb_data);
// static void rx_to_cb(const dwt_cb_data_t *cb_data);
// static void rx_err_cb(const dwt_cb_data_t *cb_data);
// static void tx_done_cb(const dwt_cb_data_t *cb_data);

static int
uwb_init() {

    /* Initialise the SPI for dw */
    dw_spi_init();

#if USE_ISR_ENABLED
    /* Configuring interrupt*/
    dw_irq_init();
#endif /* USE_ISR_ENABLED */

    /* Configure SPI rate, DW3000 supports up to 36 MHz */
    port_set_dw_ic_spi_fastrate();

    /* Reset DW IC */
    reset_DWIC(); /* Target specific drive of RSTn line into DW IC low for a period. */

    vTaskDelay(3); // Time needed for DW3000 to start up (transition from INIT_RC to IDLE_RC, or could wait for SPIRDY event)

    /* Probe for the correct device driver. */
    dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);

    while (!dwt_checkidlerc()) /* Need to make sure DW IC is in IDLE_RC before proceeding */ { };

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
        NRF_LOG_ERROR("DW INIT FAILED");
        return -1;
    }

    /* Configure DW IC */
    /* if the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device */
    if (dwt_configure(&config_option)) {

        NRF_LOG_ERROR("DW CONFIG FAILED");
        return -2;
    }

    /* Configure the TX spectrum parameters (power, PG delay and PG count) */
    dwt_configuretxrf( (config_option.chan == 5) ? &txconfig_option : &txconfig_option_ch9);

    /* get device id */
    // dwt_readdevid();


#if USE_ISR_ENABLED

    /* set callbacks */
    // dwt_setcallbacks(&tx_done_cb, &rx_ok_cb, &rx_to_cb, &rx_err_cb, NULL, NULL, NULL);
    dwt_setcallbacks(NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts and RX errors). */
    dwt_setinterrupt(     DWT_INT_SPIRDY_BIT_MASK
                        // | DWT_INT_CPERR_BIT_MASK
                        | DWT_INT_TXFRS_BIT_MASK
                        | DWT_INT_ARFE_BIT_MASK
                        | DWT_INT_RXFCG_BIT_MASK
                        | DWT_INT_RXFTO_BIT_MASK
                        | DWT_INT_RXPTO_BIT_MASK
                        | DWT_INT_RXPHE_BIT_MASK
                        | DWT_INT_RXFCE_BIT_MASK
                        | DWT_INT_RXFSL_BIT_MASK
                        | DWT_INT_RXSTO_BIT_MASK,
                        0, DWT_ENABLE_INT);

    /* Clearing the SPI ready interrupt */
    dwt_writesysstatuslo(DWT_INT_RCINIT_BIT_MASK | DWT_INT_SPIRDY_BIT_MASK);

    /* Install DW IC IRQ handler. */
    port_set_dwic_isr(dwt_isr);

#endif /* USE_ISR_ENABLED */

// #if USE_FRAME_FILTERING_ENABLED

//     /* Set PAN ID and short address. */
//     dwt_setpanid(DEVICE_PAN_ID);
//     // dwt_seteui(DEVICE_LONG_ADDR);
//     dwt_setaddress16(DEVICE_SHORT_ADDR_TX);

//     /* Configure frame filtering. Only data frames are enabled in this example. Frame filtering must be enabled for Auto ACK to work. */
//     uint16_t frame_filter = DWT_FF_DATA_EN;
//     dwt_configureframefilter(DWT_FF_ENABLE_802_15_4, frame_filter);
//     // /* Activate auto-acknowledgement. Time is set to 0 so that the ACK is sent as soon as possible after reception of a frame. */
//     // dwt_enableautoack(0, 1);

// #endif /* USE_FRAME_FILTERING_ENABLED */

    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK) ;     /**< DEBUG I/O 2&3 : configure the GPIOs which control the LEDs on HW */
    // dwt_setlnapamode(DWT_TXRX_EN);                          /**< DEBUG I/O 0&1 : configure TX/RX states to output on GPIOs */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);       /* can enable TX/RX states output on GPIOs 5 and 6 to help debug */

    /* Activate event counters. */
    dwt_configeventcounters(1);

    /* Enable IC diagnostic calculation and logging */
    // dwt_configciadiag(DW_CIA_DIAG_LOG_MIN);
    // dwt_configciadiag(DW_CIA_DIAG_LOG_MID);
    // dwt_configciadiag(DW_CIA_DIAG_LOG_MAX);
    dwt_configciadiag(DW_CIA_DIAG_LOG_ALL);
    // dwt_configciadiag(DW_CIA_DIAG_LOG_OFF);

    /* Apply default antenna delay value. */
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    return 0;
}


// static void
// rx_ok_cb(const dwt_cb_data_t *cb_data) {

// }

// static void
// rx_to_cb(const dwt_cb_data_t *cb_data) {

// }

// static void
// rx_err_cb(const dwt_cb_data_t *cb_data) {

// }

// static void
// tx_done_cb(const dwt_cb_data_t *cb_data) {

// }

__attribute__((unused))
static int
read_diag_cir_to_usb() {

    int ret = -1;
    uint16_t fp_int;

    (void)fp_int; // NOT USED HERE

    // // CALC PROCESS TIME
    // CoreDebug->DEMCR |= 0x01000000; // enable DWT
    // DWT->CYCCNT = 0;                // Reset cycle counter
    // DWT->CTRL |= 0x1;               // enable cycle counter
    // uint32_t t1 = DWT->CYCCNT;

    dwt_readeventcounters(&p_diag_frame->event_cnt);
    dwt_readdiagnostics(&p_diag_frame->rxdiag);

    p_diag_frame->rx_timestamp_ms = rtc_m_get_time_ms();
    p_diag_frame->dgc = dwt_get_dgcdecision();  /* read digital gain control */
    p_diag_frame->rssi = dwt_calc_rssi(&p_diag_frame->rxdiag, p_diag_frame->dgc);
    p_diag_frame->cir_start_idx = CIR_FP_IDX_OFFSET;
    p_diag_frame->cir_len = CIR_LEN;

    // /* first path idx [10.6 bits fixed point value] */
    // fp_int = p_diag_frame->rxdiag.ipatovFpIndex >> 6;

    /* read accumulator */
    // dwt_readaccdata(p_diag_frame->cir_buffer, CIR_BUFFER_LEN, (fp_int + CIR_FP_IDX_OFFSET));
    dwt_readaccdata(p_diag_frame->cir_buffer, CIR_BUFFER_LEN, CIR_FP_IDX_OFFSET);

#if USB_M_ENABLED

    data_packet.header = 0xAAAAAAAA;
    data_packet.data_length = sizeof(diag_frame_t);
    // data_packet.checksum = 0xEEEEEEEE;

    taskYIELD();
    ret = usb_m_write((uint8_t*)&data_packet, sizeof(data_packet_t));
    taskYIELD();

#endif

    // TEST OUTPUT
    // int cir_buffer_idx = 1;
    // for (int i=0; i<CIR_LEN; i++) {

    //     int32_t iValue = p_diag_frame->cir_buffer[cir_buffer_idx++];
    //     iValue |= ((int32_t)p_diag_frame->cir_buffer[cir_buffer_idx++]<<8);
    //     iValue |= ((int32_t)(p_diag_frame->cir_buffer[cir_buffer_idx++] & 0x03)<<16);

    //     int32_t qValue = p_diag_frame->cir_buffer[cir_buffer_idx++];
    //     qValue |= ((int32_t)p_diag_frame->cir_buffer[cir_buffer_idx++]<<8);
    //     qValue |= ((int32_t)(p_diag_frame->cir_buffer[cir_buffer_idx++] & 0x03)<<16);

    //     if (iValue & 0x020000) {  // MSB of 18 bit value is 1
    //         iValue |= 0xfffc0000;
    //     }
    //     if (qValue & 0x020000) {  // MSB of 18 bit value is 1
    //         qValue |= 0xfffc0000;
    //     }
    //     NRF_LOG_INFO("%ld %ldj", iValue, qValue);
    //     // NRF_LOG_INFO("%ld %ldj", iValue, qValue);
    //     // NRF_LOG_INFO("%ld %ldj", iValue, qValue);
    //     // NRF_LOG_INFO("%ld %ldj", iValue, qValue);
    //     i = CIR_LEN;
    // }

    // uint32_t t2 = DWT->CYCCNT;
    // NRF_LOG_INFO("t_diff: %i us", (t2-t1)>>6);

    return ret;
}

__attribute__((unused))
static void
simple_rx() {

    int ret = -1;
    int ret_usb = -1;
    uint32_t status_reg = 0;
    uint16_t frame_len;

    mhr_802_15_4_intra_pan_short_addr_t *p_mhr = (mhr_802_15_4_intra_pan_short_addr_t*)(p_diag_frame->frame_buffer);
    frame_magic_data_t *p_payload = (frame_magic_data_t*)&p_diag_frame->frame_buffer[sizeof(mhr_802_15_4_intra_pan_short_addr_t)];

    (void)p_mhr;
    (void)p_payload;

    // NRF_LOG_INFO("(%d) start simple_rx", rtc_m_get_time_ms());

    /* Clear local RX buffer to avoid having leftovers from previous receptions  This is not necessary but is included here to aid reading
     * the RX buffer.
     * This is a good place to put a breakpoint. Here (after first time through the loop) the local status register will be set for last event
     * and if a good receive has happened the data buffer will have the data in it, and frame_len will be set to the length of the RX frame. */
    memset(&data_packet, 0, sizeof(data_packet_t));

    /* Set response frame timeout. */
    // dwt_setrxtimeout(0);    // DISABLE TIMEOUT
    dwt_setrxtimeout(RX_TIMEOUT_UUS);

    /* Activate reception immediately. */
    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    /* Poll until a frame is properly received or an error/timeout occurs.
        * STATUS register is 5 bytes long but, as the event we are looking at is in the first byte of the register, we can use this simplest API
        * function to access it. */
    waitforsysstatus(&status_reg, NULL, (DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR), 0);

    p_diag_frame->status_reg = status_reg;

    if (status_reg & DWT_INT_RXFCG_BIT_MASK)
    {
        /* A frame has been received, copy it to our local buffer. */
        frame_len = dwt_getframelength();
        if (frame_len <= FRAME_LEN_MAX)
        {
            p_diag_frame->frame_len = frame_len;
            dwt_readrxdata(p_diag_frame->frame_buffer, frame_len, 0); /* read with the FCS/CRC. */
        }

        /* Clear good RX frame event in the DW IC status register. */
        // dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);

        /* check magic number */
        if (p_payload->magic == MAGIC_NUMBER)
        {
            // NRF_LOG_INFO("(%d) Frame Received: len = %d, seq=%d (SRC_ADDR: %d, ID:0x%02X, MAGIC:0x%08X)", rtc_m_get_time_ms(), frame_len, p_mhr->seq_num, p_mhr->src_addr, p_payload->id, p_payload->magic);
            ret = 0;
#if !WRITE_DIAG_TO_USB_ON_EVERY_RX_STATUS_EVENT_ENABLED
            ret_usb = read_diag_cir_to_usb();
#endif
        }
        else
        {
            // NRF_LOG_ERROR("(%d) Frame Received: len = %d (SRC_ADDR: %d, ID:0x%02X, MAGIC:0x%08X)", rtc_m_get_time_ms(), frame_len, p_mhr->src_addr, p_payload->id, p_payload->magic);
        }
    }
    else
    {

        /* just read it (even if it might be garbage)  */
        frame_len = dwt_getframelength();
        if (frame_len <= FRAME_LEN_MAX)
        {
            p_diag_frame->frame_len = frame_len;
            dwt_readrxdata(p_diag_frame->frame_buffer, frame_len, 0); /* read with the FCS/CRC. */
        }

        // // ->CHECK EACH STATUS REGISTER OR USING -> dwt_readeventcounters
        // // SFD timeout
        // if (status_reg & DWT_INT_RXSTO_BIT_MASK)
        // {
        //     NRF_LOG_ERROR("(%d) SFD timeout", rtc_m_get_time_ms());
        // }
        // // Preamble timeout
        // if (status_reg & DWT_INT_RXPTO_BIT_MASK)
        // {
        //     NRF_LOG_ERROR("(%d) Preamble timeout", rtc_m_get_time_ms());
        // }
        // // RX frame wait timeout
        // if (status_reg & DWT_INT_RXFTO_BIT_MASK)
        // {
        //     NRF_LOG_ERROR("(%d) RX frame wait timeout", rtc_m_get_time_ms());
        // }
        // // RX frame CRC error
        // if (status_reg & DWT_INT_RXFCE_BIT_MASK)
        // {
        //     NRF_LOG_ERROR("(%d) RX frame CRC error", rtc_m_get_time_ms());
        // }
        // // // add more error cases
        // // if (status_reg & ...)
        // // {
        // //     /* code */
        // // }
    }

#if WRITE_DIAG_TO_USB_ON_EVERY_RX_STATUS_EVENT_ENABLED
    ret_usb = read_diag_cir_to_usb();
#endif

    /* Clear RX error events in the DW IC status register. */
    dwt_writesysstatuslo(SYS_STATUS_ALL_RX_ERR);

    // /* Read event counters. */
    // dwt_deviceentcnts_t event_cnt;
    // dwt_readeventcounters(&event_cnt);
    // char print_buffer[256];
    // sprintf(print_buffer,
    //     "PHE:   %4i\n"
    //     "RSL:   %4i\n"
    //     "CRCG:  %4i\n"
    //     "CRCB:  %4i\n"
    //     "ARFE:  %4i\n"
    //     "OVER:  %4i\n"
    //     "SFDTO: %4i\n"
    //     "PTO:   %4i\n"
    //     "RTO:   %4i\n"
    //     "TXF:   %4i\n"
    //     "HPW:   %4i\n"
    //     "CRCE:  %4i\n"
    //     "PREJ:  %4i\n"
    //     "SFDD:  %4i\n"
    //     "STSE:  %4i\n",
    //     event_cnt.PHE,   // 12-bit number of received header error events
    //     event_cnt.RSL,   // 12-bit number of received frame sync loss event events
    //     event_cnt.CRCG,  // 12-bit number of good CRC received frame events
    //     event_cnt.CRCB,  // 12-bit number of bad CRC (CRC error) received frame events
    //     event_cnt.ARFE,   // 8-bit number of address filter error events
    //     event_cnt.OVER,   // 8-bit number of receive buffer overrun events (used in double buffer mode)
    //     event_cnt.SFDTO, // 12-bit number of SFD timeout events
    //     event_cnt.PTO,   // 12-bit number of Preamble timeout events
    //     event_cnt.RTO,    // 8-bit number of RX frame wait timeout events
    //     event_cnt.TXF,   // 12-bit number of transmitted frame events
    //     event_cnt.HPW,    // 8-bit half period warning events (when delayed RX/TX enable is used)
    //     event_cnt.CRCE,   // 8-bit SPI CRC error events
    //     event_cnt.PREJ,  // 12-bit number of Preamble rejection events
    //     event_cnt.SFDD,  // 12-bit SFD detection events ... only DW3720
    //     event_cnt.STSE   // 8-bit STS error/warning events
    // );
    // NRF_LOG_INFO("\n\n%s", print_buffer );

    if (!ret) {
        bsp_board_led_invert(BSP_BOARD_LED_1);
        if (!ret_usb) {
            bsp_board_led_invert(BSP_BOARD_LED_0);
        } else {
            bsp_board_led_off(BSP_BOARD_LED_0);
        }
    } else {
        bsp_board_leds_off();
    }
}

__attribute__((unused))
static void
simple_tx() {

    static uint8_t tx_buffer[FRAME_LEN_MAX];
    int frame_len;
    mhr_802_15_4_intra_pan_short_addr_t *p_mhr = (mhr_802_15_4_intra_pan_short_addr_t*)tx_buffer;
    frame_magic_data_t *p_payload = (frame_magic_data_t*)&tx_buffer[sizeof(mhr_802_15_4_intra_pan_short_addr_t)];

    NRF_LOG_INFO("(%d) start simple_tx", rtc_m_get_time_ms());

    /* mhr */
    /* frame_ctrl: (0x8841 to indicate a data frame using 16-bit addressing, and frame type of data, ack req: false).
        Frame Control Field: Data (0x8841)
        .... .... .... .001 = Frame Type: Data (0x0001)
        .... .... .... 0... = Security Enabled: False
        .... .... ...0 .... = Frame Pending: False
        .... .... ..0. .... = Acknowledge Request: False
        .... .... .1.. .... = Intra-PAN: True
        .... 10.. .... .... = Destination Addressing Mode: Short/16-bit (0x0002)
        ..00 .... .... .... = Frame Version: 0
        10.. .... .... .... = Source Addressing Mode: Short/16-bit (0x0002)
    */
    p_mhr->frame_ctrl[0] = 0x41;
    p_mhr->frame_ctrl[1] = 0x88;
    // p_mhr->seq_num;
    p_mhr->dst_pan_id = DEVICE_PAN_ID;
    p_mhr->dst_addr = 0xFFFF;
    p_mhr->dst_addr = DEVICE_SHORT_ADDR_RX;
    p_mhr->src_addr = DEVICE_SHORT_ADDR_TX;

    /* paylaod data */
    p_payload->id = MAGIC_ID;
    p_payload->magic = MAGIC_NUMBER;
    p_payload->tx_timestamp_ms = (uint32_t)rtc_m_get_time_ms();

    frame_len = sizeof(mhr_802_15_4_intra_pan_short_addr_t) + sizeof(frame_magic_data_t);

    /* Write frame data to DW IC and prepare transmission. */
    dwt_writetxdata(frame_len, tx_buffer, 0); /* Zero offset in TX buffer. */

    /* In this example since the length of the transmitted frame does not change,
        * nor the other parameters of the dwt_writetxfctrl function, the
        * dwt_writetxfctrl call could be outside the main while(1) loop.
        */
    dwt_writetxfctrl(frame_len + FCS_LEN, 0, 0); /* Zero offset in TX buffer, no ranging. */

    /* Start transmission. */
    dwt_starttx(DWT_START_TX_IMMEDIATE);
    /* Poll DW IC until TX frame sent event set.
        * STATUS register is 4 bytes long but, as the event we are looking
        * at is in the first byte of the register, we can use this simplest
        * API function to access it.*/
    waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0);

    /* Clear TX frame sent event. */
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);

    // NRF_LOG_INFO("(%d) TX Frame Sent", rtc_m_get_time_ms());

    /* Increment the blink frame sequence number (modulo 256). */
    p_mhr->seq_num++;

    p_payload->idx++;

    /* Execute a delay between transmissions. */
    vTaskDelay(pdMS_TO_TICKS(TX_DELAY_MS));

}

__attribute__((unused))
static void
simple_tx_disturb() {

    static uint8_t tx_buffer[FRAME_LEN_MAX];
    int frame_len;
    mhr_802_15_4_intra_pan_short_addr_t *p_mhr = (mhr_802_15_4_intra_pan_short_addr_t*)tx_buffer;
    frame_magic_data_t *p_payload = (frame_magic_data_t*)&tx_buffer[sizeof(mhr_802_15_4_intra_pan_short_addr_t)];

    // NRF_LOG_INFO("(%d) start simple_tx_disturb", rtc_m_get_time_ms());


    /* mhr */
    /* frame_ctrl: (0x8841 to indicate a data frame using 16-bit addressing, and frame type of data, ack req: false).
        Frame Control Field: Data (0x8841)
        .... .... .... .001 = Frame Type: Data (0x0001)
        .... .... .... 0... = Security Enabled: False
        .... .... ...0 .... = Frame Pending: False
        .... .... ..0. .... = Acknowledge Request: False
        .... .... .1.. .... = Intra-PAN: True
        .... 10.. .... .... = Destination Addressing Mode: Short/16-bit (0x0002)
        ..00 .... .... .... = Frame Version: 0
        10.. .... .... .... = Source Addressing Mode: Short/16-bit (0x0002)
    */
    p_mhr->frame_ctrl[0] = 0x41;
    p_mhr->frame_ctrl[1] = 0x88;
    // p_mhr->seq_num;
    p_mhr->dst_pan_id = DEVICE_PAN_ID;
    p_mhr->dst_addr = 0xFFFF;
    p_mhr->dst_addr = DEVICE_SHORT_ADDR_RX;
    p_mhr->src_addr = DEVICE_SHORT_ADDR_TX_DISTURB;

    /* paylaod data */
    p_payload->id = MAGIC_ID;
    p_payload->magic = MAGIC_NUMBER_DISTURB;

    frame_len = sizeof(mhr_802_15_4_intra_pan_short_addr_t) + sizeof(frame_magic_data_t);

    /* Write frame data to DW IC and prepare transmission. */
    dwt_writetxdata(frame_len, tx_buffer, 0); /* Zero offset in TX buffer. */

    /* In this example since the length of the transmitted frame does not change,
        * nor the other parameters of the dwt_writetxfctrl function, the
        * dwt_writetxfctrl call could be outside the main while(1) loop.
        */
    dwt_writetxfctrl(frame_len + FCS_LEN, 0, 0); /* Zero offset in TX buffer, no ranging. */

    /* Start transmission. */
    dwt_starttx(DWT_START_TX_IMMEDIATE);
    /* Poll DW IC until TX frame sent event set.
        * STATUS register is 4 bytes long but, as the event we are looking
        * at is in the first byte of the register, we can use this simplest
        * API function to access it.*/
    waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0);

    /* Clear TX frame sent event. */
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);

    // NRF_LOG_INFO("(%d) TX Frame Sent", rtc_m_get_time_ms());

    /* Increment the blink frame sequence number (modulo 256). */
    p_mhr->seq_num++;

    /* Execute a delay between transmissions. */
    Sleep(TX_DELAY_MS);
}


void app_main() {

    uwb_init();

    for ( ;; ) {

#if TX_DEVICE_ENABLED
        for ( ;; ) {
            simple_tx();
        }
#endif

#if RX_DEVICE_ENABLED
        for ( ;; ) {
            simple_rx();
        }
#endif

#if DISTURB_DEVICE_ENABLED
        for ( ;; ) {
            simple_tx_disturb();
        }
#endif

        // continue;

        // vTaskDelay(1000);
        // NRF_LOG_INFO("(%d) app_main running.", rtc_m_get_time_ms());

        // /* example to use usb out */
        // char str_buffer[256];
        // sprintf(str_buffer,"(%d) app_main running.\n", rtc_m_get_time_ms());
        // usb_m_write((uint8_t*)str_buffer, strlen(str_buffer));
    }
}
