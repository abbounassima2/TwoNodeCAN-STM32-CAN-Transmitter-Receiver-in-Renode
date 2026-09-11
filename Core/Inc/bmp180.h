#ifndef BMP180_H
#define BMP180_H

#include "stm32f4xx_hal.h"

/* BMP180 7-bit I2C address = 0x77
 * STM32 HAL expects the address shifted left by 1.
 */
#define BMP180_I2C_ADDR          (0x77 << 1)

/* Registers */
#define BMP180_REG_AC1_MSB       0xAA
#define BMP180_REG_CHIP_ID       0xD0
#define BMP180_REG_CONTROL       0xF4
#define BMP180_REG_DATA          0xF6

/* Commands */
#define BMP180_CMD_TEMP          0x2E
#define BMP180_CMD_PRESSURE      0x34

typedef enum
{
    BMP180_OK = 0,
    BMP180_ERROR,
    BMP180_TIMEOUT

} BMP180_Status_t;


/*
 * BMP180 device object
 */
typedef struct
{
    I2C_HandleTypeDef *hi2c;

    uint16_t address;

    /* Calibration coefficients */
    int16_t  AC1;
    int16_t  AC2;
    int16_t  AC3;

    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;

    int16_t  B1;
    int16_t  B2;

    int16_t  MB;
    int16_t  MC;
    int16_t  MD;

} BMP180_Handle_t;


/* Initialization */
BMP180_Status_t BMP180_Init(BMP180_Handle_t *dev,
                            I2C_HandleTypeDef *hi2c);


/* Low-level register access */
BMP180_Status_t BMP180_ReadReg(BMP180_Handle_t *dev,
                               uint8_t reg,
                               uint8_t *data);

BMP180_Status_t BMP180_WriteReg(BMP180_Handle_t *dev,
                                uint8_t reg,
                                uint8_t data);


/* Read 16-bit big-endian register */
BMP180_Status_t BMP180_Read16(BMP180_Handle_t *dev,
                              uint8_t reg,
                              uint16_t *value);


/* Temperature */
BMP180_Status_t BMP180_ReadTemperature(BMP180_Handle_t *dev,
                                        int32_t *temperature);


/* Optional: pressure */
BMP180_Status_t BMP180_ReadPressure(BMP180_Handle_t *dev,
                                     uint8_t oss,
                                     int32_t *pressure);

#endif
