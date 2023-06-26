/*! ----------------------------------------------------------------------------
 * @file    deca_shared_functions.h
 * @brief   Global functions are found here
 *
 * @attention
 *
 * Copyright 2013 - 2021 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 */

#include <deca_config_options.h>
#include <deca_device_api.h>
#include <deca_types.h>
#include <deca_regs.h>
#include <deca_port.h>
#include <deca_shared_defines.h>
#include <deca_shared_functions.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

extern dwt_config_t config_options;

/*Reference look-up table to calculate TxPower boost depending on frame duration
 * Using two different tables as per logarithmic calculation:
 *   - 1000us to 200us range - index unit of 25us
 *   - 200us to 70us -- index unit of 10us
 * The table values are in steps 0f 0.1dB
 * This allow to have a maximum granularity of 0.5dB between two frame durations, which is
 * in the magnitude of the DW3XXX TxOutput setting.
 */

const uint8_t txpower_boost_per_frame_duration_1000_200_us[LUT_1000_200_US_NUM] = {
    0,  // 1000us
    1,  // 975us  -> 1*0.1dB boost between 975us and 1000us frames
    2,  // 950us  -> 2*0.1dB boost between 950us and 1000us frames
    3,  // 925us
    4,  // 900us
    5,  // 875us
    6,  // 850us
    7,  // 825us
    8,  // 800us
    9,  // 775us
    10, // 750us
    11, // 725us
    13, // 700us
    15, // 675us
    17, // 650us
    19, // 625us
    21, // 600us
    23, // 575us
    25, // 550us
    27, // 525us
    29, // 500us
    31, // 475us
    33, // 450us
    35, // 425us
    38, // 400us
    41, // 375us
    44, // 350us
    47, // 325us
    50, // 300us
    54, // 275us
    58, // 250us
    63, // 225us
    68  // 200us
};

const uint8_t txpower_boost_per_frame_duration_200_70_us[LUT_200_70_US_NUM] = {
    68,  // 200us  -> 68*0.1dB boost between 200us frame and 1000us frame.
    70,  // 190us  -> 70*0.1dB boost between 190us and 1000us frame
    72,  // 180us
    74,  // 170us
    77,  // 160us
    80,  // 150us
    83,  // 140us
    86,  // 130us
    89,  // 120us
    93,  // 110us
    97,  // 100us
    102, // 90us
    107, // 80us
    113  // 70us
};





// -------------------------------------------------------------------------------------------------------------------
// Macros and Enumerations for SPI & CLock blocks
//
#define DW3000_SPI_FAC      (0<<6 | 1<<0)
#define DW3000_SPI_FARW     (0<<6 | 0<<0)
#define DW3000_SPI_EAMRW    (1<<6)


