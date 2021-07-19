#include "sdram_startup.h"

void SdramInit()
{
  // DelayProcessMs(500);
  // SystemClock();
  PeriphInit();
  DeviceInit();
  SdramMpuInit();
}

extern RCC_OscInitTypeDef RCC_OscInitStruct;
extern RCC_ClkInitTypeDef RCC_ClkInitStruct;
extern RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

static dsy_sdram_t dsy_sdram;

void EnableProcessTick()
{
    DWT->CTRL = 1;
}

float GetProcessTickUs()
{
    return (float) DWT->CYCCNT / TICKS_PER_US;
}

float GetProcessTickMs()
{
    return GetProcessTickUs() / 1e3f;
}

void DelayProcessUs(float microseconds)
{
  DWT->CTRL = 1; // ensure enable CYCCNT bit

  uint32_t start = DWT->CYCCNT;

  // This assumes we're using the HSI oscillator, which
  // for the STM32H750 is 64MHz

  uint32_t num_ticks = microseconds * TICKS_PER_US;

  while (DWT->CYCCNT - start < num_ticks);

  DWT->CYCCNT = 0; 

//   // Disabling the counter after its use
//   DWT->CTRL = 0;
}

void DelayProcessMs(float milliseconds)
{
    DelayProcessUs(milliseconds * 1e3f);
}

void PeriphInit()
{
  FMC_SDRAM_TimingTypeDef SdramTiming = {0};
  dsy_sdram.hsdram.Instance           = FMC_SDRAM_DEVICE;
  // Init
  dsy_sdram.hsdram.Init.SDBank             = FMC_SDRAM_BANK1;
  dsy_sdram.hsdram.Init.ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_9;
  dsy_sdram.hsdram.Init.RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_13;
  dsy_sdram.hsdram.Init.MemoryDataWidth    = FMC_SDRAM_MEM_BUS_WIDTH_32;
  dsy_sdram.hsdram.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  dsy_sdram.hsdram.Init.CASLatency         = FMC_SDRAM_CAS_LATENCY_3;
  dsy_sdram.hsdram.Init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  dsy_sdram.hsdram.Init.SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2;
  dsy_sdram.hsdram.Init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
  dsy_sdram.hsdram.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay    = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime      = 4;
  SdramTiming.RowCycleDelay        = 8; // started at 7
  SdramTiming.WriteRecoveryTime    = 3;
  SdramTiming.RPDelay              = 0;
  SdramTiming.RCDDelay             = 10; // started at 2
  //	SdramTiming.LoadToActiveDelay = 16;
  //	SdramTiming.ExitSelfRefreshDelay = 16;
  //	SdramTiming.SelfRefreshTime = 16;
  //	SdramTiming.RowCycleDelay = 16;
  //	SdramTiming.WriteRecoveryTime = 16;
  //	SdramTiming.RPDelay = 16;
  //	SdramTiming.RCDDelay = 16;

  if(HAL_SDRAM_Init(&dsy_sdram.hsdram, &SdramTiming) != HAL_OK)
  {
      //Error_Handler();
      // return Result::ERR;
  }
  // return Result::OK;
}

void DeviceInit()
{
  FMC_SDRAM_CommandTypeDef Command;

  __IO uint32_t tmpmrd = 0;
  /* Step 3:  Configure a clock configuration enable command */
  Command.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(&dsy_sdram.hsdram, &Command, 0x1000);

  /* Step 4: Insert 100 ms delay */
  // HAL_Delay(100);
  // DelayProcessMs(100);

  // volatile to ensure no optimization occurs
  for (volatile int i = 0; i < (int) 64e5; i++);


  /* Step 5: Configure a PALL (precharge all) command */
  Command.CommandMode            = FMC_SDRAM_CMD_PALL;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(&dsy_sdram.hsdram, &Command, 0x1000);

  /* Step 6 : Configure a Auto-Refresh command */
  Command.CommandMode            = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 4;
  Command.ModeRegisterDefinition = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(&dsy_sdram.hsdram, &Command, 0x1000);

  /* Step 7: Program the external memory mode register */
  tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_4
            | SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL | SDRAM_MODEREG_CAS_LATENCY_3
            | SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
  //SDRAM_MODEREG_OPERATING_MODE_STANDARD | // Used in example, but can't find reference except for "Test Mode"

  Command.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = tmpmrd;

  /* Send the command */
  HAL_SDRAM_SendCommand(&dsy_sdram.hsdram, &Command, 0x1000);

  //HAL_SDRAM_ProgramRefreshRate(hsdram, 0x56A - 20);
  HAL_SDRAM_ProgramRefreshRate(&dsy_sdram.hsdram, 0x81A - 20);
  // return Result::OK;
}

static uint32_t FMC_Initialized = 0;

static void HAL_FMC_MspInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(FMC_Initialized)
    {
        return;
    }
    FMC_Initialized = 1;

    /* Peripheral clock enable */
    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    // for SDNWE on some boards:
    __HAL_RCC_GPIOC_CLK_ENABLE();


    /** FMC GPIO Configuration  
	PE1   ------> FMC_NBL1
	PE0   ------> FMC_NBL0
	PG15   ------> FMC_SDNCAS
	PD0   ------> FMC_D2
	PI7   ------> FMC_D29
	PI6   ------> FMC_D28
	PI5   ------> FMC_NBL3
	PD1   ------> FMC_D3
	PI3   ------> FMC_D27
	PI2   ------> FMC_D26
	PI9   ------> FMC_D30
	PI4   ------> FMC_NBL2
	PH15   ------> FMC_D23
	PI1   ------> FMC_D25
	PF0   ------> FMC_A0
	PI10   ------> FMC_D31
	PH13   ------> FMC_D21
	PH14   ------> FMC_D22
	PI0   ------> FMC_D24
	PH2   ------> FMC_SDCKE0
	PH3   ------> FMC_SDNE0
	PF2   ------> FMC_A2
	PF1   ------> FMC_A1
	PG8   ------> FMC_SDCLK
	PF3   ------> FMC_A3
	PF4   ------> FMC_A4
	PF5   ------> FMC_A5
	PH12   ------> FMC_D20
	PG5   ------> FMC_BA1
	PG4   ------> FMC_BA0
	PH11   ------> FMC_D19
	PH10   ------> FMC_D18
	PD15   ------> FMC_D1
	PG2   ------> FMC_A12
	PC0   ------> FMC_SDNWE
	PG1   ------> FMC_A11
	PH8   ------> FMC_D16
	PH9   ------> FMC_D17
	PD14   ------> FMC_D0
	PF13   ------> FMC_A7
	PG0   ------> FMC_A10
	PE13   ------> FMC_D10
	PD10   ------> FMC_D15
	PF12   ------> FMC_A6
	PF15   ------> FMC_A9
	PE8   ------> FMC_D5
	PE9   ------> FMC_D6
	PE11   ------> FMC_D8
	PE14   ------> FMC_D11
	PD9   ------> FMC_D14
	PD8   ------> FMC_D13
	PF11   ------> FMC_SDNRAS
	PF14   ------> FMC_A8
	PE7   ------> FMC_D4
	PE10   ------> FMC_D7
	PE12   ------> FMC_D9
	PE15   ------> FMC_D12
	*/
    /* GPIO_InitStruct */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_13 | GPIO_PIN_8
                          | GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_14 | GPIO_PIN_7
                          | GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_15;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* GPIO_InitStruct */
    GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_4
                          | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* GPIO_InitStruct */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_14
                          | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_8;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* GPIO_InitStruct */
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_3
                          | GPIO_PIN_2 | GPIO_PIN_9 | GPIO_PIN_4 | GPIO_PIN_1
                          | GPIO_PIN_10 | GPIO_PIN_0;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

    /* GPIO_InitStruct */
    GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_2
                          | GPIO_PIN_3 | GPIO_PIN_12 | GPIO_PIN_11 | GPIO_PIN_10
                          | GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    /* GPIO_InitStruct */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_3
                          | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_12
                          | GPIO_PIN_15 | GPIO_PIN_11 | GPIO_PIN_14;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // Init for any pins that can be configured
    GPIO_TypeDef *port;
    port                      = GPIOH;
    GPIO_InitStruct.Pin       = GPIO_PIN_5;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC; // They all seem to use this
    HAL_GPIO_Init(port, &GPIO_InitStruct);

    // This pin can change between boards (SDNWE)
    //	switch(dsy_sdram.board)
    //	{
    //	case DSY_SYS_BOARD_DAISY:
    //		/* GPIO_InitStruct */
    //		GPIO_InitStruct.Pin = GPIO_PIN_0;
    //		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    //		GPIO_InitStruct.Pull = GPIO_NOPULL;
    //		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    //		GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    //		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    //		break;
    //	case DSY_SYS_BOARD_DAISY_SEED:
    //		/* GPIO_InitStruct */
    //		GPIO_InitStruct.Pin = GPIO_PIN_5;
    //		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    //		GPIO_InitStruct.Pull = GPIO_NOPULL;
    //		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    //		GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    //		HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
    //		break;
    //	case DSY_SYS_BOARD_AUDIO_BB:
    //		/* GPIO_InitStruct */
    //		GPIO_InitStruct.Pin = GPIO_PIN_5;
    //		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    //		GPIO_InitStruct.Pull = GPIO_NOPULL;
    //		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    //		GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    //		HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
    //		break;
    //	default:
    //		break;
    //	}

    /* USER CODE BEGIN FMC_MspInit 1 */

    /* USER CODE END FMC_MspInit 1 */
}

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *sdramHandle)
{
    /* USER CODE BEGIN SDRAM_MspInit 0 */

    /* USER CODE END SDRAM_MspInit 0 */
    HAL_FMC_MspInit();
    /* USER CODE BEGIN SDRAM_MspInit 1 */

    /* USER CODE END SDRAM_MspInit 1 */
}

static uint32_t FMC_DeInitialized = 0;

