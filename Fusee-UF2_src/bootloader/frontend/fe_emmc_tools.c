/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 Rajko Stojadinovic
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

#include "fe_emmc_tools.h"
#include "../config/config.h"
#include "../gfx/gfx.h"
#include "../gfx/tui.h"
#include "../libs/fatfs/ff.h"
#include "../mem/heap.h"
#include "../sec/se.h"
#include "../storage/nx_emmc.h"
#include "../storage/sdmmc.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "../utils/dirlist.h"

#define EMMC_BUF_ALIGNED 0xB5000000
#define SDXC_BUF_ALIGNED 0xB6000000
#define MIXD_BUF_ALIGNED 0xB7000000

#define NUM_SECTORS_PER_ITER 8192 // 4MB Cache.

extern sdmmc_t sd_sdmmc;
extern sdmmc_storage_t sd_storage;
extern FATFS sd_fs;
extern hekate_config h_cfg;

bool noszchk;
extern bool sd_mount();
extern void sd_unmount();
extern void auto_launch_dummy_payload();
extern void menu_autorcm();
extern void emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage);
extern int  sd_save_to_file(void *buf, u32 size, const char *filename);
u8 folder = 1;
u8 oldfolder;

void select_folder_1(){folder = 1; return;}
void select_folder_2(){folder = 2; return;}
void select_folder_3(){folder = 3; return;}
void select_folder_4(){folder = 4; return;}
void select_folder_5(){folder = 5; return;}
void select_folder_6(){folder = 6; return;}
void select_folder_7(){folder = 7; return;}
void select_folder_8(){folder = 8; return;}

static int _dump_emmc_verify(sdmmc_storage_t *storage, u32 lba_curr, char *outFilename, emmc_part_t *part)
{
	FIL fp;
	u8 sparseShouldVerify = 4;
	u32 btn = 0;
	u32 prevPct = 200;
	u32 sdFileSector = 0;
	int res = 0;

	u8 hashEm[0x20];
	u8 hashSd[0x20];

	if (f_open(&fp, outFilename, FA_READ) == FR_OK)
	{
		u32 totalSectorsVer = (u32)((u64)f_size(&fp) >> (u64)9);

		u8 *bufEm = (u8 *)EMMC_BUF_ALIGNED;
		u8 *bufSd = (u8 *)SDXC_BUF_ALIGNED;

		u32 pct = (u64)((u64)(lba_curr - part->lba_start) * 100u) / (u64)(part->lba_end - part->lba_start);
		tui_pbar(0, gfx_con.y, pct, 0xFF00FF00, 0xFF155500);

		u32 num = 0;
		while (totalSectorsVer > 0)
		{
			num = MIN(totalSectorsVer, NUM_SECTORS_PER_ITER);
			
			// Check every time or every 4.
			// Every 4 protects from fake sd, sector corruption and frequent I/O corruption.
			// Full provides all that, plus protection from extremely rare I/O corruption.
			if ((h_cfg.verification & 2) || !(sparseShouldVerify % 4))
			{
				if (!sdmmc_storage_read(storage, lba_curr, num, bufEm))
				{
					gfx_con.fntsz = 16;
					EPRINTFARGS("\nFailed to read %d blocks (@LBA %08X),\nfrom eMMC!\n\nVerification failed..\n",
						num, lba_curr);
	
					f_close(&fp);
					return 1;
				}
				f_lseek(&fp, (u64)sdFileSector << (u64)9);
				if (f_read(&fp, bufSd, num << 9, NULL))
				{
					gfx_con.fntsz = 16;
					EPRINTFARGS("\nFailed to read %d blocks (@LBA %08X),\nfrom sd card!\n\nVerification failed..\n", num, lba_curr);
	
					f_close(&fp);
					return 1;
				}

				se_calc_sha256(hashEm, bufEm, num << 9);
				se_calc_sha256(hashSd, bufSd, num << 9);
				res = memcmp(hashEm, hashSd, 0x10);

				if (res)
				{
					gfx_con.fntsz = 16;
					EPRINTFARGS("\nSD and eMMC data (@LBA %08X),\ndo not match!\n\nVerification failed..\n", lba_curr);
	
					f_close(&fp);
					return 1;
				}
			}

			pct = (u64)((u64)(lba_curr - part->lba_start) * 100u) / (u64)(part->lba_end - part->lba_start);
			if (pct != prevPct)
			{
				tui_pbar(0, gfx_con.y, pct, 0xFF00FF00, 0xFF155500);
				prevPct = pct;
			}

			lba_curr += num;
			totalSectorsVer -= num;
			sdFileSector += num;
			sparseShouldVerify++;

			btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
			if ((btn & BTN_VOL_DOWN) && (btn & BTN_VOL_UP))
			{
				gfx_con.fntsz = 16;
				WPRINTF("\n\nVerification was cancelled!");
				gfx_con.fntsz = 8;
				msleep(1000);

				f_close(&fp);

				return 0;
			}
		}
		f_close(&fp);

		tui_pbar(0, gfx_con.y, pct, 0xFFFFFFFF, 0xFF555555);

		return 0;
	}
	else
	{
		gfx_con.fntsz = 16;
		EPRINTF("\nFile not found or could not be loaded.\n\nVerification failed..\n");
		return 1;
	}
}

void restore_license_dat(){
	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);
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
		return;
	}
	//does it exist?
	if (f_stat("license.dat", NULL) == FR_OK){
		gfx_printf("\nSXOS license.dat exists!\n\nCancelled.");
		msleep(3000);
		return;
	} else {
		gfx_printf("\nPlease wait\n\n");
	sd_save_to_file(license_dat, sizeof(license_dat), "license.dat");
		gfx_printf("Success. Written %d bytes", sizeof(license_dat));
	sd_unmount();
	msleep(3000);
	}
	return;

}

