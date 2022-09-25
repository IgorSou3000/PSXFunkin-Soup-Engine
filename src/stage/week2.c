/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "week2.h"

#include "../mem.h"
#include "../archive.h"
#include "../timer.h"
#include "../random.h"

//Week 2 background structure
typedef struct
{
	//Stage background base structure
	StageBack back;
	
	//Textures
	Gfx_Tex tex_back0; //Background
	Gfx_Tex tex_back1; //Window
	Gfx_Tex tex_back2; //Lightning window

	u8 thunder, thunder_fade;
} Back_Week2;

//Week 2 background functions
void Back_Week2_DrawThunder(StageBack *back)
{
	Back_Week2 *this = (Back_Week2*)back;

	//thunder stuff
	if (stage.song_step > 0 && (stage.song_step & 0x7) == 7)
	 if (RandomRange(0, 250) == 200 && this->thunder_fade == 0)
		{
		this->thunder_fade = 255;
		stage.player->set_anim(stage.player, PlayerAnim_Sweat);
		}

	if (this->thunder_fade != 0)
	{	
	static const RECT thunder = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
	Gfx_BlendRect(&thunder, this->thunder_fade, this->thunder_fade, this->thunder_fade, 1);
	this->thunder_fade -= FIXED_MUL(FIXED_DEC(280,1), timer_dt) >> FIXED_SHIFT;

	//make sure be 0 and not like 9
	if (this->thunder_fade < 12)
	this->thunder_fade = 0;

	}
}

void Back_Week2_DrawBG(StageBack *back)
{
	Back_Week2 *this = (Back_Week2*)back;
	
	fixed_t fx, fy;
	fx = stage.camera.x;
	fy = stage.camera.y;
	
	//Draw window
	RECT window_src = {0, 0, 228, 128};
	RECT_FIXED window_dst = {
		FIXED_DEC(-170,1) - fx,
		FIXED_DEC(-125,1) - fy,
		FIXED_DEC(216,1),
		FIXED_DEC(120,1)
	};
	
	Stage_DrawTex(&this->tex_back1, &window_src, &window_dst, stage.camera.bzoom);
	
	//Draw window light
	RECT windowl_src = {0, 128, 255, 127};
	RECT_FIXED windowl_dst = {
		FIXED_DEC(-130,1) - fx,
		FIXED_DEC(44,1) - fy,
		FIXED_DEC(350,1),
		FIXED_DEC(148,1)
	};
	
	Stage_DrawTex(&this->tex_back1, &windowl_src, &windowl_dst, stage.camera.bzoom);
	
	//Draw background
	RECT back_src = {0, 0, 256, 256};
	RECT_FIXED back_dst = {
		FIXED_DEC(-185,1) - fx,
		FIXED_DEC(-125,1) - fy,
		FIXED_DEC(353,1),
		FIXED_DEC(267,1)
	};
	
	Stage_DrawTex(&this->tex_back0, &back_src, &back_dst, stage.camera.bzoom);
}

void Back_Week2_Free(StageBack *back)
{
	Back_Week2 *this = (Back_Week2*)back;
	
	//Free structure
	Mem_Free(this);
}

StageBack *Back_Week2_New(void)
{
	//Allocate background structure
	Back_Week2 *this = (Back_Week2*)Mem_Alloc(sizeof(Back_Week2));
	if (this == NULL)
		return NULL;
	
	//Set background functions
	this->back.draw_fg = Back_Week2_DrawThunder;
	this->back.draw_md = NULL;
	this->back.draw_bg = Back_Week2_DrawBG;
	this->back.free = Back_Week2_Free;

	//TODO: make sure make fade be 0 to avoid using blend in the start of da song
	this->thunder_fade = 0;
	
	//Load background textures
	IO_Data arc_back = IO_Read("\\WEEK2\\BACK.ARC;1");
	Gfx_LoadTex(&this->tex_back0, Archive_Find(arc_back, "back0.tim"), 0);
	Gfx_LoadTex(&this->tex_back1, Archive_Find(arc_back, "back1.tim"), 0);
	Gfx_LoadTex(&this->tex_back2, Archive_Find(arc_back, "back2.tim"), 0);
	Mem_Free(arc_back);
	
	return (StageBack*)this;
}
