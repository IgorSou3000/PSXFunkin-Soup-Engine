/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "debug.h"

#include "mem.h"
#include "timer.h"
#include "audio.h"
#include "pad.h"
#include "main.h"
#include "random.h"
#include "movie.h"

#include "menu.h"
#include "trans.h"
#include "loadscr.h"

//Debug state
Debug debug;

void Debug_Free(void)
{
	//reset stuff
	for (u8 i = 0; i != 255; i++)
	{
		debug.getbg[i].x = 0;
		debug.getbg[i].y = 0;
		debug.getbg[i].w = 0;
		debug.getbg[i].h = 0;
	}
	debug.selection = 0;
}
static void Debug_FocusCharacter(Character *ch, fixed_t div)
{
	//Use character focus settings to update target position and zoom
		stage.camera.tx = ch->x + ch->focus_x;
		stage.camera.ty = ch->y + ch->focus_y;
		stage.camera.tz = ch->focus_zoom;
		stage.camera.td = div;
}

static void Debug_ScrollCamera(void)
{
		//Get delta position
		fixed_t dx = stage.camera.tx - stage.camera.x;
		fixed_t dy = stage.camera.ty - stage.camera.y;
		fixed_t dz = stage.camera.tz - stage.camera.zoom;
		
		//Scroll based off current divisor
		stage.camera.x += FIXED_MUL(dx, stage.camera.td);
		stage.camera.y += FIXED_MUL(dy, stage.camera.td);
		stage.camera.zoom += FIXED_MUL(dz, stage.camera.td);
	
	//Update other camera stuff
	stage.camera.bzoom = FIXED_MUL(stage.camera.zoom, stage.bump);
}

void Debug_GetDST(RECT_FIXED* dst, u8 localization)
{
	//sorry guys
	dst->x += debug.getbg[localization].x;
	dst->y += debug.getbg[localization].y;
	dst->w += debug.getbg[localization].w;
	dst->h += debug.getbg[localization].h;
}

