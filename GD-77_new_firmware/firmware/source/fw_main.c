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

#include "fw_main.h"

#if defined(USE_SEGGER_RTT) && (USE_SEGGER_RTT > 0)
#include <SeggerRTT/RTT/SEGGER_RTT.h>
#endif

TaskHandle_t fwMainTaskHandle;

void fw_init()
{
	xTaskCreate(fw_main_task,                        /* pointer to the task */
				"fw main task",                      /* task name for kernel awareness debugging */
				5000L / sizeof(portSTACK_TYPE),      /* task stack size */
				NULL,                      			 /* optional task startup argument */
				5U,                                  /* initial priority */
				fwMainTaskHandle					 /* optional task handle to create */
				);

    vTaskStartScheduler();
}

int Display_light_Timer = 0;
bool Display_light_Touched = false;
bool Show_SplashScreen = false;
int SplashScreen_Timer = 0;
bool Shutdown = false;
int Shutdown_Timer = 0;

void show_splashscreen()
{
	UC1701_clear();
	UC1701_printCentered(1, "Experimental");
	UC1701_printCentered(2, "firmware V0.0.1a");
	UC1701_printCentered(4, "by");
	UC1701_printCentered(6, "DG4KLU");
	Display_light_Touched = true;
}

void show_poweroff()
{
	UC1701_clear();
	UC1701_printCentered(2, "Power off ...");
	UC1701_printCentered(4, "73 de DG4KLU");
	Display_light_Touched = true;
}

void reset_splashscreen()
{
	Show_SplashScreen=false;
	SplashScreen_Timer=0;
}

bool open_squelch=false;
bool HR_C6000_datalogging=false;

void trx_set_mode_band_freq_and_others()
{
	uint32_t f = current_frequency*1.6f;
	uint8_t fl_l = (f & 0x000000ff) >> 0;
	uint8_t fl_h = (f & 0x0000ff00) >> 8;
	uint8_t fh_l = (f & 0x00ff0000) >> 16;
	uint8_t fh_h = (f & 0xff000000) >> 24;

	uint8_t squelch = 0x00;
	if ((current_mode == MODE_ANALOG) && (!open_squelch))
	{
		squelch = 0x08;
	}

	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x40, 0x06 | squelch); // RX off
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x05, 0x87, 0x63); // select 'normal' frequency mode
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x29, fh_h, fh_l);
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x2a, fl_h, fl_l);
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x49, 0x0C, 0x15); // setting SQ open and shut threshold
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x40, 0x26 | squelch); // RX on

	if (current_mode == MODE_SILENT)
	{
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x44, 0x06, 0x00); // set internal volume to 0%
	    GPIO_PinWrite(GPIO_speaker_mute, Pin_speaker_mute, 0); // speaker off
	    GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 0); // connect AT1846S audio to HR_C6000
		GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);
		GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);
	    SAI_TransferTerminateSendEDMA(I2S0, &g_SAI_TX_Handle);
	    NVIC_DisableIRQ(PORTC_IRQn);
	}
	else
	{
		if (current_mode == MODE_ANALOG)
		{
			write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x44, 0x06, 0x80); // set internal volume to 50%
		    GPIO_PinWrite(GPIO_speaker_mute, Pin_speaker_mute, 1); // speaker on
		    GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1); // connect AT1846S audio to speaker
		    SAI_TransferTerminateSendEDMA(I2S0, &g_SAI_TX_Handle);
		    NVIC_DisableIRQ(PORTC_IRQn);
		}
		else if (current_mode == MODE_DIGITAL)
		{
			write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x44, 0x06, 0xCC); // set internal volume to 80%
		    GPIO_PinWrite(GPIO_speaker_mute, Pin_speaker_mute, 1); // speaker on
		    GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 0); // connect AT1846S audio to HR_C6000
	    	int_sys=false;
	    	int_ts=false;
	    	slot_state=0;
	    	tick_cnt=0;
		    NVIC_EnableIRQ(PORTC_IRQn);
		    g_TX_SAI_in_use = false;
		    SAI_TxSoftwareReset(I2S0, kSAI_ResetAll);
			SAI_TxEnable(I2S0, true);
			wavbuffer_read_idx = 0;
			wavbuffer_write_idx = 0;
			wavbuffer_count = 0;
			init_codec();
		}
		if (current_band == BAND_VHF)
		{
			GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 1);
			GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);
		}
		else if (current_band == BAND_UHF)
		{
			GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);
			GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 1);
		}
	}
}

