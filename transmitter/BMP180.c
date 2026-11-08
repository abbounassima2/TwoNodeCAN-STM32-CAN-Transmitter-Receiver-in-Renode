#include "BMP180.h"


/* ---------------------------------------------------------
 * Read one register
 * --------------------------------------------------------- */
BMP180_Status_t BMP180_ReadReg(BMP180_Handle_t *dev,
                               uint8_t reg,
                               uint8_t *data)
{
    HAL_StatusTypeDef status;

    if (dev == NULL || dev->hi2c == NULL || data == NULL)
    {
        return BMP180_ERROR;
    }

    status = HAL_I2C_Mem_Read(
        dev->hi2c,
        dev->address,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        data,
        1,
        100
    );

    if (status == HAL_OK)
    {
        return BMP180_OK;
    }

    if (status == HAL_TIMEOUT)
    {
        return BMP180_TIMEOUT;
    }

    return BMP180_ERROR;
}


/* ---------------------------------------------------------
 * Write one register
 * --------------------------------------------------------- */
BMP180_Status_t BMP180_WriteReg(BMP180_Handle_t *dev,
                                uint8_t reg,
                                uint8_t data)
{
    HAL_StatusTypeDef status;

    if (dev == NULL || dev->hi2c == NULL)
    {
        return BMP180_ERROR;
    }

    status = HAL_I2C_Mem_Write(
        dev->hi2c,
        dev->address,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        &data,
        1,
        100
    );

    if (status == HAL_OK)
    {
        return BMP180_OK;
    }

    if (status == HAL_TIMEOUT)
    {
        return BMP180_TIMEOUT;
    }

    return BMP180_ERROR;
}


/* ---------------------------------------------------------
 * Read 16-bit big-endian value
 * --------------------------------------------------------- */
BMP180_Status_t BMP180_Read16(BMP180_Handle_t *dev,
                              uint8_t reg,
                              uint16_t *value)
{
    uint8_t data[2];

    if (value == NULL)
    {
        return BMP180_ERROR;
    }

    if (BMP180_ReadReg(dev, reg, &data[0]) != BMP180_OK)
    {
        return BMP180_ERROR;
    }

    if (BMP180_ReadReg(dev, reg + 1, &data[1]) != BMP180_OK)
    {
        return BMP180_ERROR;
    }

    /*
     * IMPORTANT:
     *
     * (uint16_t)data[0]       = correct
     *
     * (uint16_t *)data[0]     = WRONG
     *
     * The second one converts the number into a pointer.
     */
    *value = ((uint16_t)data[0] << 8) |
             (uint16_t)data[1];

    return BMP180_OK;
}


/* ---------------------------------------------------------
 * Initialize BMP180
 * --------------------------------------------------------- */
