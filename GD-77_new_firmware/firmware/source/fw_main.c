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
#include "menu/menuSystem.h"
#include "menu/menuUtilityQSOData.h"
#include "fw_settings.h"

#if defined(USE_SEGGER_RTT)
#include <SeggerRTT/RTT/SEGGER_RTT.h>
#endif

void fw_main_task();

const char *FIRMWARE_VERSION_STRING = "V0.2.1";
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

static void show_lowbattery()
{
	UC1701_clearBuf();
	UC1701_printCentered(32, "LOW BATTERY !!!", UC1701_FONT_GD77_8x16);
	UC1701_render();
}

void fw_main_task()
{
	uint32_t keys;
	int key_event;
	uint32_t buttons;
	int button_event;
	
    USB_DeviceApplicationInit();

    // Init I2C
    init_I2C0a();
    setup_I2C0();
    settingsLoadSettings();

	fw_init_common();
	fw_init_buttons();
	fw_init_LEDs();
	fw_init_keyboard();
	fw_init_display();

    // Init SPI
    init_SPI();
    setup_SPI0();
    setup_SPI1();

    // Init I2S
    init_I2S();
    setup_I2S();

    // Init ADC
    adc_init();

    // Init DAC
    dac_init();

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

    SPI_Flash_init();

    // Small startup delay after initialization to stabilize system
    vTaskDelay(portTICK_PERIOD_MS * 500);

	init_pit();

	open_squelch=false;
	HR_C6000_datalogging=false;

	trx_measure_count = 0;

	if (get_battery_voltage()<CUTOFF_VOLTAGE_UPPER_HYST)
	{
		show_lowbattery();
		GPIO_PinWrite(GPIO_Keep_Power_On, Pin_Keep_Power_On, 0);
		while(1U) {};
	}

	init_hrc6000_task();

	init_watchdog();

    fw_init_beep_task();

    set_melody(melody_poweron);

#if defined(USE_SEGGER_RTT)
    SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_TRIM);
    SEGGER_RTT_printf(0,"Segger RTT initialised\n");
#endif

    lastheardInitList();
    menuInitMenuSystem();

    while (1U)
    {
    	taskENTER_CRITICAL();
    	uint32_t tmp_timer_maintask=timer_maintask;
    	taskEXIT_CRITICAL();
    	if (tmp_timer_maintask==0)
    	{
        	taskENTER_CRITICAL();
    		timer_maintask=10;
    	    alive_maintask=true;
        	taskEXIT_CRITICAL();

        	tick_com_request();

        	fw_check_button_event(&buttons, &button_event);// Read button state and event
        	fw_check_key_event(&keys, &key_event);// Read keyboard state and event

        	if (key_event==EVENT_KEY_CHANGE)
        	{
        		if (keys!=0)
        		{
            	    set_melody(melody_key_beep);
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

        		if ((buttons & BUTTON_PTT)!=0)
        		{
        			menuSystemPushNewMenu(MENU_TX_SCREEN);
        		}
        	}

        	menuSystemCallCurrentMenuTick(buttons,keys,(button_event<<1) | key_event);

        	if (((GPIO_PinRead(GPIO_Power_Switch, Pin_Power_Switch)!=0)
        			|| (battery_voltage<CUTOFF_VOLTAGE_LOWER_HYST))
        			&& (menuSystemGetCurrentMenuNumber() != MENU_POWER_OFF))
        	{
				settingsSaveSettings();

        		if (battery_voltage<CUTOFF_VOLTAGE_LOWER_HYST)
        		{
        			show_lowbattery();
        		}
        		else
        		{
					menuSystemPushNewMenu(MENU_POWER_OFF);
        		}
    		    GPIO_PinWrite(GPIO_speaker_mute, Pin_speaker_mute, 0);
    		    set_melody(NULL);
        	}

    		if (menuDisplayLightTimer > 0)
    		{
    			menuDisplayLightTimer--;
    			if (menuDisplayLightTimer==0)
    			{
    				fw_displayEnableBacklight(false);
    			}
    		}

    		tick_melody();
    	}

		vTaskDelay(0);
    }
}
