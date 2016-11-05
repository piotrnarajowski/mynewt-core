/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include "syscfg/syscfg.h"
#include "bsp/bsp.h"
#include "stm32f407xx.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_adc.h"
#include "flash_map/flash_map.h"
#include "os/os_dev.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#include "adc_stm32f4/adc_stm32f4.h"
#include "hal/hal_i2c.h"
#include "hal/hal_timer.h"
#include "hal/hal_bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_flash_int.h"
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
#include "hal/hal_spi.h"
#endif
#include "mcu/stm32f4_bsp.h"
#include "mcu/stm32f4xx_mynewt_hal.h"

static struct uart_dev hal_uart0;

/* XXX should not be here */

#if MYNEWT_VAL(ADC_1)
struct adc_dev my_dev_adc1;
#endif
#if MYNEWT_VAL(ADC_2)
struct adc_dev my_dev_adc2;
#endif
#if MYNEWT_VAL(ADC_3)
struct adc_dev my_dev_adc3;
#endif

struct stm32f4_uart_cfg;

#if MYNEWT_VAL(ADC_1)
/*
 * adc_handle is defined earlier because the DMA handle's
 * parent needs to be pointing to the adc_handle
 */
extern ADC_HandleTypeDef adc1_handle;

#define STM32F4_DEFAULT_DMA00_HANDLE {\
    .Instance = DMA2_Stream0,\
    .Init.Channel = DMA_CHANNEL_0,\
    .Init.Direction = DMA_PERIPH_TO_MEMORY,\
    .Init.PeriphInc = DMA_PINC_DISABLE,\
    .Init.MemInc = DMA_MINC_ENABLE,\
    .Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD,\
    .Init.MemDataAlignment = DMA_MDATAALIGN_WORD,\
    .Init.Mode = DMA_CIRCULAR,\
    .Init.Priority = DMA_PRIORITY_HIGH,\
    .Init.FIFOMode = DMA_FIFOMODE_DISABLE,\
    .Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL,\
    .Init.MemBurst = DMA_MBURST_SINGLE,\
    .Init.PeriphBurst = DMA_PBURST_SINGLE,\
    .Parent = &adc1_handle,\
}

DMA_HandleTypeDef adc1_dma00_handle = STM32F4_DEFAULT_DMA00_HANDLE;
#endif

#if MYNEWT_VAL(ADC_2)

extern ADC_HandleTypeDef adc2_handle;

#define STM32F4_DEFAULT_DMA21_HANDLE {\
    .Instance = DMA2_Stream2,\
    .Init.Channel = DMA_CHANNEL_1,\
    .Init.Direction = DMA_PERIPH_TO_MEMORY,\
    .Init.PeriphInc = DMA_PINC_DISABLE,\
    .Init.MemInc = DMA_MINC_ENABLE,\
    .Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD,\
    .Init.MemDataAlignment = DMA_MDATAALIGN_WORD,\
    .Init.Mode = DMA_CIRCULAR,\
    .Init.Priority = DMA_PRIORITY_HIGH,\
    .Init.FIFOMode = DMA_FIFOMODE_DISABLE,\
    .Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL,\
    .Init.MemBurst = DMA_MBURST_SINGLE,\
    .Init.PeriphBurst = DMA_PBURST_SINGLE,\
    .Parent = &adc2_handle,\
}

DMA_HandleTypeDef adc2_dma21_handle = STM32F4_DEFAULT_DMA21_HANDLE;
#endif


#if MYNEWT_VAL(ADC_3)

extern ADC_HandleTypeDef adc3_handle;

#define STM32F4_DEFAULT_DMA12_HANDLE {\
    .Instance = DMA2_Stream1,\
    .Init.Channel = DMA_CHANNEL_2,\
    .Init.Direction = DMA_PERIPH_TO_MEMORY,\
    .Init.PeriphInc = DMA_PINC_DISABLE,\
    .Init.MemInc = DMA_MINC_ENABLE,\
    .Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD,\
    .Init.MemDataAlignment = DMA_MDATAALIGN_WORD,\
    .Init.Mode = DMA_CIRCULAR,\
    .Init.Priority = DMA_PRIORITY_HIGH,\
    .Init.FIFOMode = DMA_FIFOMODE_DISABLE,\
    .Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL,\
    .Init.MemBurst = DMA_MBURST_SINGLE,\
    .Init.PeriphBurst = DMA_PBURST_SINGLE,\
    .Parent = &adc3_handle,\
}