static void HAL_FMC_MspDeInit(void)
{
    /* USER CODE BEGIN FMC_MspDeInit 0 */

    /* USER CODE END FMC_MspDeInit 0 */
    if(FMC_DeInitialized)
    {
        return;
    }
    FMC_DeInitialized = 1;
    /* Peripheral clock enable */
    __HAL_RCC_FMC_CLK_DISABLE();

    /** FMC GPIO Configuration  
	PE1   ------> FMC_NBL1
	PE0   ------> FMC_NBL0
	PG15   ------> FMC_SDNCAS
	PD0   ------> FMC_D2
	PI7   ------> FMC_D29
	PI6   ------> FMC_D28
	PI5   ------> FMC_NBL3
	PD1   ------> FMC_D3
	PI3   ------> FMC_D27
	PI2   ------> FMC_D26
	PI9   ------> FMC_D30
	PI4   ------> FMC_NBL2
	PH15   ------> FMC_D23
	PI1   ------> FMC_D25
	PF0   ------> FMC_A0
	PI10   ------> FMC_D31
	PH13   ------> FMC_D21
	PH14   ------> FMC_D22
	PI0   ------> FMC_D24
	PH2   ------> FMC_SDCKE0
	PH3   ------> FMC_SDNE0
	PF2   ------> FMC_A2
	PF1   ------> FMC_A1
	PG8   ------> FMC_SDCLK
	PF3   ------> FMC_A3
	PF4   ------> FMC_A4
	PF5   ------> FMC_A5
	PH12   ------> FMC_D20
	PG5   ------> FMC_BA1
	PG4   ------> FMC_BA0
	PH11   ------> FMC_D19
	PH10   ------> FMC_D18
	PD15   ------> FMC_D1
	PG2   ------> FMC_A12
	PC0   ------> FMC_SDNWE
	PG1   ------> FMC_A11
	PH8   ------> FMC_D16
	PH9   ------> FMC_D17
	PD14   ------> FMC_D0
	PF13   ------> FMC_A7
	PG0   ------> FMC_A10
	PE13   ------> FMC_D10
	PD10   ------> FMC_D15
	PF12   ------> FMC_A6
	PF15   ------> FMC_A9
	PE8   ------> FMC_D5
	PE9   ------> FMC_D6
	PE11   ------> FMC_D8
	PE14   ------> FMC_D11
	PD9   ------> FMC_D14
	PD8   ------> FMC_D13
	PF11   ------> FMC_SDNRAS
	PF14   ------> FMC_A8
	PE7   ------> FMC_D4
	PE10   ------> FMC_D7
	PE12   ------> FMC_D9
	PE15   ------> FMC_D12
	*/

    HAL_GPIO_DeInit(GPIOE,
                    GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_13 | GPIO_PIN_8
                        | GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_14 | GPIO_PIN_7
                        | GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_15);

    HAL_GPIO_DeInit(GPIOG,
                    GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_4
                        | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0);

    HAL_GPIO_DeInit(GPIOD,
                    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_14
                        | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_8);

    HAL_GPIO_DeInit(GPIOI,
                    GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_3
                        | GPIO_PIN_2 | GPIO_PIN_9 | GPIO_PIN_4 | GPIO_PIN_1
                        | GPIO_PIN_10 | GPIO_PIN_0);

    HAL_GPIO_DeInit(GPIOH,
                    GPIO_PIN_15 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_2
                        | GPIO_PIN_3 | GPIO_PIN_12 | GPIO_PIN_11 | GPIO_PIN_10
                        | GPIO_PIN_8 | GPIO_PIN_9);

    HAL_GPIO_DeInit(GPIOF,
                    GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_3
                        | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_12
                        | GPIO_PIN_15 | GPIO_PIN_11 | GPIO_PIN_14);

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0);

    /* USER CODE BEGIN FMC_MspDeInit 1 */

    /* USER CODE END FMC_MspDeInit 1 */
}

void HAL_SDRAM_MspDeInit(SDRAM_HandleTypeDef *sdramHandle)
{
    /* USER CODE BEGIN SDRAM_MspDeInit 0 */

    /* USER CODE END SDRAM_MspDeInit 0 */
    HAL_FMC_MspDeInit();
    /* USER CODE BEGIN SDRAM_MspDeInit 1 */

    /* USER CODE END SDRAM_MspDeInit 1 */
}

void SdramMpuInit()
{
    MPU_Region_InitTypeDef MPU_InitStruct;
    HAL_MPU_Disable();
    // Configure RAM D2 (SRAM1) as non cacheable
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress      = 0x30000000;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_32KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    MPU_InitStruct.IsCacheable  = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    MPU_InitStruct.IsShareable  = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number       = MPU_REGION_NUMBER1;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.Size         = MPU_REGION_SIZE_64MB;
    MPU_InitStruct.BaseAddress  = 0xC0000000;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void __attribute__((constructor)) SDRAM_Init()
{
    //	extern void *_sisdram_data, *_ssdram_data, *_esdram_data;
    //	extern void *_ssdram_bss, *_esdram_bss;

    //	void **pSource, **pDest;
    //	for (pSource = &_sisdram_data, pDest = &_ssdram_data; pDest != &_esdram_data; pSource++, pDest++)
    //		*pDest = *pSource;
    //
    //	for (pDest = &_ssdram_bss; pDest != &_esdram_bss; pDest++)
    //		*pDest = 0;
}

// Minimal clock startup (needs routines that don't depend on HAL_Tick!)
void SystemClock(void)
{
  // // RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  // // RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  // // RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  // /** Supply configuration update enable
  // */
  // Supply(PWR_LDO_SUPPLY);
  // /** Configure the main internal regulator output voltage
  // */
  // __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  // while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  // /** Macro to configure the PLL clock source
  // */
  // __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSI);
  // /** Initializes the CPU, AHB and APB busses clocks
  // */
  // RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  // RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  // RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  // RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  // RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

  // if (OscConfig(&RCC_OscInitStruct) != HAL_OK)
  // {
  //   // Error_Handler();
  // }
  // /** Initializes the CPU, AHB and APB busses clocks
  // */
  // RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
  //                             |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
  //                             |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  // RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  // RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  // RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  // RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  // RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  // RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  // RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  // if (ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  // {
  //   // Error_Handler();
  // }
  // PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC;
  // PeriphClkInitStruct.PLL2.PLL2M = 4;
  // PeriphClkInitStruct.PLL2.PLL2N = 12;
  // PeriphClkInitStruct.PLL2.PLL2P = 8;
  // PeriphClkInitStruct.PLL2.PLL2Q = 2;
  // PeriphClkInitStruct.PLL2.PLL2R = 2;
  // PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  // PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  // PeriphClkInitStruct.PLL2.PLL2FRACN = 4096;
  // PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;

  // if (PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  // {
  //   // Error_Handler();
  // }

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Macro to configure the PLL clock source
  */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC;
  PeriphClkInitStruct.PLL2.PLL2M = 1;
  PeriphClkInitStruct.PLL2.PLL2N = 12;
  PeriphClkInitStruct.PLL2.PLL2P = 8;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 4096;
  PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

HAL_StatusTypeDef Supply(uint32_t SupplySource)
{
  uint32_t tickstart;
  EnableProcessTick();

  /* Check the parameters */
  assert_param(IS_PWR_SUPPLY(SupplySource));

  if(!__HAL_PWR_GET_FLAG(PWR_FLAG_SCUEN))
  {
    if((PWR->CR3 & PWR_SUPPLY_CONFIG_MASK) != SupplySource)
    {
      /* Supply configuration update locked, can't apply a new regulator config */
      return HAL_ERROR;
    }
  }

  /* Set the power supply configuration */
  MODIFY_REG(PWR->CR3, PWR_SUPPLY_CONFIG_MASK, SupplySource);

  /* Get tick */
  tickstart = GetProcessTickUs();

  /* Wait till voltage level flag is set */
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_ACTVOSRDY))
  {
    if((GetProcessTickUs() - tickstart ) > 1000)
    {
      return HAL_TIMEOUT;
    }
  }

  return HAL_OK;
}

