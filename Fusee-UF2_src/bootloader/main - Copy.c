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

#include "config/config.h"
#include "gfx/di.h"
#include "gfx/gfx.h"
#include "gfx/logos.h"
#include "gfx/tui.h"
#include "hos/hos.h"
#include "hos/sept.h"
#include "ianos/ianos.h"
#include "libs/compr/blz.h"
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

#include "frontend/fe_emmc_tools.h"
#include "frontend/fe_tools.h"
#include "frontend/fe_info.h"

//TODO: ugly.
sdmmc_t sd_sdmmc;
sdmmc_storage_t sd_storage;
FATFS sd_fs;
static bool sd_mounted;
bool septactive;
u8 folder;


#ifdef MENU_LOGO_ENABLE
u8 *Kc_MENU_LOGO;
#endif //MENU_LOGO_ENABLE

hekate_config h_cfg;
boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;
const volatile ipl_ver_meta_t __attribute__((section ("._ipl_version"))) ipl_ver = {
	.magic = BL_MAGIC,
	.version = (BL_VER_MJ + '0') | ((BL_VER_MN + '0') << 8) | ((BL_VER_HF + '0') << 16),
	.rsvd0 = 0,
	.rsvd1 = 0
};

bool sd_mount()
{
	if (sd_mounted)
		return true;

	if (!sdmmc_storage_init_sd(&sd_storage, &sd_sdmmc, SDMMC_1, SDMMC_BUS_WIDTH_4, 11))
	{
		EPRINTF("Failed to init SD card.\nMake sure that it is inserted.\nOr that SD reader is properly seated!");
	}
	else
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

void *sd_file_read(char *path)
{
	FIL fp;
	if (f_open(&fp, path, FA_READ) != FR_OK)
		return NULL;

	u32 size = f_size(&fp);
	void *buf = malloc(size);

	u8 *ptr = buf;
	while (size > 0)
	{
		u32 rsize = MIN(size, 512 * 8192);
		if (f_read(&fp, ptr, rsize, NULL) != FR_OK)
		{
			free(buf);
			f_close(&fp);

			return NULL;
		}

		ptr += rsize;
		size -= rsize;
	}

	f_close(&fp);

	return buf;
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

	f_sync(&fp);
	f_write(&fp, buf, size, NULL);
	f_close(&fp);

	return 0;
}

void emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage)
{
	sdmmc_storage_t storage2;
	bool init_done = false;
	memcpy(path, "backup", 7);
	f_mkdir(path);

	u32 sub_dir_len = strlen(sub_dir);   // Can be a null-terminator.
	u32 filename_len = strlen(filename); // Can be a null-terminator.
	
	if (folder == 1){
		memcpy(path + strlen(path), "/", 2);
		memcpy(path + strlen(path), "BACKUP_1", 9);
		f_mkdir(path);
	} else if (folder == 2){
		memcpy(path + strlen(path), "/", 2);
		memcpy(path + strlen(path), "BACKUP_2", 9);
		f_mkdir(path);
	} else if (folder == 3){
		memcpy(path + strlen(path), "/", 2);
		memcpy(path + strlen(path), "BACKUP_3", 9);
		f_mkdir(path);
	} else if (folder == 4){
		memcpy(path + strlen(path), "/", 2);
		memcpy(path + strlen(path), "BACKUP_4", 9);
		f_mkdir(path);
	} else if (folder == 5){
		memcpy(path + strlen(path), "/", 2);
		memcpy(path + strlen(path), "BACKUP_5", 9);
		f_mkdir(path);
	}
	
	
	memcpy(path + strlen(path), sub_dir, sub_dir_len + 1);
	if (sub_dir_len)
		f_mkdir(path);
	memcpy(path + strlen(path), "/", 2);
	memcpy(path + strlen(path), filename, filename_len + 1);

	if (init_done)
		sdmmc_storage_end(&storage2);
}

void emmc_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage)
{
	sdmmc_storage_t storage2;
	bool init_done = false;
	
	memcpy(path, "backup", 7);
	f_mkdir(path);
	memcpy(path + strlen(path), "/", 2);
	memcpy(path + strlen(path), "Emergency_Emmc_Fix", 19);
	f_mkdir(path);
	

	u32 sub_dir_len = strlen(sub_dir);   // Can be a null-terminator.
	u32 filename_len = strlen(filename); // Can be a null-terminator.
	
	
	
	memcpy(path + strlen(path), "/", 2);
	f_mkdir(path);
	memcpy(path + strlen(path), sub_dir, sub_dir_len + 1);
	if (sub_dir_len)
		f_mkdir(path);
	memcpy(path + strlen(path), filename, filename_len + 1);

	if (init_done)
		sdmmc_storage_end(&storage2);
}