DMA_HandleTypeDef adc3_dma12_handle = STM32F4_DEFAULT_DMA12_HANDLE;
#endif

#define STM32F4_ADC_DEFAULT_INIT_TD {\
    .ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2,\
    .Resolution = ADC_RESOLUTION12b,\
    .DataAlign = ADC_DATAALIGN_RIGHT,\
    .ScanConvMode = DISABLE,\
    .EOCSelection = DISABLE,\
    .ContinuousConvMode = ENABLE,\
    .NbrOfConversion = 1,\
    .DiscontinuousConvMode = DISABLE,\
    .NbrOfDiscConversion = 0,\
    .ExternalTrigConv = ADC_SOFTWARE_START,\
    .ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE,\
    .DMAContinuousRequests = ENABLE\
}

#if MYNEWT_VAL(ADC_1)

/*****************ADC1 Config ***************/
#define STM32F4_DEFAULT_ADC1_HANDLE {\
    .Init = STM32F4_ADC_DEFAULT_INIT_TD,\
    .Instance = ADC1,\
    .NbrOfCurrentConversionRank = 0,\
    .DMA_Handle = &adc1_dma00_handle,\
    .Lock = HAL_UNLOCKED,\
    .State = 0,\
    .ErrorCode = 0\
}

ADC_HandleTypeDef adc1_handle = STM32F4_DEFAULT_ADC1_HANDLE;

#define STM32F4_ADC1_DEFAULT_SAC {\
    .c_refmv = 3300,\
    .c_res   = 12,\
    .c_configured = 1,\
    .c_cnum = ADC_CHANNEL_10\
}

struct adc_chan_config adc1_chan10_config = STM32F4_ADC1_DEFAULT_SAC;

#define STM32F4_ADC1_DEFAULT_CONFIG {\
    .sac_chan_count = 16,\
    .sac_chans = (struct adc_chan_config [16]){{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},STM32F4_ADC1_DEFAULT_SAC},\
    .sac_adc_handle = &adc1_handle,\
}

struct stm32f4_adc_dev_cfg adc1_config = STM32F4_ADC1_DEFAULT_CONFIG;
/*********************************************/
#endif

#if MYNEWT_VAL(ADC_2)

/*****************ADC2 Config ***************/
#define STM32F4_DEFAULT_ADC2_HANDLE {\
    .Init = STM32F4_ADC_DEFAULT_INIT_TD,\
    .Instance = ADC2,\
    .NbrOfCurrentConversionRank = 0,\
    .DMA_Handle = &adc2_dma21_handle,\
    .Lock = HAL_UNLOCKED,\
    .State = 0,\
    .ErrorCode = 0\
}

ADC_HandleTypeDef adc2_handle = STM32F4_DEFAULT_ADC2_HANDLE;

#define STM32F4_ADC2_DEFAULT_SAC {\
    .c_refmv = 3300,\
    .c_res   = 12,\
    .c_configured = 1,\
    .c_cnum = ADC_CHANNEL_1\
}

struct adc_chan_config adc2_chan1_config = STM32F4_ADC2_DEFAULT_SAC;

#define STM32F4_ADC2_DEFAULT_CONFIG {\
    .sac_chan_count = 16,\
    .sac_chans = (struct adc_chan_config [16]){{0},STM32F4_ADC1_DEFAULT_SAC}\
    .sac_adc_handle = &adc2_handle,\
}

struct stm32f4_adc_dev_cfg adc2_config = STM32F4_ADC2_DEFAULT_CONFIG;
/*********************************************/
#endif

#if MYNEWT_VAL(ADC_3)

