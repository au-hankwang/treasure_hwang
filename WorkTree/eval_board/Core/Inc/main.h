/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#define EVAL_BOARD_PROJECT

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define id33_0__Pin GPIO_PIN_13
#define id33_0__GPIO_Port GPIOC
#define id33_1__Pin GPIO_PIN_14
#define id33_1__GPIO_Port GPIOC
#define id33_2__Pin GPIO_PIN_15
#define id33_2__GPIO_Port GPIOC
#define id33_3__Pin GPIO_PIN_0
#define id33_3__GPIO_Port GPIOA
#define io_enable_Pin GPIO_PIN_1
#define io_enable_GPIO_Port GPIOA
#define response_i_Pin GPIO_PIN_2
#define response_i_GPIO_Port GPIOA
#define command_o_Pin GPIO_PIN_3
#define command_o_GPIO_Port GPIOA
#define psu_enable_Pin GPIO_PIN_4
#define psu_enable_GPIO_Port GPIOA
#define id33_5__Pin GPIO_PIN_5
#define id33_5__GPIO_Port GPIOA
#define id33_7__Pin GPIO_PIN_8
#define id33_7__GPIO_Port GPIOA
#define scv_enable_Pin GPIO_PIN_6
#define scv_enable_GPIO_Port GPIOF
#define test_mode_i_Pin GPIO_PIN_7
#define test_mode_i_GPIO_Port GPIOF
#define thermal_trip_o33_Pin GPIO_PIN_4
#define thermal_trip_o33_GPIO_Port GPIOB
#define reset_i33_Pin GPIO_PIN_5
#define reset_i33_GPIO_Port GPIOB
#define enable_25mhz_Pin GPIO_PIN_6
#define enable_25mhz_GPIO_Port GPIOB
#define thermal_trip_i33_Pin GPIO_PIN_8
#define thermal_trip_i33_GPIO_Port GPIOB
#define test_clock_out_Pin GPIO_PIN_9
#define test_clock_out_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
