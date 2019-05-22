/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2019 CTCaer
 * Copyright (c) 2018 balika011
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

#include "fe_info.h"
#include "../gfx/gfx.h"
#include "../hos/hos.h"
#include "../hos/pkg1.h"
#include "../libs/fatfs/ff.h"
#include "../mem/heap.h"
#include "../power/bq24193.h"
#include "../power/max17050.h"
#include "../sec/tsec.h"
#include "../soc/fuse.h"
#include "../soc/i2c.h"
#include "../soc/kfuse.h"
#include "../soc/smmu.h"
#include "../soc/t210.h"
#include "../storage/mmc.h"
#include "../storage/nx_emmc.h"
#include "../storage/sdmmc.h"
#include "../utils/btn.h"
#include "../utils/util.h"

extern sdmmc_storage_t sd_storage;
extern FATFS sd_fs;

extern bool sd_mount();
extern void sd_unmount();
extern int  sd_save_to_file(void *buf, u32 size, const char *filename);
extern void emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage);
bool fuses_got;
const int get_fuse_count()
{
	static u32 burntFuses;
	if (!burntFuses){
			for (u32 i = 0; i < 32; i++)
			{
				if ((fuse_read_odm(7) >> i) & 1)
					burntFuses++;
			}
	}
	return burntFuses;
}