__weak HAL_StatusTypeDef OscConfig(RCC_OscInitTypeDef  *RCC_OscInitStruct)
{
  uint32_t tickstart;

    /* Check Null pointer */
  if(RCC_OscInitStruct == NULL)
  {
    return HAL_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_RCC_OSCILLATORTYPE(RCC_OscInitStruct->OscillatorType));
  /*------------------------------- HSE Configuration ------------------------*/
  if(((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_HSE) == RCC_OSCILLATORTYPE_HSE)
  {
    /* Check the parameters */
    assert_param(IS_RCC_HSE(RCC_OscInitStruct->HSEState));

    const uint32_t temp_sysclksrc = __HAL_RCC_GET_SYSCLK_SOURCE();
    const uint32_t temp_pllckselr = RCC->PLLCKSELR;
    /* When the HSE is used as system clock or clock source for PLL in these cases HSE will not disabled */
    if((temp_sysclksrc == RCC_CFGR_SWS_HSE) || ((temp_sysclksrc == RCC_CFGR_SWS_PLL1) && ((temp_pllckselr & RCC_PLLCKSELR_PLLSRC) == RCC_PLLCKSELR_PLLSRC_HSE)))
    {
      if((__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) != 0U) && (RCC_OscInitStruct->HSEState == RCC_HSE_OFF))
      {
        return HAL_ERROR;
      }
    }
    else
    {
      /* Set the new HSE configuration ---------------------------------------*/
      __HAL_RCC_HSE_CONFIG(RCC_OscInitStruct->HSEState);

      /* Check the HSE State */
      if(RCC_OscInitStruct->HSEState != RCC_HSE_OFF)
      {
        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till HSE is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == 0U)
        {
          if((uint32_t) (GetProcessTickMs() - tickstart ) > HSE_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }
      }
      else
      {
        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till HSE is bypassed or disabled */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) != 0U)
        {
          if((uint32_t) (GetProcessTickMs() - tickstart ) > HSE_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }
      }
    }
  }
  /*----------------------------- HSI Configuration --------------------------*/
  if(((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_HSI) == RCC_OSCILLATORTYPE_HSI)
  {
    /* Check the parameters */
    assert_param(IS_RCC_HSI(RCC_OscInitStruct->HSIState));
    assert_param(IS_RCC_HSICALIBRATION_VALUE(RCC_OscInitStruct->HSICalibrationValue));

    /* When the HSI is used as system clock it will not disabled */
    const uint32_t temp_sysclksrc = __HAL_RCC_GET_SYSCLK_SOURCE();
    const uint32_t temp_pllckselr = RCC->PLLCKSELR;
    if((temp_sysclksrc == RCC_CFGR_SWS_HSI) || ((temp_sysclksrc == RCC_CFGR_SWS_PLL1) && ((temp_pllckselr & RCC_PLLCKSELR_PLLSRC) == RCC_PLLCKSELR_PLLSRC_HSI)))
    {
      /* When HSI is used as system clock it will not disabled */
      if((__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) != 0U) && (RCC_OscInitStruct->HSIState == RCC_HSI_OFF))
      {
        return HAL_ERROR;
      }
      /* Otherwise, just the calibration is allowed */
      else
      {
      /* Enable the Internal High Speed oscillator (HSI, HSIDIV2,HSIDIV4, or HSIDIV8) */
        __HAL_RCC_HSI_CONFIG(RCC_OscInitStruct->HSIState);

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till HSI is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == 0U)
        {
          if((uint32_t) (GetProcessTickMs() - tickstart ) > HSI_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }
        /* Adjusts the Internal High Speed oscillator (HSI) calibration value.*/
        __HAL_RCC_HSI_CALIBRATIONVALUE_ADJUST(RCC_OscInitStruct->HSICalibrationValue);
      }
    }

    else
    {
      /* Check the HSI State */
      if((RCC_OscInitStruct->HSIState)!= RCC_HSI_OFF)
      {
     /* Enable the Internal High Speed oscillator (HSI, HSIDIV2,HSIDIV4, or HSIDIV8) */
        __HAL_RCC_HSI_CONFIG(RCC_OscInitStruct->HSIState);

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till HSI is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == 0U)
        {
          if((GetProcessTickMs() - tickstart ) > HSI_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }

        /* Adjusts the Internal High Speed oscillator (HSI) calibration value.*/
        __HAL_RCC_HSI_CALIBRATIONVALUE_ADJUST(RCC_OscInitStruct->HSICalibrationValue);
      }
      else
      {
        /* Disable the Internal High Speed oscillator (HSI). */
        __HAL_RCC_HSI_DISABLE();

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till HSI is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) != 0U)
        {
          if((GetProcessTickMs() - tickstart ) > HSI_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }
      }
    }
  }
  /*----------------------------- CSI Configuration --------------------------*/
  if(((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_CSI) == RCC_OSCILLATORTYPE_CSI)
  {
    /* Check the parameters */
    assert_param(IS_RCC_CSI(RCC_OscInitStruct->CSIState));
    assert_param(IS_RCC_CSICALIBRATION_VALUE(RCC_OscInitStruct->CSICalibrationValue));

    /* When the CSI is used as system clock it will not disabled */
    const uint32_t temp_sysclksrc = __HAL_RCC_GET_SYSCLK_SOURCE();
    const uint32_t temp_pllckselr = RCC->PLLCKSELR;
    if((temp_sysclksrc == RCC_CFGR_SWS_CSI) || ((temp_sysclksrc == RCC_CFGR_SWS_PLL1) && ((temp_pllckselr & RCC_PLLCKSELR_PLLSRC) == RCC_PLLCKSELR_PLLSRC_CSI)))
    {
      /* When CSI is used as system clock it will not disabled */
      if((__HAL_RCC_GET_FLAG(RCC_FLAG_CSIRDY) != 0U) && (RCC_OscInitStruct->CSIState != RCC_CSI_ON))
      {
        return HAL_ERROR;
      }
      /* Otherwise, just the calibration is allowed */
      else
      {
        /* Adjusts the Internal High Speed oscillator (CSI) calibration value.*/
        __HAL_RCC_CSI_CALIBRATIONVALUE_ADJUST(RCC_OscInitStruct->CSICalibrationValue);
      }
    }
    else
    {
      /* Check the CSI State */
      if((RCC_OscInitStruct->CSIState)!= RCC_CSI_OFF)
      {
        /* Enable the Internal High Speed oscillator (CSI). */
        __HAL_RCC_CSI_ENABLE();

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till CSI is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_CSIRDY) == 0U)
        {
          if((GetProcessTickMs() - tickstart ) > CSI_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }

        /* Adjusts the Internal High Speed oscillator (CSI) calibration value.*/
        __HAL_RCC_CSI_CALIBRATIONVALUE_ADJUST(RCC_OscInitStruct->CSICalibrationValue);
      }
      else
      {
        /* Disable the Internal High Speed oscillator (CSI). */
        __HAL_RCC_CSI_DISABLE();

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till CSI is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_CSIRDY) != 0U)
        {
          if((GetProcessTickMs() - tickstart ) > CSI_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }
      }
    }
  }
  /*------------------------------ LSI Configuration -------------------------*/
  if(((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_LSI) == RCC_OSCILLATORTYPE_LSI)
  {
    /* Check the parameters */
    assert_param(IS_RCC_LSI(RCC_OscInitStruct->LSIState));

    /* Check the LSI State */
    if((RCC_OscInitStruct->LSIState)!= RCC_LSI_OFF)
    {
      /* Enable the Internal Low Speed oscillator (LSI). */
      __HAL_RCC_LSI_ENABLE();

      /* Get Start Tick*/
      tickstart = GetProcessTickMs();

      /* Wait till LSI is ready */
      while(__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == 0U)
      {
        if((GetProcessTickMs() - tickstart ) > LSI_TIMEOUT_VALUE)
        {
          return HAL_TIMEOUT;
        }
      }
    }
    else
    {
      /* Disable the Internal Low Speed oscillator (LSI). */
      __HAL_RCC_LSI_DISABLE();

      /* Get Start Tick*/
      tickstart = GetProcessTickMs();

      /* Wait till LSI is ready */
      while(__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) != 0U)
      {
        if((GetProcessTickMs() - tickstart ) > LSI_TIMEOUT_VALUE)
        {
          return HAL_TIMEOUT;
        }
      }
    }
  }

  /*------------------------------ HSI48 Configuration -------------------------*/
  if(((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_HSI48) == RCC_OSCILLATORTYPE_HSI48)
  {
    /* Check the parameters */
    assert_param(IS_RCC_HSI48(RCC_OscInitStruct->HSI48State));

    /* Check the HSI48 State */
    if((RCC_OscInitStruct->HSI48State)!= RCC_HSI48_OFF)
    {
      /* Enable the Internal Low Speed oscillator (HSI48). */
      __HAL_RCC_HSI48_ENABLE();

      /* Get time-out */
      tickstart = GetProcessTickMs();

      /* Wait till HSI48 is ready */
      while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) == 0U)
      {
        if((GetProcessTickMs() - tickstart ) > HSI48_TIMEOUT_VALUE)
        {
          return HAL_TIMEOUT;
        }
      }
    }
    else
    {
      /* Disable the Internal Low Speed oscillator (HSI48). */
      __HAL_RCC_HSI48_DISABLE();

      /* Get time-out */
      tickstart = GetProcessTickMs();

      /* Wait till HSI48 is ready */
      while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) != 0U)
      {
        if((GetProcessTickMs() - tickstart ) > HSI48_TIMEOUT_VALUE)
        {
          return HAL_TIMEOUT;
        }
      }
    }
  }
  /*------------------------------ LSE Configuration -------------------------*/
  if(((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_LSE) == RCC_OSCILLATORTYPE_LSE)
  {
    /* Check the parameters */
    assert_param(IS_RCC_LSE(RCC_OscInitStruct->LSEState));

    /* Enable write access to Backup domain */
    PWR->CR1 |= PWR_CR1_DBP;

    /* Wait for Backup domain Write protection disable */
    tickstart = GetProcessTickMs();

    while((PWR->CR1 & PWR_CR1_DBP) == 0U)
    {
      if((GetProcessTickMs() - tickstart ) > RCC_DBP_TIMEOUT_VALUE)
      {
        return HAL_TIMEOUT;
      }
    }

    /* Set the new LSE configuration -----------------------------------------*/
    __HAL_RCC_LSE_CONFIG(RCC_OscInitStruct->LSEState);
    /* Check the LSE State */
    if((RCC_OscInitStruct->LSEState) != RCC_LSE_OFF)
    {
      /* Get Start Tick*/
      tickstart = GetProcessTickMs();

      /* Wait till LSE is ready */
      while(__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == 0U)
      {
        if((GetProcessTickMs() - tickstart ) > RCC_LSE_TIMEOUT_VALUE)
        {
          return HAL_TIMEOUT;
        }
      }
    }
    else
    {
      /* Get Start Tick*/
      tickstart = GetProcessTickMs();

      /* Wait till LSE is ready */
      while(__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) != 0U)
      {
        if((GetProcessTickMs() - tickstart ) > RCC_LSE_TIMEOUT_VALUE)
        {
          return HAL_TIMEOUT;
        }
      }
    }
  }
  /*-------------------------------- PLL Configuration -----------------------*/
  /* Check the parameters */
  assert_param(IS_RCC_PLL(RCC_OscInitStruct->PLL.PLLState));
  if ((RCC_OscInitStruct->PLL.PLLState) != RCC_PLL_NONE)
  {
    /* Check if the PLL is used as system clock or not */
    if(__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_PLL1)
    {
      if((RCC_OscInitStruct->PLL.PLLState) == RCC_PLL_ON)
      {
        /* Check the parameters */
        assert_param(IS_RCC_PLLSOURCE(RCC_OscInitStruct->PLL.PLLSource));
        assert_param(IS_RCC_PLLM_VALUE(RCC_OscInitStruct->PLL.PLLM));
        assert_param(IS_RCC_PLLN_VALUE(RCC_OscInitStruct->PLL.PLLN));
        assert_param(IS_RCC_PLLP_VALUE(RCC_OscInitStruct->PLL.PLLP));
        assert_param(IS_RCC_PLLQ_VALUE(RCC_OscInitStruct->PLL.PLLQ));
        assert_param(IS_RCC_PLLR_VALUE(RCC_OscInitStruct->PLL.PLLR));
        assert_param(IS_RCC_PLLFRACN_VALUE(RCC_OscInitStruct->PLL.PLLFRACN));

        /* Disable the main PLL. */
        __HAL_RCC_PLL_DISABLE();

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till PLL is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) != 0U)
        {
          if((GetProcessTickMs() - tickstart ) > PLL_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }

        /* Configure the main PLL clock source, multiplication and division factors. */
        __HAL_RCC_PLL_CONFIG(RCC_OscInitStruct->PLL.PLLSource,
                             RCC_OscInitStruct->PLL.PLLM,
                             RCC_OscInitStruct->PLL.PLLN,
                             RCC_OscInitStruct->PLL.PLLP,
                             RCC_OscInitStruct->PLL.PLLQ,
                             RCC_OscInitStruct->PLL.PLLR);

         /* Disable PLLFRACN . */
         __HAL_RCC_PLLFRACN_DISABLE();

         /* Configure PLL  PLL1FRACN */
         __HAL_RCC_PLLFRACN_CONFIG(RCC_OscInitStruct->PLL.PLLFRACN);

        /* Select PLL1 input reference frequency range: VCI */
        __HAL_RCC_PLL_VCIRANGE(RCC_OscInitStruct->PLL.PLLRGE) ;

        /* Select PLL1 output frequency range : VCO */
        __HAL_RCC_PLL_VCORANGE(RCC_OscInitStruct->PLL.PLLVCOSEL) ;

        /* Enable PLL System Clock output. */
         __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVP);

        /* Enable PLL1Q Clock output. */
         __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

        /* Enable PLL1R  Clock output. */
         __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVR);

        /* Enable PLL1FRACN . */
         __HAL_RCC_PLLFRACN_ENABLE();

        /* Enable the main PLL. */
        __HAL_RCC_PLL_ENABLE();

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till PLL is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == 0U)
        {
          if((GetProcessTickMs() - tickstart ) > PLL_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }
      }
      else
      {
        /* Disable the main PLL. */
        __HAL_RCC_PLL_DISABLE();

        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till PLL is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) != 0U)
        {
          if((GetProcessTickMs() - tickstart ) > PLL_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }
      }
    }
    else
    {
      return HAL_ERROR;
    }
  }
  return HAL_OK;
}