void emmc_screenshot_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage)
{
	sdmmc_storage_t storage2;
	bool init_done = false;
	
	memcpy(path, "screenshots", 12);
	f_mkdir(path);
	

	u32 sub_dir_len = strlen(sub_dir);   // Can be a null-terminator.
	u32 filename_len = strlen(filename); // Can be a null-terminator.
	
	
	
	memcpy(path + strlen(path), "/", 2);
	f_mkdir(path);
	memcpy(path + strlen(path), sub_dir, sub_dir_len + 1);
	if (sub_dir_len)
		f_mkdir(path);
	memcpy(path + strlen(path), filename, filename_len + 1);

	if (init_done)
		sdmmc_storage_end(&storage2);
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
#define COREBOOT_ADDR       (0xD0000000 - 0x100000)
#define CBFS_DRAM_EN_ADDR   0x4003e000
#define  CBFS_DRAM_MAGIC    0x4452414D // "DRAM"

void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size)
{
	memcpy((u8 *)payload_src, (u8 *)IPL_LOAD_ADDR, PATCHED_RELOC_SZ);

	volatile reloc_meta_t *relocator = (reloc_meta_t *)(payload_src + RELOC_META_OFF);

	relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
	relocator->stack = PATCHED_RELOC_STACK;
	relocator->end   = payload_dst + payload_size;
	relocator->ep    = payload_dst;

	if (payload_size == 0x7000)
	{
		memcpy((u8 *)(payload_src + ALIGN(PATCHED_RELOC_SZ, 0x10)), (u8 *)COREBOOT_ADDR, 0x7000); //Bootblock
		*(vu32 *)CBFS_DRAM_EN_ADDR = CBFS_DRAM_MAGIC;
	}
}

bool is_ipl_updated(void *buf)
{
	ipl_ver_meta_t *update_ft = (ipl_ver_meta_t *)(buf + PATCHED_RELOC_SZ + sizeof(boot_cfg_t));	

	if (update_ft->magic == ipl_ver.magic)
	{
		if (byte_swap_32(update_ft->version) <= byte_swap_32(ipl_ver.version))
			return true;
		return false;
		
	}
	else
		return true;
}

int launch_payload(char *path, bool update)
{
	if (!update)
		gfx_clear_black( 0x00);
	gfx_con_setpos( 0, 0);
	if (!path)
		return 1;

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

		if (size < 0x30000)
			buf = (void *)RCM_PAYLOAD_ADDR;
		else
			buf = (void *)COREBOOT_ADDR;

		if (f_read(&fp, buf, size, NULL))
		{
			f_close(&fp);
			sd_unmount();

			return 1;
		}

		f_close(&fp);
		free(path);

		if (update && is_ipl_updated(buf))
			return 1;

		sd_unmount();

		if (size < 0x30000)
		{
			if (update)
				memcpy((u8 *)(RCM_PAYLOAD_ADDR + PATCHED_RELOC_SZ), &b_cfg, sizeof(boot_cfg_t)); // Transfer boot cfg.
			else
				reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

			reconfig_hw_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
		}
		else
		{
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
			reconfig_hw_workaround(true, 0);
		}

		void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;
		void (*update_ptr)() = (void *)RCM_PAYLOAD_ADDR;

		// Launch our payload.
		if (!update)
			(*ext_payload_ptr)();
		else
		{
			EMC(EMC_SCRATCH0) |= EMC_HEKA_UPD;
			(*update_ptr)();
		}
	}

	return 1;
}

