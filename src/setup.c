/*
 * setup.c - T-962 reflow controller
 *
 * Copyright (C) 2014 Werner Johansson, wj@unifiedengineering.se
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include "nvstorage.h"
#include "reflow_profiles.h"
#include "setup.h"

setupMenuStruct setupmenu[] = {
	{"Min fan speed    %4.0f", REFLOW_MIN_FAN_SPEED, 0, 254, 0, 1.0f},
	{"Cycle done beep %4.1fs", REFLOW_BEEP_DONE_LEN, 0, 254, 0, 0.1f},
	{"Max. Int. Temp. %4.0fC", REFLOW_STOP_TEMP, 154, 254, 46, 1.0f},
	{"Standby Temp.   %4.0fC", REFLOW_STANDBY_TEMP, 10, 80, 0, 1.0f},
	{"Use ext. TC as FB %1.0f", REFLOW_USE_EXT_TC, 0, 1, 0, 1.0f},
};
#define NUM_SETUP_ITEMS (sizeof(setupmenu) / sizeof(setupmenu[0]))

int Setup_getNumItems(void) {
	return NUM_SETUP_ITEMS;
}

int _getRawValue(int item) {
	return NV_GetConfig(setupmenu[item].nvval);
}

float Setup_getValue(int item) {
	int intval = _getRawValue(item);
	intval += setupmenu[item].offset;
	return ((float)intval) * setupmenu[item].multiplier;
}

void Setup_setValue(int item, int value) {
	NV_SetConfig(setupmenu[item].nvval, value);
	Reflow_ValidateNV();
}

void Setup_setRealValue(int item, float value) {
	int intval = (int)(value / setupmenu[item].multiplier);
	intval -= setupmenu[item].offset;
	Setup_setValue(item, intval);
}

void Setup_increaseValue(int item, int amount) {
	int curval = _getRawValue(item) + amount;

	int maxval = setupmenu[item].maxval;
	if (curval > maxval) curval = maxval;

	Setup_setValue(item, curval);
}

void Setup_decreaseValue(int item, int amount) {
	int curval = _getRawValue(item) - amount;

	int minval = setupmenu[item].minval;
	if (curval < minval) curval = minval;

	Setup_setValue(item, curval);
}

void Setup_printFormattedValue(int item) {
	printf(setupmenu[item].formatstr, Setup_getValue(item));
}

int Setup_snprintFormattedValue(char* buf, int n, int item) {
	return snprintf(buf, n, setupmenu[item].formatstr, Setup_getValue(item));
}
