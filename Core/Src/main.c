#include "main.h"
#include "BMP180.h"
#include "cmsis_os.h"
#include <stdio.h>


/* ---------------------------------------------------------
 * Peripheral handles
 * --------------------------------------------------------- */

CAN_HandleTypeDef hcan1;
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;


/* ---------------------------------------------------------
 * RTOS
 * --------------------------------------------------------- */

osThreadId defaultTaskHandle;


/* ---------------------------------------------------------
 * BMP180
 * --------------------------------------------------------- */

BMP180_Handle_t bmp180;


/* ---------------------------------------------------------
 * Function prototypes
 * --------------------------------------------------------- */

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);

void StartDefaultTask(void const *argument);

void Error_Handler(void);


/* ---------------------------------------------------------
 * printf -> USART1
 * --------------------------------------------------------- */

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)ptr,
        len,
        HAL_MAX_DELAY
    );

    return len;
}


/* ---------------------------------------------------------
 * MAIN
 * --------------------------------------------------------- */

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();


    /*
     * USART MUST be initialized before printf.
     */
    MX_USART1_UART_Init();

    printf("\r\n");
    printf("====================================\r\n");
    printf("STM32F407 BMP180 START\r\n");
    printf("====================================\r\n");


    /*
     * Initialize I2C
     */
    printf("Initializing I2C1...\r\n");

    MX_I2C1_Init();

    printf("I2C1 initialized\r\n");


    /*
     * Check I2C bus/device
     */
    printf("Checking BMP180 at address 0x77...\r\n");

    HAL_StatusTypeDef i2c_status;

    i2c_status = HAL_I2C_IsDeviceReady(
        &hi2c1,
        BMP180_I2C_ADDR,
        3,
        100
    );

    if (i2c_status == HAL_OK)
    {
        printf("BMP180 ACK received\r\n");
    }
    else
    {
        printf("BMP180 NOT responding\r\n");
        printf("HAL I2C status = %d\r\n", i2c_status);
    }


    /*
     * Initialize BMP180 driver
     */
    printf("Initializing BMP180 driver...\r\n");

    BMP180_Status_t bmp_status;

    bmp_status = BMP180_Init(
        &bmp180,
        &hi2c1
    );


    if (bmp_status == BMP180_OK)
    {
        printf("BMP180 initialization SUCCESS\r\n");

        printf("Calibration:\r\n");

        printf("AC1 = %d\r\n", bmp180.AC1);
        printf("AC2 = %d\r\n", bmp180.AC2);
        printf("AC3 = %d\r\n", bmp180.AC3);

        printf("AC4 = %u\r\n", bmp180.AC4);
        printf("AC5 = %u\r\n", bmp180.AC5);
        printf("AC6 = %u\r\n", bmp180.AC6);

        printf("B1  = %d\r\n", bmp180.B1);
        printf("B2  = %d\r\n", bmp180.B2);

        printf("MB  = %d\r\n", bmp180.MB);
        printf("MC  = %d\r\n", bmp180.MC);
        printf("MD  = %d\r\n", bmp180.MD);
    }
    else
    {
        printf("BMP180 initialization FAILED\r\n");

        /*
         * Don't immediately freeze here.
         *
         * Keep running so we can see the UART output.
         */
    }


    printf("Creating RTOS task...\r\n");


    osThreadDef(
        defaultTask,
        StartDefaultTask,
        osPriorityNormal,
        0,
        256
    );


    defaultTaskHandle =
        osThreadCreate(
            osThread(defaultTask),
            NULL
        );


    if (defaultTaskHandle == NULL)
    {
        printf("TASK CREATION FAILED\r\n");
        Error_Handler();
    }


    printf("TASK CREATED\r\n");

    printf("Starting RTOS...\r\n");

    osKernelStart();


    while (1)
    {
    }
}


/* ---------------------------------------------------------
 * SYSTEM CLOCK
 * --------------------------------------------------------- */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};

    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );


    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSEState =
        RCC_HSE_ON;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLM = 4;

    RCC_OscInitStruct.PLL.PLLN = 168;

    RCC_OscInitStruct.PLL.PLLP =
        RCC_PLLP_DIV2;

    RCC_OscInitStruct.PLL.PLLQ = 4;


    if (HAL_RCC_OscConfig(&RCC_OscInitStruct)
        != HAL_OK)
    {
        Error_Handler();
    }


    RCC_ClkInitStruct.ClockType =
          RCC_CLOCKTYPE_HCLK
        | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1
        | RCC_CLOCKTYPE_PCLK2;


    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV4;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV2;


    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ---------------------------------------------------------
 * I2C1
 * --------------------------------------------------------- */

static void MX_I2C1_Init(void)
{
    printf("I2C1: configuring...\r\n");


    hi2c1.Instance = I2C1;

    hi2c1.Init.ClockSpeed = 100000;

    hi2c1.Init.DutyCycle =
        I2C_DUTYCYCLE_2;

    hi2c1.Init.OwnAddress1 = 0;

    hi2c1.Init.AddressingMode =
        I2C_ADDRESSINGMODE_7BIT;

    hi2c1.Init.DualAddressMode =
        I2C_DUALADDRESS_DISABLE;

    hi2c1.Init.OwnAddress2 = 0;

    hi2c1.Init.GeneralCallMode =
        I2C_GENERALCALL_DISABLE;

    hi2c1.Init.NoStretchMode =
        I2C_NOSTRETCH_DISABLE;


    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        printf("I2C1: HAL_I2C_Init FAILED\r\n");

        Error_Handler();
    }


    printf("I2C1: HAL_I2C_Init SUCCESS\r\n");
}


/* ---------------------------------------------------------
 * USART1
 * --------------------------------------------------------- */

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;

    huart1.Init.BaudRate = 115200;

    huart1.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart1.Init.StopBits =
        UART_STOPBITS_1;

    huart1.Init.Parity =
        UART_PARITY_NONE;

    huart1.Init.Mode =
        UART_MODE_TX_RX;

    huart1.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart1.Init.OverSampling =
        UART_OVERSAMPLING_16;


    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ---------------------------------------------------------
 * GPIO
 * --------------------------------------------------------- */

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOH_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
}


/* ---------------------------------------------------------
 * BMP180 TASK
 * --------------------------------------------------------- */

void StartDefaultTask(void const *argument)
{
    int32_t temperature;

    int32_t pressure;


    printf("Sensor task started\r\n");


    for (;;)
    {
        /*
         * Temperature
         */
        if (BMP180_ReadTemperature(
                &bmp180,
                &temperature) == BMP180_OK)
        {
            printf(
                "Temperature = %ld.%ld C\r\n",
                temperature / 10,
                temperature % 10
            );
        }
        else
        {
            printf("Temperature read ERROR\r\n");
        }


        /*
         * Pressure
         */
        if (BMP180_ReadPressure(
                &bmp180,
                0,
                &pressure) == BMP180_OK)
        {
            printf(
                "Pressure = %ld Pa\r\n",
                pressure
            );
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
 * ERROR HANDLER
 * --------------------------------------------------------- */

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
    printf(
        "ASSERT FAILED: %s:%lu\r\n",
        file,
        line
    );
}

#endif