void restore_septprimary_dat(){
	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);
	unsigned char dummy_bin[928] = {
	0x00, 0x00, 0xA0, 0xE3, 0x74, 0x10, 0x9F, 0xE5, 0x50, 0x00, 0x81, 0xE5,
	0xB4, 0x01, 0x81, 0xE5, 0x40, 0x08, 0x81, 0xE5, 0x68, 0x00, 0x9F, 0xE5,
	0x68, 0x10, 0x9F, 0xE5, 0x00, 0x00, 0x81, 0xE5, 0x04, 0x00, 0xA0, 0xE3,
	0x60, 0x10, 0x9F, 0xE5, 0x60, 0x20, 0x9F, 0xE5, 0x23, 0x00, 0x00, 0xEB,
	0x05, 0x00, 0xA0, 0xE3, 0x58, 0x10, 0x9F, 0xE5, 0x58, 0x20, 0x9F, 0xE5,
	0x1F, 0x00, 0x00, 0xEB, 0x06, 0x00, 0xA0, 0xE3, 0x50, 0x10, 0x9F, 0xE5,
	0x50, 0x20, 0x9F, 0xE5, 0x1B, 0x00, 0x00, 0xEB, 0x4C, 0x00, 0x9F, 0xE5,
	0x4C, 0x10, 0x9F, 0xE5, 0x00, 0x20, 0xA0, 0xE3, 0x48, 0x30, 0x9F, 0xE5,
	0x02, 0x40, 0x90, 0xE7, 0x02, 0x40, 0x81, 0xE7, 0x04, 0x20, 0x82, 0xE2,
	0x03, 0x00, 0x52, 0xE1, 0xFA, 0xFF, 0xFF, 0x1A, 0x34, 0x00, 0x9F, 0xE5,
	0x10, 0xFF, 0x2F, 0xE1, 0x1B, 0x00, 0x00, 0xEA, 0x00, 0xE4, 0x00, 0x70,
	0x30, 0x4C, 0x00, 0x40, 0x08, 0xF2, 0x00, 0x60, 0xDC, 0x15, 0x00, 0x00,
	0x20, 0xE0, 0x00, 0x00, 0xEE, 0x4A, 0x00, 0x00, 0x5B, 0xE0, 0x00, 0x00,
	0x88, 0x4E, 0x00, 0x00, 0x18, 0xE0, 0x00, 0x00, 0x00, 0xF1, 0x03, 0x40,
	0x40, 0x00, 0x01, 0x40, 0xC0, 0x02, 0x00, 0x00, 0x10, 0x10, 0x10, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x14, 0x30, 0x9F, 0xE5, 0x00, 0x01, 0xA0, 0xE1, 0xA1, 0x10, 0xA0, 0xE1,
	0x01, 0x18, 0xA0, 0xE1, 0x02, 0x10, 0x81, 0xE1, 0x00, 0x10, 0x83, 0xE7,
	0x1E, 0xFF, 0x2F, 0xE1, 0x00, 0xDC, 0x01, 0x60, 0xF8, 0xB5, 0xC0, 0x46,
	0xF8, 0xBC, 0x08, 0xBC, 0x9E, 0x46, 0x70, 0x47, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xC0, 0x9F, 0xE5, 0x1C, 0xFF, 0x2F, 0xE1, 0x05, 0x01, 0x01, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xD0, 0x1F, 0xE5, 0x9F, 0x00, 0x00, 0xEA,
	0x00, 0x00, 0x01, 0x40, 0x06, 0x48, 0x07, 0x4B, 0x10, 0xB5, 0x83, 0x42,
	0x04, 0xD0, 0x06, 0x4B, 0x00, 0x2B, 0x01, 0xD0, 0x00, 0xF0, 0x0A, 0xF8,
	0x10, 0xBC, 0x01, 0xBC, 0x00, 0x47, 0xC0, 0x46, 0xD8, 0x02, 0x01, 0x40,
	0xD8, 0x02, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x18, 0x47, 0xC0, 0x46,
	0x08, 0x48, 0x09, 0x49, 0x09, 0x1A, 0x89, 0x10, 0xCB, 0x0F, 0x59, 0x18,
	0x10, 0xB5, 0x49, 0x10, 0x04, 0xD0, 0x06, 0x4B, 0x00, 0x2B, 0x01, 0xD0,
	0x00, 0xF0, 0x0A, 0xF8, 0x10, 0xBC, 0x01, 0xBC, 0x00, 0x47, 0xC0, 0x46,
	0xD8, 0x02, 0x01, 0x40, 0xD8, 0x02, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00,
	0x18, 0x47, 0xC0, 0x46, 0x10, 0xB5, 0x08, 0x4C, 0x23, 0x78, 0x00, 0x2B,
	0x09, 0xD1, 0xFF, 0xF7, 0xC9, 0xFF, 0x06, 0x4B, 0x00, 0x2B, 0x02, 0xD0,
	0x05, 0x48, 0x00, 0xE0, 0x00, 0xBF, 0x01, 0x23, 0x23, 0x70, 0x10, 0xBC,
	0x01, 0xBC, 0x00, 0x47, 0xE0, 0x02, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00,
	0xD8, 0x02, 0x01, 0x40, 0x06, 0x4B, 0x10, 0xB5, 0x00, 0x2B, 0x03, 0xD0,
	0x05, 0x49, 0x06, 0x48, 0x00, 0xE0, 0x00, 0xBF, 0xFF, 0xF7, 0xC4, 0xFF,
	0x10, 0xBC, 0x01, 0xBC, 0x00, 0x47, 0xC0, 0x46, 0x00, 0x00, 0x00, 0x00,
	0xE4, 0x02, 0x01, 0x40, 0xD8, 0x02, 0x01, 0x40, 0x82, 0x20, 0x56, 0x4B,
	0xC0, 0x00, 0x1A, 0x58, 0x55, 0x49, 0x11, 0x40, 0x80, 0x22, 0x12, 0x02,
	0x0A, 0x43, 0xD0, 0x21, 0x10, 0xB5, 0x1A, 0x50, 0x81, 0x22, 0x58, 0x58,
	0xD2, 0x05, 0x02, 0x43, 0x5A, 0x50, 0xAB, 0x22, 0x90, 0x39, 0x92, 0x00,
	0x99, 0x50, 0x80, 0x21, 0x18, 0x3A, 0xC9, 0x02, 0x99, 0x50, 0xC0, 0x21,
	0x70, 0x32, 0x49, 0x05, 0x99, 0x50, 0x4A, 0x4A, 0x11, 0x68, 0x13, 0x68,
	0x5B, 0x1A, 0x02, 0x2B, 0xFB, 0xD9, 0x80, 0x23, 0x47, 0x48, 0x01, 0x68,
	0xDB, 0x00, 0x19, 0x43, 0x01, 0x60, 0x01, 0x21, 0x45, 0x4C, 0x20, 0x68,
	0x88, 0x43, 0x20, 0x60, 0x44, 0x4C, 0x20, 0x68, 0x18, 0x43, 0x20, 0x60,
	0x43, 0x4C, 0x20, 0x68, 0x88, 0x43, 0x20, 0x60, 0x42, 0x4C, 0x20, 0x68,
	0x18, 0x43, 0x20, 0x60, 0x41, 0x4C, 0x20, 0x68, 0x88, 0x43, 0x20, 0x60,
	0x40, 0x4C, 0x20, 0x68, 0x18, 0x43, 0x20, 0x60, 0x3F, 0x4C, 0x20, 0x68,
	0x88, 0x43, 0x20, 0x60, 0x3E, 0x48, 0x04, 0x68, 0x23, 0x43, 0x03, 0x60,
	0x3D, 0x48, 0x03, 0x68, 0x8B, 0x43, 0x03, 0x60, 0x04, 0x20, 0x3C, 0x49,
	0x0B, 0x68, 0x03, 0x43, 0x0B, 0x60, 0x01, 0x21, 0x3A, 0x4B, 0x49, 0x42,
	0x19, 0x60, 0x11, 0x68, 0x2C, 0x4A, 0x13, 0x68, 0x5B, 0x1A, 0x02, 0x2B,
	0xFB, 0xD9, 0xAA, 0x22, 0x40, 0x21, 0x27, 0x4B, 0x92, 0x00, 0x99, 0x50,
	0xC0, 0x21, 0x58, 0x32, 0x49, 0x05, 0x99, 0x50, 0x80, 0x20, 0xA4, 0x21,
	0xC0, 0x02, 0x89, 0x00, 0x58, 0x50, 0xD1, 0x39, 0xFF, 0x39, 0x59, 0x61,
	0x2E, 0x49, 0x19, 0x61, 0x2E, 0x49, 0x99, 0x61, 0xD8, 0x21, 0x2E, 0x48,
	0x89, 0x00, 0x58, 0x50, 0x2D, 0x48, 0x04, 0x31, 0x58, 0x50, 0x2D, 0x48,
	0xE4, 0x39, 0x58, 0x50, 0x18, 0x31, 0x5A, 0x50, 0x00, 0x22, 0xA1, 0x39,
	0xFF, 0x39, 0x5A, 0x50, 0x04, 0x31, 0x5A, 0x50, 0xE8, 0x21, 0x89, 0x00,
	0x5A, 0x50, 0x04, 0x31, 0x5A, 0x50, 0x26, 0x49, 0x5A, 0x50, 0xD0, 0x21,
	0x25, 0x48, 0x5A, 0x58, 0x02, 0x40, 0x5A, 0x50, 0x82, 0x21, 0xC9, 0x00,
	0x5A, 0x58, 0x0E, 0x48, 0x02, 0x40, 0xA4, 0x20, 0x5A, 0x50, 0x80, 0x21,
	0x40, 0x00, 0x1A, 0x58, 0xD2, 0x00, 0x09, 0x06, 0xD2, 0x08, 0x0A, 0x43,
	0x1A, 0x50, 0x38, 0x30, 0x1A, 0x58, 0xD2, 0x00, 0xD2, 0x08, 0x0A, 0x43,
	0x1A, 0x50, 0xD4, 0x20, 0xC0, 0x00, 0x1A, 0x58, 0xD2, 0x00, 0xD2, 0x08,
	0x11, 0x43, 0x19, 0x50, 0xFE, 0xE7, 0xC0, 0x46, 0x00, 0x60, 0x00, 0x60,
	0xFF, 0x3F, 0xFF, 0xFF, 0x10, 0x50, 0x00, 0x60, 0xA0, 0x10, 0x2D, 0x70,
	0x88, 0x10, 0x2D, 0x70, 0xA0, 0x11, 0x2D, 0x70, 0x88, 0x11, 0x2D, 0x70,
	0xA0, 0x12, 0x2D, 0x70, 0x88, 0x12, 0x2D, 0x70, 0xA0, 0x13, 0x2D, 0x70,
	0x88, 0x13, 0x2D, 0x70, 0xA0, 0x14, 0x2D, 0x70, 0x88, 0x14, 0x2D, 0x70,
	0xF8, 0x0C, 0x20, 0x54, 0x8C, 0x00, 0x34, 0x54, 0x30, 0x01, 0x00, 0x80,
	0x00, 0x02, 0xF0, 0x01, 0x08, 0x08, 0x40, 0x80, 0xFC, 0x00, 0x20, 0x40,
	0x80, 0x07, 0x00, 0x23, 0x54, 0x05, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x1F,
	0xF8, 0xB5, 0xC0, 0x46, 0xF8, 0xBC, 0x08, 0xBC, 0x9E, 0x46, 0x70, 0x47,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x9F, 0xE5, 0x1C, 0xFF, 0x2F, 0xE1,
	0x05, 0x01, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0xDD, 0x00, 0x01, 0x40,
	0xAD, 0x00, 0x01, 0x40
};

	if (!sd_mount()){
		return;
	}
	//does it exist?
	if (f_stat("bootloader/payloads/dummy", NULL) == FR_OK){
		gfx_printf("\nDummy payload exists!\n\n");
	} else {
		gfx_printf("\nPlease wait\n\n");
		sd_save_to_file(dummy_bin, sizeof(dummy_bin), "bootloader/payloads/dummy");
		gfx_printf("Success. Written %d bytes\n\n", sizeof(dummy_bin));
		}
		gfx_printf("\nPlease press [RESET] twice on your chip\nand connect to USB.\n\nYour console will have a black screen.\n\nHold [PWR] for 12 seconds when you\nhave finished updating UF2 files.\n\nPress [POWER] when ready.");
		u8 btn = btn_wait();
	if (btn & BTN_VOL_UP){
	msleep(5000);
	auto_launch_dummy_payload();
	} else if (btn & BTN_POWER){
		auto_launch_dummy_payload();
	}
	return;
}