void auto_launch_lockpick()
{
	FIL fp;
	
	if (sd_mount())
		{
			if (f_open(&fp, "sept/payload.bak", FA_READ) == FR_OK)
			{
				f_close(&fp);
				septactive = true;
				return;
			}
			else
			{
			septactive = false;
			return;
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

void auto_launch_numbered_payload()
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
	
void auto_launch_update()
{
	if (EMC(EMC_SCRATCH0) & EMC_HEKA_UPD)
		EMC(EMC_SCRATCH0) &= ~EMC_HEKA_UPD;
	else if (sd_mount())
	{
		if (!f_stat("bootloader/update.bin", NULL))
			launch_payload("bootloader/update.bin", true);
	}
}

void launch_tools(u8 type)
{
	u8 max_entries = 61;
	char *filelist = NULL;
	char *file_sec = NULL;
	char *dir = NULL;

	ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 3));

	gfx_clear_black( 0x00);
	gfx_con_setpos( 0, 0);

	if (sd_mount())
	{
		dir = (char *)malloc(256);

		if (type == 0)
			memcpy(dir, "bootloader/payloads", 20);
		else if (type == 1)
			memcpy(dir, "bootloader/libtools", 20);
		else if (type == 2)
			memcpy(dir, "", 1);

		filelist = dirlist(dir, NULL, false);

		u32 i = 0;

		if (filelist)
		{
			// Build configuration menu.
			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			ments[1].type = MENT_CHGLINE;

			while (true)
			{
				if (i > max_entries || !filelist[i * 256])
					break;
				ments[i + 2].type = INI_CHOICE;
				ments[i + 2].caption = &filelist[i * 256];
				ments[i + 2].data = &filelist[i * 256];

				i++;
			}
		}
					
		if (i > 0)
		{
			memset(&ments[i + 2], 0, sizeof(ment_t));
			menu_t menu = {
					ments,
					"Choose a file to launch", 0, 0
			};
			
			file_sec = (char *)tui_do_menu( &menu);

			if (!file_sec)
			{
				free(ments);
				free(dir);
				free(filelist);
				sd_unmount();
				return;
			}
		}
		else
			EPRINTF("No payloads or modules found.");

		free(ments);
		free(filelist);
	}
	else
	{
		free(ments);
		goto out;
	}
		

	if (file_sec)
	{
		memcpy(dir + strlen(dir), "/", 2);
		memcpy(dir + strlen(dir), file_sec, strlen(file_sec) + 1);

		if (type == 0 || type == 2)
		{
			if (launch_payload(dir, false))
			{
				EPRINTF("Failed to launch payload.");
				free(dir);
			}
		}
		else
			ianos_loader(true, dir, DRAM_LIB, NULL);
	}

out:
	sd_unmount();
	free(dir);

	btn_wait();
}

void launch_tools_payload() { launch_tools(0); }
void launch_tools_module() { launch_tools(1); }
void launch_tools_payload_root() { launch_tools(2); }

void ini_list_launcher()
{
	u8 max_entries = 61;
	char *payload_path = NULL;

	ini_sec_t *cfg_tmp = NULL;
	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_list_sections);

	gfx_clear_black( 0x00);
	gfx_con_setpos( 0, 0);

	if (sd_mount())
	{
		if (ini_parse(&ini_list_sections, "bootloader/ini", true))
		{
			// Build configuration menu.
			ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 3));
			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			ments[1].type = MENT_CHGLINE;

			u32 i = 2;
			LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, &ini_list_sections, link)
			{
				if (!strcmp(ini_sec->name, "config") ||
					ini_sec->type == INI_COMMENT || ini_sec->type == INI_NEWLINE)
					continue;
				ments[i].type = ini_sec->type;
				ments[i].caption = ini_sec->name;
				ments[i].data = ini_sec;
				if (ini_sec->type == MENT_CAPTION)
					ments[i].color = ini_sec->color;
				i++;

				if ((i - 1) > max_entries)
					break;
			}
			if (i > 2)
			{
				memset(&ments[i], 0, sizeof(ment_t));
				menu_t menu = {
					ments, "Launch ini configurations", 0, 0
				};

				cfg_tmp = (ini_sec_t *)tui_do_menu( &menu);

				if (cfg_tmp)
				{
					u32 non_cfg = 1;
					for (int j = 2; j < i; j++)
					{
						if (ments[j].type != INI_CHOICE)
							non_cfg++;

						if (ments[j].data == cfg_tmp)
						{
							b_cfg.boot_cfg = BOOT_CFG_FROM_LAUNCH;
							b_cfg.autoboot = j - non_cfg;
							b_cfg.autoboot_list = 1;

							break;
						}
					}
				}

				payload_path = ini_check_payload_section(cfg_tmp);

				if (cfg_tmp && !payload_path)
					check_sept();

				cfg_sec = ini_clone_section(cfg_tmp);

				if (!cfg_sec)
				{
					free(ments);
					ini_free(&ini_list_sections);

					return;
				}
			}
			else
				EPRINTF("No extra configs found.");
			free(ments);
			ini_free(&ini_list_sections);
		}
		else
			EPRINTF("Could not find any ini\nin bootloader/ini!");
	}

	if (!cfg_sec)
		goto out;

	if (payload_path)
	{
		ini_free_section(cfg_sec);
		if (launch_payload(payload_path, false))
		{
			EPRINTF("Failed to launch payload.");
			free(payload_path);
		}
	}
	else if (!hos_launch(cfg_sec))
	{
		EPRINTF("Failed to launch firmware.");
		btn_wait();
	}