#define STM32F4_DEFAULT_ADC3_HANDLE {\
    .Init = STM32F4_ADC_DEFAULT_INIT_TD,\
    .Instance = ADC3,\
    .NbrOfCurrentConversionRank = 0,\
    .DMA_Handle = &adc3_dma12_handle,\
    .Lock = HAL_UNLOCKED,\
    .State = 0,\
    .ErrorCode = 0\
}

ADC_HandleTypeDef adc3_handle = STM32F4_DEFAULT_ADC3_HANDLE;

#define STM32F4_ADC3_DEFAULT_SAC {\
    .c_refmv = 3300,\
    .c_res   = 12,\
    .c_configured = 1,\
    .c_cnum = ADC_CHANNEL_4\
}

struct adc_chan_config adc3_chan4_config = STM32F4_ADC3_DEFAULT_SAC;

#define STM32F4_ADC3_DEFAULT_CONFIG {\
    .sac_chan_count = 16,\
    .sac_chans = (struct adc_chan_config [16]){{0},{0},{0},{0},STM32F4_ADC3_DEFAULT_SAC},\
    .sac_adc_handle = &adc3_handle,\
}

struct stm32f4_adc_dev_cfg adc3_config = STM32F4_ADC3_DEFAULT_CONFIG;
#endif

#if MYNEWT_VAL(I2C_0)
static struct stm32f4_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MCU_GPIO_PORTB(9),		/* PB9 */
    .hic_pin_scl = MCU_GPIO_PORTB(8),		/* PB8 */
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_speed = 100000				/* 100kHz */
};
#endif

#if MYNEWT_VAL(SPI_0_SLAVE) || MYNEWT_VAL(SPI_0_MASTER)
struct stm32f4_hal_spi_cfg spi0_cfg = {
    .ss_pin = MCU_GPIO_PORTA(4),		/* PA4 */
    .sck_pin  = MCU_GPIO_PORTA(5),		/* PA5 */
    .miso_pin = MCU_GPIO_PORTA(6),		/* PA6 */
    .mosi_pin = MCU_GPIO_PORTB(5),		/* PB5 */
    .irq_prio = 2
};
#endif

static const struct stm32f4_uart_cfg uart_cfg[UART_CNT] = {
    [0] = {
        .suc_uart = USART6,
        .suc_rcc_reg = &RCC->APB2ENR,
        .suc_rcc_dev = RCC_APB2ENR_USART6EN,
        .suc_pin_tx = MCU_GPIO_PORTC(6),	/* PC6 */
        .suc_pin_rx = MCU_GPIO_PORTC(7),	/* PC7 */
        .suc_pin_rts = -1,
        .suc_pin_cts = -1,
        .suc_pin_af = GPIO_AF8_USART6,
        .suc_irqn = USART6_IRQn
    }
};

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
    [1] = {
        .hbmd_start = &_ccram_start,
        .hbmd_size = CCRAM_SIZE
    }
};

const struct stm32f4_uart_cfg *
bsp_uart_config(int port)
{
    assert(port < UART_CNT);
    return &uart_cfg[port];
}

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &stm32f4_flash_dev;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return (0);
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    /* Add any interrupt priorities configured by the bsp here */
    return pri;
}

void
hal_bsp_init(void)
{
    int rc;

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

    rc = os_dev_create((struct os_dev *) &hal_uart0, CONSOLE_UART,
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)bsp_uart_config(0));
    assert(rc == 0);

#if MYNEWT_VAL(ADC_1)
    rc = os_dev_create((struct os_dev *) &my_dev_adc1, "adc1",
            OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
            stm32f4_adc_dev_init, &adc1_config);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(ADC_2)
    rc = os_dev_create((struct os_dev *) &my_dev_adc2, "adc2",
            OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
            stm32f4_adc_dev_init, &adc2_config);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(ADC_3)
    rc = os_dev_create((struct os_dev *) &my_dev_adc3, "adc3",
            OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
            stm32f4_adc_dev_init, &adc3_config);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, &i2c_cfg0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    hal_timer_init(0, TIM9);
#endif
}
