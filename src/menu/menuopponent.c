/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "menuopponent.h"

#include "../engine/mem.h"
#include "../engine/archive.h"
#include "../stage.h"
#include "../engine/random.h"
#include "../main.h"

//Menu Opponent types
enum
{
	MenuOpponent_ArcMain_Dad,
	MenuOpponent_ArcMain_Spooky,
	MenuOpponent_ArcMain_Pico,
	MenuOpponent_ArcMain_Mom,
	MenuOpponent_ArcMain_Xmasp0,
	MenuOpponent_ArcMain_Xmasp1,
	MenuOpponent_ArcMain_Senpai,
	
	MenuOpponent_ArcMain_Max,
};

typedef struct
{
	//Character base structure
	Character character;
	
	//Render data and state
	IO_Data arc_main;
	IO_Data arc_ptr[MenuOpponent_ArcMain_Max];

	u8 opponent;
	
	Gfx_Tex tex;
	u8 frame, tex_id;
} Char_MenuOpponent;

//Menu Opponent definitions
static const CharFrame char_menuopponent_frame[] = {
	{MenuOpponent_ArcMain_Dad, { 18,   3,  67, 123}, { 50,  122}}, //0 dad 1
	{MenuOpponent_ArcMain_Dad, { 89,   2,  68, 121}, { 50,  121}}, //1 dad 2
	{MenuOpponent_ArcMain_Dad, { 10, 132,  68, 120}, { 50,  120}}, //2 dad 3
	{MenuOpponent_ArcMain_Dad, { 84, 131,  66, 123}, { 49,  122}}, //3 dad 4

	{MenuOpponent_ArcMain_Spooky, { 12,   1,  58,  84}, { 58,   84}}, //4 spooky 1
	{MenuOpponent_ArcMain_Spooky, { 72,   1,  58,  83}, { 58,   83}}, //5 spooky 2
	{MenuOpponent_ArcMain_Spooky, { 19,  89,  54,  75}, { 54,   75}}, //6 spooky 3
	{MenuOpponent_ArcMain_Spooky, { 75,  89,  53,  76}, { 53,   76}}, //7 spooky 4
	{MenuOpponent_ArcMain_Spooky, { 19, 167,  76,  82}, { 67,   81}}, //8 spooky 5
	{MenuOpponent_ArcMain_Spooky, { 99, 168,  73,  80}, { 64,   80}}, //9 spooky 6

	{MenuOpponent_ArcMain_Pico, {  1,   3,  68,  72}, { 40,   72}}, //10 pico 1
	{MenuOpponent_ArcMain_Pico, { 70,   2,  68,  74}, { 39,   74}}, //11 pico 2
	{MenuOpponent_ArcMain_Pico, {  6,  77,  70,  74}, { 39,   74}}, //12 pico 3
	{MenuOpponent_ArcMain_Pico, { 83,  78,  70,  74}, { 38,   74}}, //13 pico 4

	{MenuOpponent_ArcMain_Mom, {  9,   1,  58, 126}, { 43,  117}}, //14 mom 1
	{MenuOpponent_ArcMain_Mom, { 75,   1,  58, 124}, { 44,  115}}, //15 mom 2
	{MenuOpponent_ArcMain_Mom, {141,   2,  59, 125}, { 44,  116}}, //16 mom 3
	{MenuOpponent_ArcMain_Mom, { 68, 128,  58, 126}, { 44,  117}}, //17 mom 4

	{MenuOpponent_ArcMain_Xmasp0, {  3,   2, 140, 124}, { 78,  122}}, //18 xmasp 1
	{MenuOpponent_ArcMain_Xmasp0, {  2, 132, 140, 122}, { 79,  119}}, //19 xmasp 2
	{MenuOpponent_ArcMain_Xmasp1, {  3,   4, 140, 123}, { 78,  120}}, //20 xmasp 3
	{MenuOpponent_ArcMain_Xmasp1, {  4, 129, 138, 124}, { 76,  122}}, //21 xmasp 4

	{MenuOpponent_ArcMain_Senpai, {  3,   5, 112,  96}, { 78,  122}}, //22 senpai 1
	{MenuOpponent_ArcMain_Senpai, {120,   5, 112,  96}, { 78,  122}}, //23 senpai 2
	{MenuOpponent_ArcMain_Senpai, {  2, 110, 112,  96}, { 78,  122}}, //24 senpai 3
	{MenuOpponent_ArcMain_Senpai, {119, 110, 112,  96}, { 78,  122}}, //25 senpai 4
};