out:
	ini_free_section(cfg_sec);

	btn_wait();
}

void launch_firmware()
{
	u8 max_entries = 61;
	char *payload_path = NULL;

	ini_sec_t *cfg_tmp = NULL;
	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_sections);

	gfx_clear_black( 0x00);
	gfx_con_setpos( 0, 0);

	if (sd_mount())
	{
		if (ini_parse(&ini_sections, "bootloader/hekate_ipl.ini", false))
		{
			// Build configuration menu.
			ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 1));
			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			u32 i = 1;
			LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, &ini_sections, link)
			{
				if (!strcmp(ini_sec->name, "config") ||
					ini_sec->type == INI_COMMENT || ini_sec->type == INI_NEWLINE)
					continue;
				ments[i].type = ini_sec->type;
				ments[i].caption = ini_sec->name;
				ments[i].data = ini_sec;
				if (ini_sec->type == MENT_CAPTION)
					ments[i].color = ini_sec->color;
				i++;

				if (i > max_entries)
					break;
			}
			if (i < 6)
			{
				ments[i].type = MENT_CAPTION;
				ments[i].caption = "No main configs found...";
				ments[i].color = 0xFFFFDD00;
				i++;
			}
			memset(&ments[i], 0, sizeof(ment_t));
			menu_t menu = {
				ments, "Launch configurations", 0, 0
			};

			cfg_tmp = (ini_sec_t *)tui_do_menu( &menu);

			if (cfg_tmp)
			{
				u8 non_cfg = 4;
				for (int j = 5; j < i; j++)
				{
					if (ments[j].type != INI_CHOICE)
						non_cfg++;
					if (ments[j].data == cfg_tmp)
					{
						b_cfg.boot_cfg = BOOT_CFG_FROM_LAUNCH;
						b_cfg.autoboot = j - non_cfg;
						b_cfg.autoboot_list = 0;

						break;
					}
				}
			}

			payload_path = ini_check_payload_section(cfg_tmp);

			if (cfg_tmp && !payload_path)
				check_sept();

			cfg_sec = ini_clone_section(cfg_tmp);
			if (!cfg_sec)
			{
				free(ments);
				ini_free(&ini_sections);
				sd_unmount();
				return;
			}

			free(ments);
			ini_free(&ini_sections);
		}
		else
			EPRINTF("Could not open 'bootloader/hekate_ipl.ini'.\nMake sure it exists!");
	}

	if (!cfg_sec)
	{
		gfx_puts( "\nPress POWER to Continue.\nPress VOL to go to the menu.\n\n");
		gfx_printf( "\nUsing default launch configuration...\n\n\n");

		u32 btn = btn_wait();
		if (!(btn & BTN_POWER))
			goto out;
	}

	if (payload_path)
	{
		ini_free_section(cfg_sec);
		if (launch_payload(payload_path, false))
		{
			EPRINTF("Failed to launch payload.");
			free(payload_path);
		}
	}
	else if (!hos_launch(cfg_sec))
		EPRINTF("Failed to launch firmware.");

out:
	ini_free_section(cfg_sec);
	sd_unmount();

	btn_wait();
}

