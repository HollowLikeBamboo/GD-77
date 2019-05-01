/*
 * Copyright (C)2019 Kai Ludwig, DG4KLU
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The name of the author may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "fw_i2s.h"

#define I2S_DMA_TX 0
#define I2S_DMA_RX 1

edma_handle_t g_EDMA_TX_Handle;
edma_handle_t g_EDMA_RX_Handle;

sai_edma_handle_t g_SAI_TX_Handle;
sai_edma_handle_t g_SAI_RX_Handle;

void init_I2S()
{
    /* PORTA16 is configured as I2S_FS*/
    PORT_SetPinMux(Port_I2S_FS_C6000, Pin_I2S_FS_C6000, kPORT_MuxAlt6);

    /* PORTA14 is configured as I2S_CK */
    PORT_SetPinMux(Port_I2S_CK_C6000, Pin_I2S_CK_C6000, kPORT_MuxAlt6);

    /* PORTA12 is configured as I2S_RX */
    PORT_SetPinMux(Port_I2S_RX_C6000, Pin_I2S_RX_C6000, kPORT_MuxAlt6);

    /* PORTA15 is configured as I2S_TX */
    PORT_SetPinMux(Port_I2S_TX_C6000, Pin_I2S_TX_C6000, kPORT_MuxAlt6);

    NVIC_SetPriority(I2S0_Tx_IRQn, 3);
    NVIC_SetPriority(I2S0_Rx_IRQn, 3);
}

void SAI_TX_Callback(I2S_Type *base, sai_edma_handle_t *handle, status_t status, void *userData)
{
}

void SAI_RX_Callback(I2S_Type *base, sai_edma_handle_t *handle, status_t status, void *userData)
{
}

void setup_I2S()
{
	sai_config_t s_TxConfig;
	sai_config_t s_RxConfig;

	edma_config_t edma_config;

    SAI_TxGetDefaultConfig(&s_TxConfig);
    SAI_RxGetDefaultConfig(&s_RxConfig);

    SAI_TxInit(I2S0, &s_TxConfig);
    SAI_RxInit(I2S0, &s_RxConfig);

	DMAMUX_Init(DMAMUX0);
    DMAMUX_SetSource(DMAMUX0, I2S_DMA_TX, 0b001101); // 0b001101..I2S0_Tx_Signal
    DMAMUX_SetSource(DMAMUX0, I2S_DMA_RX, 0b001100); // 0b001100..I2S0_Rx_Signal
    DMAMUX_EnableChannel(DMAMUX0, I2S_DMA_TX);
    DMAMUX_EnableChannel(DMAMUX0, I2S_DMA_RX);

	EDMA_GetDefaultConfig(&edma_config);
	EDMA_Init(DMA0, &edma_config);
    EDMA_CreateHandle(&g_EDMA_TX_Handle, DMA0, I2S_DMA_TX);
    EDMA_CreateHandle(&g_EDMA_RX_Handle, DMA0, I2S_DMA_RX);

    SAI_TransferTxCreateHandleEDMA(I2S0, &g_SAI_TX_Handle, SAI_TX_Callback, NULL, &g_EDMA_TX_Handle);
    SAI_TransferRxCreateHandleEDMA(I2S0, &g_SAI_RX_Handle, SAI_RX_Callback, NULL, &g_EDMA_RX_Handle);

    sai_transfer_format_t SAI_TX_format;
    SAI_TX_format.sampleRate_Hz = kSAI_SampleRate8KHz;
    SAI_TX_format.bitWidth = kSAI_WordWidth16bits;
    SAI_TX_format.stereo = kSAI_Stereo;
    SAI_TX_format.masterClockHz = 512 * SAI_TX_format.sampleRate_Hz;
    SAI_TX_format.watermark = 4;
    SAI_TX_format.channel = 0;
    SAI_TX_format.protocol = kSAI_BusI2S;
    SAI_TransferTxSetFormatEDMA(I2S0, &g_SAI_TX_Handle, &SAI_TX_format, SAI_TX_format.masterClockHz, SAI_TX_format.masterClockHz);

    sai_transfer_format_t SAI_RX_format;
    SAI_RX_format.sampleRate_Hz = kSAI_SampleRate8KHz;
    SAI_RX_format.bitWidth = kSAI_WordWidth16bits;
    SAI_RX_format.stereo = kSAI_Stereo;
    SAI_RX_format.masterClockHz = 512 * SAI_RX_format.sampleRate_Hz;
    SAI_RX_format.watermark = 4;
    SAI_RX_format.channel = 1;
    SAI_RX_format.protocol = kSAI_BusI2S;
    SAI_TransferTxSetFormatEDMA(I2S0, &g_SAI_RX_Handle, &SAI_RX_format, SAI_RX_format.masterClockHz, SAI_RX_format.masterClockHz);
}