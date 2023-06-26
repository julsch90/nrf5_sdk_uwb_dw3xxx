/*! ----------------------------------------------------------------------------
 * @file    deca_spi.c
 * @brief   SPI access functions
 *
 * @attention
 *
 * Copyright 2015 - 2021 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */

#include "deca_spi.h"
#include "deca_port.h"
#include <deca_device_api.h>

#include "nrf_drv_gpiote.h"
#include "nrf_drv_ppi.h"
#include "main.h"

// [INFO] Currently, only SPIM3 in nRF52840 supports the extended spi features (not used yet).
// TODO: seems like thats only working when not using sleep mode (not working yet 100 percent safe?)
/* control (set/clear) ss pin via ppi (improvement to prepare next spi trx) //  (nRF52840 supports the extended spi features) */
#define configUSE_PPI   0

#define SPIM_FREQ_SLOW      NRF_SPIM_FREQ_2M

#if defined(NRF52832_XXAA)
    #define SPIM_FREQ_FAST      NRF_SPIM_FREQ_8M
#elif defined(NRF52833_XXAA) || defined(NRF52840_XXAA)
    // #define SPIM_FREQ_FAST      NRF_SPIM_FREQ_16M
    #define SPIM_FREQ_FAST      NRF_SPIM_FREQ_32M
#endif


#if defined(NRF52832_XXAA)

#if defined(UWB_CIR)
#error "NRF52832 max 255 bytes spi buffer" // [NOTE]
#else
#define DATALEN1 240
#endif

#else

#if defined(UWB_CIR) // increase spi buffer needed for a complete cir register read
#define DATALEN1 (7*1024)
#else
#define DATALEN1 2048
#endif

#endif


static spi_handle_t spi_handler;
static spi_handle_t *pgSpiHandler = &spi_handler;

uint16_t  current_cs_pin=DW3000_CS_Pin;
uint16_t  current_irq_pin=DW3000_IRQn_Pin;

dw_t s = {
    .irqPin    = DW3000_IRQn_Pin,
    .rstPin    = DW3000_RESET_Pin,
    // .wkupPin   = DW3000_WAKEUP_Pin,
    .csPin     = DW3000_CS_Pin,    //'1' steady state
    .pSpi      = &spi_handler,
};

const dw_t *SPI = &s;

static volatile bool spi_xfer_done;
static uint8_t spi_init_stat = 0; // use 1 for slow, use 2 for fast;

static uint8_t idatabuf[DATALEN1] = { 0 }; // Never define this inside the Spi read/write
static uint8_t itempbuf[DATALEN1] = { 0 }; // As that will use the stack from the Task, which are not such long!!!!
                                           // You will face a crashes which are not expected!


#if (configUSE_PPI == 1)
nrf_ppi_channel_t m_ppi_channel_ss_pin_set;
nrf_ppi_channel_t m_ppi_channel_ss_pin_clr;
#endif

/****************************************************************************
 *
 *                              DW3000 SPI section
 *
 *******************************************************************************/


#if defined(NRF52832_XXAA) || defined(NRF52833_XXAA) || defined(NRF52840_XXAA)
/* @fn    dw_spi_init
 * Initialise   nRF52832-DK SPI
                nRF52833-DK SPI
                nRF52840-DK SPI
 * */