BMP180_Status_t BMP180_Init(BMP180_Handle_t *dev,
                            I2C_HandleTypeDef *hi2c)
{
    uint8_t chip_id;

    if (dev == NULL || hi2c == NULL)
    {
        return BMP180_ERROR;
    }

    /* Store I2C peripheral */
    dev->hi2c = hi2c;

    /* Store HAL address */
    dev->address = BMP180_I2C_ADDR;


    /* -----------------------------------------------------
     * Check chip ID
     * ----------------------------------------------------- */

    if (BMP180_ReadReg(dev,
                       BMP180_REG_CHIP_ID,
                       &chip_id) != BMP180_OK)
    {
        return BMP180_ERROR;
    }

    /*
     * BMP180 chip ID must be 0x55
     */
    if (chip_id != 0x55)
    {
        return BMP180_ERROR;
    }


    /* -----------------------------------------------------
     * Read calibration coefficients
     *
     * Calibration registers:
     *
     * AC1 = 0xAA
     * AC2 = 0xAC
     * AC3 = 0xAE
     * AC4 = 0xB0
     * AC5 = 0xB2
     * AC6 = 0xB4
     * B1  = 0xB6
     * B2  = 0xB8
     * MB  = 0xBA
     * MC  = 0xBC
     * MD  = 0xBE
     * ----------------------------------------------------- */

    uint16_t value;


    if (BMP180_Read16(dev, 0xAA, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->AC1 = (int16_t)value;


    if (BMP180_Read16(dev, 0xAC, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->AC2 = (int16_t)value;


    if (BMP180_Read16(dev, 0xAE, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->AC3 = (int16_t)value;


    if (BMP180_Read16(dev, 0xB0, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->AC4 = value;


    if (BMP180_Read16(dev, 0xB2, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->AC5 = value;


    if (BMP180_Read16(dev, 0xB4, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->AC6 = value;


    if (BMP180_Read16(dev, 0xB6, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->B1 = (int16_t)value;


    if (BMP180_Read16(dev, 0xB8, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->B2 = (int16_t)value;


    if (BMP180_Read16(dev, 0xBA, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->MB = (int16_t)value;


    if (BMP180_Read16(dev, 0xBC, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->MC = (int16_t)value;


    if (BMP180_Read16(dev, 0xBE, &value) != BMP180_OK)
        return BMP180_ERROR;

    dev->MD = (int16_t)value;


    return BMP180_OK;
}


/* ---------------------------------------------------------
 * Read temperature
 *
 * Returned temperature:
 *
 * Example:
 *  253 = 25.3 °C
 * --------------------------------------------------------- */
BMP180_Status_t BMP180_ReadTemperature(BMP180_Handle_t *dev,
                                        int32_t *temperature)
{
    uint8_t data[2];

    int32_t UT;

    int32_t X1;
    int32_t X2;
    int32_t B5;


    if (dev == NULL ||
        temperature == NULL)
    {
        return BMP180_ERROR;
    }


    /* -----------------------------------------------------
     * Start temperature conversion
     * ----------------------------------------------------- */

    if (BMP180_WriteReg(dev,
                        BMP180_REG_CONTROL,
                        BMP180_CMD_TEMP) != BMP180_OK)
    {
        return BMP180_ERROR;
    }


    /*
     * BMP180 temperature conversion requires
     * approximately 4.5 ms.
     */
    HAL_Delay(5);


    /* -----------------------------------------------------
     * Read uncompensated temperature
     * ----------------------------------------------------- */

    if (HAL_I2C_Mem_Read(
            dev->hi2c,
            dev->address,
            BMP180_REG_DATA,
            I2C_MEMADD_SIZE_8BIT,
            data,
            2,
            100) != HAL_OK)
    {
        return BMP180_ERROR;
    }


    /*
     * Combine MSB + LSB.
     *
     * DO NOT use:
     *
     * (uint16_t *)data[0]
     *
     * because that is a pointer cast.
     */
    UT = ((int32_t)data[0] << 8) |
         (int32_t)data[1];


    /* -----------------------------------------------------
     * BMP180 temperature compensation
     * ----------------------------------------------------- */

    X1 = ((UT - (int32_t)dev->AC6) *
          (int32_t)dev->AC5) >> 15;


    X2 = ((int32_t)dev->MC << 11) /
         (X1 + (int32_t)dev->MD);


    B5 = X1 + X2;


    /*
     * Temperature is returned in 0.1°C.
     *
     * Example:
     *
     * 253 -> 25.3°C
     * 250 -> 25.0°C
     */
    *temperature = (B5 + 8) >> 4;


    return BMP180_OK;
}


/* ---------------------------------------------------------
 * Read pressure
 * --------------------------------------------------------- */
BMP180_Status_t BMP180_ReadPressure(BMP180_Handle_t *dev,
                                     uint8_t oss,
                                     int32_t *pressure)
{
    uint8_t data[3];

    int32_t UT;
    int32_t UP;

    int32_t X1;
    int32_t X2;
    int32_t X3;
    int32_t B3;
    int32_t B5;
    int32_t B6;

    int32_t B4;
    int32_t B7;

    int32_t p;


    if (dev == NULL || pressure == NULL)
    {
        return BMP180_ERROR;
    }

    if (oss > 3)
    {
        return BMP180_ERROR;
    }


    /* -----------------------------------------------------
     * Temperature conversion
     * ----------------------------------------------------- */

    if (BMP180_WriteReg(dev,
                        BMP180_REG_CONTROL,
                        BMP180_CMD_TEMP) != BMP180_OK)
    {
        return BMP180_ERROR;
    }

    HAL_Delay(5);


    if (HAL_I2C_Mem_Read(
            dev->hi2c,
            dev->address,
            BMP180_REG_DATA,
            I2C_MEMADD_SIZE_8BIT,
            data,
            2,
            100) != HAL_OK)
    {
        return BMP180_ERROR;
    }


    UT = ((int32_t)data[0] << 8) |
         (int32_t)data[1];


    /* -----------------------------------------------------
     * Calculate B5
     * ----------------------------------------------------- */

    X1 = ((UT - (int32_t)dev->AC6) *
          (int32_t)dev->AC5) >> 15;

    X2 = ((int32_t)dev->MC << 11) /
         (X1 + (int32_t)dev->MD);

    B5 = X1 + X2;


    /* -----------------------------------------------------
     * Start pressure conversion
     * ----------------------------------------------------- */

    if (BMP180_WriteReg(
            dev,
            BMP180_REG_CONTROL,
            BMP180_CMD_PRESSURE + (oss << 6)) != BMP180_OK)
    {
        return BMP180_ERROR;
    }


    /* Conversion time depends on OSS */
    switch (oss)
    {
        case 0:
            HAL_Delay(5);
            break;

        case 1:
            HAL_Delay(8);
            break;

        case 2:
            HAL_Delay(14);
            break;

        case 3:
            HAL_Delay(26);
            break;
    }


    /* -----------------------------------------------------
     * Read uncompensated pressure
     * ----------------------------------------------------- */

    if (HAL_I2C_Mem_Read(
            dev->hi2c,
            dev->address,
            BMP180_REG_DATA,
            I2C_MEMADD_SIZE_8BIT,
            data,
            3,
            100) != HAL_OK)
    {
        return BMP180_ERROR;
    }


    UP = (((int32_t)data[0] << 16) |
          ((int32_t)data[1] << 8) |
          (int32_t)data[2]) >> (8 - oss);


    /* -----------------------------------------------------
     * Pressure compensation
     * ----------------------------------------------------- */

    B6 = B5 - 4000;


    X1 = ((int32_t)dev->B2 *
          ((B6 * B6) >> 12)) >> 11;

    X2 = ((int32_t)dev->AC2 * B6) >> 11;

    X3 = X1 + X2;

    B3 = ((((int32_t)dev->AC1 * 4 + X3)
           << oss) + 2) / 4;


    X1 = ((int32_t)dev->AC3 * B6) >> 13;

    X2 = ((int32_t)dev->B1 *
          ((B6 * B6) >> 12)) >> 16;

    X3 = ((X1 + X2) + 2) >> 2;

    B4 = ((uint32_t)dev->AC4 *
          (uint32_t)(X3 + 32768)) >> 15;


    B7 = ((uint32_t)UP - B3) *
         (50000 >> oss);


    if (B7 < 0x80000000)
    {
        p = (B7 * 2) / B4;
    }
    else
    {
        p = (B7 / B4) * 2;
    }


    X1 = (p >> 8) * (p >> 8);

    X1 = (X1 * 3038) >> 16;

    X2 = (-7357 * p) >> 16;

    p = p + ((X1 + X2 + 3791) >> 4);


    *pressure = p;

    return BMP180_OK;
}
