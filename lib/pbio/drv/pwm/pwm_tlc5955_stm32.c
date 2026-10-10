// SPDX-License-Identifier: MIT
// Copyright (c) 2020-2026 The Pybricks Authors

// PWM driver using TI TLC5955 LED driver connected to STM32 MCU via SPI.

#include <pbdrv/config.h>

#if PBDRV_CONFIG_PWM_TLC5955_STM32

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include STM32_HAL_H

#include <pbdrv/pwm.h>
#include <pbdrv/tlc5955.h>

#include <pbio/busy_count.h>
#include <pbio/error.h>
#include <pbio/util.h>
#include <pbio/os.h>

#include "pwm_tlc5955_stm32.h"
#include "pwm.h"

typedef struct {
    /** HAL SPI data */
    SPI_HandleTypeDef hspi;
    /** HAL Rx DMA data */
    DMA_HandleTypeDef hdma_rx;
    /** HAL Tx DMA data */
    DMA_HandleTypeDef hdma_tx;
    /** Process */
    pbio_os_process_t process;
    /** Pointer to generic PWM device instance */
    pbdrv_pwm_dev_t *pwm;
    /** Grayscale latch register data */
    uint8_t *grayscale_latch;
    /** grayscale value has changed, update needed */
    bool changed;
} pbdrv_pwm_tlc5955_stm32_priv_t;

static pbdrv_pwm_tlc5955_stm32_priv_t dev_priv[PBDRV_CONFIG_PWM_TLC5955_STM32_NUM_DEV];

// Control latch data setting dot correction to 100%, max current to 3.2 mA,
// global brightness to 100%, auto repeat enabled, display timing reset disabled,
// auto refresh disabled, ES-PWM mode enabled, LSD detection voltage 90%.
static const TLC5955_CONTROL_DATA(control_latch_3mA, 127, TLC5955_MC_3_2, 127, 1, 0, 0, 1, 1);

static uint8_t grayscale_latch[PBDRV_CONFIG_PWM_TLC5955_STM32_NUM_DEV][TLC5955_DATA_SIZE];

// channels are mapped to GS registers in reverse order. CH 0: GSB15, CH 1: GSG15,
// CH 2: GSR15 ... CH 45: GSB0, CH 46: GSG0, CH 47: GSR0
static pbio_error_t pbdrv_pwm_tlc5955_stm32_set_duty(pbdrv_pwm_dev_t *dev, uint32_t ch, uint32_t value) {
    pbdrv_pwm_tlc5955_stm32_priv_t *priv = dev->priv;

    assert(ch < TLC5955_NUM_CHANNEL);
    assert(value <= UINT16_MAX);

    pbdrv_tlc5955_set_duty(priv->grayscale_latch, ch, value);
    priv->changed = true;
    pbio_os_request_poll();

    return PBIO_SUCCESS;
}

static const pbdrv_pwm_driver_funcs_t pbdrv_pwm_tlc5955_stm32_funcs = {
    .set_duty = pbdrv_pwm_tlc5955_stm32_set_duty,
};

// toggles LAT signal on and off to latch data in shift register
static pbio_error_t pbdrv_pwm_tlc5955_toggle_wait_and_latch(pbio_os_state_t *state, pbdrv_pwm_tlc5955_stm32_priv_t *priv) {

    const pbdrv_pwm_tlc5955_stm32_platform_data_t *pdata = priv->pwm->pdata;

    PBIO_OS_ASYNC_BEGIN(state);

    // Wait for SPI to finish transmitting data before toggling LAT.
    PBIO_OS_AWAIT_UNTIL(state, priv->hspi.State == HAL_SPI_STATE_READY);

    // Need to await between toggles or else LTO will inline the two GPIO
    // writes into back-to-back BSRR accesses (~ns apart), below the TLC5955
    // minimum latch pulse width.
    HAL_GPIO_WritePin(pdata->lat_gpio, pdata->lat_gpio_pin, GPIO_PIN_SET);
    PBIO_OS_AWAIT_ONCE(state);
    HAL_GPIO_WritePin(pdata->lat_gpio, pdata->lat_gpio_pin, GPIO_PIN_RESET);
    PBIO_OS_ASYNC_END(PBIO_SUCCESS);
}

