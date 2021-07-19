
#ifndef SDRAM_STARTUP_H
#define SDRAM_STARTUP_H

#include <stm32h7xx_hal.h>
// #include "clock_structs.h"
// #include <stm32h7xx_hal_rcc.h>
#include <stdint.h>
#include "util/hal_map.h"


#define SDRAM_MODEREG_BURST_LENGTH_2 ((1 << 0))
#define SDRAM_MODEREG_BURST_LENGTH_4 ((1 << 1))

#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL ((0 << 3))

#define SDRAM_MODEREG_CAS_LATENCY_3 ((1 << 4) | (1 << 5))

#define SDRAM_MODEREG_OPERATING_MODE_STANDARD ()

#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE ((1 << 9))
#define SDRAM_MODEREG_WRITEBURST_MODE_PROG_BURST ((0 << 9))

#define PLL2_TIMEOUT_VALUE         PLL_TIMEOUT_VALUE    /* 2 ms */
#define PLL3_TIMEOUT_VALUE         PLL_TIMEOUT_VALUE    /* 2 ms */

#define DIVIDER_P_UPDATE          0U
#define DIVIDER_Q_UPDATE          1U
#define DIVIDER_R_UPDATE          2U

typedef struct
{
  uint8_t             board;
  SDRAM_HandleTypeDef hsdram;
} dsy_sdram_t;

void SdramInit();
void PeriphInit();
void DeviceInit();
void SdramMpuInit();
void SystemClock();
HAL_StatusTypeDef Supply(uint32_t SupplySource);
__weak HAL_StatusTypeDef OscConfig(RCC_OscInitTypeDef  *RCC_OscInitStruct);
HAL_StatusTypeDef ClockConfig(RCC_ClkInitTypeDef  *RCC_ClkInitStruct, uint32_t FLatency);
HAL_StatusTypeDef PeriphCLKConfig(RCC_PeriphCLKInitTypeDef  *PeriphClkInit);
HAL_StatusTypeDef PLL3_Config(RCC_PLL3InitTypeDef *pll3, uint32_t Divider);
HAL_StatusTypeDef PLL2_Config(RCC_PLL2InitTypeDef *pll2, uint32_t Divider);

#define TICKS_PER_US 480e6f * 1e-6f

#endif