static const Animation char_menuopponent_anim[6] = { // how many "constants" it have aka opponents
	{2, (const u8[]){ 1,  2,  3,  0, ASCR_BACK, 1}}, //Dad
	{2, (const u8[]){ 4,  5,  6,  7,  8,  9,  6,  7, ASCR_CHGANI, 1}},   //Spooky
	{2, (const u8[]){10, 11, 12, 13, ASCR_BACK, 1}},   //Pico
	{2, (const u8[]){14, 15, 16, 17, ASCR_BACK, 1}},   //Mom
	{2, (const u8[]){18, 19, 20, 21, ASCR_BACK, 1}},   //Xmasp
	{2, (const u8[]){22, 23, 24, 25, ASCR_BACK, 1}},   //Senpai
};

//Boyfriend opponent functions
void Char_MenuOpponent_SetFrame(void *user, u8 frame)
{
	Char_MenuOpponent *this = (Char_MenuOpponent*)user;
	
	//Check if this is a new frame
	if (frame != this->frame)
	{
		//Check if new art shall be loaded
		const CharFrame *cframe = &char_menuopponent_frame[this->frame = frame];
		if (cframe->tex != this->tex_id)
			Gfx_LoadTex(&this->tex, this->arc_ptr[this->tex_id = cframe->tex], 0);
	}
}

void Char_MenuOpponent_Tick(Character *character)
{
	Char_MenuOpponent *this = (Char_MenuOpponent*)character;
	
	if (stage.flag & STAGE_FLAG_JUST_STEP)
	{
		//Perform idle dance
		if (Animatable_Ended(&character->animatable) &&
			(stage.song_step & 0x7) == 0)
			character->set_anim(character, this->opponent);
	}
	
	//Animate and draw character
	Animatable_Animate(&character->animatable, (void*)this, Char_MenuOpponent_SetFrame);
	Character_Draw(character, &this->tex, &char_menuopponent_frame[this->frame]);
}

void Char_MenuOpponent_SetAnim(Character *character, u8 anim)
{
	Char_MenuOpponent *this = (Char_MenuOpponent*)character;

	//Set animation
	this->opponent = anim;

	Animatable_SetAnim(&character->animatable, this->opponent);
	Character_CheckStartSing(character);
}

void Char_MenuOpponent_Free(Character *character)
{
	Char_MenuOpponent *this = (Char_MenuOpponent*)character;
	
	//Free art
	Mem_Free(this->arc_main);
}

Character *Char_MenuOpponent_New(fixed_t x, fixed_t y)
{
	//Allocate boyfriend object
	Char_MenuOpponent *this = Mem_Alloc(sizeof(Char_MenuOpponent));
	if (this == NULL)
	{
		sprintf(error_msg, "[Char_MenuOpponent_New] Failed to allocate boyfriend object");
		ErrorLock();
		return NULL;
	}
	
	//Initialize character
	this->character.tick = Char_MenuOpponent_Tick;
	this->character.set_anim = Char_MenuOpponent_SetAnim;
	this->character.free = Char_MenuOpponent_Free;
	
	Animatable_Init(&this->character.animatable, char_menuopponent_anim);
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
	this->arc_main = IO_Read("\\MENU\\OPPO.ARC;1");
	
	const char **pathp = (const char *[]){
		"dad.tim",   //MenuOpponent_ArcMain_Dad
		"spooky.tim",  //MenuOpponent_ArcMain_Spooky
		"pico.tim", //MenuOpponent_ArcMain_Pico
		"mom.tim",  //MenuOpponent_ArcMain_Mom
		"xmasp0.tim",  //MenuOpponent_ArcMain_Xmasp0
		"xmasp1.tim",  //MenuOpponent_ArcMain_Xmasp1
		"senpai.tim",  //MenuOpponent_ArcMain_Senpai
		NULL
	};
	IO_Data *arc_ptr = this->arc_ptr;
	for (; *pathp != NULL; pathp++)
		*arc_ptr++ = Archive_Find(this->arc_main, *pathp);
	
	//Initialize render state
	this->tex_id = this->frame = 0xFF;
	
	return (Character*)this;
}