HAL_StatusTypeDef ClockConfig(RCC_ClkInitTypeDef  *RCC_ClkInitStruct, uint32_t FLatency)
{
  HAL_StatusTypeDef halstatus;
  uint32_t tickstart;

   /* Check Null pointer */
  if(RCC_ClkInitStruct == NULL)
  {
    return HAL_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_RCC_CLOCKTYPE(RCC_ClkInitStruct->ClockType));
  assert_param(IS_FLASH_LATENCY(FLatency));

  /* To correctly read data from FLASH memory, the number of wait states (LATENCY)
    must be correctly programmed according to the frequency of the CPU clock
    (HCLK) and the supply voltage of the device. */

  /* Increasing the CPU frequency */
  if(FLatency > __HAL_FLASH_GET_LATENCY())
  {
    /* Program the new number of wait states to the LATENCY bits in the FLASH_ACR register */
    __HAL_FLASH_SET_LATENCY(FLatency);

    /* Check that the new number of wait states is taken into account to access the Flash
    memory by reading the FLASH_ACR register */
    if(__HAL_FLASH_GET_LATENCY() != FLatency)
    {
      return HAL_ERROR;
    }

  }

  /* Increasing the BUS frequency divider */
  /*-------------------------- D1PCLK1 Configuration ---------------------------*/
  if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_D1PCLK1) == RCC_CLOCKTYPE_D1PCLK1)
  {
    if((RCC_ClkInitStruct->APB3CLKDivider) > (RCC->D1CFGR & RCC_D1CFGR_D1PPRE))
    {
      assert_param(IS_RCC_D1PCLK1(RCC_ClkInitStruct->APB3CLKDivider));
      MODIFY_REG(RCC->D1CFGR, RCC_D1CFGR_D1PPRE, RCC_ClkInitStruct->APB3CLKDivider);
    }
  }

  /*-------------------------- PCLK1 Configuration ---------------------------*/
  if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK1) == RCC_CLOCKTYPE_PCLK1)
  {
    if((RCC_ClkInitStruct->APB1CLKDivider) > (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1))
    {
      assert_param(IS_RCC_PCLK1(RCC_ClkInitStruct->APB1CLKDivider));
      MODIFY_REG(RCC->D2CFGR, RCC_D2CFGR_D2PPRE1, (RCC_ClkInitStruct->APB1CLKDivider));
    }
  }

  /*-------------------------- PCLK2 Configuration ---------------------------*/
  if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK2) == RCC_CLOCKTYPE_PCLK2)
  {
    if((RCC_ClkInitStruct->APB2CLKDivider) > (RCC->D2CFGR & RCC_D2CFGR_D2PPRE2))
    {
      assert_param(IS_RCC_PCLK2(RCC_ClkInitStruct->APB2CLKDivider));
      MODIFY_REG(RCC->D2CFGR, RCC_D2CFGR_D2PPRE2, (RCC_ClkInitStruct->APB2CLKDivider));
    }
  }

  /*-------------------------- D3PCLK1 Configuration ---------------------------*/
  if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_D3PCLK1) == RCC_CLOCKTYPE_D3PCLK1)
  {
    if((RCC_ClkInitStruct->APB4CLKDivider) > (RCC->D3CFGR & RCC_D3CFGR_D3PPRE))
    {
      assert_param(IS_RCC_D3PCLK1(RCC_ClkInitStruct->APB4CLKDivider));
      MODIFY_REG(RCC->D3CFGR, RCC_D3CFGR_D3PPRE, (RCC_ClkInitStruct->APB4CLKDivider) );
    }
  }

   /*-------------------------- HCLK Configuration --------------------------*/
  if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_HCLK) == RCC_CLOCKTYPE_HCLK)
  {
    if((RCC_ClkInitStruct->AHBCLKDivider) > (RCC->D1CFGR & RCC_D1CFGR_HPRE))
    {
      /* Set the new HCLK clock divider */
      assert_param(IS_RCC_HCLK(RCC_ClkInitStruct->AHBCLKDivider));
      MODIFY_REG(RCC->D1CFGR, RCC_D1CFGR_HPRE, RCC_ClkInitStruct->AHBCLKDivider);
    }
  }

    /*------------------------- SYSCLK Configuration -------------------------*/
    if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_SYSCLK) == RCC_CLOCKTYPE_SYSCLK)
    {
      assert_param(IS_RCC_SYSCLK(RCC_ClkInitStruct->SYSCLKDivider));
      assert_param(IS_RCC_SYSCLKSOURCE(RCC_ClkInitStruct->SYSCLKSource));
      MODIFY_REG(RCC->D1CFGR, RCC_D1CFGR_D1CPRE, RCC_ClkInitStruct->SYSCLKDivider);
      /* HSE is selected as System Clock Source */
      if(RCC_ClkInitStruct->SYSCLKSource == RCC_SYSCLKSOURCE_HSE)
      {
        /* Check the HSE ready flag */
        if(__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == 0U)
        {
          return HAL_ERROR;
        }
      }
      /* PLL is selected as System Clock Source */
      else if(RCC_ClkInitStruct->SYSCLKSource == RCC_SYSCLKSOURCE_PLLCLK)
      {
        /* Check the PLL ready flag */
        if(__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == 0U)
        {
          return HAL_ERROR;
        }
      }
      /* CSI is selected as System Clock Source */
      else if(RCC_ClkInitStruct->SYSCLKSource == RCC_SYSCLKSOURCE_CSI)
      {
        /* Check the PLL ready flag */
        if(__HAL_RCC_GET_FLAG(RCC_FLAG_CSIRDY) == 0U)
        {
          return HAL_ERROR;
        }
      }
      /* HSI is selected as System Clock Source */
      else
      {
        /* Check the HSI ready flag */
        if(__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == 0U)
        {
          return HAL_ERROR;
        }
      }
      MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_ClkInitStruct->SYSCLKSource);

      /* Get Start Tick*/
      tickstart = GetProcessTickMs();

        while (__HAL_RCC_GET_SYSCLK_SOURCE() !=  (RCC_ClkInitStruct->SYSCLKSource << RCC_CFGR_SWS_Pos))
        {
          if((GetProcessTickMs() - tickstart ) > CLOCKSWITCH_TIMEOUT_VALUE)
          {
            return HAL_TIMEOUT;
          }
        }

    }

    /* Decreasing the BUS frequency divider */
   /*-------------------------- HCLK Configuration --------------------------*/
  if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_HCLK) == RCC_CLOCKTYPE_HCLK)
  {
    if((RCC_ClkInitStruct->AHBCLKDivider) < (RCC->D1CFGR & RCC_D1CFGR_HPRE))
    {
      /* Set the new HCLK clock divider */
      assert_param(IS_RCC_HCLK(RCC_ClkInitStruct->AHBCLKDivider));
      MODIFY_REG(RCC->D1CFGR, RCC_D1CFGR_HPRE, RCC_ClkInitStruct->AHBCLKDivider);
    }
  }

  /* Decreasing the number of wait states because of lower CPU frequency */
  if(FLatency < __HAL_FLASH_GET_LATENCY())
  {
    /* Program the new number of wait states to the LATENCY bits in the FLASH_ACR register */
    __HAL_FLASH_SET_LATENCY(FLatency);

    /* Check that the new number of wait states is taken into account to access the Flash
    memory by reading the FLASH_ACR register */
    if(__HAL_FLASH_GET_LATENCY() != FLatency)
    {
      return HAL_ERROR;
    }
 }
 /*-------------------------- D1PCLK1 Configuration ---------------------------*/
 if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_D1PCLK1) == RCC_CLOCKTYPE_D1PCLK1)
 {
   if((RCC_ClkInitStruct->APB3CLKDivider) < (RCC->D1CFGR & RCC_D1CFGR_D1PPRE))
   {
     assert_param(IS_RCC_D1PCLK1(RCC_ClkInitStruct->APB3CLKDivider));
     MODIFY_REG(RCC->D1CFGR, RCC_D1CFGR_D1PPRE, RCC_ClkInitStruct->APB3CLKDivider);
   }
 }

  /*-------------------------- PCLK1 Configuration ---------------------------*/
 if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK1) == RCC_CLOCKTYPE_PCLK1)
 {
   if((RCC_ClkInitStruct->APB1CLKDivider) < (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1))
   {
     assert_param(IS_RCC_PCLK1(RCC_ClkInitStruct->APB1CLKDivider));
     MODIFY_REG(RCC->D2CFGR, RCC_D2CFGR_D2PPRE1, (RCC_ClkInitStruct->APB1CLKDivider));
   }
 }

  /*-------------------------- PCLK2 Configuration ---------------------------*/
 if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK2) == RCC_CLOCKTYPE_PCLK2)
 {
   if((RCC_ClkInitStruct->APB2CLKDivider) < (RCC->D2CFGR & RCC_D2CFGR_D2PPRE2))
   {
     assert_param(IS_RCC_PCLK2(RCC_ClkInitStruct->APB2CLKDivider));
     MODIFY_REG(RCC->D2CFGR, RCC_D2CFGR_D2PPRE2, (RCC_ClkInitStruct->APB2CLKDivider));
   }
 }

  /*-------------------------- D3PCLK1 Configuration ---------------------------*/
 if(((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_D3PCLK1) == RCC_CLOCKTYPE_D3PCLK1)
 {
   if((RCC_ClkInitStruct->APB4CLKDivider) < (RCC->D3CFGR & RCC_D3CFGR_D3PPRE))
   {
     assert_param(IS_RCC_D3PCLK1(RCC_ClkInitStruct->APB4CLKDivider));
     MODIFY_REG(RCC->D3CFGR, RCC_D3CFGR_D3PPRE, (RCC_ClkInitStruct->APB4CLKDivider) );
   }
 }

  /* Update the SystemCoreClock global variable */
  SystemCoreClock = HAL_RCC_GetSysClockFreq() >> ((D1CorePrescTable[(RCC->D1CFGR & RCC_D1CFGR_D1CPRE)>> RCC_D1CFGR_D1CPRE_Pos]) & 0x1FU);

  /* Configure the source of time base considering new system clocks settings*/
  // halstatus = HAL_InitTick (TICK_INT_PRIORITY);
  halstatus = HAL_OK;

  return halstatus;
}