static pbio_error_t pbdrv_pwm_tlc5955_stm32_process_thread(pbio_os_state_t *state, void *context) {

    pbdrv_pwm_tlc5955_stm32_priv_t *priv = context;
    static pbio_os_state_t sub;

    PBIO_OS_ASYNC_BEGIN(state);

    // need to allow all drivers to init first
    PBIO_OS_AWAIT_ONCE(state);

    HAL_SPI_Transmit_DMA(&priv->hspi, (uint8_t *)control_latch_3mA, TLC5955_DATA_SIZE);
    PBIO_OS_AWAIT(state, &sub, pbdrv_pwm_tlc5955_toggle_wait_and_latch(&sub, priv));

    // have to send twice for max current to take effect
    HAL_SPI_Transmit_DMA(&priv->hspi, (uint8_t *)control_latch_3mA, TLC5955_DATA_SIZE);
    PBIO_OS_AWAIT(state, &sub, pbdrv_pwm_tlc5955_toggle_wait_and_latch(&sub, priv));

    // initialization is finished so consumers can use this PWM device now.
    priv->pwm->funcs = &pbdrv_pwm_tlc5955_stm32_funcs;
    pbio_busy_count_down();

    for (;;) {
        PBIO_OS_AWAIT_UNTIL(state, priv->changed);
        HAL_SPI_Transmit_DMA(&priv->hspi, priv->grayscale_latch, TLC5955_DATA_SIZE);
        priv->changed = false;
        PBIO_OS_AWAIT(state, &sub, pbdrv_pwm_tlc5955_toggle_wait_and_latch(&sub, priv));
    }

    // Unreachable
    PBIO_OS_ASYNC_END(PBIO_ERROR_FAILED);
}

