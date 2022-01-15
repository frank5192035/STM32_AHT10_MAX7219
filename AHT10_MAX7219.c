// This example is for Nucleo-F411 board connecting with AHT10 Module by I2C1 @ PB8&PB9
// Send out reading data using SPI1 port; display temperature and humidity on MAX7219 LED module
// PB5: SPI1_MOSI;  PB4: SPI1_LOAD(GPIO with open-drain output);  PB3: SPI1_SCK;
// State Machine of AHT10_MAX7219 @ AHT10_MAX7219.graphml(yEd)
//                                                  2022 Jan, by Frank Hsiung

#include "main.h"
#include <string.h>
#include <stdio.h>

I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;
HAL_StatusTypeDef status;

const char *statusArray[] = {"HAL_OK", "HAL_ERROR", "HAL_BUSY", "HAL_TIMEOUT"};
const uint32_t tickWait = 1000; // read data every tickWait mini-second

static uint32_t tickEnd;                           // for State Machine Time-out
static uint8_t buf_I2C_Tx[3] = {0xE1, 0x08, 0x00}; // AHT10 Initialization: datasheet is not clear here!
static uint8_t buf_I2C_Rx[6];                      // Rx Buffer from AHT10
static uint8_t buf_SPI[2];                         // Scan Limit of MAX7219
static uint8_t buf_UART[64];                       // report information through UART2

void s_AHT10_MAX7219_Init(void); // state machine
void s_AHT10_MAX7219_InitDelay(void);
void s_AHT10_MAX7219_IssueMeasureCmd(void);
void s_AHT10_MAX7219_MeasurementDelay(void);
void (*ps_AHT10_MAX7219)(void); // state machine functional pointer of AHT10

void MAX7219_Write(uint8_t adr, uint8_t reg) // Function Call by Value
{
    buf_SPI[0] = adr;                                                      // address (Command)
    buf_SPI[1] = reg;                                                      // data
    HAL_SPI_Transmit(&hspi1, buf_SPI, 2, HAL_MAX_DELAY);                   // into MAX7219 FIFO
    HAL_GPIO_WritePin(SPI1_LOAD_GPIO_Port, SPI1_LOAD_Pin, GPIO_PIN_SET);   // rising edge latch
    HAL_GPIO_WritePin(SPI1_LOAD_GPIO_Port, SPI1_LOAD_Pin, GPIO_PIN_RESET); // back to LOW level
}

void s_AHT10_MAX7219_Init(void)
{
    // MAX7219 Initialization
    HAL_GPIO_WritePin(SPI1_LOAD_GPIO_Port, SPI1_LOAD_Pin, GPIO_PIN_RESET);
    MAX7219_Write(0x0B, 0x07); // Scan Limit
    MAX7219_Write(0x0A, 0x01); // Intensity 09
    MAX7219_Write(0x09, 0xFF); // Decode Mode
    MAX7219_Write(0x0C, 0x01); // Normal Operation
    MAX7219_Write(0x04, 0x0F); // Digit-3: blank

    // AHT10 Initialization
    HAL_I2C_Master_Transmit(&hi2c1, 0x70, buf_I2C_Tx, 3, HAL_MAX_DELAY); // AHT10 Issue Init Command

    tickEnd = HAL_GetTick() + tickWait;           // set None Blocking Delay for Initialization
    ps_AHT10_MAX7219 = s_AHT10_MAX7219_InitDelay; // change state
}

void s_AHT10_MAX7219_InitDelay(void)
{
    if (HAL_GetTick() > tickEnd)
    {
        strcpy((char *)buf_UART, "Start Reading AHT10 Data\r\n\0");
        HAL_UART_Transmit(&huart2, buf_UART, strlen((char *)buf_UART), HAL_MAX_DELAY);

        buf_I2C_Tx[0] = 0xAC;                               // rewrite Measurement Command Buffer
        buf_I2C_Tx[1] = 0x33;                               // only set once
        ps_AHT10_MAX7219 = s_AHT10_MAX7219_IssueMeasureCmd; // change state
    }
}