HAL_StatusTypeDef PeriphCLKConfig(RCC_PeriphCLKInitTypeDef  *PeriphClkInit)
{
  uint32_t tmpreg;
  uint32_t tickstart;
  HAL_StatusTypeDef ret = HAL_OK;      /* Intermediate status */
  HAL_StatusTypeDef status = HAL_OK;   /* Final status */

  /*---------------------------- SPDIFRX configuration -------------------------------*/

  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SPDIFRX) == RCC_PERIPHCLK_SPDIFRX)
  {

    switch(PeriphClkInit->SpdifrxClockSelection)
    {
    case RCC_SPDIFRXCLKSOURCE_PLL:      /* PLL is used as clock source for SPDIFRX*/
      /* Enable SAI Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SPDIFRXCLKSOURCE_PLL2: /* PLL2 is used as clock source for SPDIFRX*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_R_UPDATE);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SPDIFRXCLKSOURCE_PLL3:  /* PLL3 is used as clock source for SPDIFRX*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SPDIFRXCLKSOURCE_HSI:
      /* Internal OSC clock is used as source of SPDIFRX clock*/
      /* SPDIFRX clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SPDIFRX clock*/
      __HAL_RCC_SPDIFRX_CONFIG(PeriphClkInit->SpdifrxClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- SAI1 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SAI1) == RCC_PERIPHCLK_SAI1)
  {
    switch(PeriphClkInit->Sai1ClockSelection)
    {
    case RCC_SAI1CLKSOURCE_PLL:      /* PLL is used as clock source for SAI1*/
      /* Enable SAI Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI1CLKSOURCE_PLL2: /* PLL2 is used as clock source for SAI1*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI1CLKSOURCE_PLL3:  /* PLL3 is used as clock source for SAI1*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_P_UPDATE);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI1CLKSOURCE_PIN:
      /* External clock is used as source of SAI1 clock*/
      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI1CLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of SAI1 clock */
      /* SAI1 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SAI1 clock*/
      __HAL_RCC_SAI1_CONFIG(PeriphClkInit->Sai1ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- SAI2/3 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SAI23) == RCC_PERIPHCLK_SAI23)
  {
    switch(PeriphClkInit->Sai23ClockSelection)
    {
    case RCC_SAI23CLKSOURCE_PLL:      /* PLL is used as clock source for SAI2/3 */
      /* Enable SAI Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* SAI2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI23CLKSOURCE_PLL2: /* PLL2 is used as clock source for SAI2/3 */

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* SAI2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI23CLKSOURCE_PLL3:  /* PLL3 is used as clock source for SAI2/3 */
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_P_UPDATE);

      /* SAI2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI23CLKSOURCE_PIN:
      /* External clock is used as source of SAI2/3 clock*/
      /* SAI2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI23CLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of SAI2/3 clock */
      /* SAI2/3 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SAI2/3 clock*/
      __HAL_RCC_SAI23_CONFIG(PeriphClkInit->Sai23ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- SAI4A configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SAI4A) == RCC_PERIPHCLK_SAI4A)
  {
    switch(PeriphClkInit->Sai4AClockSelection)
    {
    case RCC_SAI4ACLKSOURCE_PLL:      /* PLL is used as clock source for SAI2*/
      /* Enable SAI Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4ACLKSOURCE_PLL2: /* PLL2 is used as clock source for SAI2*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* SAI2 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4ACLKSOURCE_PLL3:  /* PLL3 is used as clock source for SAI2*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_P_UPDATE);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4ACLKSOURCE_PIN:
      /* External clock is used as source of SAI2 clock*/
      /* SAI2 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4ACLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of SAI2 clock */
      /* SAI1 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SAI2 clock*/
      __HAL_RCC_SAI4A_CONFIG(PeriphClkInit->Sai4AClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }
  /*---------------------------- SAI4B configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SAI4B) == RCC_PERIPHCLK_SAI4B)
  {
    switch(PeriphClkInit->Sai4BClockSelection)
    {
    case RCC_SAI4BCLKSOURCE_PLL:      /* PLL is used as clock source for SAI2*/
      /* Enable SAI Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4BCLKSOURCE_PLL2: /* PLL2 is used as clock source for SAI2*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* SAI2 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4BCLKSOURCE_PLL3:  /* PLL3 is used as clock source for SAI2*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3), DIVIDER_P_UPDATE);

      /* SAI1 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4BCLKSOURCE_PIN:
      /* External clock is used as source of SAI2 clock*/
      /* SAI2 clock source configuration done later after clock selection check */
      break;

    case RCC_SAI4BCLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of SAI2 clock */
      /* SAI1 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SAI2 clock*/
      __HAL_RCC_SAI4B_CONFIG(PeriphClkInit->Sai4BClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }
  /*---------------------------- QSPI configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_QSPI) == RCC_PERIPHCLK_QSPI)
  {
    switch(PeriphClkInit->QspiClockSelection)
    {
    case RCC_QSPICLKSOURCE_PLL:      /* PLL is used as clock source for QSPI*/
      /* Enable QSPI Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* QSPI clock source configuration done later after clock selection check */
      break;

    case RCC_QSPICLKSOURCE_PLL2: /* PLL2 is used as clock source for QSPI*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_R_UPDATE);

      /* QSPI clock source configuration done later after clock selection check */
      break;


    case RCC_QSPICLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of QSPI clock */
      /* QSPI clock source configuration done later after clock selection check */
      break;

    case RCC_QSPICLKSOURCE_D1HCLK:
      /* Domain1 HCLK  clock selected as QSPI kernel peripheral clock */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of QSPI clock*/
      __HAL_RCC_QSPI_CONFIG(PeriphClkInit->QspiClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- SPI1/2/3 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SPI123) == RCC_PERIPHCLK_SPI123)
  {
    switch(PeriphClkInit->Spi123ClockSelection)
    {
    case RCC_SPI123CLKSOURCE_PLL:      /* PLL is used as clock source for SPI1/2/3 */
      /* Enable SPI Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* SPI1/2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI123CLKSOURCE_PLL2: /* PLL2 is used as clock source for SPI1/2/3 */
      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* SPI1/2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI123CLKSOURCE_PLL3:  /* PLL3 is used as clock source for SPI1/2/3 */
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_P_UPDATE);

      /* SPI1/2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI123CLKSOURCE_PIN:
      /* External clock is used as source of SPI1/2/3 clock*/
      /* SPI1/2/3 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI123CLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of SPI1/2/3 clock */
      /* SPI1/2/3 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SPI1/2/3 clock*/
      __HAL_RCC_SPI123_CONFIG(PeriphClkInit->Spi123ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- SPI4/5 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SPI45) == RCC_PERIPHCLK_SPI45)
  {
    switch(PeriphClkInit->Spi45ClockSelection)
    {
    case RCC_SPI45CLKSOURCE_D2PCLK1:      /* D2PCLK1 as clock source for SPI4/5 */
      /* SPI4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI45CLKSOURCE_PLL2: /* PLL2 is used as clock source for SPI4/5 */

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_Q_UPDATE);

      /* SPI4/5 clock source configuration done later after clock selection check */
      break;
    case RCC_SPI45CLKSOURCE_PLL3:  /* PLL3 is used as clock source for SPI4/5 */
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_Q_UPDATE);
      /* SPI4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI45CLKSOURCE_HSI:
      /* HSI oscillator clock is used as source of SPI4/5 clock*/
      /* SPI4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI45CLKSOURCE_CSI:
      /*  CSI oscillator clock is used as source of SPI4/5 clock */
      /* SPI4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI45CLKSOURCE_HSE:
      /* HSE,  oscillator is used as source of SPI4/5 clock */
      /* SPI4/5 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SPI4/5 clock*/
      __HAL_RCC_SPI45_CONFIG(PeriphClkInit->Spi45ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- SPI6 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SPI6) == RCC_PERIPHCLK_SPI6)
  {
    switch(PeriphClkInit->Spi6ClockSelection)
    {
    case RCC_SPI6CLKSOURCE_D3PCLK1:      /* D3PCLK1 as clock source for SPI6*/
      /* SPI6 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI6CLKSOURCE_PLL2: /* PLL2 is used as clock source for SPI6*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_Q_UPDATE);

      /* SPI6 clock source configuration done later after clock selection check */
      break;
    case RCC_SPI6CLKSOURCE_PLL3:  /* PLL3 is used as clock source for SPI6*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_Q_UPDATE);
      /* SPI6 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI6CLKSOURCE_HSI:
      /* HSI oscillator clock is used as source of SPI6 clock*/
      /* SPI6 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI6CLKSOURCE_CSI:
      /*  CSI oscillator clock is used as source of SPI6 clock */
      /* SPI6 clock source configuration done later after clock selection check */
      break;

    case RCC_SPI6CLKSOURCE_HSE:
      /* HSE,  oscillator is used as source of SPI6 clock */
      /* SPI6 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SPI6 clock*/
      __HAL_RCC_SPI6_CONFIG(PeriphClkInit->Spi6ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

#if defined(DSI)
  /*---------------------------- DSI configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_DSI) == RCC_PERIPHCLK_DSI)
  {
    switch(PeriphClkInit->DsiClockSelection)
    {

    case RCC_DSICLKSOURCE_PLL2: /* PLL2 is used as clock source for DSI*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_Q_UPDATE);

      /* DSI clock source configuration done later after clock selection check */
      break;

    case RCC_DSICLKSOURCE_PHY:
      /* PHY is used as clock source for DSI*/
      /* DSI clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of DSI clock*/
      __HAL_RCC_DSI_CONFIG(PeriphClkInit->DsiClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }
#endif /*DSI*/

#if defined(FDCAN1) || defined(FDCAN2)
  /*---------------------------- FDCAN configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_FDCAN) == RCC_PERIPHCLK_FDCAN)
  {
    switch(PeriphClkInit->FdcanClockSelection)
    {
    case RCC_FDCANCLKSOURCE_PLL:      /* PLL is used as clock source for FDCAN*/
      /* Enable FDCAN Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* FDCAN clock source configuration done later after clock selection check */
      break;

    case RCC_FDCANCLKSOURCE_PLL2: /* PLL2 is used as clock source for FDCAN*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_Q_UPDATE);

      /* FDCAN clock source configuration done later after clock selection check */
      break;

    case RCC_FDCANCLKSOURCE_HSE:
      /* HSE is used as clock source for FDCAN*/
      /* FDCAN clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of FDCAN clock*/
      __HAL_RCC_FDCAN_CONFIG(PeriphClkInit->FdcanClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

#endif /*FDCAN1 || FDCAN2*/
  /*---------------------------- FMC configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_FMC) == RCC_PERIPHCLK_FMC)
  {
    switch(PeriphClkInit->FmcClockSelection)
    {
    case RCC_FMCCLKSOURCE_PLL:      /* PLL is used as clock source for FMC*/
      /* Enable FMC Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* FMC clock source configuration done later after clock selection check */
      break;

    case RCC_FMCCLKSOURCE_PLL2: /* PLL2 is used as clock source for FMC*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_R_UPDATE);

      /* FMC clock source configuration done later after clock selection check */
      break;


    case RCC_FMCCLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of FMC clock */
      /* FMC clock source configuration done later after clock selection check */
      break;

    case RCC_FMCCLKSOURCE_D1HCLK:
      /* Domain1 HCLK  clock selected as QSPI kernel peripheral clock */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of FMC clock*/
      __HAL_RCC_FMC_CONFIG(PeriphClkInit->FmcClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- RTC configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_RTC) == RCC_PERIPHCLK_RTC)
  {
    /* check for RTC Parameters used to output RTCCLK */
    assert_param(IS_RCC_RTCCLKSOURCE(PeriphClkInit->RTCClockSelection));

    /* Enable write access to Backup domain */
    SET_BIT(PWR->CR1, PWR_CR1_DBP);

    /* Wait for Backup domain Write protection disable */
    tickstart = GetProcessTickMs();

    while((PWR->CR1 & PWR_CR1_DBP) == 0U)
    {
      if((GetProcessTickMs() - tickstart) > RCC_DBP_TIMEOUT_VALUE)
      {
        ret = HAL_TIMEOUT;
        break;
      }
    }

    if(ret == HAL_OK)
    {
      /* Reset the Backup domain only if the RTC Clock source selection is modified */
      if((RCC->BDCR & RCC_BDCR_RTCSEL) != (PeriphClkInit->RTCClockSelection & RCC_BDCR_RTCSEL))
      {
        /* Store the content of BDCR register before the reset of Backup Domain */
        tmpreg = (RCC->BDCR & ~(RCC_BDCR_RTCSEL));
        /* RTC Clock selection can be changed only if the Backup Domain is reset */
        __HAL_RCC_BACKUPRESET_FORCE();
        __HAL_RCC_BACKUPRESET_RELEASE();
        /* Restore the Content of BDCR register */
        RCC->BDCR = tmpreg;
      }

      /* If LSE is selected as RTC clock source, wait for LSE reactivation */
      if(PeriphClkInit->RTCClockSelection == RCC_RTCCLKSOURCE_LSE)
      {
        /* Get Start Tick*/
        tickstart = GetProcessTickMs();

        /* Wait till LSE is ready */
        while(__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == 0U)
        {
          if((GetProcessTickMs() - tickstart) > RCC_LSE_TIMEOUT_VALUE)
          {
            ret = HAL_TIMEOUT;
            break;
          }
        }
      }

      if(ret == HAL_OK)
      {
        __HAL_RCC_RTC_CONFIG(PeriphClkInit->RTCClockSelection);
      }
      else
      {
        /* set overall return value */
        status = ret;
      }
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }


  /*-------------------------- USART1/6 configuration --------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_USART16) == RCC_PERIPHCLK_USART16)
  {
    switch(PeriphClkInit->Usart16ClockSelection)
    {
    case RCC_USART16CLKSOURCE_D2PCLK2: /* D2PCLK2 as clock source for USART1/6 */
      /* USART1/6 clock source configuration done later after clock selection check */
      break;

    case RCC_USART16CLKSOURCE_PLL2: /* PLL2 is used as clock source for USART1/6 */
      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_Q_UPDATE);
      /* USART1/6 clock source configuration done later after clock selection check */
      break;

    case RCC_USART16CLKSOURCE_PLL3: /* PLL3 is used as clock source for USART1/6 */
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_Q_UPDATE);
      /* USART1/6 clock source configuration done later after clock selection check */
      break;

    case RCC_USART16CLKSOURCE_HSI:
      /* HSI oscillator clock is used as source of USART1/6 clock */
      /* USART1/6 clock source configuration done later after clock selection check */
      break;

    case RCC_USART16CLKSOURCE_CSI:
      /* CSI oscillator clock is used as source of USART1/6 clock */
      /* USART1/6 clock source configuration done later after clock selection check */
      break;

    case RCC_USART16CLKSOURCE_LSE:
      /* LSE,  oscillator is used as source of USART1/6 clock */
      /* USART1/6 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of USART1/6 clock */
      __HAL_RCC_USART16_CONFIG(PeriphClkInit->Usart16ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*-------------------------- USART2/3/4/5/7/8 Configuration --------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_USART234578) == RCC_PERIPHCLK_USART234578)
  {
    switch(PeriphClkInit->Usart234578ClockSelection)
    {
    case RCC_USART234578CLKSOURCE_D2PCLK1: /* D2PCLK1 as clock source for USART2/3/4/5/7/8 */
      /* USART2/3/4/5/7/8 clock source configuration done later after clock selection check */
      break;

    case RCC_USART234578CLKSOURCE_PLL2: /* PLL2 is used as clock source for USART2/3/4/5/7/8 */
      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_Q_UPDATE);
      /* USART2/3/4/5/7/8 clock source configuration done later after clock selection check */
      break;

    case RCC_USART234578CLKSOURCE_PLL3: /* PLL3 is used as clock source for USART2/3/4/5/7/8 */
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_Q_UPDATE);
      /* USART2/3/4/5/7/8 clock source configuration done later after clock selection check */
      break;

    case RCC_USART234578CLKSOURCE_HSI:
      /* HSI oscillator clock is used as source of USART2/3/4/5/7/8 clock */
      /* USART2/3/4/5/7/8 clock source configuration done later after clock selection check */
      break;

    case RCC_USART234578CLKSOURCE_CSI:
      /* CSI oscillator clock is used as source of USART2/3/4/5/7/8 clock */
      /* USART2/3/4/5/7/8 clock source configuration done later after clock selection check */
      break;

    case RCC_USART234578CLKSOURCE_LSE:
      /* LSE,  oscillator is used as source of USART2/3/4/5/7/8 clock */
      /* USART2/3/4/5/7/8 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of USART2/3/4/5/7/8 clock */
      __HAL_RCC_USART234578_CONFIG(PeriphClkInit->Usart234578ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*-------------------------- LPUART1 Configuration -------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_LPUART1) == RCC_PERIPHCLK_LPUART1)
  {
    switch(PeriphClkInit->Lpuart1ClockSelection)
    {
    case RCC_LPUART1CLKSOURCE_D3PCLK1: /* D3PCLK1 as clock source for LPUART1 */
      /* LPUART1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPUART1CLKSOURCE_PLL2: /* PLL2 is used as clock source for LPUART1 */
      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_Q_UPDATE);
      /* LPUART1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPUART1CLKSOURCE_PLL3: /* PLL3 is used as clock source for LPUART1 */
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_Q_UPDATE);
      /* LPUART1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPUART1CLKSOURCE_HSI:
      /* HSI oscillator clock is used as source of LPUART1 clock */
      /* LPUART1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPUART1CLKSOURCE_CSI:
      /* CSI oscillator clock is used as source of LPUART1 clock */
      /* LPUART1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPUART1CLKSOURCE_LSE:
      /* LSE,  oscillator is used as source of LPUART1 clock */
      /* LPUART1 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of LPUART1 clock */
      __HAL_RCC_LPUART1_CONFIG(PeriphClkInit->Lpuart1ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- LPTIM1 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_LPTIM1) == RCC_PERIPHCLK_LPTIM1)
  {
    switch(PeriphClkInit->Lptim1ClockSelection)
    {
    case RCC_LPTIM1CLKSOURCE_D2PCLK1:      /* D2PCLK1 as clock source for LPTIM1*/
      /* LPTIM1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM1CLKSOURCE_PLL2: /* PLL2 is used as clock source for LPTIM1*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* LPTIM1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM1CLKSOURCE_PLL3:  /* PLL3 is used as clock source for LPTIM1*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE);

      /* LPTIM1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM1CLKSOURCE_LSE:
      /* External low speed OSC clock is used as source of LPTIM1 clock*/
      /* LPTIM1 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM1CLKSOURCE_LSI:
      /* Internal  low speed OSC clock is used  as source of LPTIM1 clock*/
      /* LPTIM1 clock source configuration done later after clock selection check */
      break;
    case RCC_LPTIM1CLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of LPTIM1 clock */
      /* LPTIM1 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of LPTIM1 clock*/
      __HAL_RCC_LPTIM1_CONFIG(PeriphClkInit->Lptim1ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- LPTIM2 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_LPTIM2) == RCC_PERIPHCLK_LPTIM2)
  {
    switch(PeriphClkInit->Lptim2ClockSelection)
    {
    case RCC_LPTIM2CLKSOURCE_D3PCLK1:      /* D3PCLK1 as clock source for LPTIM2*/
      /* LPTIM2 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM2CLKSOURCE_PLL2: /* PLL2 is used as clock source for LPTIM2*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* LPTIM2 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM2CLKSOURCE_PLL3:  /* PLL3 is used as clock source for LPTIM2*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE);

      /* LPTIM2 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM2CLKSOURCE_LSE:
      /* External low speed OSC clock is used as source of LPTIM2 clock*/
      /* LPTIM2 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM2CLKSOURCE_LSI:
      /* Internal  low speed OSC clock is used  as source of LPTIM2 clock*/
      /* LPTIM2 clock source configuration done later after clock selection check */
      break;
    case RCC_LPTIM2CLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of LPTIM2 clock */
      /* LPTIM2 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of LPTIM2 clock*/
      __HAL_RCC_LPTIM2_CONFIG(PeriphClkInit->Lptim2ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*---------------------------- LPTIM345 configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_LPTIM345) == RCC_PERIPHCLK_LPTIM345)
  {
    switch(PeriphClkInit->Lptim345ClockSelection)
    {

    case RCC_LPTIM345CLKSOURCE_D3PCLK1:      /* D3PCLK1 as clock source for LPTIM3/4/5 */
      /* LPTIM3/4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM345CLKSOURCE_PLL2: /* PLL2 is used as clock source for LPTIM3/4/5 */
      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* LPTIM3/4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM345CLKSOURCE_PLL3:  /* PLL3 is used as clock source for LPTIM3/4/5 */
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE);

      /* LPTIM3/4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM345CLKSOURCE_LSE:
      /* External low speed OSC clock is used as source of LPTIM3/4/5 clock */
      /* LPTIM3/4/5 clock source configuration done later after clock selection check */
      break;

    case RCC_LPTIM345CLKSOURCE_LSI:
      /* Internal  low speed OSC clock is used  as source of LPTIM3/4/5 clock */
      /* LPTIM3/4/5 clock source configuration done later after clock selection check */
      break;
    case RCC_LPTIM345CLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of LPTIM3/4/5 clock */
      /* LPTIM3/4/5 clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of LPTIM3/4/5 clock */
      __HAL_RCC_LPTIM345_CONFIG(PeriphClkInit->Lptim345ClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*------------------------------ I2C1/2/3 Configuration ------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_I2C123) == RCC_PERIPHCLK_I2C123)
  {
    /* Check the parameters */
    assert_param(IS_RCC_I2C123CLKSOURCE(PeriphClkInit->I2c123ClockSelection));

    if ((PeriphClkInit->I2c123ClockSelection )== RCC_I2C123CLKSOURCE_PLL3 )
    {
        if(PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE)!= HAL_OK)
        {
          status = HAL_ERROR;
        }
    }

    else
    {
      __HAL_RCC_I2C123_CONFIG(PeriphClkInit->I2c123ClockSelection);
    }

  }

  /*------------------------------ I2C4 Configuration ------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_I2C4) == RCC_PERIPHCLK_I2C4)
  {
    /* Check the parameters */
    assert_param(IS_RCC_I2C4CLKSOURCE(PeriphClkInit->I2c4ClockSelection));

    if ((PeriphClkInit->I2c4ClockSelection) == RCC_I2C4CLKSOURCE_PLL3 )
    {
      if(PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE)!= HAL_OK)
      {
        status = HAL_ERROR;
      }
    }

    else
    {
      __HAL_RCC_I2C4_CONFIG(PeriphClkInit->I2c4ClockSelection);
    }
  }

  /*---------------------------- ADC configuration -------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_ADC) == RCC_PERIPHCLK_ADC)
  {
    switch(PeriphClkInit->AdcClockSelection)
    {

    case RCC_ADCCLKSOURCE_PLL2: /* PLL2 is used as clock source for ADC*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_P_UPDATE);

      /* ADC clock source configuration done later after clock selection check */
      break;

    case RCC_ADCCLKSOURCE_PLL3:  /* PLL3 is used as clock source for ADC*/
      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE);

      /* ADC clock source configuration done later after clock selection check */
      break;

    case RCC_ADCCLKSOURCE_CLKP:
      /* HSI, HSE, or CSI oscillator is used as source of ADC clock */
      /* ADC clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of ADC clock*/
      __HAL_RCC_ADC_CONFIG(PeriphClkInit->AdcClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

  /*------------------------------ USB Configuration -------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_USB) == RCC_PERIPHCLK_USB)
  {

    switch(PeriphClkInit->UsbClockSelection)
    {
    case RCC_USBCLKSOURCE_PLL:      /* PLL is used as clock source for USB*/
      /* Enable USB Clock output generated form System USB . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* USB clock source configuration done later after clock selection check */
      break;

    case RCC_USBCLKSOURCE_PLL3: /* PLL3 is used as clock source for USB*/

      ret = PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_Q_UPDATE);

      /* USB clock source configuration done later after clock selection check */
      break;

    case RCC_USBCLKSOURCE_HSI48:
      /* HSI48 oscillator is used as source of USB clock */
      /* USB clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of USB clock*/
      __HAL_RCC_USB_CONFIG(PeriphClkInit->UsbClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }

  }

  /*------------------------------------- SDMMC Configuration ------------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SDMMC) == RCC_PERIPHCLK_SDMMC)
  {
    /* Check the parameters */
    assert_param(IS_RCC_SDMMC(PeriphClkInit->SdmmcClockSelection));

    switch(PeriphClkInit->SdmmcClockSelection)
    {
    case RCC_SDMMCCLKSOURCE_PLL:      /* PLL is used as clock source for SDMMC*/
      /* Enable SDMMC Clock output generated form System PLL . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* SDMMC clock source configuration done later after clock selection check */
      break;

    case RCC_SDMMCCLKSOURCE_PLL2: /* PLL2 is used as clock source for SDMMC*/

      ret = PLL2_Config(&(PeriphClkInit->PLL2),DIVIDER_R_UPDATE);

      /* SDMMC clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of SDMMC clock*/
      __HAL_RCC_SDMMC_CONFIG(PeriphClkInit->SdmmcClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }
  }

