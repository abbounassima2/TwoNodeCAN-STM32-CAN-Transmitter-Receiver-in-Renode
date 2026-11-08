
#include "main.h"
#include <stdio.h>
#include <stdint.h>


CAN_HandleTypeDef hcan2;
UART_HandleTypeDef huart1;


volatile uint32_t test_stage = 0;
volatile uint32_t received_messages = 0;

volatile uint8_t received_command = 0;
volatile uint8_t received_sensor_status = 0;
volatile int16_t received_temperature_x10 = 0;

volatile HAL_StatusTypeDef can_status;
volatile uint32_t can_error_code = 0;


void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_CAN2_Init(void);

static void CAN2_Filter_Config(void);
static void CAN2_Receive(void);

void Error_Handler(void);


int __write(int ch)
{
    uint8_t c = (uint8_t)ch;

    HAL_UART_Transmit(&huart1, &c, 1, 100);

    return ch;
}


int main(void)
{
    HAL_Init();

    test_stage = 1;

    SystemClock_Config();

    test_stage = 2;

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    printf("\r\n");
    printf("STM32 #2 - CAN2 RECEIVER\r\n");

    test_stage = 3;

    MX_CAN2_Init();

    test_stage = 4;

    CAN2_Filter_Config();

    test_stage = 5;

    if (HAL_CAN_Start(&hcan2) != HAL_OK)
    {
        can_error_code = HAL_CAN_GetError(&hcan2);

        printf("[ERROR] CAN start failed: 0x%08lX\r\n",
               (unsigned long)can_error_code);

        Error_Handler();
    }

    printf("[OK] CAN2 started\r\n");
    printf("[WAIT] Waiting for temperature data...\r\n");

    test_stage = 6;

    while (1)
    {
        CAN2_Receive();

        HAL_Delay(10);
    }
}


static void MX_CAN2_Init(void)
{
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

    can_status = HAL_CAN_Init(&hcan2);

    if (can_status != HAL_OK)
    {
        can_error_code = HAL_CAN_GetError(&hcan2);

        test_stage = 31;
        Error_Handler();
    }
}

static void CAN2_Filter_Config(void)
{
    CAN_FilterTypeDef filter = {0};

    filter.FilterBank = 14;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;

    filter.FilterIdHigh = (0x123 << 5);
    filter.FilterIdLow = 0x0000;

    filter.FilterMaskIdHigh = (0x7FF << 5);
    filter.FilterMaskIdLow = 0x0000;

    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;

    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan2, &filter) != HAL_OK)
    {
        can_error_code = HAL_CAN_GetError(&hcan2);

        test_stage = 41;
        Error_Handler();
    }
}


static void CAN2_Receive(void)
{
    CAN_RxHeaderTypeDef rxHeader = {0};
    uint8_t rxData[8] = {0};


    if (HAL_CAN_GetRxFifoFillLevel(&hcan2,
                                   CAN_RX_FIFO0) == 0)
    {
        return;
    }

    can_status = HAL_CAN_GetRxMessage(&hcan2,
                                      CAN_RX_FIFO0,
                                      &rxHeader,
                                      rxData);

    if (can_status != HAL_OK)
    {
        can_error_code = HAL_CAN_GetError(&hcan2);
        printf("[ERROR] CAN reception failed\r\n");
        return;
    }

    if (rxHeader.IDE != CAN_ID_STD ||
        rxHeader.StdId != 0x123 ||
        rxHeader.DLC != 4)
    {
        return;
    }

    received_messages++;

    received_command = rxData[0];

    received_temperature_x10 =
        (int16_t)((uint16_t)rxData[1] |
                  ((uint16_t)rxData[2] << 8));

    received_sensor_status = rxData[3];

    printf("\r\n[CAN] Message received\r\n");

    printf("Temperature: %d.%d C\r\n",
           received_temperature_x10 / 10,
           received_temperature_x10 < 0 ?
           -(received_temperature_x10 % 10) :
           received_temperature_x10 % 10);

    /* Sensor failure: turn output OFF */
    if (received_sensor_status == 0)
    {
        HAL_GPIO_WritePin(GPIOD,
                          GPIO_PIN_10,
                          GPIO_PIN_RESET);

        printf("[OUTPUT] PD10 LOW - SENSOR ERROR\r\n");

        return;
    }

    if (received_command == 1)
    {
        HAL_GPIO_WritePin(GPIOD,
                          GPIO_PIN_10,
                          GPIO_PIN_SET);

        printf("[OUTPUT] PD10 HIGH - ABOVE 20 C\r\n");
    }
    else
    {
        HAL_GPIO_WritePin(GPIOD,
                          GPIO_PIN_10,
                          GPIO_PIN_RESET);

        printf("[OUTPUT] PD10 LOW - 20 C OR BELOW\r\n");
    }
}



static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOD,
                      GPIO_PIN_10,
                      GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
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
        Error_Handler();
}


void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;

    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 4;
    osc.PLL.PLLN = 168;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 4;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}


void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