void auto_launch_firmware()
{
	if (!h_cfg.sept_run)
		auto_launch_update();

	u8 *BOOTLOGO = NULL;
	char *payload_path = NULL;
	u32 btn = 0;

	struct _bmp_data
	{
		u32 size;
		u32 size_x;
		u32 size_y;
		u32 offset;
		u32 pos_x;
		u32 pos_y;
	};

	struct _bmp_data bmpData;
	bool bootlogoFound = false;
	char *bootlogoCustomEntry = NULL;

	if (!(b_cfg.boot_cfg & BOOT_CFG_FROM_LAUNCH))
		gfx_con.mute = true;

	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_sections);
	LIST_INIT(ini_list_sections);

	if (sd_mount())
	{
		if (f_stat("bootloader/hekate_ipl.ini", NULL))
			create_config_entry();

		if (ini_parse(&ini_sections, "bootloader/hekate_ipl.ini", false))
		{
			u32 configEntry = 0;
			u32 boot_entry_id = 0;

			// Load configuration.
			LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, &ini_sections, link)
			{
				// Skip other ini entries for autoboot.
				if (ini_sec->type == INI_CHOICE)
				{
					if (!strcmp(ini_sec->name, "config"))
					{
						configEntry = 1;
						LIST_FOREACH_ENTRY(ini_kv_t, kv, &ini_sec->kvs, link)
						{
							if (!strcmp("autoboot", kv->key))
								h_cfg.autoboot = atoi(kv->val);
							else if (!strcmp("autoboot_list", kv->key))
								h_cfg.autoboot_list = atoi(kv->val);
							else if (!strcmp("bootwait", kv->key))
								h_cfg.bootwait = atoi(kv->val);
							else if (!strcmp("verification", kv->key))
								h_cfg.verification = atoi(kv->val);
							else if (!strcmp("backlight", kv->key))
								h_cfg.backlight = atoi(kv->val);
							else if (!strcmp("autohosoff", kv->key))
								h_cfg.autohosoff = atoi(kv->val);
							else if (!strcmp("autonogc", kv->key))
								h_cfg.autonogc = atoi(kv->val);
						}
						boot_entry_id++;

						// Override autoboot, otherwise save it for a possbile sept run.
						if (b_cfg.boot_cfg & BOOT_CFG_AUTOBOOT_EN)
						{
							h_cfg.autoboot = b_cfg.autoboot;
							h_cfg.autoboot_list = b_cfg.autoboot_list;
						}
						else
						{
							b_cfg.autoboot = h_cfg.autoboot;
							b_cfg.autoboot_list = h_cfg.autoboot_list;
						}

						continue;
					}

					if (h_cfg.autoboot == boot_entry_id && configEntry)
					{
						cfg_sec = ini_clone_section(ini_sec);
						LIST_FOREACH_ENTRY(ini_kv_t, kv, &cfg_sec->kvs, link)
						{
							if (!strcmp("logopath", kv->key))
								bootlogoCustomEntry = kv->val;
						}
						break;
					}
					boot_entry_id++;
				}
			}

			if (h_cfg.autohosoff && !(b_cfg.boot_cfg & BOOT_CFG_AUTOBOOT_EN))
				check_power_off_from_hos();

			if (h_cfg.autoboot_list)
			{
				ini_free_section(cfg_sec);
				boot_entry_id = 1;
				bootlogoCustomEntry = NULL;

				if (ini_parse(&ini_list_sections, "bootloader/ini", true))
				{
					LIST_FOREACH_ENTRY(ini_sec_t, ini_sec_list, &ini_list_sections, link)
					{
						if (ini_sec_list->type == INI_CHOICE)
						{
							if (!strcmp(ini_sec_list->name, "config"))
								continue;

							if (h_cfg.autoboot == boot_entry_id)
							{
								cfg_sec = ini_clone_section(ini_sec_list);
								LIST_FOREACH_ENTRY(ini_kv_t, kv, &cfg_sec->kvs, link)
								{
									if (!strcmp("logopath", kv->key))
										bootlogoCustomEntry = kv->val;
								}
								break;
							}
							boot_entry_id++;
						}
						
					}

				}

			}

			// Add missing configuration entry.
			if (!configEntry)
				create_config_entry();

			if (!h_cfg.autoboot)
				goto out; // Auto boot is disabled.

			if (!cfg_sec)
				goto out; // No configurations.
		}
		else
			goto out; // Can't load hekate_ipl.ini.
	}
	else
		goto out;

	u8 *bitmap = NULL;
	if (!(b_cfg.boot_cfg & BOOT_CFG_FROM_LAUNCH) && h_cfg.bootwait)
	{
		if (bootlogoCustomEntry) // Check if user set custom logo path at the boot entry.
			bitmap = (u8 *)sd_file_read(bootlogoCustomEntry);

		if (!bitmap) // Custom entry bootlogo not found, trying default custom one.
			bitmap = (u8 *)sd_file_read("bootloader/bootlogo.bmp");

		if (bitmap)
		{
			// Get values manually to avoid unaligned access.
			bmpData.size = bitmap[2] | bitmap[3] << 8 |
				bitmap[4] << 16 | bitmap[5] << 24;
			bmpData.offset = bitmap[10] | bitmap[11] << 8 |
				bitmap[12] << 16 | bitmap[13] << 24;
			bmpData.size_x = bitmap[18] | bitmap[19] << 8 |
				bitmap[20] << 16 | bitmap[21] << 24;
			bmpData.size_y = bitmap[22] | bitmap[23] << 8 |
				bitmap[24] << 16 | bitmap[25] << 24;
			// Sanity check.
			if (bitmap[0] == 'B' &&
				bitmap[1] == 'M' &&
				bitmap[28] == 32 && // Only 32 bit BMPs allowed.
				bmpData.size_x <= 720 &&
				bmpData.size_y <= 1280)
			{
				if ((bmpData.size - bmpData.offset) <= 0x400000)
				{
					// Avoid unaligned access from BM 2-byte MAGIC and remove header.
					BOOTLOGO = (u8 *)malloc(0x400000);
					memcpy(BOOTLOGO, bitmap + bmpData.offset, bmpData.size - bmpData.offset);
					free(bitmap);
					// Center logo if res < 720x1280.
					bmpData.pos_x = (720  - bmpData.size_x) >> 1;
					bmpData.pos_y = (1280 - bmpData.size_y) >> 1;
					// Get background color from 1st pixel.
					if (bmpData.size_x < 720 || bmpData.size_y < 1280)
						gfx_clear_color( *(u32 *)BOOTLOGO);

					bootlogoFound = true;
				}
			}
			else
				free(bitmap);
		}

		// Render boot logo.
		if (bootlogoFound)
		{
			gfx_render_bmp_argb( (u32 *)BOOTLOGO, bmpData.size_x, bmpData.size_y,
				bmpData.pos_x, bmpData.pos_y);
		}
		else
			
		free(BOOTLOGO);
	}

	ini_free(&ini_sections);
	if (h_cfg.autoboot_list)
		ini_free(&ini_list_sections);

	if (h_cfg.sept_run)
		display_backlight_brightness(h_cfg.backlight, 0);
	else if (h_cfg.bootwait)
		display_backlight_brightness(h_cfg.backlight, 1000);

	// Wait before booting. If VOL- is pressed go into bootloader menu.
	if (!h_cfg.sept_run)
	{
		btn = btn_wait_timeout(h_cfg.bootwait * 1000, BTN_VOL_DOWN);

		if (btn & BTN_VOL_DOWN)
			goto out;
	}

	payload_path = ini_check_payload_section(cfg_sec);

	if (payload_path)
	{
		ini_free_section(cfg_sec);
		if (launch_payload(payload_path, false))
			free(payload_path);
	}
	else
	{
		check_sept();
		hos_launch(cfg_sec);
	}