#if defined(LTDC)
  /*-------------------------------------- LTDC Configuration -----------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_LTDC) == RCC_PERIPHCLK_LTDC)
  {
    if(PLL3_Config(&(PeriphClkInit->PLL3),DIVIDER_R_UPDATE)!=HAL_OK)
    {
      status=HAL_ERROR;
    }
  }
#endif /* LTDC */

  /*------------------------------ RNG Configuration -------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_RNG) == RCC_PERIPHCLK_RNG)
  {

    switch(PeriphClkInit->RngClockSelection)
    {
    case RCC_RNGCLKSOURCE_PLL:     /* PLL is used as clock source for RNG*/
      /* Enable RNG Clock output generated form System RNG . */
      __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);

      /* RNG clock source configuration done later after clock selection check */
      break;

    case RCC_RNGCLKSOURCE_LSE: /* LSE is used as clock source for RNG*/

      /* RNG clock source configuration done later after clock selection check */
      break;

    case RCC_RNGCLKSOURCE_LSI: /* LSI is used as clock source for RNG*/

      /* RNG clock source configuration done later after clock selection check */
      break;
    case RCC_RNGCLKSOURCE_HSI48:
      /* HSI48 oscillator is used as source of RNG clock */
      /* RNG clock source configuration done later after clock selection check */
      break;

    default:
      ret = HAL_ERROR;
      break;
    }

    if(ret == HAL_OK)
    {
      /* Set the source of RNG clock*/
      __HAL_RCC_RNG_CONFIG(PeriphClkInit->RngClockSelection);
    }
    else
    {
      /* set overall return value */
      status = ret;
    }

  }

  /*------------------------------ SWPMI1 Configuration ------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_SWPMI1) == RCC_PERIPHCLK_SWPMI1)
  {
    /* Check the parameters */
    assert_param(IS_RCC_SWPMI1CLKSOURCE(PeriphClkInit->Swpmi1ClockSelection));

    /* Configure the SWPMI1 interface clock source */
    __HAL_RCC_SWPMI1_CONFIG(PeriphClkInit->Swpmi1ClockSelection);
  }

  /*------------------------------ HRTIM1 clock Configuration ----------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_HRTIM1) == RCC_PERIPHCLK_HRTIM1)
  {
    /* Check the parameters */
    assert_param(IS_RCC_HRTIM1CLKSOURCE(PeriphClkInit->Hrtim1ClockSelection));

    /* Configure the HRTIM1 clock source */
    __HAL_RCC_HRTIM1_CONFIG(PeriphClkInit->Hrtim1ClockSelection);
  }

  /*------------------------------ DFSDM1 Configuration ------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_DFSDM1) == RCC_PERIPHCLK_DFSDM1)
  {
    /* Check the parameters */
    assert_param(IS_RCC_DFSDM1CLKSOURCE(PeriphClkInit->Dfsdm1ClockSelection));

    /* Configure the DFSDM1 interface clock source */
    __HAL_RCC_DFSDM1_CONFIG(PeriphClkInit->Dfsdm1ClockSelection);
  }

  /*------------------------------------ TIM configuration --------------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_TIM) == RCC_PERIPHCLK_TIM)
  {
    /* Check the parameters */
    assert_param(IS_RCC_TIMPRES(PeriphClkInit->TIMPresSelection));

    /* Configure Timer Prescaler */
    __HAL_RCC_TIMCLKPRESCALER(PeriphClkInit->TIMPresSelection);
  }

  /*------------------------------------ CKPER configuration --------------------------------------*/
  if(((PeriphClkInit->PeriphClockSelection) & RCC_PERIPHCLK_CKPER) == RCC_PERIPHCLK_CKPER)
  {
    /* Check the parameters */
    assert_param(IS_RCC_CLKPSOURCE(PeriphClkInit->CkperClockSelection));

    /* Configure the CKPER clock source */
    __HAL_RCC_CLKP_CONFIG(PeriphClkInit->CkperClockSelection);
  }

  if (status == HAL_OK)
  {
    return HAL_OK;
  }
  return HAL_ERROR;
}

