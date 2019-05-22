/*
 * Copyright (C) 2018 CTCaer
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

#ifndef _HEKATE_LOGOS_H_
#define _HEKATE_LOGOS_H_

#ifdef MENU_LOGO_ENABLE
// 119 x 57 @24bpp RGB RAW positioned at 577 x 1199
#define SZ_MENU_LOGO     0
#define SZ_MENU_LOGO_BLZ  0
static unsigned char Kc_MENU_LOGO_blz[SZ_MENU_LOGO_BLZ] = {}; //left in skeleton if anyone wants to add back.

#endif //MENU_LOGO_ENABLE

// 68 x 192 @8bpp Grayscale RAW.
#define X_BOOTLOGO         0
#define Y_BOOTLOGO        0
#define SZ_BOOTLOGO     0
#define SZ_BOOTLOGO_BLZ  0
//static u8 BOOTLOGO_BLZ[SZ_BOOTLOGO_BLZ] = {}; //left in skeleton if anyone wants to add back.

#endif
