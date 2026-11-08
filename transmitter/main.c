#include "main.h"
#include "BMP180.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdint.h>

/* ---------------------------------------------------------
 * Peripheral handles
 * --------------------------------------------------------- */

CAN_HandleTypeDef hcan2;
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;

osThreadId defaultTaskHandle;

BMP180_Handle_t bmp180;

/* ---------------------------------------------------------
 * Prototypes
 * --------------------------------------------------------- */

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init();
static void MX_CAN2_Init(void);

static void CAN_SendTemperature(int32_t temperature,
                                uint8_t command,
                                uint8_t valid);

void StartDefaultTask(void const *argument);
void Error_Handler(void);



/* ---------------------------------------------------------
 * _write / for showAnalyzer usart  in Renode
 * --------------------------------------------------------- */

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}


int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    printf("\r\n");
    printf("====================================\r\n");
    printf("STM32F407 BMP180 + CAN TRANSMITTER\r\n");
    printf("====================================\r\n");


    printf("Initializing I2C1...\r\n");
    MX_I2C1_Init();
    printf("I2C1 initialized\r\n");

    printf("Checking BMP180 at address 0x77...\r\n");

    HAL_StatusTypeDef i2c_status =
        HAL_I2C_IsDeviceReady(&hi2c1, BMP180_I2C_ADDR, 3, 100);

    if (i2c_status == HAL_OK)
    {
        printf("BMP180 ACK received\r\n");
    }
    else
    {
        printf("BMP180 NOT responding\r\n");
        printf("HAL I2C status = %d\r\n", i2c_status);
    }

    printf("Initializing BMP180 driver...\r\n");

    BMP180_Status_t bmp_status = BMP180_Init(&bmp180, &hi2c1);

    if (bmp_status == BMP180_OK)
    {
        printf("BMP180 initialization SUCCESS\r\n");
    }
    else
    {
        printf("BMP180 initialization FAILED\r\n");
    }

    printf("Initializing CAN2...\r\n");

    MX_CAN2_Init();

    if (HAL_CAN_Start(&hcan2) != HAL_OK)
    {
        printf("[ERROR] CAN2 start failed: 0x%08lX\r\n",
               (unsigned long)HAL_CAN_GetError(&hcan2));
        Error_Handler();
    }

    printf("[OK] CAN2 started (500 kbps)\r\n");



    osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 512);

    defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

    if (defaultTaskHandle == NULL)
    {
        printf("TASK CREATION FAILED\r\n");
        Error_Handler();
    }

    printf("Starting RTOS...\r\n");

    osKernelStart();

    while (1)
    {
    }
}

/* ---------------------------------------------------------
 * SENSOR + CAN TASK
 * --------------------------------------------------------- */

void StartDefaultTask(void const *argument)
{
    int32_t temperature = 0;
    int32_t pressure = 0;
    uint8_t command = 0;

    printf("Sensor task started\r\n");

    for (;;)
    {
        if (BMP180_ReadTemperature(&bmp180, &temperature) == BMP180_OK)
        {
            printf("Temperature = %ld.%ld C\r\n",
                   (long)(temperature / 10),
                   (long)(temperature < 0 ? -(temperature % 10)
                                          : (temperature % 10)));

            /* Threshold = 20.0 C (value is in 0.1 C units) */
            command = (temperature > 200) ? 1 : 0;

            printf("[CAN] Command = %u\r\n", command);

            CAN_SendTemperature(temperature, command, 1);
        }
        else
        {
            printf("Temperature read ERROR\r\n");

            /* Safe OFF command with invalid-sensor flag */
            CAN_SendTemperature(0, 0, 0);
        }

        if (BMP180_ReadPressure(&bmp180, 0, &pressure) == BMP180_OK)
        {
            printf("Pressure = %ld Pa\r\n", (long)pressure);
        }
        else
        {
            printf("Pressure read ERROR\r\n");
        }

        printf("-----------------------------\r\n");

        osDelay(1000);
    }
}

/* ---------------------------------------------------------
 * CAN TRANSMIT
 *
 * ID 0x123, DLC 4
 *  Byte 0: command (1 = ON, 0 = OFF)
 *  Byte 1: temperature LSB (x10)
 *  Byte 2: temperature MSB
 *  Byte 3: sensor status (1 = valid)
 * --------------------------------------------------------- */

static void CAN_SendTemperature(int32_t temperature,
                                uint8_t command,
                                uint8_t valid)
{
    CAN_TxHeaderTypeDef txHeader = {0};
    uint8_t txData[4] = {0};
    uint32_t mailbox;
    uint32_t start;

    int16_t temp = (int16_t)temperature;

    txHeader.StdId = 0x123;
    txHeader.IDE = CAN_ID_STD;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.DLC = 4;
    txHeader.TransmitGlobalTime = DISABLE;

    txData[0] = command;
    txData[1] = (uint8_t)(temp & 0xFF);
    txData[2] = (uint8_t)(((uint16_t)temp >> 8) & 0xFF);
    txData[3] = valid;

    /* Wait for a free mailbox */
    start = HAL_GetTick();

    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) == 0)
    {
        if ((HAL_GetTick() - start) > 200)
        {
            printf("[ERROR] CAN mailbox timeout\r\n");
            return;
        }
        osDelay(1);
    }

    if (HAL_CAN_AddTxMessage(&hcan2, &txHeader, txData, &mailbox) != HAL_OK)
    {
        printf("[ERROR] CAN TX failed: 0x%08lX\r\n",
               (unsigned long)HAL_CAN_GetError(&hcan2));
        return;
    }

    /* Wait for transmission to finish */
    start = HAL_GetTick();

    while (HAL_CAN_IsTxMessagePending(&hcan2, mailbox))
    {
        if ((HAL_GetTick() - start) > 200)
        {
            printf("[ERROR] CAN TX timeout (no receiver/ACK?)\r\n");
            return;
        }
        osDelay(1);
    }

    printf("[CAN] Frame transmitted successfully\r\n");
}



static void MX_CAN2_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_CAN2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF9_CAN2;
    HAL_GPIO_Init(GPIOB, &gpio);

    hcan2.Instance = CAN2;

    hcan2.Init.Prescaler = 6;
    hcan2.Init.Mode = CAN_MODE_NORMAL;
    hcan2.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan2.Init.TimeSeg1 = CAN_BS1_11TQ;
    hcan2.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan2.Init.TimeTriggeredMode = DISABLE;
    hcan2.Init.AutoBusOff = DISABLE;
    hcan2.Init.AutoWakeUp = DISABLE;
    hcan2.Init.AutoRetransmission = ENABLE;
    hcan2.Init.ReceiveFifoLocked = DISABLE;
    hcan2.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan2) != HAL_OK)
    {
        printf("[ERROR] HAL_CAN_Init failed\r\n");
        Error_Handler();
    }
}



void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}


static void MX_I2C1_Init(void)
{
    printf("I2C1: configuring...\r\n");

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        printf("I2C1: HAL_I2C_Init FAILED\r\n");
        Error_Handler();
    }

    printf("I2C1: HAL_I2C_Init SUCCESS\r\n");
}


static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}


static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}


void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("ASSERT FAILED: %s:%lu\r\n", file, (unsigned long)line);
}
#endif