void s_AHT10_MAX7219_IssueMeasureCmd(void)
{
    // issue AHT10 measurement command
    status = HAL_I2C_Master_Transmit(&hi2c1, 0x70, buf_I2C_Tx, 3, HAL_MAX_DELAY);
    if (status != HAL_OK)
    {
        strcpy((char *)buf_UART, statusArray[status]);
        HAL_UART_Transmit(&huart2, buf_UART, strlen((char *)buf_UART), HAL_MAX_DELAY);
        strcpy((char *)buf_UART, " @ s_AHT10_MAX7219_IssueMeasureCmd State\r\n\0");
        HAL_UART_Transmit(&huart2, buf_UART, strlen((char *)buf_UART), HAL_MAX_DELAY);
    }
    else
    {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);          // LED Blinking
        tickEnd = HAL_GetTick() + tickWait;                  // None Blocking Delay
        ps_AHT10_MAX7219 = s_AHT10_MAX7219_MeasurementDelay; // change state
    }
}

void s_AHT10_MAX7219_MeasurementDelay(void)
{
    if (HAL_GetTick() > tickEnd)
    {
        status = HAL_I2C_Master_Receive(&hi2c1, 0x70, buf_I2C_Rx, 6, HAL_MAX_DELAY);
        if (status == HAL_OK)
        {
            // ------------------------- calculate Temperature ---------------------------------------
            strcpy((char *)buf_UART, " ％RH\r\nTemperature = \0");
            HAL_UART_Transmit(&huart2, buf_UART, strlen((char *)buf_UART), HAL_MAX_DELAY);

            // restructure to 20-bit data
            uint32_t x = ((uint32_t)(buf_I2C_Rx[3] & 0x0F) << 16) | ((uint32_t)buf_I2C_Rx[4] << 8) | buf_I2C_Rx[5];
            float tmp = (float)x * 0.000191 - 50; // formula in datasheet
            if (tmp >= 0)
                MAX7219_Write(0x08, 0x0F); // Digit-7: blank
            else
                MAX7219_Write(0x08, 0x0A); // Digit-7: negative

            sprintf((char *)buf_UART, "%04.1f", tmp); // output truncation
            HAL_UART_Transmit(&huart2, buf_UART, 4, HAL_MAX_DELAY);
            // transform ASCII code to 7-segment Code-B font
            MAX7219_Write(0x07, buf_UART[0] - 48); // Digit-6
            MAX7219_Write(0x06, buf_UART[1] + 80); // Digit-5; 80=128-48; with dot
            MAX7219_Write(0x05, buf_UART[3] - 48); // Digit-4
            // ----------------------- calculate Humidity ------------------------------------------
            strcpy((char *)buf_UART, " ℃\tHumidity = \0");
            HAL_UART_Transmit(&huart2, buf_UART, strlen((char *)buf_UART), HAL_MAX_DELAY);

            // restructure to 20 - bit data
            x = ((uint32_t)buf_I2C_Rx[1] << 12) | ((uint32_t)buf_I2C_Rx[2] << 4) | (buf_I2C_Rx[3] >> 4);
            tmp = (float)x * 0.000095; // formula in datasheet
            if (tmp > 99.9)
                tmp = 99.9;
            sprintf((char *)buf_UART, "%04.1f", tmp); // output truncation
            HAL_UART_Transmit(&huart2, buf_UART, 4, HAL_MAX_DELAY);
            MAX7219_Write(0x03, buf_UART[0] - 48); // Digit-2
            MAX7219_Write(0x02, buf_UART[1] + 80); // Digit-1; 80=128-48; with dot
            MAX7219_Write(0x01, buf_UART[3] - 48); // Digit-0

            ps_AHT10_MAX7219 = s_AHT10_MAX7219_IssueMeasureCmd; // change state
        }
        else
        { // report exception status
            strcpy((char *)buf_UART, statusArray[status]);
            HAL_UART_Transmit(&huart2, buf_UART, strlen((char *)buf_UART), HAL_MAX_DELAY);
            strcpy((char *)buf_UART, " @ s_AHT10_MAX7219_MeasurementDelay State\r\n\0");
            HAL_UART_Transmit(&huart2, buf_UART, strlen((char *)buf_UART), HAL_MAX_DELAY);
        }
    }
}