void print_mmc_info()
{
	gfx_clear_partial_black(0x00, 0, 1256);
	gfx_con_setpos(0, 0);

	static const u32 SECTORS_TO_MIB_COEFF = 11;

	sdmmc_storage_t storage;
	sdmmc_t sdmmc;

	if (!sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4))
	{
		EPRINTF("Failed to init eMMC.");
		goto out;
	}
	else
	{
		u16 card_type;
		u32 speed = 0;

		gfx_printf("%kCID:%k\n", 0xFF00DDFF, 0xFFFFFFFF);
		switch (storage.csd.mmca_vsn)
		{
		case 0: /* MMC v1.0 - v1.2 */
		case 1: /* MMC v1.4 */
			gfx_printf(
				" Vendor ID:  %03X\n"
				" Model:      %c%c%c%c%c%c%c\n"
				" HW rev:     %X\n"
				" FW rev:     %X\n"
				" S/N:        %03X\n"
				" Month/Year: %02d/%04d\n\n",
				storage.cid.manfid,
				storage.cid.prod_name[0], storage.cid.prod_name[1],	storage.cid.prod_name[2],
				storage.cid.prod_name[3], storage.cid.prod_name[4],	storage.cid.prod_name[5],
				storage.cid.prod_name[6], storage.cid.hwrev, storage.cid.fwrev,
				storage.cid.serial, storage.cid.month, storage.cid.year);
			break;
		case 2: /* MMC v2.0 - v2.2 */
		case 3: /* MMC v3.1 - v3.3 */
		case 4: /* MMC v4 */
			gfx_printf(
				" Vendor ID:  %X\n"
				" Card/BGA:   %X\n"
				" OEM ID:     %02X\n"
				" Model:      %c%c%c%c%c%c\n"
				" Prd Rev:    %X\n"
				" S/N:        %04X\n"
				" Month/Year: %02d/%04d\n\n",
				storage.cid.manfid, storage.cid.card_bga, storage.cid.oemid,
				storage.cid.prod_name[0], storage.cid.prod_name[1], storage.cid.prod_name[2],
				storage.cid.prod_name[3], storage.cid.prod_name[4],	storage.cid.prod_name[5],
				storage.cid.prv, storage.cid.serial, storage.cid.month, storage.cid.year);
			break;
		default:
			EPRINTFARGS("eMMC has unknown MMCA version %d", storage.csd.mmca_vsn);
			break;
		}

		if (storage.csd.structure == 0)
			EPRINTF("Unknown CSD structure.");
		else
		{
			gfx_printf("%kExtended CSD V1.%d:%k\n",
				0xFF00DDFF, storage.ext_csd.ext_struct, 0xFFFFFFFF);
			card_type = storage.ext_csd.card_type;
			u8 card_type_support[96];
			u8 pos_type = 0;
			card_type_support[0] = 0;
			if (card_type & EXT_CSD_CARD_TYPE_HS_26)
			{
				memcpy(card_type_support, "HS26", 4);
				speed = (26 << 16) | 26;
				pos_type += 4;
			}
			if (card_type & EXT_CSD_CARD_TYPE_HS_52)
			{
				memcpy(card_type_support + pos_type, ", HS52", 6);
				speed = (52 << 16) | 52;
				pos_type += 6;
			}
			if (card_type & EXT_CSD_CARD_TYPE_DDR_1_8V)
			{
				memcpy(card_type_support + pos_type, ", DDR52_1.8V", 12);
				speed = (52 << 16) | 104;
				pos_type += 12;
			}
			if (card_type & EXT_CSD_CARD_TYPE_HS200_1_8V)
			{
				memcpy(card_type_support + pos_type, ", HS200_1.8V", 12);
				speed = (200 << 16) | 200;
				pos_type += 12;
			}
			if (card_type & EXT_CSD_CARD_TYPE_HS400_1_8V)
			{
				memcpy(card_type_support + pos_type, ", HS400_1.8V", 12);
				speed = (200 << 16) | 400;
				pos_type += 12;
			}
			card_type_support[pos_type] = 0;

			gfx_printf(
				" Spec Version:  %02X\n"
				" Extended Rev:  1.%d\n"
				" Dev Version:   %d\n"
				" Cmd Classes:   %02X\n"
				" Capacity:      %s\n"
				" Max Rate:      %d MB/s (%d MHz)\n"
				" Current Rate:  %d MB/s\n"
				" Type Support:  ",
				storage.csd.mmca_vsn, storage.ext_csd.rev, storage.ext_csd.dev_version, storage.csd.cmdclass,
				storage.csd.capacity == (4096 * 512) ? "High" : "Low", speed & 0xFFFF, (speed >> 16) & 0xFFFF,
				storage.csd.busspeed);
			gfx_con.fntsz = 8;
			gfx_printf("%s", card_type_support);
			gfx_con.fntsz = 16;
			gfx_printf("\n\n", card_type_support);

			u32 boot_size = storage.ext_csd.boot_mult << 17;
			u32 rpmb_size = storage.ext_csd.rpmb_mult << 17;
			gfx_printf("%keMMC Partitions:%k\n", 0xFF00DDFF, 0xFFFFFFFF);
			gfx_printf(" 1: %kBOOT0      %k\n    Size: %5d KiB (LBA Sectors: 0x%07X)\n", 0xFF00FF00, 0xFFFFFFFF,
				boot_size / 1024, boot_size / 1024 / 512);
			gfx_put_small_sep();
			gfx_printf(" 2: %kBOOT1      %k\n    Size: %5d KiB (LBA Sectors: 0x%07X)\n", 0xFF00FF00, 0xFFFFFFFF,
				boot_size / 1024, boot_size / 1024 / 512);
			gfx_put_small_sep();
			gfx_printf(" 3: %kRPMB       %k\n    Size: %5d KiB (LBA Sectors: 0x%07X)\n", 0xFF00FF00, 0xFFFFFFFF,
				rpmb_size / 1024, rpmb_size / 1024 / 512);
			gfx_put_small_sep();
			gfx_printf(" 0: %kGPP (USER) %k\n    Size: %5d MiB (LBA Sectors: 0x%07X)\n\n", 0xFF00FF00, 0xFFFFFFFF,
				storage.sec_cnt >> SECTORS_TO_MIB_COEFF, storage.sec_cnt);
			gfx_put_small_sep();
			gfx_printf("%kGPP (eMMC USER) partition table:%k\n", 0xFF00DDFF, 0xFFFFFFFF);

			sdmmc_storage_set_mmc_partition(&storage, 0);
			LIST_INIT(gpt);
			nx_emmc_gpt_parse(&gpt, &storage);
			int gpp_idx = 0;
			LIST_FOREACH_ENTRY(emmc_part_t, part, &gpt, link)
			{
				gfx_printf(" %02d: %k%s%k\n     Size: % 5d MiB (LBA Sectors 0x%07X)\n     LBA Range: %08X-%08X\n",
					gpp_idx++, 0xFFAEFD14, part->name, 0xFFFFFFFF, (part->lba_end - part->lba_start + 1) >> SECTORS_TO_MIB_COEFF,
					part->lba_end - part->lba_start + 1, part->lba_start, part->lba_end);
				gfx_put_small_sep();
			}
			nx_emmc_gpt_free(&gpt);
		}
	}

