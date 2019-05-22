/*
 * Copyright (c) 2018 naehrwert
 *
 * Copyright (c) 2018-2019 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//#include "config/config.h"
#include "gfx/di.h"
#include "gfx/gfx.h"
#include "gfx/logos.h"
#include "gfx/tui.h"
#include "libs/fatfs/ff.h"
#include "mem/heap.h"
#include "mem/sdram.h"
#include "power/max77620.h"
#include "rtc/max77620-rtc.h"
#include "soc/hw_init.h"
#include "soc/i2c.h"
#include "soc/t210.h"
#include "soc/uart.h"
#include "storage/sdmmc.h"
#include "utils/btn.h"
#include "utils/dirlist.h"
#include "utils/list.h"
#include "utils/util.h"


//TODO: ugly.
sdmmc_t sd_sdmmc;
sdmmc_storage_t sd_storage;
FATFS sd_fs;
static bool sd_mounted;
bool septactive;
u8 folder;

bool sd_mount()
{
	if (sd_mounted)
		return true;

	if (sdmmc_storage_init_sd(&sd_storage, &sd_sdmmc, SDMMC_1, SDMMC_BUS_WIDTH_4, 11))
	
	{
		int res = 0;
		res = f_mount(&sd_fs, "", 1);
		if (res == FR_OK)
		{
			sd_mounted = 1;
			return true;
		}
		else
		{
			EPRINTFARGS("Failed to mount SD card (FatFS Error %d).\nMake sure that a FAT partition exists..", res);
		}
	}

	return false;
}

void sd_unmount()
{
	if (sd_mounted)
	{
		f_mount(NULL, "", 1);
		sdmmc_storage_end(&sd_storage);
		sd_mounted = false;
	}
}

int sd_save_to_file(void *buf, u32 size, const char *filename)
{
	FIL fp;
	u32 res = 0;
	res = f_open(&fp, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res)
	{
		EPRINTFARGS("Error (%d) creating file\n%s.\n", res, filename);
		return 1;
	}

	f_write(&fp, buf, size, NULL);
	f_close(&fp);

	return 0;
}

void check_power_off_from_hos()
{
	// Power off on AutoRCM wakeup from HOS shutdown. For modchips/dongles.
	u8 hosWakeup = i2c_recv_byte(I2C_5, MAX77620_I2C_ADDR, MAX77620_REG_IRQTOP);
	if (hosWakeup & MAX77620_IRQ_TOP_RTC_MASK)
	{
		sd_unmount();

		// Stop the alarm, in case we injected too fast.
		max77620_rtc_stop_alarm();
		power_off();
	}
}

// This is a safe and unused DRAM region for our payloads.
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_SZ    0x94
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR    0xC03C0000
#define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size)
{
	memcpy((u8 *)payload_src, (u8 *)IPL_LOAD_ADDR, PATCHED_RELOC_SZ);

	volatile reloc_meta_t *relocator = (reloc_meta_t *)(payload_src + RELOC_META_OFF);

	relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
	relocator->stack = PATCHED_RELOC_STACK;
	relocator->end   = payload_dst + payload_size;
	relocator->ep    = payload_dst;
}

int launch_payload(char *path, bool update)
{
	if (sd_mount())
	{
		FIL fp;
		if (f_open(&fp, path, FA_READ))
		{
			EPRINTFARGS("Payload file is missing!\n(%s)", path);
			sd_unmount();

			return 1;
		}

		// Read and copy the payload to our chosen address
		void *buf;
		u32 size = f_size(&fp);

		buf = (void *)RCM_PAYLOAD_ADDR;

		if (f_read(&fp, buf, size, NULL))
		{
			f_close(&fp);
			sd_unmount();

			return 1;
		}

		f_close(&fp);
		free(path);

		sd_unmount();

		if (size < 0x30000)
		{
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));
			reconfig_hw_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
		}
		else
		{
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
			reconfig_hw_workaround(true, 0);
		}

		void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;

			(*ext_payload_ptr)();
	}

	return 1;
}

void auto_launch_switchboot()
{
	
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "bootloader/switchboot.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("bootloader/switchboot.bin", false);
			}

		}
	}
	void auto_launch_argon()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "argon/argon.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("argon/argon.bin", false);
			}

		}
	}
	
void auto_launch_payload_bin()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "payload.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("payload.bin", false);
			}

		}
	}
	
	void auto_launch_payload_bin_folder()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "bootloader/payloads/payload.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("bootloader/payloads/payload.bin", false);
			}

		}
	}
	
	void auto_launch_update_bin()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "bootloader/update.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("bootloader/update.bin", false);
			}

		}
	}
	
void auto_launch_numbered_payload_bin()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "payload1.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("payload1.bin", false);
			}

		}
	}
	void auto_launch_numbered_payload_bin_folder()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "bootloader/payloads/payload1.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("bootloader/payloads/payload1.bin", false);
			}

		}
	}
	
int create_chip_data()
{
//this file is created on-the-fly. The SAMD/straps decide if to change these values or not by sending YES or NO to a buffered usb write. Cheeky.
FIL fp;
	if (sd_mount())
		{
			f_mkdir("bootloader");
			f_mkdir("bootloader/switchboot_info");
			f_mkdir("bootloader/payloads");
			if (f_open(&fp, "bootloader/switchboot_info/straps_info.txt", FA_CREATE_ALWAYS | FA_WRITE))
				return 0;
			else
			{
				f_puts("a                                           a", &fp);
				f_puts("\n\n", &fp);
				f_puts("b                                           b", &fp);
				f_puts("\n\n", &fp);
				f_puts("c                                           c", &fp);
				f_puts("\n\n", &fp);
				f_puts("d                                           d", &fp);
				f_puts("\n\n", &fp);
				f_puts("e                                           e", &fp);
				f_puts("\n\n", &fp);
				f_puts("f                                           f", &fp);
				f_puts("\n\n", &fp);
				f_puts("g                                           g", &fp);
				f_close(&fp);
				return 1;
			}
		} else return 0;
}

int create_readme()
{

FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "bootloader/switchboot_info/paths_info.txt", FA_CREATE_ALWAYS | FA_WRITE))
				return 0;
			else
			{
				f_puts("Welcome to Fusee-UF2 Information.\n", &fp);
				f_puts("Usable paths... Only payload.bin and payloadx.bin are displayed on error screen.\n", &fp);
				f_puts("Made to just boot payload.bin(or payloadx.bin depending of selection in SAMD21, eg payload2.bin)", &fp);
				f_puts("\n\n", &fp);
				f_puts("Hidden hardcoded paths:", &fp);
				f_puts("\n\n", &fp);
				f_puts("Heirarchy of payloads looked for (in order...paths/files should be EXACTLY as below... Choose one...", &fp);
				f_puts("\n\n", &fp);
				f_puts("The first one found will boot.", &fp);
				f_puts("\n", &fp);
				f_puts("\n1. bootloader/switchboot.bin (my hekate mod for chipped units)", &fp);
				f_puts("\n2. argon/argon.bin (touchscreen payload launcher)", &fp);
				f_puts("\n3. payload.bin (Can be anything you want it to be)", &fp);
				f_puts("\n4. bootloader/payloads/payload.bin (Can be anything you want it to be)(keeps SD root tidy)", &fp);
				f_puts("\n5. payload1.bin (changes depending on SAMD setting)", &fp);
				f_puts("\n6. bootloader/payloads/payload1.bin (changes depending on SAMD setting)(keeps SD root tidy)", &fp);
				f_puts("\n7. bootloader/update.bin (last chance saloon - for Kosmos etc users - no need for payload.bin if this present - probably hekate)", &fp);
				f_puts("\n\n", &fp);
				f_puts("Remember... First entry found will boot. Kosmos users don`t need to do anything... update.bin will boot automatically", &fp);
				f_puts("\n", &fp);
				f_puts("\nControls:", &fp);
				f_puts("\n[VOL+]+[VOL-] - create paths_info.txt", &fp);
				f_puts("\n[VOL+] - create SXOS licence.dat", &fp);
				f_puts("\n\n", &fp);
				f_close(&fp);
				return 1;
			}
		} else return 0;
}

int create_license_dat(){
	unsigned char license_dat[256] = { 
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53, 0x53, 0x58, 0x4F, 0x53,
	0x53, 0x58, 0x4F, 0x53
};
	if (!sd_mount()){
		return 0;
	}
	//does it exist?
	if (f_stat("license.dat", NULL) == FR_OK){
		gfx_printf(&gfx_con,"\nRelease button!\n\nSXOS license.dat exists!\n\nCancelled.\n\n");
		msleep(3000);
		return 0;
	} else {
		gfx_printf(&gfx_con,"\nRelease button!\n\nPlease wait\n\n");
	sd_save_to_file(license_dat, sizeof(license_dat), "license.dat");
		gfx_printf(&gfx_con,"Success. Written %d bytes\n\n", sizeof(license_dat));
	sd_unmount();
	msleep(3000);
	}
	return 1;

}


#define IPL_STACK_TOP  0x90010000
#define IPL_HEAP_START 0x90020000
#define IPL_HEAP_END   0xB5000000

extern void pivot_stack(u32 stack_top);

void ipl_main()
{
	config_hw();
	pivot_stack(IPL_STACK_TOP);
	heap_init(IPL_HEAP_START);
	
	

	display_init();
	display_backlight_pwm_init();
	display_backlight_brightness(100, 1000);
	u32 *fb = display_init_framebuffer();
	gfx_init_ctxt(&gfx_ctxt, fb, 720, 1280, 720);
	gfx_con_init(&gfx_con, &gfx_ctxt);
	gfx_clear_grey(&gfx_ctxt, 0x00);
	gfx_con_setpos(&gfx_con, 0, 0);
	
	check_power_off_from_hos();
	u8 btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
	if (btn & BTN_VOL_DOWN && btn & BTN_VOL_UP){
	u8 res = create_readme();
	if (res) gfx_puts(&gfx_con, "\nReadme Created\n");
	msleep(1000);
	} else if ((!(btn & BTN_VOL_DOWN) && btn & BTN_VOL_UP)){	
	u8 res = create_license_dat(); 
	if (res) gfx_puts(&gfx_con, "\nSXOS Licence.dat created\n");
	msleep(1000);
	} 
	create_chip_data();
	auto_launch_switchboot();
	auto_launch_argon();
	auto_launch_payload_bin();
	auto_launch_payload_bin_folder();
	auto_launch_numbered_payload_bin();
	auto_launch_numbered_payload_bin_folder();
	auto_launch_update_bin();
	
	
	gfx_puts(&gfx_con, "\nPlease check SD card\n\nNot found:\n\npayload.bin\npayload1.bin\n");
	msleep(1500);
	
	power_off();
}