void Debug_Tick(void)
{
	//show which mode you are selection
	static const char * mode_options[] = {
	  "Character Mode",
	  "Camera Mode",
	  "Background Mode"
  };

  //for reset some variables
  static u8 option = 0;

  if (option != debug.mode)
  {
	option = debug.mode;
	debug.selection = 0;
  }

	//handle mode change
	if (pad_state.press & PAD_R2)
	{
		if (debug.mode < COUNT_OF(mode_options) - 1)
			debug.mode++;

		else
			debug.mode = 0;
	}
	if (pad_state.press & PAD_L2)
	{
		if (debug.mode > 0)
			debug.mode--;

		else
			debug.mode = COUNT_OF(mode_options) - 1;
	}

	//Draw text's
	stage.font_cdr.draw(&stage.font_cdr,
	mode_options[debug.mode],
	-10,
	-110,
	FontAlign_Left,
	"stage"
	);

	//Draw text's
	stage.font_cdr.draw(&stage.font_cdr,
	"Welcome to Soup Debug!",
	-150,
	-110,
	FontAlign_Left,
	"stage"
	);

	stage.font_cdr.draw(&stage.font_cdr,
	"Change The Current Mode\nBy Pressing L2 Or R2!",
	-150,
	-60,
	FontAlign_Left,
	"stage"
	);

	const struct
	{
		const char *text; //text who will be displayed
		Character* character; // which character information will be displayed

	}Debug_character[] = {
		{"Player", stage.player},
		{"Opponent", stage.opponent},
		{"Girlfriend", stage.gf}
	};

	char text[0x100];

	switch(debug.mode)
   	{
	//Character Mode
	case 0:
		//Change character camera
		if (pad_state.press & PAD_TRIANGLE)
		{
			if (debug.selection < COUNT_OF(Debug_character) - 1)
				debug.selection++;
	
			else
				debug.selection = 0;
		}

		//input stuff
		if (pad_state.held & PAD_LEFT)
			Debug_character[debug.selection].character->x -= FIXED_DEC(1,1);
		if (pad_state.held & PAD_RIGHT)
			Debug_character[debug.selection].character->x += FIXED_DEC(1,1);
		if (pad_state.held & PAD_UP)
			Debug_character[debug.selection].character->y -= FIXED_DEC(1,1);
		if (pad_state.held & PAD_DOWN)
			Debug_character[debug.selection].character->y += FIXED_DEC(1,1);

		//format text
		sprintf(text, "Move Characters with your pad!\n%s x is %d and y is %d\n Press Triangle To Change\nCharacter!", 
		Debug_character[debug.selection].text, 
		Debug_character[debug.selection].character->x / 1024, 
		Debug_character[debug.selection].character->y / 1024
		); //show character x and y
	break;

	//Camera mode
	case 1:
		//Scroll camera
		//0 = bf, 1 = opponent
			switch (debug.selection)
			{
				case 0:
					Debug_FocusCharacter(stage.player, FIXED_UNIT / 24);
				break;

				case 1:
					Debug_FocusCharacter(stage.opponent, FIXED_UNIT / 24);
				break;

				default:
					Debug_FocusCharacter(stage.player, FIXED_UNIT / 24);
				break;
			}

		if (pad_state.press & PAD_TRIANGLE)
		{
			if (debug.selection < 1)
				debug.selection++;

			else
				debug.selection = 0;
		}

			if (pad_state.held & PAD_LEFT)
				stage.camera.x -= FIXED_DEC(2,1);
			if (pad_state.held & PAD_UP)
				stage.camera.y -= FIXED_DEC(2,1);
			if (pad_state.held & PAD_RIGHT)
					stage.camera.x += FIXED_DEC(2,1);
			if (pad_state.held & PAD_DOWN)
					stage.camera.y += FIXED_DEC(2,1);
		//format text
		sprintf(text, "Move Your Camera With You Pad!\nTo Focus In Another Character\nPress Triangle"); 
	break;

	//background mode
	case 2:
		//input stuff
		if (pad_state.press & PAD_L1)
		{
			if (debug.selection > 0)
				debug.selection--;
		}

		if (pad_state.press & PAD_R1)
		{
			if (debug.selection < 255)
				debug.selection++;
		}

		if (pad_state.held & PAD_LEFT)
			(debug.getbg[debug.selection]).x -= FIXED_DEC(1,1);
		if (pad_state.held & PAD_RIGHT)
			(debug.getbg[debug.selection]).x += FIXED_DEC(1,1);
		if (pad_state.held & PAD_UP)
			(debug.getbg[debug.selection]).y -= FIXED_DEC(1,1);
		if (pad_state.held & PAD_DOWN)
			(debug.getbg[debug.selection]).y += FIXED_DEC(1,1);

		if (pad_state.held & PAD_SQUARE)
			(debug.getbg[debug.selection]).w -= FIXED_DEC(1,1);
		if (pad_state.held & PAD_CIRCLE)
			(debug.getbg[debug.selection]).w += FIXED_DEC(1,1);
		if (pad_state.held & PAD_TRIANGLE)
			(debug.getbg[debug.selection]).h -= FIXED_DEC(1,1);
		if (pad_state.held & PAD_CROSS)
			(debug.getbg[debug.selection]).h += FIXED_DEC(1,1);

		sprintf(text, "Move Background with your pad!\nx is %d and y is %d\nStretch Background\nWith Your Buttons\nw is %d and h %d", 
		(debug.getbg[debug.selection]).x / 1024, 
		(debug.getbg[debug.selection]).y / 1024, 
		(debug.getbg[debug.selection]).w / 1024,
		(debug.getbg[debug.selection]).h / 1024
		);
		break;
}

	Debug_ScrollCamera();

	//draw information
	stage.font_cdr.draw(&stage.font_cdr,
	text,
	-10,
	-90,
	FontAlign_Left,
	"stage"
	);
	
}