/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef PSXF_GUARD_DEBUG_H
#define PSXF_GUARD_DEBUG_H

#include "engine/io.h"
#include "engine/gfx.h"
#include "engine/pad.h"
#include "stage.h"

#include "engine/fixed.h"
#include "engine/character.h"
#include "engine/player.h"
#include "object/object.h"

#include "font/font.h"

typedef struct
{	

	u8 mode, selection;

  RECT_FIXED getbg[255];
} Debug;

extern Debug debug;

//Debug functions
void Debug_Tick(void);
void Debug_GetDST(RECT_FIXED* dst, u8 localization);
void Debug_Free(void);

#endif