void _update_filename(char *outFilename, u32 sdPathLen, u32 numSplitParts, u32 currPartIdx)
{
	if (numSplitParts >= 10 && currPartIdx < 10)
	{
		outFilename[sdPathLen] = '0';
		itoa(currPartIdx, &outFilename[sdPathLen + 1], 10);
	}
	else
		itoa(currPartIdx, &outFilename[sdPathLen], 10);
}

static int _dump_emmc_part(char *sd_path, sdmmc_storage_t *storage, emmc_part_t *part)
{
	static const u32 FAT32_FILESIZE_LIMIT = 0xFFFFFFFF;
	static const u32 SECTORS_TO_MIB_COEFF = 11;

	u32 multipartSplitSize = (1u << 31);
	u32 totalSectors = part->lba_end - part->lba_start + 1;
	u32 currPartIdx = 0;
	u32 numSplitParts = 0;
	u32 maxSplitParts = 0;
	u32 btn = 0;
	bool isSmallSdCard = false;
	bool partialDumpInProgress = false;
	int res = 0;
	char *outFilename = sd_path;
	u32 sdPathLen = strlen(sd_path);

	FIL partialIdxFp;
	char partialIdxFilename[12];
	memcpy(partialIdxFilename, "partial.idx", 12);

	gfx_con.fntsz = 8;
	gfx_printf("\nSD Card free space: %d MiB, Total backup size %d MiB\n\n",
		sd_fs.free_clst * sd_fs.csize >> SECTORS_TO_MIB_COEFF,
		totalSectors >> SECTORS_TO_MIB_COEFF);

	// 1GB parts for sd cards 8GB and less.
	if ((sd_storage.csd.capacity >> (20 - sd_storage.csd.read_blkbits)) <= 8192)
		multipartSplitSize = (1u << 30);
	// Maximum parts fitting the free space available.
	maxSplitParts = (sd_fs.free_clst * sd_fs.csize) / (multipartSplitSize / NX_EMMC_BLOCKSIZE);

	// Check if the USER partition or the RAW eMMC fits the sd card free space.
	if (totalSectors > (sd_fs.free_clst * sd_fs.csize))
	{
		isSmallSdCard = true;

		gfx_printf("%k\nSD card free space is smaller than total backup size.%k\n", 0xFFFFBA00, 0xFFFFFFFF);

		if (!maxSplitParts)
		{
			gfx_con.fntsz = 16;
			EPRINTF("Not enough free space for Partial Backup.");

			return 0;
		}
	}
	// Check if we are continuing a previous raw eMMC or USER partition backup in progress.
	if (f_open(&partialIdxFp, partialIdxFilename, FA_READ) == FR_OK && totalSectors > (FAT32_FILESIZE_LIMIT / NX_EMMC_BLOCKSIZE))
	{
		gfx_printf("%kFound Partial Backup in progress. Continuing...%k\n\n", 0xFFAEFD14, 0xFFFFFFFF);

		partialDumpInProgress = true;
		// Force partial dumping, even if the card is larger.
		isSmallSdCard = true;

		f_read(&partialIdxFp, &currPartIdx, 4, NULL);
		f_close(&partialIdxFp);

		if (!maxSplitParts)
		{
			gfx_con.fntsz = 16;
			EPRINTF("Not enough free space for Partial Backup.");

			return 0;
		}

		// Increase maxSplitParts to accommodate previously backed up parts.
		maxSplitParts += currPartIdx;
	}
	else if (isSmallSdCard)
		gfx_printf("%kPartial Backup enabled (with %d MiB parts)...%k\n\n", 0xFFFFBA00, multipartSplitSize >> 20, 0xFFFFFFFF);

	// Check if filesystem is FAT32 or the free space is smaller and backup in parts.
	if (((sd_fs.fs_type != FS_EXFAT) && totalSectors > (FAT32_FILESIZE_LIMIT / NX_EMMC_BLOCKSIZE)) | isSmallSdCard)
	{
		u32 multipartSplitSectors = multipartSplitSize / NX_EMMC_BLOCKSIZE;
		numSplitParts = (totalSectors + multipartSplitSectors - 1) / multipartSplitSectors;

		outFilename[sdPathLen++] = '.';

		// Continue from where we left, if Partial Backup in progress.
		_update_filename(outFilename, sdPathLen, numSplitParts, partialDumpInProgress ? currPartIdx : 0);
	}

	FIL fp;
	gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);
	if (!f_open(&fp, outFilename, FA_READ))
	{
		f_close(&fp);
		gfx_con.fntsz = 16;

		WPRINTF("Existing backup detected in this folder!\n");
		WPRINTF("[POWER] - Overwrite. [VOL] - Back.\n");
		msleep(500);

		if (!(btn_wait() & BTN_POWER))
			return 0;
		gfx_con.fntsz = 8;
		gfx_clear_partial_black(0x00, gfx_con.savedy, 48);
	}
	gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
	gfx_printf("Filename: %s\n\n", outFilename);
	res = f_open(&fp, outFilename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res)
	{
		gfx_con.fntsz = 16;
		EPRINTFARGS("Error (%d) creating file %s.\n", res, outFilename);

		return 0;
	}

	u8 *buf = (u8 *)MIXD_BUF_ALIGNED;

	u32 lba_curr = part->lba_start;
	u32 lbaStartPart = part->lba_start;
	u32 bytesWritten = 0;
	u32 prevPct = 200;
	int retryCount = 0;

	// Continue from where we left, if Partial Backup in progress.
	if (partialDumpInProgress)
	{
		lba_curr += currPartIdx * (multipartSplitSize / NX_EMMC_BLOCKSIZE);
		totalSectors -= currPartIdx * (multipartSplitSize / NX_EMMC_BLOCKSIZE);
		lbaStartPart = lba_curr; // Update the start LBA for verification.
	}
	u64 totalSize = (u64)((u64)totalSectors << 9);
	if (!isSmallSdCard && (sd_fs.fs_type == FS_EXFAT || totalSize <= FAT32_FILESIZE_LIMIT))
		f_lseek(&fp, totalSize);
	else
		f_lseek(&fp, MIN(totalSize, multipartSplitSize));
	f_lseek(&fp, 0);

	u32 num = 0;
	u32 pct = 0;
	while (totalSectors > 0)
	{
		if (numSplitParts != 0 && bytesWritten >= multipartSplitSize)
		{
			f_close(&fp);
			memset(&fp, 0, sizeof(fp));
			currPartIdx++;

			if (h_cfg.verification)
			{
				// Verify part.
				if (_dump_emmc_verify(storage, lbaStartPart, outFilename, part))
				{
					EPRINTF("\nPress any key and try again...\n");

					return 0;
				}
			}

			_update_filename(outFilename, sdPathLen, numSplitParts, currPartIdx);

			// Always create partial.idx before next part, in case a fatal error occurs.
			if (isSmallSdCard)
			{
				// Create partial backup index file.
				if (f_open(&partialIdxFp, partialIdxFilename, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
				{
					f_write(&partialIdxFp, &currPartIdx, 4, NULL);
					f_close(&partialIdxFp);
				}
				else
				{
					gfx_con.fntsz = 16;
					EPRINTF("\nError creating partial.idx file.\n");

					return 0;
				}

				// More parts to backup that do not currently fit the sd card free space or fatal error.
				if (currPartIdx >= maxSplitParts)
				{
					gfx_puts("\n\n1. Press any key to unmount SD Card.\n\
						2. Remove SD Card and move files to free space.\n\
						   Don\'t move the partial.idx file!\n\
						3. Re-insert SD Card.\n\
						4. Select the SAME option again to continue.\n");
					gfx_con.fntsz = 16;

					return 1;
				}
			}

			// Create next part.
			gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
			gfx_printf("Filename: %s\n\n", outFilename);
			lbaStartPart = lba_curr;
			res = f_open(&fp, outFilename, FA_CREATE_ALWAYS | FA_WRITE);
			if (res)
			{
				gfx_con.fntsz = 16;
				EPRINTFARGS("Error (%d) creating file %s.\n", res, outFilename);

				return 0;
			}
			bytesWritten = 0;

			totalSize = (u64)((u64)totalSectors << 9);
			f_lseek(&fp, MIN(totalSize, multipartSplitSize));
			f_lseek(&fp, 0);
		}

		retryCount = 0;
		num = MIN(totalSectors, NUM_SECTORS_PER_ITER);
		while (!sdmmc_storage_read(storage, lba_curr, num, buf))
		{
			EPRINTFARGS("Error reading %d blocks @ LBA %08X,\nfrom eMMC (try %d), retrying...",
				num, lba_curr, ++retryCount);

			msleep(150);
			if (retryCount >= 3)
			{
				gfx_con.fntsz = 16;
				EPRINTFARGS("\nFailed to read %d blocks @ LBA %08X\nfrom eMMC. Aborting..\n",
					num, lba_curr);
				EPRINTF("\nPress any key and try again...\n");

				f_close(&fp);
				f_unlink(outFilename);

				return 0;
			}
		}
		res = f_write(&fp, buf, NX_EMMC_BLOCKSIZE * num, NULL);
		if (res)
		{
			gfx_con.fntsz = 16;
			EPRINTFARGS("\nFatal error (%d) when writing to SD Card", res);
			EPRINTF("\nPress any key and try again...\n");

			f_close(&fp);
			f_unlink(outFilename);

			return 0;
		}
		pct = (u64)((u64)(lba_curr - part->lba_start) * 100u) / (u64)(part->lba_end - part->lba_start);
		if (pct != prevPct)
		{
			tui_pbar(0, gfx_con.y, pct, 0xFFFFFFFF, 0xFF555555);
			prevPct = pct;
		}

		lba_curr += num;
		totalSectors -= num;
		bytesWritten += num * NX_EMMC_BLOCKSIZE;

		// Force a flush after a lot of data if not splitting.
		if (numSplitParts == 0 && bytesWritten >= multipartSplitSize)
		{
			f_sync(&fp);
			bytesWritten = 0;
		}

		btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
		if ((btn & BTN_VOL_DOWN) && (btn & BTN_VOL_UP))
		{
			gfx_con.fntsz = 16;
			WPRINTF("\n\nThe backup was cancelled!");
			EPRINTF("\nPress any key...\n");
			msleep(1500);

			f_close(&fp);
			f_unlink(outFilename);

			return 0;
		}
	}
	tui_pbar(0, gfx_con.y, 100, 0xFFFFFFFF, 0xFF555555);

	// Backup operation ended successfully.
	f_close(&fp);

	if (h_cfg.verification)
	{
		// Verify last part or single file backup.
		if (_dump_emmc_verify(storage, lbaStartPart, outFilename, part))
		{
			EPRINTF("\nPress any key and try again...\n");

			return 0;
		}
		else
			tui_pbar(0, gfx_con.y, 100, 0xFF00FF00, 0xFF155500);
	}

	gfx_con.fntsz = 16;
	// Remove partial backup index file if no fatal errors occurred.
	if (isSmallSdCard)
	{
		f_unlink(partialIdxFilename);
		gfx_printf("%k\n\nYou can now join the files\nand get the complete eMMC RAW GPP backup.", 0xFFFFFFFF);
	}
	gfx_puts("\n\n");

	return 1;
}

typedef enum
{
	PART_BOOT =				(1 << 0),
	PART_SYSTEM =			(1 << 1),
	PART_USER =				(1 << 2),
	PART_RAW =				(1 << 3),
	PART_PRODINFO =			(1 << 4),
	PART_PRODINFO_ONLY =	(1 << 5),
	PART_GP_ALL =			(1 << 7),
	
} emmcPartType_t;

static void _dump_emmc_selected(emmcPartType_t dumpType)
{
	int res = 0;
	u32 timer = 0;
	gfx_clear_partial_black(0x00, 0, 1256);
	tui_sbar(true);
	gfx_con_setpos(0, 0);

	if (!sd_mount())
		goto out;
	if (!(dumpType & PART_PRODINFO || dumpType & PART_PRODINFO_ONLY))
		{
		WPRINTFARGS("Folder %d is selected.\n", folder);
		WPRINTFARGS("Backup will be saved to BACKUP_%d\n\n", folder);
		} else {
			WPRINTF("Backing up to safe folder.\n");
		WPRINTF("Please keep in a safe place!\n");
		}
	gfx_puts("Checking for available free space...\n\n");
	// Get SD Card free space for Partial Backup.
	f_getfree("", &sd_fs.free_clst, NULL);

	sdmmc_storage_t storage;
	sdmmc_t sdmmc;
	if (!sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4))
	{
		EPRINTF("Failed to init eMMC.");
		goto out;
	}

	int i = 0;
	char sdPath[80];

	
	timer = get_tmr_s();
	if (dumpType & PART_BOOT)
	{
		const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;

		emmc_part_t bootPart;
		memset(&bootPart, 0, sizeof(bootPart));
		bootPart.lba_start = 0;
		bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
		for (i = 0; i < 2; i++)
		{
			memcpy(bootPart.name, "BOOT", 5);
			bootPart.name[4] = (u8)('0' + i);
			bootPart.name[5] = 0;

			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i,
				bootPart.name, bootPart.lba_start, bootPart.lba_end, 0xFFFFFFFF);

			sdmmc_storage_set_mmc_partition(&storage, i + 1);

			emmcsn_path_impl(sdPath, "/BOOTS", bootPart.name, &storage);
			res = _dump_emmc_part(sdPath, &storage, &bootPart);
		}
	}

	if ((dumpType & PART_SYSTEM) || (dumpType & PART_USER) || (dumpType & PART_RAW) || (dumpType & PART_PRODINFO))
	{
		sdmmc_storage_set_mmc_partition(&storage, 0);

		if ((dumpType & PART_SYSTEM) || (dumpType & PART_USER))
		{
			LIST_INIT(gpt);
			nx_emmc_gpt_parse(&gpt, &storage);
			LIST_FOREACH_ENTRY(emmc_part_t, part, &gpt, link)
			{
				if ((dumpType & PART_USER) == 0 && !strcmp(part->name, "USER"))
					continue;
				if ((dumpType & PART_SYSTEM) == 0 && strcmp(part->name, "USER"))
					continue;

				gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i++,
					part->name, part->lba_start, part->lba_end, 0xFFFFFFFF);

				emmcsn_path_impl(sdPath, "", part->name, &storage);
				res = _dump_emmc_part(sdPath, &storage, part);
				// If a part failed, don't continue.
				if (!res)
					break;
			}
			nx_emmc_gpt_free(&gpt);
		}

		if (dumpType & PART_RAW)
		{
			// Get GP partition size dynamically.
			const u32 RAW_AREA_NUM_SECTORS = storage.sec_cnt;

			emmc_part_t rawPart;
			memset(&rawPart, 0, sizeof(rawPart));
			rawPart.lba_start = 0;
			rawPart.lba_end = RAW_AREA_NUM_SECTORS - 1;
			strcpy(rawPart.name, "rawnand.bin");
			{
				gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i++,
					rawPart.name, rawPart.lba_start, rawPart.lba_end, 0xFFFFFFFF);

				emmcsn_path_impl(sdPath, "", rawPart.name, &storage);
				res = _dump_emmc_part(sdPath, &storage, &rawPart);
			}
		}
		
		if (dumpType & PART_PRODINFO)
		{
			oldfolder = folder;
			folder = 0;

			emmc_part_t prodinfoPart;
			memset(&prodinfoPart, 0, sizeof(prodinfoPart));
			prodinfoPart.lba_start = 34;
			prodinfoPart.lba_end = 8192 - 1;
			strcpy(prodinfoPart.name, "PRODINFO");
			{
				gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i++,
					prodinfoPart.name, prodinfoPart.lba_start, prodinfoPart.lba_end, 0xFFFFFFFF);

				emmcsn_path_impl(sdPath, "", prodinfoPart.name, &storage);
				res = _dump_emmc_part(sdPath, &storage, &prodinfoPart);
			}
			const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;

		emmc_part_t bootPart;
		memset(&bootPart, 0, sizeof(bootPart));
		bootPart.lba_start = 0;
		bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
			for (i = 0; i < 2; i++)
			{
			memcpy(bootPart.name, "BOOT", 5);
			bootPart.name[4] = (u8)('0' + i);
			bootPart.name[5] = 0;

			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i,
				bootPart.name, bootPart.lba_start, bootPart.lba_end, 0xFFFFFFFF);

			sdmmc_storage_set_mmc_partition(&storage, i + 1);

			emmcsn_path_impl(sdPath, "", bootPart.name, &storage);
			res = _dump_emmc_part(sdPath, &storage, &bootPart);
			}
			folder = oldfolder;
		}
	}

	gfx_putc('\n');
	timer = get_tmr_s() - timer;
	gfx_printf("Time taken: %dm %ds.\n", timer / 60, timer % 60);
	sdmmc_storage_end(&storage);
	if (res){
		if ((dumpType & PART_BOOT) || (dumpType & PART_PRODINFO)) goto out;
		else {
		gfx_printf("\n%kComplete.%k\n\nBackup current BOOT0/1?\n\n%k[POWER] - Yes (Recommended)\n%k[VOL+/-] - Return to menu\n", 0xFF00FF00, 0xFFFFFFFF, 0xFF00FF00, 0xFFFFFF00);
		u32 btn = btn_wait();
		if (btn & BTN_POWER){
				_dump_emmc_selected(PART_BOOT);
				}
		}
	}