void fw_main_task()
{
    USB_DeviceApplicationInit();

	fw_init_common();
	fw_init_buttons();
	fw_init_LEDs();
	fw_init_keyboard();
	fw_init_display();

    // Init I2C
    init_I2C0a();
    setup_I2C0();

    // Init SPI
    init_SPI();
    setup_SPI0();
    setup_SPI1();

    // Init I2S
    init_I2S();
    setup_I2S();

    // Init AT1846S
    I2C_AT1846S_init();

    // Init HR-C6000
    SPI_HR_C6000_init();

    // Additional init stuff
    SPI_C6000_postinit1();
    SPI_C6000_postinit2();
    SPI_C6000_postinit3a();
    SPI_C6000_postinit3b();
    I2C_AT1846_Postinit();

    // Init HR-C6000 interrupts
    init_HR_C6000_interrupts();

    // Small startup delay after initialization to stabilize system
    vTaskDelay(portTICK_PERIOD_MS * 500);

    fw_init_beep_task();

    set_melody(melody_poweron);

	Show_SplashScreen = true;

	reset_menu();

	init_edit();

	open_squelch=false;
	HR_C6000_datalogging=false;

	custom_value=0;

#if defined(USE_SEGGER_RTT) && (USE_SEGGER_RTT > 0)
    SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
#endif

    while (1U)
    {
    	if (current_mode==MODE_DIGITAL)
    	{
    		tick_HR_C6000();
    		custom_value++;
    	}

    	// Read button state and event
    	fw_check_button_event(&buttons, &button_event);

    	// Read keyboard state and event
    	fw_check_key_event(&keys, &key_event);

    	if (key_event==EVENT_KEY_CHANGE)
    	{
    		if (keys!=0)
    		{
        	    set_melody(melody_key_beep);

        	    if (current_menu_level==-1)
        	    {
        	    	tick_edit();
        	    }
    		}
    	}

    	if (button_event==EVENT_BUTTON_CHANGE)
    	{
    		if ((buttons & BUTTON_SK1)!=0)
    		{
        	    set_melody(melody_sk1_beep);
    		}
    		else if ((buttons & BUTTON_SK2)!=0)
    		{
        	    set_melody(melody_sk2_beep);
    		}
    		else if ((buttons & BUTTON_ORANGE)!=0)
    		{
        	    set_melody(melody_orange_beep);
    		}
    	}

    	if (!Shutdown)
    	{
    		tick_menu();
    	}

    	if ((GPIO_PinRead(GPIO_Power_Switch, Pin_Power_Switch)!=0) && (!Shutdown))
    	{
    		reset_splashscreen();
    		reset_menu();
    		show_poweroff();
		    GPIO_PinWrite(GPIO_speaker_mute, Pin_speaker_mute, 0);
		    set_melody(NULL);
    		Shutdown=true;
			Shutdown_Timer = 2000;
    	}
    	else if ((GPIO_PinRead(GPIO_Power_Switch, Pin_Power_Switch)==0) && (Shutdown))
    	{
			update_screen();
			Shutdown=false;
			Shutdown_Timer = 0;
    	}

    	if (Shutdown)
    	{
    		if (Shutdown_Timer>0)
    		{
    			Shutdown_Timer--;
    			if (Shutdown_Timer==0)
    			{
    				GPIO_PinWrite(GPIO_Keep_Power_On, Pin_Keep_Power_On, 0);
    			}
    		}
    	}

		if (Show_SplashScreen)
		{
			if ((keys==KEY_1) || (keys==KEY_2) || (keys==KEY_3) || (keys==KEY_STAR))
			{
				if (keys==KEY_1)
				{
					create_song(melody1);
				}
				else if (keys==KEY_2)
				{
					create_song(melody2);
				}
				else if (keys==KEY_3)
				{
					create_song(melody3);
				}
				else if (keys==KEY_STAR)
				{
					create_song(melody4);
				}
				set_melody(melody_generic);
				key_event=EVENT_KEY_NONE;
				update_screen();
			}
			else if (key_event!=EVENT_KEY_CHANGE)
			{
				show_splashscreen();
				SplashScreen_Timer = 4000;
			}
			Show_SplashScreen = false;
		}

		if (SplashScreen_Timer>0)
		{
			SplashScreen_Timer--;
			if (SplashScreen_Timer==0)
			{
				update_screen();
			}
		}

		if (Display_light_Touched)
		{
			if (Display_light_Timer==0)
			{
				GPIO_PinWrite(GPIO_Display_Light, Pin_Display_Light, 1);
			}
			Display_light_Timer = 4000;
			Display_light_Touched = false;
		}

		if (Display_light_Timer>0)
		{
			Display_light_Timer--;
			if (Display_light_Timer==0)
			{
				GPIO_PinWrite(GPIO_Display_Light, Pin_Display_Light, 0);
			}
		}

		tick_melody();

		vTaskDelay(portTICK_PERIOD_MS);
    }
}
