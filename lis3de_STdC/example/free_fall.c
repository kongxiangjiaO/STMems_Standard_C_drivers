/*
 ******************************************************************************
 * @file    free_fall.c
 * @author  Sensors Software Solution Team
 * @brief   LIS3DE driver file
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "lis3de_reg.h"
#include <string.h>
#include <stdio.h>

//#define MKI109V2
#define NUCLEO_STM32F411RE

#ifdef MKI109V2
#include "stm32f1xx_hal.h"
#include "usbd_cdc_if.h"
#include "spi.h"
#include "i2c.h"
#endif

#ifdef NUCLEO_STM32F411RE
#include "stm32f4xx_hal.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"
#endif

/* Private macro -------------------------------------------------------------*/
#ifdef MKI109V2
#define CS_SPI2_GPIO_Port   CS_DEV_GPIO_Port
#define CS_SPI2_Pin         CS_DEV_Pin
#define CS_SPI1_GPIO_Port   CS_RF_GPIO_Port
#define CS_SPI1_Pin         CS_RF_Pin
#endif

#ifdef NUCLEO_STM32F411RE
/* N/A on NUCLEO_STM32F411RE + IKS01A1 */
/* N/A on NUCLEO_STM32F411RE + IKS01A2 */
#define CS_SPI2_GPIO_Port   0
#define CS_SPI2_Pin         0
#define CS_SPI1_GPIO_Port   0
#define CS_SPI1_Pin         0

/* Pin configured as platform interrupt from LIS3DE INT1. */
#define LIS3DE_INT1_PIN 	GPIO_PIN_4
#define LIS3DE_INT1_GPIO_PORT GPIOA

#endif

#define TX_BUF_DIM          1000

/* Private variables ---------------------------------------------------------*/
static uint8_t tx_buffer[TX_BUF_DIM];

/* Extern variables ----------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/*
 *   Replace the functions "platform_write" and "platform_read" with your
 *   platform specific read and write function.
 *   This example use an STM32 evaluation board and CubeMX tool.
 *   In this case the "*handle" variable is useful in order to select the
 *   correct interface but the usage of "*handle" is not mandatory.
 */

static int32_t platform_write(void *handle, uint8_t Reg, uint8_t *Bufp,
                              uint16_t len)
{
  if (handle == &hi2c1)
  {
    /* enable auto incremented in multiple read/write commands */
    Reg |= 0x80;
    HAL_I2C_Mem_Write(handle, LIS3DE_I2C_ADD_H, Reg,
                      I2C_MEMADD_SIZE_8BIT, Bufp, len, 1000);
  }
#ifdef MKI109V2
  else if (handle == &hspi2)
  {
    /* enable auto incremented in multiple read/write commands */
    Reg |= 0x40;
    HAL_GPIO_WritePin(CS_SPI2_GPIO_Port, CS_SPI2_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &Reg, 1, 1000);
    HAL_SPI_Transmit(handle, Bufp, len, 1000);
    HAL_GPIO_WritePin(CS_SPI2_GPIO_Port, CS_SPI2_Pin, GPIO_PIN_SET);
  }
  else if (handle == &hspi1)
  {
    /* enable auto incremented in multiple read/write commands */
    Reg |= 0x40;
    HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &Reg, 1, 1000);
    HAL_SPI_Transmit(handle, Bufp, len, 1000);
    HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_SET);
  }
#endif
  return 0;
}

