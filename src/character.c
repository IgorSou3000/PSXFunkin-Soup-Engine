/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "character.h"

#include "mem.h"
#include "stage.h"

//Character functions
void Character_Free(Character *this)
{
	//Check if NULL
	if (this == NULL)
		return;
	
	//Free character
	this->free(this);
	Mem_Free(this);
}

void Character_Init(Character *this, fixed_t x, fixed_t y)
{
	//Perform common character initialization
	this->x = x;
	this->y = y;
	
	this->set_anim(this, CharAnim_Idle);
	this->pad_held = 0;
	
	this->sing_end = 0;
}

void Character_DrawParallax(Character *this, Gfx_Tex *tex, const CharFrame *cframe, fixed_t parallax)
{
	//Draw character
	fixed_t x = this->x - FIXED_MUL(stage.camera.x, parallax) - FIXED_MUL(cframe->off[0] << FIXED_SHIFT, this->scale);
	fixed_t y = this->y - FIXED_MUL(stage.camera.y, parallax) - FIXED_MUL(cframe->off[1] << FIXED_SHIFT, this->scale);
	
	RECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};
	RECT_FIXED dst = {
	x,
	y,
	FIXED_MUL(src.w << FIXED_SHIFT, this->scale), //scale character width
	FIXED_MUL(src.h << FIXED_SHIFT, this->scale) //scale character height
};

	Stage_DrawTex(tex, &src, &dst, stage.camera.bzoom);
}

void Character_Draw(Character *this, Gfx_Tex *tex, const CharFrame *cframe)
{
	Character_DrawParallax(this, tex, cframe, FIXED_UNIT);
}

void Character_CheckStartSing(Character *this)
{
	//Update sing end if singing animation
	if (this->animatable.anim == CharAnim_Left ||
	    this->animatable.anim == CharAnim_LeftAlt ||
	    this->animatable.anim == CharAnim_Down ||
	    this->animatable.anim == CharAnim_DownAlt ||
	    this->animatable.anim == CharAnim_Up ||
	    this->animatable.anim == CharAnim_UpAlt ||
	    this->animatable.anim == CharAnim_Right ||
	    this->animatable.anim == CharAnim_RightAlt ||
	    ((this->spec & CHAR_SPEC_MISSANIM) &&
	    (this->animatable.anim == PlayerAnim_LeftMiss ||
	     this->animatable.anim == PlayerAnim_DownMiss ||
	     this->animatable.anim == PlayerAnim_UpMiss ||
	     this->animatable.anim == PlayerAnim_RightMiss)))
		this->sing_end = stage.note_scroll + (FIXED_DEC(12,1) << 2); //1 beat
}

void Character_CheckEndSing(Character *this)
{
	if ((this->animatable.anim == CharAnim_Left ||
	     this->animatable.anim == CharAnim_LeftAlt ||
	     this->animatable.anim == CharAnim_Down ||
	     this->animatable.anim == CharAnim_DownAlt ||
	     this->animatable.anim == CharAnim_Up ||
	     this->animatable.anim == CharAnim_UpAlt ||
	     this->animatable.anim == CharAnim_Right ||
	     this->animatable.anim == CharAnim_RightAlt ||
	    ((this->spec & CHAR_SPEC_MISSANIM) &&
	    (this->animatable.anim == PlayerAnim_LeftMiss ||
	     this->animatable.anim == PlayerAnim_DownMiss ||
	     this->animatable.anim == PlayerAnim_UpMiss ||
	     this->animatable.anim == PlayerAnim_RightMiss))) &&
	    stage.note_scroll >= this->sing_end)
		this->set_anim(this, CharAnim_Idle);
}

void Character_PerformIdle(Character *this)
{
	Character_CheckEndSing(this);
	if (stage.flag & STAGE_FLAG_JUST_STEP)
	{
		if (Animatable_Ended(&this->animatable) &&
		    (this->animatable.anim != CharAnim_Left &&
		     this->animatable.anim != CharAnim_LeftAlt &&
		     this->animatable.anim != CharAnim_Down &&
		     this->animatable.anim != CharAnim_DownAlt &&
		     this->animatable.anim != CharAnim_Up &&
		     this->animatable.anim != CharAnim_UpAlt &&
		     this->animatable.anim != CharAnim_Right &&
		     this->animatable.anim != CharAnim_RightAlt) &&
		    (stage.song_step & 0x7) == 0)
			this->set_anim(this, CharAnim_Idle);
	}
}