/*! ------------------------------------------------------------------------------------------------------------------
* @brief  this function is used to read/write to the DW3000 device registers
*
* input parameters:
* @param recordNumber  - ID of register file or buffer being accessed
* @param index         - byte index into register file or buffer being accessed
* @param length        - number of bytes being written
* @param buffer        - pointer to buffer containing the 'length' bytes to be written
* @param rw            - DW3000_SPI_WR_BIT/DW3000_SPI_RD_BIT
*
* no return value
*/
static
void dwt_xfer3000
(
    const uint32_t    regFileID,  //0x0, 0x04-0x7F ; 0x10000, 0x10004, 0x10008-0x1007F; 0x20000 etc
    const uint16_t    indx,       //sub-index, calculated from regFileID 0..0x7F,
    const uint16_t    length,
    uint8_t           *buffer,
    const spi_modes_e mode
)
{
    uint8_t  header[2];           // Buffer to compose header in
    uint16_t cnt = 0;             // Counter for length of a header

    uint16_t reg_file     = 0x1F & ((regFileID + indx) >> 16);
    uint16_t reg_offset   = 0x7F &  (regFileID + indx);

    assert(reg_file     <= 0x1F);
    assert(reg_offset   <= 0x7F);
    assert(length       < 0x3100);
    assert(mode == DW3000_SPI_WR_BIT ||\
           mode == DW3000_SPI_RD_BIT ||\
           mode == DW3000_SPI_AND_OR_8 ||\
           mode == DW3000_SPI_AND_OR_16 ||\
           mode == DW3000_SPI_AND_OR_32);

    // Write message header selecting WRITE operation and addresses as appropriate
    uint16_t  addr;
    addr = (reg_file << 9) | (reg_offset << 2);

    header[0] = (uint8_t)((mode | addr) >> 8);//  & 0xFF; //bit7 + addr[4:0] + sub_addr[6:6]
    header[1] = (uint8_t)(addr | (mode & 0x03));// & 0xFF; //EAM: subaddr[5:0]+ R/W/AND_OR

    if (/*reg_offset == 0 && */length == 0)
    {   /* Fast Access Commands (FAC)
         * only write operation is possible for this mode
         * bit_7=one is W operation, bit_6=zero: FastAccess command, bit_[5..1] addr, bits_0=one: MODE of FastAccess
         */
        assert(mode == DW3000_SPI_WR_BIT);

        header[0] = (uint8_t)((DW3000_SPI_WR_BIT>>8) | (regFileID<<1) | DW3000_SPI_FAC);
        cnt = 1;
    }
    else if (reg_offset == 0 /*&& length > 0*/ && (mode == DW3000_SPI_WR_BIT || mode == DW3000_SPI_RD_BIT))
    {   /* Fast Access Commands with Read/Write support (FACRW)
         * bit_7 is R/W operation, bit_6=zero: FastAccess command, bit_[5..1] addr, bits_0=zero: MODE of FastAccess
         */
        header[0] |= DW3000_SPI_FARW;
        cnt = 1;
    }
    else
    {   /* Extended Address Mode with Read/Write support (EAMRW)
         * b[0] = bit_7 is R/W operation, bit_6 one = ExtendedAddressMode;
         * b[1] = addr<<2 | (mode&0x3)
         */
        header[0] |= DW3000_SPI_EAMRW;
        cnt = 2;
    }

    switch (mode)
    {
    case    DW3000_SPI_AND_OR_8:
    case    DW3000_SPI_AND_OR_16:
    case    DW3000_SPI_AND_OR_32:
    case    DW3000_SPI_WR_BIT:
    {
        // uint8_t crc8 = 0;
        // if (pdw3000local->spicrc != DWT_SPI_CRC_MODE_NO)
        // {
        //     //generate 8 bit CRC
        //     crc8 = dwt_generatecrc8(header, cnt, 0);
        //     crc8 = dwt_generatecrc8(buffer, length, crc8);

        //     // Write it to the SPI
        //     writetospiwithcrc(cnt, header, length, buffer, crc8);
        // }
        // else
        // {
            // Write it to the SPI
            writetospi(cnt, header, length, buffer);
        // }
        break;
    }
    case DW3000_SPI_RD_BIT:
        {
            readfromspi(cnt, header, length, buffer);

            // //check that the SPI read has correct CRC-8 byte
            // //also don't do for SPICRC_CFG_ID register itself to prevent infinite recursion
            // if ((pdw3000local->spicrc == DWT_SPI_CRC_MODE_WRRD) && (regFileID != SPICRC_CFG_ID))
            // {
            //     uint8_t crc8, dwcrc8;
            //     //generate 8 bit CRC from the read data
            //     crc8 = dwt_generatecrc8(header, cnt, 0);
            //     crc8 = dwt_generatecrc8(buffer, length, crc8);

            //     //read the CRC that was generated in the DW3000 for the read transaction
            //     dwcrc8 = dwt_read8bitoffsetreg(SPICRC_CFG_ID, 0);

            //     //if the two CRC don't match report SPI read error
            //     //potential problem in callback if it will try to read/write SPI with CRC again.
            //     if (crc8 != dwcrc8)
            //     {
            //         if (pdw3000local->cbSPIRDErr != NULL)
            //             pdw3000local->cbSPIRDErr();
            //     }

            // }
            break;
        }
    default:
        while(1);
        break;
    }

} // end dwt_xfer3000()

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to write to the DW3000 device registers
 *
 * input parameters:
 * @param recordNumber  - ID of register file or buffer being accessed
 * @param index         - byte index into register file or buffer being accessed
 * @param length        - number of bytes being written
 * @param buffer        - pointer to buffer containing the 'length' bytes to be written
 *
 * output parameters
 *
 * no return value
 */
