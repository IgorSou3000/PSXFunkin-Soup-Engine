/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "menuplayer.h"

#include "../engine/mem.h"
#include "../engine/archive.h"
#include "../stage.h"
#include "../engine/random.h"
#include "../main.h"

//Menu Player types
enum
{
	MenuPlayer_ArcMain_BF0,
	MenuPlayer_ArcMain_BF1,
	
	MenuPlayer_ArcMain_Max,
};

typedef struct
{
	//Character base structure
	Character character;
	
	//Render data and state
	IO_Data arc_main;
	IO_Data arc_ptr[MenuPlayer_ArcMain_Max];
	
	Gfx_Tex tex;
	u8 frame, tex_id;
} Char_MenuPlayer;

//Menu Player definitions
static const CharFrame char_menuplayer_frame[] = {
	{MenuPlayer_ArcMain_BF0, {  1,   1, 109, 105}, {100, 105}}, //idle0 0
	{MenuPlayer_ArcMain_BF0, {112,   1, 107, 106}, { 99, 106}}, //idle1 1
	{MenuPlayer_ArcMain_BF0, {  2, 114, 108, 104}, {100, 104}}, //idle2 2
	{MenuPlayer_ArcMain_BF0, {111, 109, 108, 110}, { 98, 110}}, //idle3 3
	{MenuPlayer_ArcMain_BF1, {  7,   4, 109, 110}, {100, 110}}, //idle4 4

	{MenuPlayer_ArcMain_BF1, {117,   4, 105, 111}, {100, 111}}, //peace0 5
	{MenuPlayer_ArcMain_BF1, {  8, 118, 110, 110}, {101, 110}}, //peace1 6
	{MenuPlayer_ArcMain_BF1, {121, 118, 111, 110}, {101, 110}}, //peace2 7
};

static const Animation char_menuplayer_anim[CharAnim_Max] = { // how many "constants" it have
	{2, (const u8[]){ 0,  2,  1,  3,  4, ASCR_BACK, 1}}, //Idle
	{2, (const u8[]){ 5,  6,  7,  7,  7,  7,  7,  7,  7,  7, ASCR_BACK, 1}},             //Peace
};

//Boyfriend player functions
void Char_MenuPlayer_SetFrame(void *user, u8 frame)
{
	Char_MenuPlayer *this = (Char_MenuPlayer*)user;
	
	//Check if this is a new frame
	if (frame != this->frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &char_menuplayer_frame[this->frame = frame];
		if (cframe->tex != this->tex_id)
			Gfx_LoadTex(&this->tex, this->arc_ptr[this->tex_id = cframe->tex], 0);
	}
}

void Char_MenuPlayer_Tick(Character *character)
{
	Char_MenuPlayer *this = (Char_MenuPlayer*)character;
	
	//Handle animation updates
	if (stage.flag & STAGE_FLAG_JUST_STEP)
	{
		//Perform idle dance
		if (Animatable_Ended(&character->animatable) &&
			(stage.song_step & 0x7) == 0)
			character->set_anim(character, 0);
	}
	
	//Animate and draw character
	Animatable_Animate(&character->animatable, (void*)this, Char_MenuPlayer_SetFrame);
	Character_Draw(character, &this->tex, &char_menuplayer_frame[this->frame]);
}

void Char_MenuPlayer_SetAnim(Character *character, u8 anim)
{
	Char_MenuPlayer *this = (Char_MenuPlayer*)character;

	//Set animation
	Animatable_SetAnim(&character->animatable, anim);
	Character_CheckStartSing(character);
}

void Char_MenuPlayer_Free(Character *character)
{
	Char_MenuPlayer *this = (Char_MenuPlayer*)character;
	
	//Free art
	Mem_Free(this->arc_main);
}

Character *Char_MenuPlayer_New(fixed_t x, fixed_t y)
{
	//Allocate boyfriend object
	Char_MenuPlayer *this = Mem_Alloc(sizeof(Char_MenuPlayer));
	if (this == NULL)
	{
		sprintf(error_msg, "[Char_MenuPlayer_New] Failed to allocate boyfriend object");
		ErrorLock();
		return NULL;
	}
	
	//Initialize character
	this->character.tick = Char_MenuPlayer_Tick;
	this->character.set_anim = Char_MenuPlayer_SetAnim;
	this->character.free = Char_MenuPlayer_Free;
	
	Animatable_Init(&this->character.animatable, char_menuplayer_anim);
	Character_Init((Character*)this, x, y);
	
	//Set character information
	this->character.spec = CHAR_SPEC_MISSANIM;
	
	//icon
	this->character.health_i = 0;

	//health bar color
	this->character.health_bar = 0xFF29B5D6;
	
	this->character.focus_x = FIXED_DEC(-50,1);
	this->character.focus_y = (stage.stage_id == StageId_1_4) ? FIXED_DEC(-85,1) : FIXED_DEC(-65,1);
	this->character.focus_zoom = FIXED_DEC(1,1);

	//character scale
	this->character.scale = FIXED_DEC(100,100);
	
	//Load art
	this->arc_main = IO_Read("\\MENU\\PLAYER.ARC;1");
	
	const char **pathp = (const char *[]){
		"bf0.tim",   //MenuPlayer_ArcMain_BF0
		"bf1.tim",   //MenuPlayer_ArcMain_BF1
		NULL
	};
	IO_Data *arc_ptr = this->arc_ptr;
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_main, *pathp);
	
	//Initialize render state
	this->tex_id = this->frame = 0xFF;
	
	return (Character*)this;
}