out:
	sdmmc_storage_end(&storage);

	btn_wait();
}

void print_sdcard_info()
{
	static const u32 SECTORS_TO_MIB_COEFF = 11;

	gfx_clear_partial_black(0x00, 0, 1256);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		u32 capacity;

		gfx_printf("%kCard IDentification:%k\n", 0xFF00DDFF, 0xFFFFFFFF);
		gfx_printf(
			" Vendor ID:  %02x\n"
			" OEM ID:     %c%c\n"
			" Model:      %c%c%c%c%c\n"
			" HW rev:     %X\n"
			" FW rev:     %X\n"
			" S/N:        %08x\n"
			" Month/Year: %02d/%04d\n\n",
			sd_storage.cid.manfid, (sd_storage.cid.oemid >> 8) & 0xFF, sd_storage.cid.oemid & 0xFF,
			sd_storage.cid.prod_name[0], sd_storage.cid.prod_name[1], sd_storage.cid.prod_name[2],
			sd_storage.cid.prod_name[3], sd_storage.cid.prod_name[4],
			sd_storage.cid.hwrev, sd_storage.cid.fwrev, sd_storage.cid.serial,
			sd_storage.cid.month, sd_storage.cid.year);

		gfx_printf("%kCard-Specific Data V%d.0:%k\n", 0xFF00DDFF, sd_storage.csd.structure + 1, 0xFFFFFFFF);
		capacity = sd_storage.csd.capacity >> (20 - sd_storage.csd.read_blkbits);
		gfx_printf(
			" Cmd Classes:    %02X\n"
			" Capacity:       %d MiB\n"
			" Bus Width:      %d\n"
			" Current Rate:   %d MB/s (%d MHz)\n"
			" Speed Class:    %d\n"
			" UHS Grade:      U%d\n"
			" Video Class:    V%d\n"
			" App perf class: A%d\n"
			" Write Protect:  %d\n\n",
			sd_storage.csd.cmdclass, capacity,
			sd_storage.ssr.bus_width, sd_storage.csd.busspeed, sd_storage.csd.busspeed * 2,
			sd_storage.ssr.speed_class, sd_storage.ssr.uhs_grade, sd_storage.ssr.video_class,
			sd_storage.ssr.app_class, sd_storage.csd.write_protect);

		gfx_puts("Acquiring FAT volume info...\n\n");
		f_getfree("", &sd_fs.free_clst, NULL);
		gfx_printf("%kFound %s volume:%k\n Free:    %d MiB\n Cluster: %d KiB\n",
				0xFF00DDFF, sd_fs.fs_type == FS_EXFAT ? "exFAT" : "FAT32", 0xFFFFFFFF,
				sd_fs.free_clst * sd_fs.csize >> SECTORS_TO_MIB_COEFF, (sd_fs.csize > 1) ? (sd_fs.csize >> 1) : 512);
		sd_unmount();
	}

	btn_wait();
}