HAL_StatusTypeDef PLL2_Config(RCC_PLL2InitTypeDef *pll2, uint32_t Divider)
{

  uint32_t tickstart;
  HAL_StatusTypeDef status = HAL_OK;
  assert_param(IS_RCC_PLL2M_VALUE(pll2->PLL2M));
  assert_param(IS_RCC_PLL2N_VALUE(pll2->PLL2N));
  assert_param(IS_RCC_PLL2P_VALUE(pll2->PLL2P));
  assert_param(IS_RCC_PLL2R_VALUE(pll2->PLL2R));
  assert_param(IS_RCC_PLL2Q_VALUE(pll2->PLL2Q));
  assert_param(IS_RCC_PLL2RGE_VALUE(pll2->PLL2RGE));
  assert_param(IS_RCC_PLL2VCO_VALUE(pll2->PLL2VCOSEL));
  assert_param(IS_RCC_PLLFRACN_VALUE(pll2->PLL2FRACN));

  /* Check that PLL2 OSC clock source is already set */
  if(__HAL_RCC_GET_PLL_OSCSOURCE() == RCC_PLLSOURCE_NONE)
  {
    return HAL_ERROR;
  }


  else
  {
    /* Disable  PLL2. */
    __HAL_RCC_PLL2_DISABLE();

    /* Get Start Tick*/
    tickstart = GetProcessTickMs();

    /* Wait till PLL is disabled */
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) != 0U)
    {
      if( (GetProcessTickMs() - tickstart ) > PLL2_TIMEOUT_VALUE)
      {
        return HAL_TIMEOUT;
      }
    }

    /* Configure PLL2 multiplication and division factors. */
    __HAL_RCC_PLL2_CONFIG(pll2->PLL2M,
                          pll2->PLL2N,
                          pll2->PLL2P,
                          pll2->PLL2Q,
                          pll2->PLL2R);

    /* Select PLL2 input reference frequency range: VCI */
    __HAL_RCC_PLL2_VCIRANGE(pll2->PLL2RGE) ;

    /* Select PLL2 output frequency range : VCO */
    __HAL_RCC_PLL2_VCORANGE(pll2->PLL2VCOSEL) ;

    /* Disable PLL2FRACN . */
    __HAL_RCC_PLL2FRACN_DISABLE();

    /* Configures PLL2 clock Fractional Part Of The Multiplication Factor */
    __HAL_RCC_PLL2FRACN_CONFIG(pll2->PLL2FRACN);

    /* Enable PLL2FRACN . */
    __HAL_RCC_PLL2FRACN_ENABLE();

    /* Enable the PLL2 clock output */
    if(Divider == DIVIDER_P_UPDATE)
    {
      __HAL_RCC_PLL2CLKOUT_ENABLE(RCC_PLL2_DIVP);
    }
    else if(Divider == DIVIDER_Q_UPDATE)
    {
      __HAL_RCC_PLL2CLKOUT_ENABLE(RCC_PLL2_DIVQ);
    }
    else
    {
      __HAL_RCC_PLL2CLKOUT_ENABLE(RCC_PLL2_DIVR);
    }

    /* Enable  PLL2. */
    __HAL_RCC_PLL2_ENABLE();

    /* Get Start Tick*/
    tickstart = GetProcessTickMs();

    /* Wait till PLL2 is ready */
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) == 0U)
    {
      if( (GetProcessTickMs() - tickstart ) > PLL2_TIMEOUT_VALUE)
      {
        return HAL_TIMEOUT;
      }
    }

  }


  return status;
}