out:
	ini_free(&ini_sections);
	if (h_cfg.autoboot_list)
		ini_free(&ini_list_sections);
	ini_free_section(cfg_sec);

	sd_unmount();
	gfx_con.mute = false;

	b_cfg.boot_cfg &= ~(BOOT_CFG_AUTOBOOT_EN | BOOT_CFG_FROM_LAUNCH);
}

void about()
{	//large blank areas are for additional external information
	static const char credits[] =
		"\nhekate     (C) 2018 naehrwert, st4rk\n\n"
		"CTCaer mod (C) 2019 CTCaer\n"
		"Lockpick (C) 2019 Shchmue\n"
		"Initial SAMD-Fusee implementation Atlas44\n"
		"Switchboot brought to you by Mattytrog\n"
		" ___________________________________________\n\n"
		"Open source and free packages used:\n\n"
		" - FatFs R0.13b,\n"
		"   Copyright (C) 2018, ChaN\n\n"
		" - bcl-1.2.0,\n"
		"   Copyright (C) 2003-2006, Marcus Geelnard\n\n"
		" - Atmosphere (SE sha256, prc id patches),\n"
		"   Copyright (C) 2018, Atmosphere-NX\n\n"
		" - elfload,\n"
		"   Copyright (C) 2014, Owen Shepherd\n"
		"   Copyright (C) 2018, M4xw\n"
		" ___________________________________________\n\n";
		
	gfx_clear_black( 0x00);
	gfx_con_setpos( 0, 0);

	gfx_printf( credits, 0xFF00CCFF, 0xFFFFFFFF);
	gfx_con.fntsz = 8;

	btn_wait();
}