void print_fuel_gauge_info()
{
	int value = 0;

	gfx_printf("%kFuel Gauge IC Info:\n%k", 0xFF00DDFF, 0xFFFFFFFF);

	max17050_get_property(MAX17050_RepSOC, &value);
	gfx_printf("Capacity now:           %3d%\n", value >> 8);

	max17050_get_property(MAX17050_RepCap, &value);
	gfx_printf("Capacity now:           %4d mAh\n", value);

	max17050_get_property(MAX17050_FullCAP, &value);
	gfx_printf("Capacity full:          %4d mAh\n", value);

	max17050_get_property(MAX17050_DesignCap, &value);
	gfx_printf("Capacity (design):      %4d mAh\n", value);

	max17050_get_property(MAX17050_Current, &value);
	if (value >= 0)
		gfx_printf("Current now:            %d mA\n", value / 1000);
	else
		gfx_printf("Current now:            -%d mA\n", ~value / 1000);

	max17050_get_property(MAX17050_AvgCurrent, &value);
	if (value >= 0)
		gfx_printf("Current average:        %d mA\n", value / 1000);
	else
		gfx_printf("Current average:        -%d mA\n", ~value / 1000);

	max17050_get_property(MAX17050_VCELL, &value);
	gfx_printf("Voltage now:            %4d mV\n", value);

	max17050_get_property(MAX17050_OCVInternal, &value);
	gfx_printf("Voltage open-circuit:   %4d mV\n", value);

	max17050_get_property(MAX17050_MinVolt, &value);
	gfx_printf("Min voltage reached:    %4d mV\n", value);

	max17050_get_property(MAX17050_MaxVolt, &value);
	gfx_printf("Max voltage reached:    %4d mV\n", value);

	max17050_get_property(MAX17050_V_empty, &value);
	gfx_printf("Empty voltage (design): %4d mV\n", value);

	max17050_get_property(MAX17050_TEMP, &value);
	if (value >= 0)
		gfx_printf("Battery temperature:    %d.%d oC\n", value / 10, value % 10);
	else
		gfx_printf("Battery temperature:    -%d.%d oC\n", ~value / 10, (~value) % 10);
}

void print_battery_charger_info()
{
	int value = 0;

	gfx_printf("%k\n\nBattery Charger IC Info:\n%k", 0xFF00DDFF, 0xFFFFFFFF);

	bq24193_get_property(BQ24193_InputVoltageLimit, &value);
	gfx_printf("Input voltage limit:       %4d mV\n", value);

	bq24193_get_property(BQ24193_InputCurrentLimit, &value);
	gfx_printf("Input current limit:       %4d mA\n", value);

	bq24193_get_property(BQ24193_SystemMinimumVoltage, &value);
	gfx_printf("Min voltage limit:         %4d mV\n", value);

	bq24193_get_property(BQ24193_FastChargeCurrentLimit, &value);
	gfx_printf("Fast charge current limit: %4d mA\n", value);

	bq24193_get_property(BQ24193_ChargeVoltageLimit, &value);
	gfx_printf("Charge voltage limit:      %4d mV\n", value);

	bq24193_get_property(BQ24193_ChargeStatus, &value);
	gfx_printf("Charge status:             ");
	switch (value)
	{
	case 0:
		gfx_printf("Not charging\n");
		break;
	case 1:
		gfx_printf("Pre-charging\n");
		break;
	case 2:
		gfx_printf("Fast charging\n");
		break;
	case 3:
		gfx_printf("Charge terminated\n");
		break;
	default:
		gfx_printf("Unknown (%d)\n", value);
		break;
	}
	bq24193_get_property(BQ24193_TempStatus, &value);
	gfx_printf("Temperature status:        ");
	switch (value)
	{
	case 0:
		gfx_printf("Normal\n");
		break;
	case 2:
		gfx_printf("Warm\n");
		break;
	case 3:
		gfx_printf("Cool\n");
		break;
	case 5:
		gfx_printf("Cold\n");
		break;
	case 6:
		gfx_printf("Hot\n");
		break;
	default:
		gfx_printf("Unknown (%d)\n", value);
		break;
	}
}

void print_battery_info()
{
	gfx_clear_partial_black(0x00, 0, 1256);
	gfx_con_setpos(0, 0);

	//print_fuel_gauge_info();

	print_battery_charger_info();
	btn_wait();
	}

void _ipatch_process(u32 offset, u32 value)
{
	gfx_printf("%8x %8x", BOOTROM_BASE + offset, value);
	u8 lo = value & 0xff;
	switch (value >> 8)
	{
	case 0x20:
		gfx_printf("    MOVS R0, #0x%02X", lo);
		break;
	case 0xDF:
		gfx_printf("    SVC #0x%02X", lo);
		break;
	}
	gfx_puts("\n");
}
