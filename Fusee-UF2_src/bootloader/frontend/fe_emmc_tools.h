/*
 * Copyright (c) 2018 Rajko Stojadinovic
 * Copyright (c) 2018 CTCaer
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

#ifndef _FE_EMMC_TOOLS_H_
#define _FE_EMMC_TOOLS_H_

void select_folder_0();
void select_folder_1();
void select_folder_2();
void select_folder_3();
void select_folder_4();
void select_folder_5();
void select_folder_6();
void select_folder_7();
void select_folder_8();
void select_folder_9();

void dump_emmc_system();
void dump_emmc_user();
void dump_emmc_boot();
void dump_emmc_rawnand();
void dump_emmc_quick();

void restore_emmc_boot();
void restore_emmc_rawnand();
void restore_emmc_gpp_parts();
void restore_emmc_quick();
void restore_emmc_quick_prodinfo();
void restore_emmc_boot_noszchk();
#endif