void pbdrv_pwm_tlc5955_stm32_init(pbdrv_pwm_dev_t *devs) {
    for (int i = 0; i < PBDRV_CONFIG_PWM_TLC5955_STM32_NUM_DEV; i++) {
        const pbdrv_pwm_tlc5955_stm32_platform_data_t *pdata = &pbdrv_pwm_tlc5955_stm32_platform_data[i];
        pbdrv_pwm_dev_t *pwm = &devs[pdata->id];
        pbdrv_pwm_tlc5955_stm32_priv_t *priv = &dev_priv[i];

        HAL_GPIO_WritePin(pdata->lat_gpio, pdata->lat_gpio_pin, GPIO_PIN_RESET);
        GPIO_InitTypeDef gpio_init;
        gpio_init.Pin = pdata->lat_gpio_pin;
        gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(pdata->lat_gpio, &gpio_init);

        priv->hdma_rx.Instance = pdata->rx_dma;
        #if defined(STM32H5)
        priv->hdma_rx.Init.Request = pdata->rx_dma_req;
        priv->hdma_rx.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
        priv->hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        priv->hdma_rx.Init.SrcInc = DMA_SINC_FIXED;
        priv->hdma_rx.Init.DestInc = DMA_DINC_INCREMENTED;
        priv->hdma_rx.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
        priv->hdma_rx.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
        priv->hdma_rx.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
        priv->hdma_rx.Init.SrcBurstLength = 1;
        priv->hdma_rx.Init.DestBurstLength = 1;
        priv->hdma_rx.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
        priv->hdma_rx.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
        priv->hdma_rx.Init.Mode = DMA_NORMAL;
        #else
        priv->hdma_rx.Init.Channel = pdata->rx_dma_ch;
        priv->hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        priv->hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        priv->hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
        priv->hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        priv->hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        priv->hdma_rx.Init.Mode = DMA_NORMAL;
        priv->hdma_rx.Init.Priority = DMA_PRIORITY_LOW;
        priv->hdma_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        priv->hdma_rx.Init.MemBurst = DMA_MBURST_SINGLE;
        priv->hdma_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;
        #endif
        HAL_DMA_Init(&priv->hdma_rx);

        HAL_NVIC_SetPriority(pdata->rx_dma_irq, 7, 0);
        HAL_NVIC_EnableIRQ(pdata->rx_dma_irq);

        priv->hdma_tx.Instance = pdata->tx_dma;
        #if defined(STM32H5)
        priv->hdma_tx.Init.Request = pdata->tx_dma_req;
        priv->hdma_tx.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
        priv->hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        priv->hdma_tx.Init.SrcInc = DMA_SINC_INCREMENTED;
        priv->hdma_tx.Init.DestInc = DMA_DINC_FIXED;
        priv->hdma_tx.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
        priv->hdma_tx.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
        priv->hdma_tx.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
        priv->hdma_tx.Init.SrcBurstLength = 1;
        priv->hdma_tx.Init.DestBurstLength = 1;
        priv->hdma_tx.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
        priv->hdma_tx.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
        priv->hdma_tx.Init.Mode = DMA_NORMAL;
        #else
        priv->hdma_tx.Init.Channel = pdata->tx_dma_ch;
        priv->hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        priv->hdma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        priv->hdma_tx.Init.MemInc = DMA_MINC_ENABLE;
        priv->hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        priv->hdma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        priv->hdma_tx.Init.Mode = DMA_NORMAL;
        priv->hdma_tx.Init.Priority = DMA_PRIORITY_LOW;
        priv->hdma_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        priv->hdma_tx.Init.MemBurst = DMA_MBURST_SINGLE;
        priv->hdma_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;
        #endif
        HAL_DMA_Init(&priv->hdma_tx);

        HAL_NVIC_SetPriority(pdata->tx_dma_irq, 7, 1);
        HAL_NVIC_EnableIRQ(pdata->tx_dma_irq);

        priv->hspi.Instance = pdata->spi;
        priv->hspi.Init.Mode = SPI_MODE_MASTER;
        priv->hspi.Init.Direction = SPI_DIRECTION_2LINES;
        priv->hspi.Init.DataSize = SPI_DATASIZE_8BIT;
        priv->hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
        priv->hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
        priv->hspi.Init.NSS = SPI_NSS_SOFT;
        priv->hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
        priv->hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
        priv->hspi.Init.TIMode = SPI_TIMODE_DISABLE;
        priv->hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        priv->hspi.Init.CRCPolynomial = 0;
        HAL_SPI_Init(&priv->hspi);

        __HAL_LINKDMA(&priv->hspi, hdmarx, priv->hdma_rx);
        __HAL_LINKDMA(&priv->hspi, hdmatx, priv->hdma_tx);

        HAL_NVIC_SetPriority(pdata->spi_irq, 7, 2);
        HAL_NVIC_EnableIRQ(pdata->spi_irq);

        priv->pwm = pwm;
        priv->grayscale_latch = grayscale_latch[i];
        pwm->pdata = pdata;
        pwm->priv = priv;
        // don't set funcs yet since we are not fully initialized
        pbio_busy_count_up();

        pbio_os_process_start(&priv->process, pbdrv_pwm_tlc5955_stm32_process_thread, priv);
    }
}

/**
 * Interrupt handler for Rx DMA IRQ. Needs to be called from IRQ handler in platform.c.
 */
void pbdrv_pwm_tlc5955_stm32_rx_dma_irq(uint8_t index) {
    pbdrv_pwm_tlc5955_stm32_priv_t *priv = &dev_priv[index];

    HAL_DMA_IRQHandler(&priv->hdma_rx);
}

/**
 * Interrupt handler for Tx DMA IRQ. Needs to be called from IRQ handler in platform.c.
 */
void pbdrv_pwm_tlc5955_stm32_tx_dma_irq(uint8_t index) {
    pbdrv_pwm_tlc5955_stm32_priv_t *priv = &dev_priv[index];

    HAL_DMA_IRQHandler(&priv->hdma_tx);
}

/**
 * Interrupt handler for SPI IRQ. Needs to be called from IRQ handler in platform.c.
 */
void pbdrv_pwm_tlc5955_stm32_spi_irq(uint8_t index) {
    pbdrv_pwm_tlc5955_stm32_priv_t *priv = &dev_priv[index];

    HAL_SPI_IRQHandler(&priv->hspi);
}

/**
 * Tx transfer complete. Needs to be called from handler in platform.c.
 */
void pbdrv_pwm_tlc5955_stm32_spi_tx_complete(void) {
    pbio_os_request_poll();
}

#endif // PBDRV_CONFIG_PWM_TLC5955_STM32