out:
	sd_unmount();
	gfx_printf("\nAll finished. Press any key.");
	btn_wait();
	
}

void dump_emmc_system()  { _dump_emmc_selected(PART_SYSTEM); }
void dump_emmc_user()    { _dump_emmc_selected(PART_USER); }
void dump_emmc_boot()    { _dump_emmc_selected(PART_BOOT); }
void dump_emmc_rawnand() { _dump_emmc_selected(PART_RAW); }
void dump_emmc_quick()	 { _dump_emmc_selected(PART_PRODINFO); }

static int _restore_emmc_part(char *sd_path, sdmmc_storage_t *storage, emmc_part_t *part, bool allow_multi_part)
{
	static const u32 SECTORS_TO_MIB_COEFF = 11;

	u32 totalSectors = part->lba_end - part->lba_start + 1;
	u32 currPartIdx = 0;
	u32 numSplitParts = 0;
	u32 lbaStartPart = part->lba_start;
	int res = 0;
	char *outFilename = sd_path;
	u32 sdPathLen = strlen(sd_path);
	u64 fileSize = 0;
	u64 totalCheckFileSize = 0;
	gfx_con.fntsz = 8;

	FIL fp;
	FILINFO fno;

	gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);

	bool use_multipart = false;

	if (allow_multi_part)
	{
		// Check to see if there is a combined file and if so then use that.
		if (f_stat(outFilename, &fno))
		{
			// If not, check if there are partial files and the total size matches.
			gfx_printf("No single file, checking for part files...\n");

			outFilename[sdPathLen++] = '.';

			// Stat total size of the part files.
			while ((u32)((u64)totalCheckFileSize >> (u64)9) != totalSectors)
			{
				_update_filename(outFilename, sdPathLen, 99, numSplitParts);

				gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
				gfx_printf("\nFilename: %s\n", outFilename);

				if (f_stat(outFilename, &fno))
				{
					WPRINTFARGS("Error (%d) file not found '%s'. Aborting...\n", res, outFilename);
					return 0;
				}
				else
					totalCheckFileSize += (u64)fno.fsize;

				numSplitParts++;
			}

			gfx_printf("\n%X sectors total.\n", (u32)((u64)totalCheckFileSize >> (u64)9));

			if ((u32)((u64)totalCheckFileSize >> (u64)9) != totalSectors)
			{
				gfx_con.fntsz = 16;
				EPRINTF("Size of SD Card split backups does not match,\neMMC's selected part size.\n");

				return 0;
			}
			else
			{
				use_multipart = true;
				_update_filename(outFilename, sdPathLen, numSplitParts, 0);
			}
		}
	}

	res = f_open(&fp, outFilename, FA_READ);
	gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
	gfx_printf("\nFilename: %s\n", outFilename);
	if (res)
	{
		if (res != FR_NO_FILE)
			EPRINTFARGS("Error (%d) while opening backup. Continuing...\n", res);
		else
			WPRINTFARGS("Error (%d) file not found. Continuing...\n", res);
		gfx_con.fntsz = 16;

		return 0;
	}
	else if (!use_multipart && (((u32)((u64)f_size(&fp) >> (u64)9)) != totalSectors)) // Check total restore size vs emmc size.
	{
		if (!noszchk){
		gfx_con.fntsz = 16;
		EPRINTF("Size of the SD Card backup does not match,\neMMC's selected part size.\n");
		f_close(&fp);
		return 0;
		} else {
			gfx_con.fntsz = 16;
			EPRINTF("This is dangerous. Ensure you\nhave a backup before continuing\n");
			gfx_printf("\n[PWR] - Continue.  [VOL] - Cancel\n\n");
			u32 btn = btn_wait();
			if (!(btn & BTN_POWER)){
				gfx_printf("Cancelled.\n\n");
				totalSectors = 0;
				}
			totalSectors = ((u32)((u64)f_size(&fp) >> (u64)9));
		}
	}
	else
	{
		fileSize = (u64)f_size(&fp);
		gfx_printf("\nTotal restore size: %d MiB.\n\n",
			(u32)((use_multipart ? (u64)totalCheckFileSize : fileSize) >> (u64)9) >> SECTORS_TO_MIB_COEFF);
	}

	u8 *buf = (u8 *)MIXD_BUF_ALIGNED;

	u32 lba_curr = part->lba_start;
	u32 bytesWritten = 0;
	u32 prevPct = 200;
	int retryCount = 0;

	u32 num = 0;
	u32 pct = 0;

	gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);

	while (totalSectors > 0)
	{
		// If we have more than one part, check the size for the split parts and make sure that the bytes written is not more than that.
		if (numSplitParts != 0 && bytesWritten >= fileSize)
		{
			// If we have more bytes written then close the file pointer and increase the part index we are using
			f_close(&fp);
			memset(&fp, 0, sizeof(fp));
			currPartIdx++;

			if (h_cfg.verification)
			{
				// Verify part.
				if (_dump_emmc_verify(storage, lbaStartPart, outFilename, part))
				{
					EPRINTF("\nPress any key and try again...\n");

					return 0;
				}
			}

			_update_filename(outFilename, sdPathLen, numSplitParts, currPartIdx);

			// Read from next part.
			gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
			gfx_printf("Filename: %s\n\n", outFilename);

			lbaStartPart = lba_curr;

			// Try to open the next file part
			res = f_open(&fp, outFilename, FA_READ);
			if (res)
			{
				gfx_con.fntsz = 16;
				EPRINTFARGS("Error (%d) opening file %s.\n", res, outFilename);

				return 0;
			}
			fileSize = (u64)f_size(&fp);
			bytesWritten = 0;
		}

		retryCount = 0;
		num = MIN(totalSectors, NUM_SECTORS_PER_ITER);

		res = f_read(&fp, buf, NX_EMMC_BLOCKSIZE * num, NULL);
		if (res)
		{
			gfx_con.fntsz = 16;
			EPRINTFARGS("\nFatal error (%d) when reading from SD Card", res);
			EPRINTF("\nYour device may be in an inoperative state!\n\nPress any key and try again now...\n");

			f_close(&fp);
			return 0;
		}
		while (!sdmmc_storage_write(storage, lba_curr, num, buf))
		{
			EPRINTFARGS("Error writing %d blocks @ LBA %08X\nto eMMC (try %d), retrying...",
				num, lba_curr, ++retryCount);

			msleep(150);
			if (retryCount >= 3)
			{
				gfx_con.fntsz = 16;
				EPRINTFARGS("\nFailed to write %d blocks @ LBA %08X\nfrom eMMC. Aborting..\n",
					num, lba_curr);
				EPRINTF("\nYour device may be in an inoperative state!\n\nPress any key and try again...\n");

				f_close(&fp);
				return 0;
			}
		}
		pct = (u64)((u64)(lba_curr - part->lba_start) * 100u) / (u64)(part->lba_end - part->lba_start);
		if (pct != prevPct)
		{
			tui_pbar(0, gfx_con.y, pct, 0xFFFFFFFF, 0xFF555555);
			prevPct = pct;
		}

		lba_curr += num;
		totalSectors -= num;
		bytesWritten += num * NX_EMMC_BLOCKSIZE;
	}
	tui_pbar(0, gfx_con.y, 100, 0xFFFFFFFF, 0xFF555555);

	// Restore operation ended successfully.
	f_close(&fp);

	if (h_cfg.verification)
	{
		// Verify restored data.
		if (_dump_emmc_verify(storage, lbaStartPart, outFilename, part))
		{
			EPRINTF("\nPress any key and try again...\n");

			return 0;
		}
		else
			tui_pbar(0, gfx_con.y, 100, 0xFF00FF00, 0xFF155500);
	}

	gfx_con.fntsz = 16;
	gfx_puts("\n\n");

	return 1;
}