void
dw_spi_init(void) {

    nrf_drv_spi_t *spi_inst;
    nrf_drv_spi_config_t *spi_config;

    spi_handle_t *pSPI_handler = SPI->pSpi;

    pSPI_handler->frequency_slow = SPIM_FREQ_SLOW;
    pSPI_handler->frequency_fast = SPIM_FREQ_FAST;

    pSPI_handler->lock = DW_HAL_NODE_UNLOCKED;

    spi_inst = &pSPI_handler->spi_inst;
    spi_config = &pSPI_handler->spi_config;

    /* spi (up to 8 mhz) default */
    spi_inst->inst_idx = 1;
    spi_inst->use_easy_dma = SPI1_USE_EASY_DMA;
    spi_inst->u.spim.p_reg = NRF_SPIM1;
    spi_inst->u.spim.drv_inst_idx = NRFX_SPIM1_INST_IDX;

#if defined(NRF52833_XXAA) || defined(NRF52840_XXAA)
#if (SPIM_FREQ_FAST == NRF_SPIM_FREQ_16M) || (SPIM_FREQ_FAST == NRF_SPIM_FREQ_32M)
    /* spi (16 - 32 mhz) working only with spi3 on nrf52840 / nrf52833? */
    spi_inst->inst_idx = 3;
    spi_inst->use_easy_dma = SPI3_USE_EASY_DMA;
    spi_inst->u.spim.p_reg = NRF_SPIM3;
    spi_inst->u.spim.drv_inst_idx = NRFX_SPIM3_INST_IDX;
#endif
#endif

    spi_config->sck_pin = DW3000_CLK_Pin;
    spi_config->mosi_pin = DW3000_MOSI_Pin;
    spi_config->miso_pin = DW3000_MISO_Pin;
#if (configUSE_PPI == 1)
    spi_config->ss_pin   = NRF_DRV_SPI_PIN_NOT_USED;
#else
    spi_config->ss_pin   = DW3000_CS_Pin;
#endif
    spi_config->irq_priority = (APP_IRQ_PRIORITY_MID - 2);
    spi_config->orc = 0xFF;
    spi_config->frequency = pSPI_handler->frequency_slow;
    spi_config->mode = NRF_DRV_SPI_MODE_0;
    spi_config->bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;

    // Configure the chip select as an output pin that can be toggled
    nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_TASK_TOGGLE(NRF_GPIOTE_INITIAL_VALUE_HIGH);
    nrf_drv_gpiote_out_init(DW3000_CS_Pin, &out_config);

#if (configUSE_PPI == 1)

    nrf_drv_gpiote_out_task_enable(DW3000_CS_Pin);
    uint32_t err_code;

    /* init ppi */
    nrf_drv_ppi_init();

    /* configure available ppi channel to capture the TIMER counter on IRQ EVENT */
    err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel_ss_pin_clr);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel_ss_pin_set);
    APP_ERROR_CHECK(err_code);


    uint32_t spim_start_addr = nrf_spim_event_address_get(spi_inst->u.spim.p_reg, NRF_SPIM_EVENT_STARTED);
    uint32_t spim_end_addr = nrf_spim_event_address_get(spi_inst->u.spim.p_reg, NRF_SPIM_EVENT_END);

    // nrf_spim_shorts_enable(); // TODO: USE???

    uint32_t ss_in_clr_addr = nrf_drv_gpiote_clr_task_addr_get(DW3000_CS_Pin);
    uint32_t ss_in_set_addr = nrf_drv_gpiote_set_task_addr_get(DW3000_CS_Pin);

    err_code = nrf_drv_ppi_channel_assign(m_ppi_channel_ss_pin_clr, spim_start_addr, ss_in_clr_addr);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_ppi_channel_assign(m_ppi_channel_ss_pin_set, spim_end_addr, ss_in_set_addr);
    APP_ERROR_CHECK(err_code);

    /* enable configured ppi channel */
    err_code = nrf_drv_ppi_channel_enable(m_ppi_channel_ss_pin_clr);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_ppi_channel_enable(m_ppi_channel_ss_pin_set);
    APP_ERROR_CHECK(err_code);

#endif

}
#endif

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
static int
openspi(nrf_drv_spi_t *p_instance) {
    NRF_SPIM_Type *p_spi = p_instance->u.spim.p_reg;
    nrf_spim_enable(p_spi);
    return 0;
} // end openspi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
static int
closespi(nrf_drv_spi_t *p_instance) {
    NRF_SPIM_Type *p_spi = p_instance->u.spim.p_reg;
    nrf_spim_disable(p_spi);
    return 0;
} // end closespi()

/**
 * @brief SPI user event handler.
 * @param event
 */
void spi_event_handler(nrf_drv_spi_evt_t const *p_event, void *p_context)
{
    UNUSED_PARAMETER(p_event);
    UNUSED_PARAMETER(p_context);
    spi_xfer_done = true;
}

/* @fn      port_set_dw_ic_spi_slowrate
 * @brief   set 2MHz
 * */