/**
  * @brief  Configure the PLL3 VCI,VCO ranges, multiplication and division factors and enable it
  * @param  pll3: Pointer to an RCC_PLL3InitTypeDef structure that
  *         contains the configuration parameters  as well as VCI, VCO clock ranges.
  * @param  Divider  divider parameter to be updated
  * @note   PLL3 is temporary disabled to apply new parameters
  *
  * @retval HAL status
  */
HAL_StatusTypeDef PLL3_Config(RCC_PLL3InitTypeDef *pll3, uint32_t Divider)
{
  uint32_t tickstart;
  HAL_StatusTypeDef status = HAL_OK;
  assert_param(IS_RCC_PLL3M_VALUE(pll3->PLL3M));
  assert_param(IS_RCC_PLL3N_VALUE(pll3->PLL3N));
  assert_param(IS_RCC_PLL3P_VALUE(pll3->PLL3P));
  assert_param(IS_RCC_PLL3R_VALUE(pll3->PLL3R));
  assert_param(IS_RCC_PLL3Q_VALUE(pll3->PLL3Q));
  assert_param(IS_RCC_PLL3RGE_VALUE(pll3->PLL3RGE));
  assert_param(IS_RCC_PLL3VCO_VALUE(pll3->PLL3VCOSEL));
  assert_param(IS_RCC_PLLFRACN_VALUE(pll3->PLL3FRACN));

  /* Check that PLL3 OSC clock source is already set */
  if(__HAL_RCC_GET_PLL_OSCSOURCE() == RCC_PLLSOURCE_NONE)
  {
    return HAL_ERROR;
  }


  else
  {
    /* Disable  PLL3. */
    __HAL_RCC_PLL3_DISABLE();

    /* Get Start Tick*/
    tickstart = GetProcessTickMs();
    /* Wait till PLL3 is ready */
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY) != 0U)
    {
      if( (GetProcessTickMs() - tickstart ) > PLL3_TIMEOUT_VALUE)
      {
        return HAL_TIMEOUT;
      }
    }

    /* Configure the PLL3  multiplication and division factors. */
    __HAL_RCC_PLL3_CONFIG(pll3->PLL3M,
                          pll3->PLL3N,
                          pll3->PLL3P,
                          pll3->PLL3Q,
                          pll3->PLL3R);

    /* Select PLL3 input reference frequency range: VCI */
    __HAL_RCC_PLL3_VCIRANGE(pll3->PLL3RGE) ;

    /* Select PLL3 output frequency range : VCO */
    __HAL_RCC_PLL3_VCORANGE(pll3->PLL3VCOSEL) ;

    /* Disable PLL3FRACN . */
    __HAL_RCC_PLL3FRACN_DISABLE();

    /* Configures PLL3 clock Fractional Part Of The Multiplication Factor */
    __HAL_RCC_PLL3FRACN_CONFIG(pll3->PLL3FRACN);

    /* Enable PLL3FRACN . */
    __HAL_RCC_PLL3FRACN_ENABLE();

    /* Enable the PLL3 clock output */
    if(Divider == DIVIDER_P_UPDATE)
    {
      __HAL_RCC_PLL3CLKOUT_ENABLE(RCC_PLL3_DIVP);
    }
    else if(Divider == DIVIDER_Q_UPDATE)
    {
      __HAL_RCC_PLL3CLKOUT_ENABLE(RCC_PLL3_DIVQ);
    }
    else
    {
      __HAL_RCC_PLL3CLKOUT_ENABLE(RCC_PLL3_DIVR);
    }

    /* Enable  PLL3. */
    __HAL_RCC_PLL3_ENABLE();

    /* Get Start Tick*/
    tickstart = GetProcessTickMs();

    /* Wait till PLL3 is ready */
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY) == 0U)
    {
      if( (GetProcessTickMs() - tickstart ) > PLL3_TIMEOUT_VALUE)
      {
        return HAL_TIMEOUT;
      }
    }

  }


  return status;
}