static int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp,
                             uint16_t len)
{
  if (handle == &hi2c1)
  {
    /* enable auto incremented in multiple read/write commands */
    Reg |= 0x80;
    HAL_I2C_Mem_Read(handle, LIS3DE_I2C_ADD_H, Reg,
                     I2C_MEMADD_SIZE_8BIT, Bufp, len, 1000);
  }
#ifdef MKI109V2
  else if (handle == &hspi2)
  {
    /* enable auto incremented in multiple read/write commands */
    Reg |= 0xC0;
    HAL_GPIO_WritePin(CS_DEV_GPIO_Port, CS_DEV_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &Reg, 1, 1000);
    HAL_SPI_Receive(handle, Bufp, len, 1000);
    HAL_GPIO_WritePin(CS_DEV_GPIO_Port, CS_DEV_Pin, GPIO_PIN_SET);
  }
  else
  {
    /* enable auto incremented in multiple read/write commands */
    Reg |= 0xC0;
    HAL_GPIO_WritePin(CS_RF_GPIO_Port, CS_RF_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(handle, &Reg, 1, 1000);
    HAL_SPI_Receive(handle, Bufp, len, 1000);
    HAL_GPIO_WritePin(CS_RF_GPIO_Port, CS_RF_Pin, GPIO_PIN_SET);
  }
#endif
  return 0;
}

/*
 *  Function to print messages
 */
static void tx_com( uint8_t *tx_buffer, uint16_t len )
{
  #ifdef NUCLEO_STM32F411RE
  HAL_UART_Transmit( &huart2, tx_buffer, len, 1000 );
  #endif
  #ifdef MKI109V2
  CDC_Transmit_FS( tx_buffer, len );
  #endif
}

/*
 * Function to read external interrupt pin.
 */
static int32_t platform_reap_int_pin(void)
{
#ifdef NUCLEO_STM32F411RE
    return HAL_GPIO_ReadPin(LIS3DE_INT1_GPIO_PORT, LIS3DE_INT1_PIN);
#else /* NUCLEO_STM32F411RE */
    return 0;
#endif /* NUCLEO_STM32F411RE */
}

/* Main Example --------------------------------------------------------------*/

/*
 * Set interrupt threshold to 16h -> 350 mg
 * Set interrupt Duration to 03h -> minimum event duration 30 ms
 * Configure free-fall recognition
 * Poll on platform INT pin 1 waiting for free fall event detection
 */
void example_freefall_lis3de(void)
{
  stmdev_ctx_t dev_ctx;
  lis3de_ctrl_reg3_t ctrl_reg3;
  lis3de_ig1_cfg_t ig1_cfg;
 
  uint8_t whoamI;

  /* Initialize mems driver interface. */
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.handle = &hi2c1;

  /* Check device ID. */
  whoamI = 0;
  lis3de_device_id_get(&dev_ctx, &whoamI);
  if (whoamI != LIS3DE_ID)
    while(1); /* manage here device not found */

  /* Set Output Data Rate to 100 Hz. */
  lis3de_data_rate_set(&dev_ctx, LIS3DE_ODR_100Hz);

  /* Set full scale to 2 g. */
  lis3de_full_scale_set(&dev_ctx, LIS3DE_2g);

  /* Enable AOI1 interrupt on INT pin 1. */
  memset((uint8_t *)&ctrl_reg3, 0, sizeof(ctrl_reg3));
  ctrl_reg3.int1_ig1 = PROPERTY_ENABLE;
  lis3de_pin_int1_config_set(&dev_ctx, &ctrl_reg3);

  /* Enable Interrupt 1 pin latched. */
  lis3de_int1_pin_notification_mode_set(&dev_ctx, LIS3DE_INT1_LATCHED);

  /*
   * Set threshold to 16h -> 350 mg
   * Set Duration to 03h -> minimum event duration
   * If acceleration an all axis is below the threshold for more
   * than 30 ms than device is falling down
   */
  lis3de_int1_gen_threshold_set(&dev_ctx, 0x16);
  lis3de_int1_gen_duration_set(&dev_ctx, 0x03);

  /*
   * Configure free-fall recognition
   * Enable condiction (AND) for x, y, z acc. data below threshold.
   */
  memset((uint8_t *)&ig1_cfg, 0, sizeof(ig1_cfg));
  ig1_cfg.aoi = PROPERTY_ENABLE;
  ig1_cfg.zlie = PROPERTY_ENABLE;
  ig1_cfg.ylie = PROPERTY_ENABLE;
  ig1_cfg.xlie = PROPERTY_ENABLE;
  lis3de_int1_gen_conf_set(&dev_ctx, &ig1_cfg);

  /* Set device in HR mode. */
  lis3de_operating_mode_set(&dev_ctx, LIS3DE_LP);

  while(1)
  {
 /* Read INT pin 1 in polling mode. */
  lis3de_ig1_source_t src;

    if (platform_reap_int_pin())
    {
      lis3de_int1_gen_source_get(&dev_ctx, &src);
      sprintf((char*)tx_buffer, "freefall detected\r\n");
      tx_com(tx_buffer, strlen((char const*)tx_buffer));
    }
  }
}