void
port_set_dw_ic_spi_slowrate(void) {

    if(spi_init_stat) {
#if (configUSE_PPI == 1)
        closespi(&pgSpiHandler->spi_inst);
#endif
        nrf_drv_spi_uninit(&pgSpiHandler->spi_inst);
    }

    pgSpiHandler->spi_config.frequency = pgSpiHandler->frequency_slow;
    APP_ERROR_CHECK(nrf_drv_spi_init(&pgSpiHandler->spi_inst,
                                     &pgSpiHandler->spi_config,
#if (configUSE_PPI == 1)
                                     spi_event_handler, // using only as dummy (event not called because using NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER on trx)
#else
                                     NULL,
#endif
                                     NULL) );

    nrf_gpio_cfg(pgSpiHandler->spi_config.sck_pin,
                     NRF_GPIO_PIN_DIR_OUTPUT,
                     NRF_GPIO_PIN_INPUT_CONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_H0H1,
                     NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_cfg( pgSpiHandler->spi_config.mosi_pin,
                     NRF_GPIO_PIN_DIR_OUTPUT,
                     NRF_GPIO_PIN_INPUT_DISCONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_H0H1,
                     NRF_GPIO_PIN_NOSENSE);

    spi_init_stat = 1;

    nrf_delay_ms(2);

#if (configUSE_PPI == 1)
    openspi(&pgSpiHandler->spi_inst);
#endif
}

/* @fn      port_set_dw_ic_spi_fastrate
 * @brief   set 8MHz for SPI_1 (todo: 16mhz if nrf52840)
 *          up to 38 Mhz possible on dw3000
 * */
void
port_set_dw_ic_spi_fastrate(void) {

    if(spi_init_stat) {
#if (configUSE_PPI == 1)
        closespi(&pgSpiHandler->spi_inst);
#endif
        nrf_drv_spi_uninit(&pgSpiHandler->spi_inst);
    }

    pgSpiHandler->spi_config.frequency = pgSpiHandler->frequency_fast;
    APP_ERROR_CHECK(nrf_drv_spi_init(&pgSpiHandler->spi_inst,
                                     &pgSpiHandler->spi_config,
#if (configUSE_PPI == 1)
                                     spi_event_handler, // using only as dummy (event not called because using NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER on trx)
#else
                                     NULL,
#endif
                                     NULL) );

    nrf_gpio_cfg(pgSpiHandler->spi_config.sck_pin,
                     NRF_GPIO_PIN_DIR_OUTPUT,
                     NRF_GPIO_PIN_INPUT_CONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_H0H1,
                     NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_cfg( pgSpiHandler->spi_config.mosi_pin,
                     NRF_GPIO_PIN_DIR_OUTPUT,
                     NRF_GPIO_PIN_INPUT_DISCONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_H0H1,
                     NRF_GPIO_PIN_NOSENSE);

    spi_init_stat = 2;

    nrf_delay_ms(2);

#if (configUSE_PPI == 1)
    openspi(&pgSpiHandler->spi_inst);
#endif
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetospiwithcrc()
 *
 * Low level abstract function to write to the SPI when SPI CRC mode is used
 * Takes two separate byte buffers for write header and write data, and a CRC8 byte which is written last
 * returns 0 for success, or -1 for error
 */
int
writetospiwithcrc(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength, const uint8_t *bodyBuffer, uint8_t crc8) {

#ifdef DWT_ENABLE_CRC
    uint8_t *p1;
    uint32_t idatalength = headerLength + bodyLength + sizeof(crc8); // It cannot be more than 255 in total length (header + body)

    if (idatalength > DATALEN1) {
        return NRF_ERROR_NO_MEM;
    }

    while(pgSpiHandler->lock);

    __HAL_LOCK(pgSpiHandler);

    openspi(&pgSpiHandler->spi_inst);

    p1 = idatabuf;
    memcpy(p1, headerBuffer, headerLength);
    p1 += headerLength;
    memcpy(p1, bodyBuffer, bodyLength);
    p1 += bodyLength;
    memcpy(p1, &crc8, 1);

    nrfx_gpiote_out_toggle(current_cs_pin);

    // spi_xfer_done = false;
    nrf_drv_spi_transfer(&pgSpiHandler->spi_inst, idatabuf, idatalength, itempbuf, idatalength);
    // while (!spi_xfer_done);

    closespi(&pgSpiHandler->spi_inst);
    nrfx_gpiote_out_toggle(current_cs_pin);

    __HAL_UNLOCK(pgSpiHandler);
#endif //DWT_ENABLE_CRC
    return 0;
} // end writetospiwithcrc()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetospi()
 *
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success, or -1 for error
 */
int
writetospi(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength, const uint8_t *bodyBuffer) {

    uint8_t *p1;
    uint32_t idatalength = headerLength + bodyLength;

    if (idatalength > DATALEN1) {
        return NRF_ERROR_NO_MEM;
    }

    // while(pgSpiHandler->lock);
    // __HAL_LOCK(pgSpiHandler);

    // TODO: some improvement when preload other buffer (double buffering)?
#if (configUSE_PPI == 1)
    /* wait for finish last trx */
    while ( nrf_gpio_pin_read(DW3000_CS_Pin) == 0 ) { }
    // while ( nrf_gpio_pin_out_read(DW3000_CS_Pin) == 0 ) { }
    // while (nrf_spim_event_check(pgSpiHandler->spi_inst.u.spim.p_reg, NRF_SPIM_EVENT_STARTED)){ }

#endif

    p1 = idatabuf;
    memcpy(p1, headerBuffer, headerLength);
    p1 += headerLength;
    memcpy(p1, bodyBuffer, bodyLength);

    // nrfx_gpiote_out_toggle(current_cs_pin);
    // nrfx_gpiote_out_clear(current_cs_pin);

    nrfx_spim_xfer_desc_t const spim_xfer_desc = NRFX_SPIM_XFER_TX(idatabuf, idatalength);

#if (configUSE_PPI == 1) // TODO: check with ppi (check errata spim3 ppi)
    nrfx_spim_xfer(&pgSpiHandler->spi_inst.u.spim, &spim_xfer_desc, NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER); // need to set interrupt
    // while (!nrf_spim_event_check(pgSpiHandler->spi_inst.u.spim.p_reg, NRF_SPIM_EVENT_END)){ }
#else
    openspi(&pgSpiHandler->spi_inst);
    nrfx_spim_xfer(&pgSpiHandler->spi_inst.u.spim, &spim_xfer_desc, 0);
    closespi(&pgSpiHandler->spi_inst);

    // errata #195, nRF52840_Rev_3_Errata_v1.1
#if defined(NRF52840_XXAA)
    *(volatile uint32_t *)0x4002F004 = 1;
#endif

#endif

    // nrfx_gpiote_out_set(current_cs_pin);
    // nrfx_gpiote_out_toggle(current_cs_pin);

    // __HAL_UNLOCK(pgSpiHandler);

    return 0;
} // end writetospi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: readfromspi()
 *
 * Low level abstract function to read from the SPI
 * Takes two separate byte buffers for write header and read data
 * returns the offset into read buffer where first byte of read data may be found,
 * or returns -1 if there was an error
 */
int readfromspi(uint16_t headerLength, uint8_t *headerBuffer, uint16_t readLength, uint8_t *readBuffer)
{
    uint8_t *p1;
    uint32_t idatalength = headerLength + readLength;

    if (idatalength > DATALEN1) {
        return NRF_ERROR_NO_MEM;
    }

    // while(pgSpiHandler->lock);
    // __HAL_LOCK(pgSpiHandler);

#if (configUSE_PPI == 1)
    /* wait for finish last trx */
    while ( nrf_gpio_pin_read(DW3000_CS_Pin) == 0 ) { }
    // while ( nrf_gpio_pin_out_read(DW3000_CS_Pin) == 0 ) { }
    // while (nrf_spim_event_check(pgSpiHandler->spi_inst.u.spim.p_reg, NRF_SPIM_EVENT_STARTED)){ }
#endif

    p1 = idatabuf;
    memcpy(p1, headerBuffer, headerLength);

    p1 += headerLength;
    memset(p1, 0x00, readLength);

    idatalength = headerLength + readLength;

    // nrfx_gpiote_out_toggle(current_cs_pin);
    // nrfx_gpiote_out_clear(current_cs_pin);

    nrfx_spim_xfer_desc_t const spim_xfer_desc = NRFX_SPIM_XFER_TRX(idatabuf, idatalength, itempbuf, idatalength);

#if (configUSE_PPI == 1)
    nrfx_spim_xfer(&pgSpiHandler->spi_inst.u.spim, &spim_xfer_desc, NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER); // need to set interrupt
    /* wait for rx data available (end of spi) */
    while (!nrf_spim_event_check(pgSpiHandler->spi_inst.u.spim.p_reg, NRF_SPIM_EVENT_END)){ }
#else
    openspi(&pgSpiHandler->spi_inst);
    nrfx_spim_xfer(&pgSpiHandler->spi_inst.u.spim, &spim_xfer_desc, 0);
    closespi(&pgSpiHandler->spi_inst);

    // errata #195, nRF52840_Rev_3_Errata_v1.1
#if defined(NRF52840_XXAA)
    *(volatile uint32_t *)0x4002F004 = 1;
#endif

#endif

    // nrfx_gpiote_out_set(current_cs_pin);
    // nrfx_gpiote_out_toggle(current_cs_pin);

    p1 = itempbuf + headerLength;
    memcpy(readBuffer, p1, readLength);

    // __HAL_UNLOCK(pgSpiHandler);

    return 0;
} // end readfromspi()

/****************************************************************************
 *
 *                              END OF DW3000 SPI section
 *
 *******************************************************************************/