static
void dwt_writetodevice
(
    uint32_t      regFileID,
    uint16_t      index,
    uint16_t      length,
    uint8_t       *buffer
)
{
    dwt_xfer3000(regFileID, index, length, buffer, DW3000_SPI_WR_BIT);
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to read from the DW3000 device registers
 *
 * @param recordNumber  - ID of register file or buffer being accessed
 * @param index         - byte index into register file or buffer being accessed
 * @param length        - number of bytes being read
 * @param buffer        - pointer to buffer in which to return the read data.
 *
 * output parameters
 *
 * no return value
 */
static
void dwt_readfromdevice
(
    uint32_t  regFileID,
    uint16_t  index,
    uint16_t  length,
    uint8_t   *buffer
)
{
    dwt_xfer3000(regFileID, index, length, buffer, DW3000_SPI_RD_BIT);
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to read 32-bit value from the DW3000 device registers
 *
 * input parameters:
 * @param regFileID - ID of register file or buffer being accessed
 * @param regOffset - the index into register file or buffer being accessed
 *
 * output parameters
 *
 * returns 32 bit register value
 */
static
uint32_t dwt_read32bitoffsetreg(int regFileID, int regOffset)
{
    int     j ;
    uint32_t  regval = 0 ;
    uint8_t   buffer[4] ;

    dwt_readfromdevice(regFileID,regOffset,4,buffer); // Read 4 bytes (32-bits) register into buffer

    for (j = 3 ; j >= 0 ; j --)
    {
        regval = (regval << 8) + buffer[j] ;
    }

    return (regval);

} // end dwt_read32bitoffsetreg()

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to read 16-bit value from the DW3000 device registers
 *
 * input parameters:
 * @param regFileID - ID of register file or buffer being accessed
 * @param regOffset - the index into register file or buffer being accessed
 *
 * output parameters
 *
 * returns 16 bit register value
 */
static
uint16_t dwt_read16bitoffsetreg(int regFileID,int regOffset)
{
    uint16_t  regval = 0 ;
    uint8_t   buffer[2] ;

    dwt_readfromdevice(regFileID,regOffset,2,buffer); // Read 2 bytes (16-bits) register into buffer

    regval = ((uint16_t)buffer[1] << 8) + buffer[0] ;
    return regval ;

} // end dwt_read16bitoffsetreg()

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to read an 8-bit value from the DW3000 device registers
 *
 * input parameters:
 * @param regFileID - ID of register file or buffer being accessed
 * @param regOffset - the index into register file or buffer being accessed
 *
 * output parameters
 *
 * returns 8-bit register value
 */
__attribute__((unused)) static
uint8_t dwt_read8bitoffsetreg(int regFileID, int regOffset)
{
    uint8_t regval;

    dwt_readfromdevice(regFileID, regOffset, 1, &regval);

    return regval ;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to write 32-bit value to the DW3000 device registers
 *
 * input parameters:
 * @param regFileID - ID of register file or buffer being accessed
 * @param regOffset - the index into register file or buffer being accessed
 * @param regval    - the value to write
 *
 * output parameters
 *
 * no return value
 */
__attribute__((unused)) static
void dwt_write32bitoffsetreg(int regFileID, int regOffset, uint32_t regval)
{
    int     j ;
    uint8_t   buffer[4] ;

    for ( j = 0 ; j < 4 ; j++ )
    {
        buffer[j] = (uint8_t)regval;
        regval >>= 8 ;
    }

    dwt_writetodevice(regFileID,regOffset,4,buffer);
} // end dwt_write32bitoffsetreg()

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to write 16-bit value to the DW3000 device registers
 *
 * input parameters:
 * @param regFileID - ID of register file or buffer being accessed
 * @param regOffset - the index into register file or buffer being accessed
 * @param regval    - the value to write
 *
 * output parameters
 *
 * no return value
 */
__attribute__((unused)) static
void dwt_write16bitoffsetreg(int regFileID, int regOffset, uint16_t regval)
{
    uint8_t   buffer[2] ;

    buffer[0] = (uint8_t)regval;
    buffer[1] = regval >> 8 ;

    dwt_writetodevice(regFileID,regOffset,2,buffer);
} // end dwt_write16bitoffsetreg()

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief  this function is used to write an 8-bit value to the DW3000 device registers
 *
 * input parameters:
 * @param regFileID - ID of register file or buffer being accessed
 * @param regOffset - the index into register file or buffer being accessed
 * @param regval    - the value to write
 *
 * output parameters
 *
 * no return value
 */
__attribute__((unused)) static
void dwt_write8bitoffsetreg(int regFileID, int regOffset, uint8_t regval)
{
    //uint8_t   buf[1];
    //buf[0] = regval;
    dwt_writetodevice(regFileID, regOffset, 1, &regval);
}






























/*! ------------------------------------------------------------------------------------------------------------------
 * @fn calculate_power_boost()
 *
 * @brief Calculation the allowed power boost for a frame_duration_us frame relatively to a 1ms frame.
 *
 * @param reg: uint16_t duration of frame..
 *
 * @return boost: the amount of boost in 0.1dB steps which is allowed when transmitting the frame_dur_us frame
 *                relatively to a 1ms frame. For example, if the frame duration is 500us, then relatively to 1ms,
 *                a 3dB boost is allowed, and the function will return 30.
 */
uint8_t calculate_power_boost(uint16_t frame_duration_us)
{

    const uint8_t *lut = NULL;
    uint16_t lut_i;
    uint16_t lut_num;
    uint16_t lut_min;
    uint16_t lut_step;
    uint16_t limit;

    // If the frame is longer than the reference duration, then no boost to apply
    if (frame_duration_us >= FRAME_DURATION_REF)
    {
        return LUT_1000_200_US_MIN_BST;
    }
    else if (frame_duration_us < LUT_200_70_US_MIN) // If frame shorter than 70us apply the maximum boost
    {
        return LUT_200_70_US_MAX_BST;
    }
    else if (frame_duration_us > LUT_1000_200_US_MIN) // Select LUT table for frame 1000us > duration > 200us
    {
        lut_num = LUT_1000_200_US_NUM;
        lut_min = LUT_1000_200_US_MIN;
        lut_step = LUT_1000_200_US_STEP;
        lut = txpower_boost_per_frame_duration_1000_200_us;
    }
    else // Select LUT table for frame 200us > duration > 70us
    {
        lut_num = LUT_200_70_US_NUM;
        lut_min = LUT_200_70_US_MIN;
        lut_step = LUT_200_70_US_STEP;
        lut = txpower_boost_per_frame_duration_200_70_us;
    }

    // Calculating the LUT index corresponding to the frame duration
    lut_i = (lut_num - (frame_duration_us - lut_min) / lut_step);
    limit = (lut_num - lut_i) * lut_step + lut_min;

    // Selecting the index that gives the closest LUT duration to the one passed as argument.
    if (abs(frame_duration_us - limit) > lut_step / 2)
    {
        lut_i--;
    }

    // Boost is stored in the LUT at the calculated index - 1.
    // -1 to account for index 0
    // lut_i cannot be == 0 or > lut_num here
    return lut[lut_i - 1];
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn check_for_status_errors()
 *
 * @brief This function is used to get a value to increase the delay timer by dependent on the current TX preamble length set.
 *
 * @param reg: uint32_t value representing the current status register value.
 * @param errors: pointer to a uint32_t buffer that contains the sum of different errors logged during program operation.
 *
 * @return none
 */
void check_for_status_errors(uint32_t reg, uint32_t *errors)
{
    uint16_t stsStatus = 0;

    if (!(reg & DWT_INT_RXFCG_BIT_MASK))
    {
        errors[BAD_FRAME_ERR_IDX] += 1;
    }

    if (reg & DWT_INT_RXFSL_BIT_MASK)
    {
        errors[RSE_ERR_IDX] += 1;
    }

    if (reg & DWT_INT_RXPHE_BIT_MASK)
    {
        errors[PHE_ERR_IDX] += 1;
    }

    if (reg & DWT_INT_RXPTO_BIT_MASK)
    {
        errors[PTO_ERR_IDX] += 1;
    }

    if (reg & DWT_INT_ARFE_BIT_MASK)
    {
        errors[ARFE_ERR_IDX] += 1;
    }

    if ((reg & DWT_INT_RXFR_BIT_MASK) && !(reg & DWT_INT_RXFCG_BIT_MASK))
    {
        errors[CRC_ERR_IDX] += 1;
    }

    if ((reg & DWT_INT_RXFTO_BIT_MASK) || (reg & SYS_STATUS_ALL_RX_TO))
    {
        errors[RTO_ERR_IDX] += 1;
    }

    if (reg & DWT_INT_RXSTO_BIT_MASK)
    {
        errors[SFDTO_ERR_IDX] += 1;
    }

    if (reg & DWT_INT_CPERR_BIT_MASK)
    {
        // There is a general STS error
        errors[STS_PREAMBLE_ERR] += 1;

        // Get the status for a more detailed error reading of what went wrong with the STS
        dwt_readstsstatus(&stsStatus, 0);
        if (stsStatus & 0x100)
        {
            // Peak growth rate warning
            errors[STS_PEAK_GROWTH_RATE_ERR] += 1;
        }
        if (stsStatus & 0x080)
        {
            // ADC count warning
            errors[STS_ADC_COUNT_ERR] += 1;
        }
        if (stsStatus & 0x040)
        {
            // SFD count warning
            errors[STS_SFD_COUNT_ERR] += 1;
        }
        if (stsStatus & 0x020)
        {
            // Late first path estimation
            errors[STS_LATE_FIRST_PATH_ERR] += 1;
        }
        if (stsStatus & 0x010)
        {
            // Late coarse estimation
            errors[STS_LATE_COARSE_EST_ERR] += 1;
        }
        if (stsStatus & 0x008)
        {
            // Coarse estimation empty
            errors[STS_COARSE_EST_EMPTY_ERR] += 1;
        }
        if (stsStatus & 0x004)
        {
            // High noise threshold
            errors[STS_HIGH_NOISE_THREASH_ERR] += 1;
        }
        if (stsStatus & 0x002)
        {
            // Non-triangle
            errors[STS_NON_TRIANGLE_ERR] += 1;
        }
        if (stsStatus & 0x001)
        {
            // Logistic regression failed
            errors[STS_LOG_REG_FAILED_ERR] += 1;
        }
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_preamble_time()
 *
 * @brief This function is used to get a value of preamble time approx in us.
 *
 * @param None
 *
 * @return preamble_time - a uint32_t value.
 */
uint32_t get_preamble_time(void)
{
    uint32_t delay_time = 10;

    switch (config_options.txPreambLength)
    {
    case DWT_PLEN_64:
        delay_time += 64;
        break;
    case DWT_PLEN_128:
        delay_time += 128;
        break;
    case DWT_PLEN_256:
        delay_time += 256;
        break;
    case DWT_PLEN_512:
        delay_time += 512;
        break;
    case DWT_PLEN_1024:
        delay_time += 1024;
        break;
    case DWT_PLEN_1536:
        delay_time += 1536;
        break;
    case DWT_PLEN_2048:
        delay_time += 2048;
        break;
    case DWT_PLEN_4096:
        delay_time += 4096;
        break;
    case DWT_PLEN_32:
        delay_time += 32;
        break;
    case DWT_PLEN_72:
        delay_time += 72;
        break;
    default:
        delay_time = 0;
        break;
    }

    return delay_time;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_delay_time_txpreamble()
 *
 * @brief This function is used to get a value to increase the delay timer by dependent on the current TX preamble length set.
 *
 * @param None
 *
 * @return delay_time - a uint32_t value indicating the required increase needed to delay the time by.
 */
uint32_t get_rx_delay_time_txpreamble(void)
{
    uint32_t delay_time = 0;
    /* Standard delay values for preamble lengths of 32, 64, 72 & 128 should be adequate.
     * Additional time delay will be needed for larger preamble lengths.
     * Delay required is dependent on the preamble length as it increases the frame length. */
    switch (config_options.txPreambLength)
    {
    case DWT_PLEN_128:
        delay_time += 96; /* 128 - 64 */
        break;
    case DWT_PLEN_256:
        delay_time += 128; /* 256 - 128 */
        break;
    case DWT_PLEN_512:
        delay_time += 384; /* 512 - 128 */
        break;
    case DWT_PLEN_1024:
        delay_time += 896; /* 1024 - 128 */
        break;
    case DWT_PLEN_1536:
        delay_time += 1408; /* 1536 - 128 */
        break;
    case DWT_PLEN_2048:
        delay_time += 1920; /* 2048 - 128 */
        break;
    case DWT_PLEN_4096:
        delay_time += 3968; /* 4096 - 128 */
        break;
    case DWT_PLEN_32:
    case DWT_PLEN_64:
    case DWT_PLEN_72:
        break;
    default:
        break;
    }

    return delay_time;
}
/*!
* @fn uint32_t get_sts_time()
* @brief
*
* @param None
*
* @return
*/
uint32_t get_sts_time(dwt_sts_mode_e sts_mode)
{
    uint32_t delay_time = 0;

    // if(config_options.stsMode) // TODO: use config_options.stsMode
    if(sts_mode)
    {
        delay_time = ((1<<(config_options.stsLength+2))*8);
    }

    return delay_time;
}
/*!
* @fn uint16_t get_payload_time()
* @brief calc the time duration in us depend by payload len
*
* @param uint16_t - payload len
*
* @return uint16_t - payload time duration [us]
*/
uint16_t get_payload_time(uint16_t payload_len)
{
    uint16_t time;

    // TODO: 850 kbit/s

    time = (uint16_t)(1.172f*payload_len + 26.60f);

    return time;
}
/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_delay_time_data_rate()
 *
 * @brief This function is used to get a value to increase the delay timer by dependent on the current data rate set.
 *
 * @param None
 *
 * @return delay_time - a uint32_t value indicating the required increase needed to delay the time by.
 */
uint32_t get_rx_delay_time_data_rate(void)
{
    uint32_t delay_time = 0;
    /*
     * If data rate is set to 850k (slower rate),
     * increase the delay time
     */
    switch (config_options.dataRate)
    {
    case DWT_BR_850K:
        delay_time += 200;
        break;
    case DWT_BR_6M8:
    default:
        break;
    }

    return delay_time;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn set_delayed_rx_time()
 *
 * @brief This function is used to set the delayed RX time before running dwt_rxenable()
 *
 * @param delay - This is a defined delay value (usually POLL_TX_TO_RESP_RX_DLY_UUS)
 * @param config_options - pointer to dwt_config_t configuration structure that is in use at the time this function
 *                         is called.
 *
 * @return None
 */
void set_delayed_rx_time(uint32_t delay, dwt_config_t *config_options)
{
    uint32_t delay_time = delay;

    switch (config_options->txPreambLength)
    {
    case DWT_PLEN_32:
        delay_time -= 32;
        break;
    case DWT_PLEN_64:
        delay_time -= 64;
        break;
    case DWT_PLEN_72:
        delay_time -= 72;
        break;
    case DWT_PLEN_128:
        delay_time -= 128;
        break;
    case DWT_PLEN_256:
        delay_time -= 256;
        break;
    case DWT_PLEN_512:
        delay_time -= 512;
        break;
    case DWT_PLEN_1024:
        delay_time -= 1024;
        break;
    case DWT_PLEN_1536:
        delay_time -= 1536;
        break;
    case DWT_PLEN_2048:
    case DWT_PLEN_4096:
        delay_time -= 2048;
        break;
    default:
        break;
    }

    /* Length of the STS effects the size of the frame also.
     * This means the delay required is greater for larger STS lengths. */
    delay_time += ((1 << (config_options->stsLength + 2)) * 8);

    dwt_setdelayedtrxtime((uint32_t)((delay_time * UUS_TO_DWT_TIME) >> 8));
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn set_resp_rx_timeout()
 *
 * @brief This function is used to set the RX timeout value
 *
 * @param delay - This is a defined delay value (usually RESP_RX_TIMEOUT_UUS)
 * @param config_options - pointer to dwt_config_t configuration structure that is in use at the time this function
 *                         is called.
 *
 * @return None
 */
void set_resp_rx_timeout(uint32_t delay, dwt_config_t *config_options)
{
    /*
     * The program will need to adjust the timeout value depending on the size of the frame
     * Different sized frames require different time delays.
     */
    uint32_t delay_time = delay + get_rx_delay_time_data_rate() + get_rx_delay_time_txpreamble() + 500;

    /* Length of the STS effects the size of the frame also.
     * This means the delay required is greater for larger STS lengths. */
    switch (config_options->stsLength)
    {
    case DWT_STS_LEN_256:
    case DWT_STS_LEN_512:
    case DWT_STS_LEN_1024:
    case DWT_STS_LEN_2048:
        delay_time += ((1 << (config_options->stsLength + 2)) * 8);
        break;
    case DWT_STS_LEN_32:
    case DWT_STS_LEN_64:
    case DWT_STS_LEN_128:
    default:
        break;
    }

    dwt_setrxtimeout(delay_time);
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn resp_msg_get_ts()
 *
 * @brief Read a given timestamp value from the response message. In the timestamp fields of the response message, the
 *        least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to get
 *         ts  timestamp value
 *
 * @return none
 */
void resp_msg_get_ts(uint8_t *ts_field, uint32_t *ts)
{
    int i;
    *ts = 0;
    for (i = 0; i < RESP_MSG_TS_LEN; i++)
    {
        *ts += (uint32_t)ts_field[i] << (i * 8);
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_tx_timestamp_u64()
 *
 * @brief Get the TX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
uint64_t get_tx_timestamp_u64(void)
{
    uint8_t ts_tab[5];
    uint64_t ts = 0;
    int8_t i;
    dwt_readtxtimestamp(ts_tab);
    for (i = 4; i >= 0; i--)
    {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_timestamp_u64()
 *
 * @brief Get the RX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
uint64_t get_rx_timestamp_u64(void)
{
    uint8_t ts_tab[5];
    uint64_t ts = 0;
    int8_t i;
    dwt_readrxtimestamp(ts_tab);
    for (i = 4; i >= 0; i--)
    {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_timestamp_ipatov_u64()
 *
 * @brief Get the RX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
uint64_t get_rx_timestamp_ipatov_u64(void)
{
    uint8_t ts_tab[5];
    uint64_t ts = 0;
    int8_t i;
    dwt_readrxtimestamp_ipatov(ts_tab);
    for (i = 4; i >= 0; i--)
    {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_get_ts()
 *
 * @brief Read a given timestamp value from the final message. In the timestamp fields of the final message, the least
 *        significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to read
 *         ts  timestamp value
 *
 * @return none
 */
void final_msg_get_ts(const uint8_t *ts_field, uint32_t *ts)
{
    uint8_t i;
    *ts = 0;
    for (i = 0; i < FINAL_MSG_TS_LEN; i++)
    {
        *ts += ((uint32_t)ts_field[i] << (i * 8));
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_set_ts()
 *
 * @brief Fill a given timestamp field in the final message with the given value. In the timestamp fields of the final
 *        message, the least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to fill
 *         ts  timestamp value
 *
 * @return none
 */
void final_msg_set_ts(uint8_t *ts_field, uint64_t ts)
{
    uint8_t i;
    for (i = 0; i < FINAL_MSG_TS_LEN; i++)
    {
        ts_field[i] = (uint8_t)ts;
        ts >>= 8;
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn resp_msg_set_ts()
 *
 * @brief Fill a given timestamp field in the response message with the given value. In the timestamp fields of the
 *        response message, the least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to fill
 *         ts  timestamp value
 *
 * @return none
 */
void resp_msg_set_ts(uint8_t *ts_field, const uint64_t ts)
{
    uint8_t i;
    for (i = 0; i < RESP_MSG_TS_LEN; i++)
    {
        ts_field[i] = (uint8_t)(ts >> (i * 8));
    }
}

/**
 * @brief
 *
 * @param rx_diag
 * @param dgc_decision
 * @return float
 */
float dwt_calc_rssi(dwt_rxdiag_t *rx_diag, uint32_t dgc_decision)
{
    float rssi = -99.99;

    if(rx_diag->ipatovAccumCount > 0)
    {
        // [INFO] WITH log10f DW SLEEP MODE NOT WORKING CORRECLTY (UNKNOWN BEHAVIOR) -> log10f irq cause sleep problems of mcu
        // TODO: -> log10f irq cause sleep problems of mcu, [WORKAROUND] -> clear fpu irq before enter sleep
        rssi = 10.0 * log10f( (rx_diag->ipatovPower * 0x200000) / (rx_diag->ipatovAccumCount * rx_diag->ipatovAccumCount) ) + (6 * dgc_decision) - 121.7;
    }

    /* limit value */
    if(rssi <= -99.9)
    {
        rssi = -99.9;
    }

    return rssi;
}


/**
 * @brief
 *
 * @return float
 */
float dwt_get_rssi()
{
    dwt_rxdiag_t rx_diag;
    uint32_t dgc_decision; // DGC_DECISION 0-7
    float rssi;

    rx_diag.ipatovPower = dwt_read32bitoffsetreg(IP_DIAG_1_ID, 0);
    rx_diag.ipatovAccumCount = dwt_read16bitoffsetreg(IP_DIAG_12_ID, 0);
    // dgc_decision = (dwt_read8bitoffsetreg(DGC_DBG_ID, 3) >> 4) & 0b111;

    // [INFO] only enable DGC for PRF 64 (TODO: PRF 16? STS_CIR?)
    if ((config_options.rxCode >= 9) && (config_options.rxCode <= 24)) {
        dgc_decision = dwt_get_dgcdecision();
    } else {
        dgc_decision = 0;
    }

    rssi = dwt_calc_rssi(&rx_diag, dgc_decision);

    return rssi;
}


/**
 * @brief
 *
 * @param xtaloffset
 * @return float
 */
float dwt_calc_clock_offset(int16_t xtaloffset)
{
    if (xtaloffset & B11_SIGN_EXTEND_TEST)
    {
        xtaloffset |= B11_SIGN_EXTEND_MASK;             // sign extend bit #12 to the whole short
    }

    return ((float)xtaloffset) / (uint32_t)(1<<26);
}

/**
 * @brief
 *
 * @param ts_tab
 * @return uint64_t
 */
uint64_t dwt_calc_timestamp_u64(uint8_t *ts_tab)
{
    uint64_t ts = 0;
    int8_t i;
    for (i = 4; i >= 0; i--)
    {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @brief This function will continuously read the system status register until it matches the bits set in the mask
 *        input parameter. It will then exit the function.
 *        This is useful to use when waiting on particular events to occurs. For example, the user could wait for a
 *        good UWB frame to be received and/or no receive errors have occurred.
 *        The lower 32-bits of the system status register will be read in a while loop. Each iteration of the loop will check if a matching
 *        mask value for the higher 32-bits of the system status register is set. If the mask value is set in the higher 32-bits of the system
 *        status register, the function will return that value along with the last recorded value of the lower 32-bits of the system status
 *        register. Thus, the user should be aware that this function will not wait for high and low mask values to be set in both the low and high
 *        system status registers. Alternatively, the user can call this function to *only* check the higher or lower system status registers.
 *
 * input parameters
 * @param lo_result - A pointer to a uint32_t that will contain the final value of the system status register (lower 32 bits).
 *                    Pass in a NULL pointer to ignore returning this value.
 * @param hi_result - A pointer to a uint32_t that will contain the final value of the system status register (higher 32 bits).
 *                    Pass in a NULL pointer to ignore returning this value.
 * @param lo_mask - a uint32 mask value that is used to check for certain bits to be set in the system status register (lower 32 bits).
 *               Example values to use are as follows:
 *               DWT_INT_TXFRS_BIT_MASK - Wait for a TX frame to be sent.
 *               SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_ERR - Wait for frame to be received and no reception errors.
 *               SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR - Wait for frame to be received and no receive timeout errors
 *                                                                                          and no reception errors.
 *               SYS_STATUS_RXFR_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_ND_RX_ERR - Wait for packet to be received and no receive timeout errors
 *                                                                                            and no reception errors.
 *                                                                                            These flags are useful when polling for STS Mode 4 (no data)
 *                                                                                            packets.
 *               0 - The function will not wait for any bits in the system status register (lower 32 bits).
 * @param hi_mask - a uint32 mask value that is used to check for certain bits to be set in the system status register (higher 32 bits).
 *               Example values to use are as follows:
 *               SYS_STATUS_HI_CCA_FAIL_BIT_MASK - Check for CCA fail status.
 *               0 - The function will not wait for any bits in the system status register (lower 32 bits).
 *
 * return None
 */
void waitforsysstatus(uint32_t *lo_result, uint32_t *hi_result, uint32_t lo_mask, uint32_t hi_mask)
{
    uint32_t lo_result_tmp = 0;
    uint32_t hi_result_tmp = 0;

    // If a mask has been passed into the function for the system status register (lower 32-bits)
    if (lo_mask)
    {
        while (!((lo_result_tmp = dwt_readsysstatuslo()) & (lo_mask)))
        {
            // If a mask value is set for the system status register (higher 32-bits)
            if (hi_mask)
            {
                // If mask value for the system status register (higher 32-bits) is found
                if ((hi_result_tmp = dwt_readsysstatushi()) & hi_mask)
                {
                    break;
                }
            }
        }
    }
    // if only a mask value for the system status register (higher 32-bits) is set
    else if (hi_mask)
    {
        while (!((hi_result_tmp = dwt_readsysstatushi()) & (hi_mask))) { };
    }

    if (lo_result != NULL)
    {
        *lo_result = lo_result_tmp;
    }

    if (hi_result != NULL)
    {
        *hi_result = hi_result_tmp;
    }
}
