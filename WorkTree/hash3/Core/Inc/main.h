/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#define HASH3_PROJECT


/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define rx_enable1_Pin GPIO_PIN_13
#define rx_enable1_GPIO_Port GPIOC
#define spi_switch_reset_n_Pin GPIO_PIN_14
#define spi_switch_reset_n_GPIO_Port GPIOC
#define vdd_enable1_Pin GPIO_PIN_15
#define vdd_enable1_GPIO_Port GPIOC
#define thermal_trip2_Pin GPIO_PIN_0
#define thermal_trip2_GPIO_Port GPIOA
#define led_red_Pin GPIO_PIN_4
#define led_red_GPIO_Port GPIOA
#define led_blue_Pin GPIO_PIN_5
#define led_blue_GPIO_Port GPIOA
#define thermal_trip1_Pin GPIO_PIN_6
#define thermal_trip1_GPIO_Port GPIOA
#define rx_enable2_Pin GPIO_PIN_0
#define rx_enable2_GPIO_Port GPIOB
#define psu_enable_Pin GPIO_PIN_1
#define psu_enable_GPIO_Port GPIOB
#define rx_enable3_Pin GPIO_PIN_2
#define rx_enable3_GPIO_Port GPIOB
#define sw2_Pin GPIO_PIN_8
#define sw2_GPIO_Port GPIOE
#define sw1_Pin GPIO_PIN_9
#define sw1_GPIO_Port GPIOE
#define tx_enable3_Pin GPIO_PIN_14
#define tx_enable3_GPIO_Port GPIOB
#define vdd_enable3_Pin GPIO_PIN_15
#define vdd_enable3_GPIO_Port GPIOB
#define reset_n_Pin GPIO_PIN_8
#define reset_n_GPIO_Port GPIOA
#define thermal_trip3_Pin GPIO_PIN_4
#define thermal_trip3_GPIO_Port GPIOB
#define vdd_enable2_Pin GPIO_PIN_5
#define vdd_enable2_GPIO_Port GPIOB
#define tx_enable2_Pin GPIO_PIN_6
#define tx_enable2_GPIO_Port GPIOB
#define tx_enable1_Pin GPIO_PIN_8
#define tx_enable1_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