static void _restore_emmc_selected(emmcPartType_t restoreType)
{
	int res = 0;
	u32 timer = 0;
	gfx_clear_partial_black(0x00, 0, 1256);
	tui_sbar(true);
	gfx_con_setpos(0, 0);

	gfx_printf("%kThis may render your device inoperative!\n\n", 0xFFFFFF00);
	gfx_printf("Are you really sure?\n\n%k", 0xFFFFFFFF);
	if ((restoreType & PART_BOOT) || (restoreType & PART_GP_ALL))
	{
		gfx_puts("The mode you selected will only restore\nthe ");
		if (restoreType & PART_BOOT)
			gfx_puts("boot ");
		gfx_puts("partitions that it can find.\n");
		gfx_puts("If it is not found, it will be skipped\nand continue with the next.\n\n");
	}
	gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);

	u8 failsafe_wait = 5;
	while (failsafe_wait > 0)
	{
		gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
		gfx_printf("%kWait... (%ds)    %k", 0xFFFFFFFF, failsafe_wait, 0xFFFFFFFF);
		msleep(1000);
		failsafe_wait--;
	}
	gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);

	gfx_puts("Press POWER to Continue.\nPress VOL to go to the menu.\n\n\n");

	u32 btn = btn_wait();
	if (!(btn & BTN_POWER))
		goto out;

	if (!sd_mount())
		goto out;

	sdmmc_storage_t storage;
	sdmmc_t sdmmc;
	if (!sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4))
	{
		EPRINTF("Failed to init eMMC.");
		goto out;
	}

	int i = 0;
	char sdPath[80];

	timer = get_tmr_s();
	if (restoreType & PART_BOOT)
	{
		const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;

		emmc_part_t bootPart;
		memset(&bootPart, 0, sizeof(bootPart));
		bootPart.lba_start = 0;
		bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
		for (i = 0; i < 2; i++)
		{
			memcpy(bootPart.name, "BOOT", 4);
			bootPart.name[4] = (u8)('0' + i);
			bootPart.name[5] = 0;

			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i,
				bootPart.name, bootPart.lba_start, bootPart.lba_end, 0xFFFFFFFF);

			sdmmc_storage_set_mmc_partition(&storage, i + 1);

			emmcsn_path_impl(sdPath, "/BOOTS", bootPart.name, &storage);
			res = _restore_emmc_part(sdPath, &storage, &bootPart, false);
		}
	}

	if (restoreType & PART_GP_ALL)
	{
		sdmmc_storage_set_mmc_partition(&storage, 0);

		LIST_INIT(gpt);
		nx_emmc_gpt_parse(&gpt, &storage);
		LIST_FOREACH_ENTRY(emmc_part_t, part, &gpt, link)
		{
			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i++,
				part->name, part->lba_start, part->lba_end, 0xFFFFFFFF);

			emmcsn_path_impl(sdPath, "", part->name, &storage);
			res = _restore_emmc_part(sdPath, &storage, part, false);
		}
		nx_emmc_gpt_free(&gpt);
	}

	if (restoreType & PART_RAW)
	{
		// Get GP partition size dynamically.
		const u32 RAW_AREA_NUM_SECTORS = storage.sec_cnt;

		emmc_part_t rawPart;
		memset(&rawPart, 0, sizeof(rawPart));
		rawPart.lba_start = 0;
		rawPart.lba_end = RAW_AREA_NUM_SECTORS - 1;
		strcpy(rawPart.name, "rawnand.bin");
		{
			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i++,
				rawPart.name, rawPart.lba_start, rawPart.lba_end, 0xFFFFFFFF);

			emmcsn_path_impl(sdPath, "", rawPart.name, &storage);
			res = _restore_emmc_part(sdPath, &storage, &rawPart, true);
		}
	}
	
	if (restoreType & PART_PRODINFO)
	{
		// Get GP partition size dynamically.
		//const u32 RAW_AREA_NUM_SECTORS = storage.sec_cnt;

		emmc_part_t prodinfoPart;
		memset(&prodinfoPart, 0, sizeof(prodinfoPart));
		prodinfoPart.lba_start = 34;
		prodinfoPart.lba_end = 8192 - 1;
		strcpy(prodinfoPart.name, "PRODINFO");
		{
			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i++,
				prodinfoPart.name, prodinfoPart.lba_start, prodinfoPart.lba_end, 0xFFFFFFFF);

			emmcsn_path_impl(sdPath, "", prodinfoPart.name, &storage);
			res = _restore_emmc_part(sdPath, &storage, &prodinfoPart, false);
		}
		
		const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;

		emmc_part_t bootPart;
		memset(&bootPart, 0, sizeof(bootPart));
		bootPart.lba_start = 0;
		bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
		for (i = 0; i < 2; i++)
		{
			memcpy(bootPart.name, "BOOT", 4);
			bootPart.name[4] = (u8)('0' + i);
			bootPart.name[5] = 0;

			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i,
				bootPart.name, bootPart.lba_start, bootPart.lba_end, 0xFFFFFFFF);

			sdmmc_storage_set_mmc_partition(&storage, i + 1);

			emmcsn_path_impl(sdPath, "", bootPart.name, &storage);
			res = _restore_emmc_part(sdPath, &storage, &bootPart, false);
		}
		folder = oldfolder;
	}
	
	if (restoreType & PART_PRODINFO_ONLY)
	{
		// Get GP partition size dynamically.
		//const u32 RAW_AREA_NUM_SECTORS = storage.sec_cnt;

		emmc_part_t prodinfoPart;
		memset(&prodinfoPart, 0, sizeof(prodinfoPart));
		prodinfoPart.lba_start = 34;
		prodinfoPart.lba_end = 8192 - 1;
		strcpy(prodinfoPart.name, "PRODINFO");
		{
			gfx_printf("%k%02d: %s (%07X-%07X)%k\n", 0xFF00FF00, i++,
				prodinfoPart.name, prodinfoPart.lba_start, prodinfoPart.lba_end, 0xFFFFFFFF);

			emmcsn_path_impl(sdPath, "", prodinfoPart.name, &storage);
			res = _restore_emmc_part(sdPath, &storage, &prodinfoPart, false);
		}
		folder = oldfolder;
	}

	gfx_putc('\n');
	timer = get_tmr_s() - timer;
	gfx_printf("Time taken: %dm %ds.\n", timer / 60, timer % 60);
	sdmmc_storage_end(&storage);
	if (res && h_cfg.verification){
		gfx_printf("\n%kFinished and verified!%k\nPress any key...\n", 0xFFFFFF00, 0xFFFFFFFF);
	} else if (res) {
		gfx_printf("\nFinished! Press any key...\n");
	}
	btn_wait();
	if (noszchk){
		menu_autorcm();
	}

out:
	sd_unmount();
	btn_wait();
}


void restore_emmc_boot()			{ 
noszchk = false;
_restore_emmc_selected(PART_BOOT); }
void restore_emmc_rawnand()			{ 
noszchk = false;
_restore_emmc_selected(PART_RAW); }
void restore_emmc_gpp_parts()		{ 
noszchk = false;
_restore_emmc_selected(PART_GP_ALL); }
void restore_emmc_quick()     		{ 
oldfolder = folder;
folder = 0;
noszchk = false;
_restore_emmc_selected(PART_PRODINFO); }
void restore_emmc_quick_prodinfo()	{ 
oldfolder = folder;
folder = 0;
noszchk = false;
_restore_emmc_selected(PART_PRODINFO_ONLY); }
void restore_emmc_boot_noszchk()	{
noszchk = true;
_restore_emmc_selected(PART_BOOT);
}