ment_t ment_options[] = {
	MDEF_BACK(),
	MDEF_CAPTION("-- hekate_ipl.ini settings --", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Auto boot", config_autoboot),

	MDEF_HANDLER("Boot time delay", config_bootdelay),

	MDEF_HANDLER("Auto NoGC", config_nogc),

	MDEF_HANDLER("Auto HOS power off", config_auto_hos_poweroff),

	MDEF_HANDLER("Backlight", config_backlight),
	MDEF_END()
};

menu_t menu_options = {
	ment_options,
	"Launch Options", 0, 0
};

ment_t ment_restore[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("------ Quick EMMC Fix --------", 0xFF00FF00),
	MDEF_HANDLER("Restore BOOT0/1 + Prodinfo (EMMC Rebuild)", restore_emmc_quick),
	MDEF_HANDLER("Restore Prodinfo ONLY(EMMC Rebuild)", restore_emmc_quick_prodinfo),
	MDEF_CHGLINE(),
	MDEF_CAPTION("------ Full --------", 0xFF00FF00),
	MDEF_HANDLER("Restore eMMC BOOT0/1", restore_emmc_boot),
	MDEF_HANDLER("Restore eMMC RAW GPP (exFAT only)", restore_emmc_rawnand),
	MDEF_HANDLER("Restore eMMC RAW GPP", restore_emmc_rawnand),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- GPP Partitions --", 0xFF00FF00),
	MDEF_HANDLER("Restore GPP partitions", restore_emmc_gpp_parts),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Dangerous --", 0xFFFF0000),
	MDEF_HANDLER("Restore BOOT0/1 without size check", restore_emmc_boot_noszchk),
	MDEF_END()
};
menu_t menu_restore = {
	ment_restore,
	"Restore Options", 0, 0
};

ment_t ment_backup[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("------ Quick EMMC fix--------", 0xFF00FF00),
	MDEF_HANDLER("Backup BOOT0/1 + Prodinfo (EMMC Rebuild)", dump_emmc_quick),
	MDEF_CHGLINE(),
	MDEF_CAPTION("------ Full --------", 0xFF00FF00),
	MDEF_HANDLER("Backup eMMC BOOT0/1", dump_emmc_boot),
	MDEF_HANDLER("Backup eMMC RAW GPP", dump_emmc_rawnand),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- GPP Partitions --", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Backup eMMC SYS", dump_emmc_system),
	MDEF_HANDLER("Backup eMMC USER", dump_emmc_user),
	MDEF_END()
};

menu_t menu_backup = {
	ment_backup,
	"Backup Options", 0, 0
};

ment_t ment_backup_choose_folder[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("BOOT0, BOOT1 will be saved to", 0xFF00FF00),
	MDEF_CAPTION("BOOTS subfolder if selected", 0xFF00FF00),
	MDEF_CAPTION("-----------------------------", 0xFF444444),
	MDEF_CHGLINE(),
	MDEF_HANDLER("BACKUP FOLDER 1 (BACKUP_1)", select_folder_1),
	MDEF_HANDLER("BACKUP FOLDER 2 (BACKUP_2)", select_folder_2),
	MDEF_HANDLER("BACKUP FOLDER 3 (BACKUP_3)", select_folder_3),
	MDEF_HANDLER("BACKUP FOLDER 4 (BACKUP_4)", select_folder_4),
	MDEF_HANDLER("BACKUP FOLDER 5 (BACKUP_5)", select_folder_5),
	MDEF_CHGLINE(),
	MDEF_END()
};

menu_t menu_backup_choose_folder = {
	ment_backup_choose_folder,
	"Choose a folder to backup to", 0, 0
};

ment_t ment_tools[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Device Keys --", 0xFF00FF00),
	MDEF_HANDLER("Dump all keys", dump_all_keys),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Backup & Restore --", 0xFF00FF00),
	MDEF_MENU("Choose folder for backup / restore", &menu_backup_choose_folder),
	MDEF_MENU("Backup", &menu_backup),
	MDEF_MENU("Restore", &menu_restore),
	MDEF_HANDLER("Verification options", config_verification),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-------- Misc --------", 0xFF00FF00),
	MDEF_HANDLER("Dump package1/2", dump_packages12),
	MDEF_HANDLER("Fix battery de-sync", fix_battery_desync),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-------- Info --------", 0xFF00FF00),
	MDEF_HANDLER("Print fuse info", print_fuseinfo),
	MDEF_HANDLER("Print eMMC info", print_mmc_info),
	MDEF_HANDLER("Print SD Card info", print_sdcard_info),
	//MDEF_HANDLER("Print battery info", print_battery_info),
	//MDEF_HANDLER("Fix archive bit (except Nintendo)", fix_sd_all_attr),
	//MDEF_HANDLER("Fix archive bit (Nintendo only)", fix_sd_nin_attr),
	//MDEF_HANDLER("Fix fuel gauge configuration", fix_fuel_gauge_configuration),
	//MDEF_HANDLER("Reset all battery cfg", reset_pmic_fuel_gauge_charger_config),
	//MDEF_HANDLER("Minerva", minerva), // Uncomment for testing Minerva Training Cell
	MDEF_CHGLINE(),
	MDEF_CAPTION("------ Dangerous -----", 0xFFFF0000),
	MDEF_HANDLER("AutoRCM", menu_autorcm),
	MDEF_END()
};

menu_t menu_tools = {
	ment_tools,
	"Tools / Info", 0, 0
};

ment_t ment_launch[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Payload Launch Menu --", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Launch payload from payloads folder", launch_tools_payload),
	MDEF_HANDLER("Launch payload from SD card root", launch_tools_payload_root),
	MDEF_END()
};

menu_t menu_launch = {
	ment_launch,
	"Choose to launch", 0, 0
};

ment_t ment_top[] = {
	MDEF_CHGLINE(),
	MDEF_HANDLER("Launch Configurations", launch_firmware),
	MDEF_HANDLER("More Configurations", ini_list_launcher),
	
	MDEF_CAPTION("---------------------", 0xFF444444),
	MDEF_MENU("Launch Payload", &menu_launch),
	MDEF_CAPTION("---------------------", 0xFF444444),
	MDEF_MENU("Options", &menu_options),
	MDEF_CAPTION("---------------------", 0xFF444444),
	MDEF_MENU("Tools", &menu_tools),
	MDEF_CAPTION("---------------------", 0xFF444444),
	MDEF_HANDLER("Reboot (Normal)", reboot_normal),
	MDEF_HANDLER("Reboot (RCM)", reboot_rcm),
	MDEF_HANDLER("Power off", power_off),
	MDEF_CAPTION("---------------------", 0xFF444444),
	MDEF_HANDLER("About", about),
	MDEF_CAPTION("---------------------", 0xFF444444),
	MDEF_CHGLINE(),
	MDEF_CAPTION("Modchip Payload Setting:       -", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_CAPTION("Modchip Mode Setting:          -", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_CAPTION("USB Strap detected:            -", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_CAPTION("VOL+ Strap detected:           -", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_CAPTION("Joycon Strap Detected:         -", 0xFF00FF00),
	MDEF_END()
};
menu_t menu_top = {
	ment_top,
	"Switchboot v1.00 - Hekate CTCaer v4.9.1", 0, 0
};

#define IPL_STACK_TOP  0x90010000
#define IPL_HEAP_START 0x90020000
#define IPL_HEAP_END   0xB5000000

extern void pivot_stack(u32 stack_top);
extern bool septactive;

void ipl_main()
{
	// Do initial HW configuration. This is compatible with consecutive reruns without a reset.
	config_hw();

	//Pivot the stack so we have enough space.
	pivot_stack(IPL_STACK_TOP);

	//Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
	heap_init(IPL_HEAP_START);

#ifdef DEBUG_UART_PORT
	uart_send(DEBUG_UART_PORT, (u8 *)"Hekate: Hello!\r\n", 16);
	uart_wait_idle(DEBUG_UART_PORT, UART_TX_IDLE);
#endif

	// Set bootloader's default configuration.
	set_default_configuration();

	// Save sdram lp0 config.
	if (ianos_loader(true, "bootloader/sys/libsys_lp0.bso", DRAM_LIB, (void *)sdram_get_params_patched()))
		h_cfg.errors |= ERR_LIBSYS_LP0;

	display_init();

	u32 *fb = display_init_framebuffer();
	gfx_init_ctxt( fb, 720, 1280, 720);

#ifdef MENU_LOGO_ENABLE
	Kc_MENU_LOGO = (u8 *)malloc(ALIGN(SZ_MENU_LOGO, 0x1000));
	blz_uncompress_srcdest(Kc_MENU_LOGO_blz, SZ_MENU_LOGO_BLZ, Kc_MENU_LOGO, SZ_MENU_LOGO);
#endif

	gfx_con_init( &gfx_ctxt);

	display_backlight_pwm_init();
	u32 btn = btn_wait_timeout(1000, BTN_VOL_DOWN);
	if (btn & BTN_VOL_DOWN){
		while (true)
		tui_do_menu( &menu_top);

	} else {
		auto_launch_lockpick();
		if (!septactive){
		auto_launch_payload_bin();
		auto_launch_numbered_payload();
		auto_launch_firmware();
		} else if (septactive){
			dump_all_keys();
		}
		msleep(1000);
		while (true)
		tui_do_menu( &menu_top);
	}
}
