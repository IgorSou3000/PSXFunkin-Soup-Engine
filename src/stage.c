/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "stage.h"

#include "engine/mem.h"
#include "engine/timer.h"
#include "engine/audio.h"
#include "engine/pad.h"
#include "main.h"
#include "engine/random.h"
#include "engine/movie.h"

#include "engine/save.h"
#include "debug.h"
#include "menu/menu.h"
#include "engine/trans.h"
#include "engine/loadscr.h"

#include "object/combo.h"
#include "object/splash.h"

//Stage constants
//#define STAGE_NOHUD //Disable the HUD

//#define STAGE_FREECAM //Freecam
//#define STAGE_DEBUG //Game is in debug mode

static const u16 note_key[] = {INPUT_LEFT, INPUT_DOWN, INPUT_UP, INPUT_RIGHT};
static const u8 note_anims[4][3] = {
	{CharAnim_Left,  CharAnim_LeftAlt,  PlayerAnim_LeftMiss},
	{CharAnim_Down,  CharAnim_DownAlt,  PlayerAnim_DownMiss},
	{CharAnim_Up,    CharAnim_UpAlt,    PlayerAnim_UpMiss},
	{CharAnim_Right, CharAnim_RightAlt, PlayerAnim_RightMiss},
};

//Stage definitions
#include "character/bf.h"
#include "character/bfweeb.h"
#include "character/dad.h"
#include "character/spook.h"
#include "character/monster.h"
#include "character/pico.h"
#include "character/mom.h"
#include "character/xmasbf.h"
#include "character/xmasgf.h"
#include "character/xmasp.h"
#include "character/monsterx.h"
#include "character/senpai.h"
#include "character/senpaim.h"
#include "character/spirit.h"
#include "character/tank.h"
#include "character/gf.h"
#include "character/gfweeb.h"
#include "character/clucky.h"

#include "stage/dummy.h"
#include "stage/week1.h"
#include "stage/week2.h"
#include "stage/week3.h"
#include "stage/week4.h"
#include "stage/week5.h"
#include "stage/week6.h"

static const StageDef stage_defs[StageId_Max] = {
	#include "stagedef_disc1.h"
};

//Stage state
Stage stage;

//Stage music functions
static void Stage_StartVocal(void)
{
	if (!(stage.flag & STAGE_FLAG_VOCAL_ACTIVE))
	{
		Audio_ChannelXA(stage.stage_def->music_channel);
		stage.flag |= STAGE_FLAG_VOCAL_ACTIVE;
	}
}

static void Stage_CutVocal(void)
{
	if (stage.flag & STAGE_FLAG_VOCAL_ACTIVE)
	{
		Audio_ChannelXA(stage.stage_def->music_channel + 1);
		stage.flag &= ~STAGE_FLAG_VOCAL_ACTIVE;
	}
}

//Stage camera functions
static void Stage_FocusCharacter(Character *ch, fixed_t div)
{
	//Use character focus settings to update target position and zoom
	stage.camera.tx = ch->x + ch->focus_x;
	stage.camera.ty = ch->y + ch->focus_y;
	stage.camera.tz = ch->focus_zoom;
	stage.camera.td = div;

	//psych engine camera
	if (stage.prefs.psychcamera)
	{
		switch (ch->animatable.anim)
		{
			case CharAnim_Left:
			case CharAnim_LeftAlt:
				stage.camera.x -= FIXED_DEC(5,10);
			break;

			case CharAnim_Down:
			case CharAnim_DownAlt:
				stage.camera.y += FIXED_DEC(5,10);
			break;

			case CharAnim_Up:
			case CharAnim_UpAlt:
				stage.camera.y -= FIXED_DEC(5,10);
			break;

			case CharAnim_Right:
			case CharAnim_RightAlt:
				stage.camera.x += FIXED_DEC(5,10);
			break;
			default:

			break;
		}
	}
}

static void Stage_ScrollCamera(void)
{
		//Get delta position
		fixed_t dx = stage.camera.tx - stage.camera.x;
		fixed_t dy = stage.camera.ty - stage.camera.y;
		fixed_t dz = stage.camera.tz - stage.camera.zoom;
		
		//Scroll based off current divisor
		stage.camera.x += FIXED_MUL(dx, stage.camera.td);
		stage.camera.y += FIXED_MUL(dy, stage.camera.td);
		stage.camera.zoom += FIXED_MUL(dz, stage.camera.td);
		
		//Shake in Week 4
		if (stage.stage_id >= StageId_4_1 && stage.stage_id <= StageId_4_3)
		{
			stage.camera.x += RandomRange(FIXED_DEC(-1,10),FIXED_DEC(1,10));
			stage.camera.y += RandomRange(FIXED_DEC(-25,100),FIXED_DEC(25,100));
		}
	
	//Update other camera stuff
	stage.camera.bzoom = FIXED_MUL(stage.camera.zoom, stage.bump);
}

//Stage section functions
static void Stage_ChangeBPM(u16 bpm, u16 step)
{
	//Update last BPM
	stage.last_bpm = bpm;
	
	//Update timing base
	if (stage.step_crochet)
		stage.time_base += FIXED_DIV(((fixed_t)step - stage.step_base) << FIXED_SHIFT, stage.step_crochet);
	stage.step_base = step;
	
	//Get new crochet and times
	stage.step_crochet = ((fixed_t)bpm << FIXED_SHIFT) * 8 / 240; //15/12/24
	stage.step_time = FIXED_DIV(FIXED_DEC(12,1), stage.step_crochet);
	
	//Get new crochet based values
	stage.early_safe = stage.late_safe = stage.step_crochet / 6; //166 ms
	stage.late_sus_safe = (stage.late_safe * 3) >> 1;
	stage.early_sus_safe = stage.early_safe >> 1;
}

static Section *Stage_GetPrevSection(Section *section)
{
	if (section > stage.sections)
		return section - 1;
	return NULL;
}

static u16 Stage_GetSectionStart(Section *section)
{
	Section *prev = Stage_GetPrevSection(section);
	if (prev == NULL)
		return 0;
	return prev->end;
}

//Section scroll structure
typedef struct
{
	fixed_t start;   //Seconds
	fixed_t length;  //Seconds
	u16 start_step;  //Sub-steps
	u16 length_step; //Sub-steps
	
	fixed_t size; //Note height
} SectionScroll;

static void Stage_GetSectionScroll(SectionScroll *scroll, Section *section)
{
	//Get BPM
	u16 bpm = section->flag & SECTION_FLAG_BPM_MASK;
	
	//Get section step info
	scroll->start_step = Stage_GetSectionStart(section);
	scroll->length_step = section->end - scroll->start_step;
	
	//Get section time length
	scroll->length = (scroll->length_step * FIXED_DEC(15,1) / 12) * 24 / bpm;
	
	//Get note height
	scroll->size = FIXED_MUL(stage.speed, scroll->length * (12 * 150) / scroll->length_step) + FIXED_UNIT;
}

//Note hit detection
static u8 Stage_HitNote(PlayerState *this, u8 type, fixed_t offset)
{
	//Get hit type
	if (offset < 0)
		offset = -offset;
	
	u8 hit_type;

	if (offset > stage.late_safe * 81 / 100) //135ms
		hit_type = 3; //SHIT
	else if (offset > stage.late_safe * 54 / 100) //90ms
		hit_type = 2; //BAD
	else if (offset > stage.late_safe * 27 / 100) //45ms
		hit_type = 1; //GOOD
	else
		hit_type = 0; //SICK
	
	//Increment combo and score
	this->combo++;
	
	static const s32 score_inc[] = {
		35, //SICK
		20, //GOOD
		10, //BAD
		 5, //SHIT
	};
	this->score += score_inc[hit_type];
	this->refresh_score = true;

	this->min_accuracy += 20;
	this->max_accuracy += (hit_type * 10) + 20;
	this->refresh_accuracy = true;
	
	//Restore vocals and health
	Stage_StartVocal();
	this->health += 230;
	
	//Create combo object telling of our combo
	Obj_Combo *combo = Obj_Combo_New(
		this->character->focus_x,
		this->character->focus_y,
		hit_type,
		this->combo >= 10 ? this->combo : 0xFFFF
	);
	if (combo != NULL)
		ObjectList_Add(&stage.objlist_fg, (Object*)combo);
	
	//Create note splashes if SICK
	if (hit_type == 0)
	{
		for (int i = 0; i < 3; i++)
		{
			//Create splash object
			Obj_Splash *splash = Obj_Splash_New(
				stage.note_x[type],
				stage.note_y[type] * (stage.prefs.downscroll ? -1 : 1),
				type & 0x3
			);
			if (splash != NULL)
				ObjectList_Add(&stage.objlist_splash, (Object*)splash);
		}
	}
	
	return hit_type;
}

static void Stage_MissNote(PlayerState *this)
{
	if (this->combo)
	{
		//Kill combo
		if (stage.gf != NULL && this->combo > 5)
			stage.gf->set_anim(stage.gf, CharAnim_DownAlt); //Cry if we lost a large combo
		this->combo = 0;
		
		//Create combo object telling of our lost combo
		Obj_Combo *combo = Obj_Combo_New(
			this->character->focus_x,
			this->character->focus_y,
			0xFF,
			0
		);
		if (combo != NULL)
			ObjectList_Add(&stage.objlist_fg, (Object*)combo);
	}
}

static void Stage_NoteCheck(PlayerState *this, u8 type)
{
	//Perform note check
	for (Note *note = stage.cur_note;; note++)
	{
		if (!(note->type & NOTE_FLAG_MINE))
		{
			//Check if note can be hit
			fixed_t note_fp = (fixed_t)note->pos << FIXED_SHIFT;
			if (note_fp - stage.early_safe > stage.note_scroll)
				break;
			if (note_fp + stage.late_safe < stage.note_scroll)
				continue;
			if ((note->type & NOTE_FLAG_HIT) || (note->type & (NOTE_FLAG_OPPONENT | 0x3)) != type || (note->type & NOTE_FLAG_SUSTAIN))
				continue;
			
			//Hit the note
			note->type |= NOTE_FLAG_HIT;
			
			this->character->set_anim(this->character, note_anims[type & 0x3][(note->type & NOTE_FLAG_ALT_ANIM) != 0]);
			u8 hit_type = Stage_HitNote(this, type, stage.note_scroll - note_fp);
			this->arrow_hitan[type & 0x3] = stage.step_time;	

				(void)hit_type;
			return;
		}
		else
		{
			//Check if mine can be hit
			fixed_t note_fp = (fixed_t)note->pos << FIXED_SHIFT;
			if (note_fp - (stage.late_safe * 3 / 5) > stage.note_scroll)
				break;
			if (note_fp + (stage.late_safe * 2 / 5) < stage.note_scroll)
				continue;
			if ((note->type & NOTE_FLAG_HIT) || (note->type & (NOTE_FLAG_OPPONENT | 0x3)) != type || (note->type & NOTE_FLAG_SUSTAIN))
				continue;
			
			//Hit the mine
			note->type |= NOTE_FLAG_HIT;
			
			if (stage.stage_id == StageId_2_4)
				this->health = -0x7000;
			else
				this->health -= 2000;
			if (this->character->spec & CHAR_SPEC_MISSANIM)
				this->character->set_anim(this->character, note_anims[type & 0x3][2]);
			else
				this->character->set_anim(this->character, note_anims[type & 0x3][0]);
			this->arrow_hitan[type & 0x3] = -1;
			return;
		}
	}
	
	//Missed a note
	this->arrow_hitan[type & 0x3] = -1;
	
	if (!stage.prefs.ghost)
	{
		if (this->character->spec & CHAR_SPEC_MISSANIM)
			this->character->set_anim(this->character, note_anims[type & 0x3][2]);
		else
			this->character->set_anim(this->character, note_anims[type & 0x3][0]);
		Stage_MissNote(this);
		
		this->health -= 1000;
		this->score -= 5;
		this->refresh_score = true;

		this->miss++;
		this->refresh_miss = true;
	}
}

static void Stage_SustainCheck(PlayerState *this, u8 type)
{
	//Perform note check
	for (Note *note = stage.cur_note;; note++)
	{
		//Check if note can be hit
		fixed_t note_fp = (fixed_t)note->pos << FIXED_SHIFT;
		if (note_fp - stage.early_sus_safe > stage.note_scroll)
			break;
		if (note_fp + stage.late_sus_safe < stage.note_scroll)
			continue;
		if ((note->type & NOTE_FLAG_HIT) || (note->type & (NOTE_FLAG_OPPONENT | 0x3)) != type || !(note->type & NOTE_FLAG_SUSTAIN))
			continue;
		
		//Hit the note
		note->type |= NOTE_FLAG_HIT;
		
		this->character->set_anim(this->character, note_anims[type & 0x3][(note->type & NOTE_FLAG_ALT_ANIM) != 0]);
		
		Stage_StartVocal();
		this->health += 230;
		this->arrow_hitan[type & 0x3] = stage.step_time;
	}
}

static void Stage_ProcessPlayer(PlayerState *this, Pad *pad, boolean playing)
{
	//Handle player note presses
	if (!stage.prefs.botplay)
	{
		if (playing)
		{
			u8 i = (this->character == stage.opponent) ? NOTE_FLAG_OPPONENT : 0;
			
			this->pad_held = this->character->pad_held = pad->held;
			this->pad_press = pad->press;
			
			if (this->pad_held & INPUT_LEFT)
				Stage_SustainCheck(this, 0 | i);
			if (this->pad_held & INPUT_DOWN)
				Stage_SustainCheck(this, 1 | i);
			if (this->pad_held & INPUT_UP)
				Stage_SustainCheck(this, 2 | i);
			if (this->pad_held & INPUT_RIGHT)
				Stage_SustainCheck(this, 3 | i);
			
			if (this->pad_press & INPUT_LEFT)
				Stage_NoteCheck(this, 0 | i);
			if (this->pad_press & INPUT_DOWN)
				Stage_NoteCheck(this, 1 | i);
			if (this->pad_press & INPUT_UP)
				Stage_NoteCheck(this, 2 | i);
			if (this->pad_press & INPUT_RIGHT)
				Stage_NoteCheck(this, 3 | i);
		}
		else
		{
			this->pad_held = this->character->pad_held = 0;
			this->pad_press = 0;
		}
}
		//Do perfect note checks
	if (stage.prefs.botplay)
	{
		if (playing)
		{
			u8 i = (this->character == stage.opponent) ? NOTE_FLAG_OPPONENT : 0;
			
			u8 hit[4] = {0, 0, 0, 0};
			for (Note *note = stage.cur_note;; note++)
			{
				//Check if note can be hit
				fixed_t note_fp = (fixed_t)note->pos << FIXED_SHIFT;
				if (note_fp - stage.early_safe - FIXED_DEC(12,1) > stage.note_scroll)
					break;
				if (note_fp + stage.late_safe < stage.note_scroll)
					continue;
				if ((note->type & NOTE_FLAG_MINE) || (note->type & NOTE_FLAG_OPPONENT) != i)
					continue;
				
				//Handle note hit
				if (!(note->type & NOTE_FLAG_SUSTAIN))
				{
					if (note->type & NOTE_FLAG_HIT)
						continue;
					if (stage.note_scroll >= note_fp)
						hit[note->type & 0x3] |= 1;
					else if (!(hit[note->type & 0x3] & 8))
						hit[note->type & 0x3] |= 2;
				}
				else if (!(hit[note->type & 0x3] & 2))
				{
					if (stage.note_scroll <= note_fp)
						hit[note->type & 0x3] |= 4;
					hit[note->type & 0x3] |= 8;
				}
			}
			
			//Handle input
			this->pad_held = 0;
			this->pad_press = 0;
			
			for (u8 j = 0; j < 4; j++)
			{
				if (hit[j] & 5)
				{
					this->pad_held |= note_key[j];
					Stage_SustainCheck(this, j | i);
				}
				if (hit[j] & 1)
				{
					this->pad_press |= note_key[j];
					Stage_NoteCheck(this, j | i);
				}
			}
			
			this->character->pad_held = this->pad_held;
		}
		else
		{
			this->pad_held = this->character->pad_held = 0;
			this->pad_press = 0;
		}
	}
}

//Stage drawing functions
void Stage_DrawTexCol(Gfx_Tex *tex, const RECT *src, const RECT_FIXED *dst, fixed_t zoom, u8 cr, u8 cg, u8 cb)
{
	fixed_t xz = dst->x;
	fixed_t yz = dst->y;
	fixed_t wz = dst->w;
	fixed_t hz = dst->h;
	
	if (stage.stage_id >= StageId_6_1 && stage.stage_id <= StageId_6_3)
	{
		//Handle HUD drawing
		if (tex == &stage.tex_hud0)
		{
			#ifdef STAGE_NOHUD
				return;
			#endif
			if (src->y >= 128 && src->y < 224)
			{
				//Pixel perfect scrolling
				xz &= FIXED_UAND;
				yz &= FIXED_UAND;
				wz &= FIXED_UAND;
				hz &= FIXED_UAND;
			}
		}
		else if (tex == &stage.tex_hud1)
		{
			#ifdef STAGE_NOHUD
				return;
			#endif
		}
		else
		{
			//Pixel perfect scrolling
			xz &= FIXED_UAND;
			yz &= FIXED_UAND;
			wz &= FIXED_UAND;
			hz &= FIXED_UAND;
		}
	}
	else
	{
		//Don't draw if HUD and is disabled
		if (tex == &stage.tex_hud0 || tex == &stage.tex_hud1)
		{
			#ifdef STAGE_NOHUD
				return;
			#endif
		}
	}
	
	fixed_t l = (SCREEN_WIDTH2  << FIXED_SHIFT) + FIXED_MUL(xz, zoom);// + FIXED_DEC(1,2);
	fixed_t t = (SCREEN_HEIGHT2 << FIXED_SHIFT) + FIXED_MUL(yz, zoom);// + FIXED_DEC(1,2);
	fixed_t r = l + FIXED_MUL(wz, zoom);
	fixed_t b = t + FIXED_MUL(hz, zoom);
	
	l >>= FIXED_SHIFT;
	t >>= FIXED_SHIFT;
	r >>= FIXED_SHIFT;
	b >>= FIXED_SHIFT;
	
	RECT sdst = {
		l,
		t,
		r - l,
		b - t,
	};
	Gfx_DrawTexCol(tex, src, &sdst, cr, cg, cb);
}

void Stage_DrawTex(Gfx_Tex *tex, const RECT *src, const RECT_FIXED *dst, fixed_t zoom)
{
	Stage_DrawTexCol(tex, src, dst, zoom, 0x80, 0x80, 0x80);
}

void Stage_BlendTex(Gfx_Tex *tex, const RECT *src, const RECT_FIXED *dst, fixed_t zoom, u8 opacity, u8 mode)
{
	fixed_t xz = dst->x;
	fixed_t yz = dst->y;
	fixed_t wz = dst->w;
	fixed_t hz = dst->h;
	
	if (stage.stage_id >= StageId_6_1 && stage.stage_id <= StageId_6_3)
	{
		//Handle HUD drawing
		if (tex == &stage.tex_hud0)
		{
			#ifdef STAGE_NOHUD
				return;
			#endif
			if (src->y >= 128 && src->y < 224)
			{
				//Pixel perfect scrolling
				xz &= FIXED_UAND;
				yz &= FIXED_UAND;
				wz &= FIXED_UAND;
				hz &= FIXED_UAND;
			}
		}
		else if (tex == &stage.tex_hud1)
		{
			#ifdef STAGE_NOHUD
				return;
			#endif
		}
		else
		{
			//Pixel perfect scrolling
			xz &= FIXED_UAND;
			yz &= FIXED_UAND;
			wz &= FIXED_UAND;
			hz &= FIXED_UAND;
		}
	}
	else
	{
		//Don't draw if HUD and is disabled
		if (tex == &stage.tex_hud0 || tex == &stage.tex_hud1)
		{
			#ifdef STAGE_NOHUD
				return;
			#endif
		}
	}
	
	fixed_t l = (SCREEN_WIDTH2  << FIXED_SHIFT) + FIXED_MUL(xz, zoom);// + FIXED_DEC(1,2);
	fixed_t t = (SCREEN_HEIGHT2 << FIXED_SHIFT) + FIXED_MUL(yz, zoom);// + FIXED_DEC(1,2);
	fixed_t r = l + FIXED_MUL(wz, zoom);
	fixed_t b = t + FIXED_MUL(hz, zoom);
	
	l >>= FIXED_SHIFT;
	t >>= FIXED_SHIFT;
	r >>= FIXED_SHIFT;
	b >>= FIXED_SHIFT;
	
	RECT sdst = {
		l,
		t,
		r - l,
		b - t,
	};
	Gfx_BlendTex(tex, src, &sdst, opacity, mode);
}


void Stage_DrawTexArb(Gfx_Tex *tex, const RECT *src, const POINT_FIXED *p0, const POINT_FIXED *p1, const POINT_FIXED *p2, const POINT_FIXED *p3, fixed_t zoom)
{
	//Don't draw if HUD and HUD is disabled
	#ifdef STAGE_NOHUD
		if (tex == &stage.tex_hud0 || tex == &stage.tex_hud1)
			return;
	#endif
	
	//Get screen-space points
	POINT s0 = {SCREEN_WIDTH2 + (FIXED_MUL(p0->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p0->y, zoom) >> FIXED_SHIFT)};
	POINT s1 = {SCREEN_WIDTH2 + (FIXED_MUL(p1->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p1->y, zoom) >> FIXED_SHIFT)};
	POINT s2 = {SCREEN_WIDTH2 + (FIXED_MUL(p2->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p2->y, zoom) >> FIXED_SHIFT)};
	POINT s3 = {SCREEN_WIDTH2 + (FIXED_MUL(p3->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p3->y, zoom) >> FIXED_SHIFT)};
	
	Gfx_DrawTexArb(tex, src, &s0, &s1, &s2, &s3);
}

void Stage_BlendTexArb(Gfx_Tex *tex, const RECT *src, const POINT_FIXED *p0, const POINT_FIXED *p1, const POINT_FIXED *p2, const POINT_FIXED *p3, fixed_t zoom, u8 mode)
{
	//Don't draw if HUD and HUD is disabled
	#ifdef STAGE_NOHUD
		if (tex == &stage.tex_hud0 || tex == &stage.tex_hud1)
			return;
	#endif
	
	//Get screen-space points
	POINT s0 = {SCREEN_WIDTH2 + (FIXED_MUL(p0->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p0->y, zoom) >> FIXED_SHIFT)};
	POINT s1 = {SCREEN_WIDTH2 + (FIXED_MUL(p1->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p1->y, zoom) >> FIXED_SHIFT)};
	POINT s2 = {SCREEN_WIDTH2 + (FIXED_MUL(p2->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p2->y, zoom) >> FIXED_SHIFT)};
	POINT s3 = {SCREEN_WIDTH2 + (FIXED_MUL(p3->x, zoom) >> FIXED_SHIFT), SCREEN_HEIGHT2 + (FIXED_MUL(p3->y, zoom) >> FIXED_SHIFT)};
	
	Gfx_BlendTexArb(tex, src, &s0, &s1, &s2, &s3, mode);
}

//Timer Code
#include "engine/audio_def.h"

static void Stage_TimerGetLength(void)
{
	const XA_TrackDef *track_def = &xa_tracks[stage.stage_def->music_track]; //get length of the music
	stage.timerlength = (track_def->length / 75 / IO_SECT_SIZE) - 1; //convert in seconds

	(void)xa_paths; //just for remove boring warning

	//minutes stuff
	stage.timermin = stage.timerlength / 60;

	//seconds stuff
	stage.timersec = stage.timerlength % 60;

	stage.timepassed = 0;
}

static void Stage_TimerTick(void)
{
	if (stage.song_step >= 0)
	{
		//increasing variable using delta time to avoid desync
		if (stage.timepassed < stage.timerlength * 60)
			stage.timepassed += timer_dt/12;

		//seconds checker
		if ((stage.timepassed % 60) == 59) //everytime dat variable be a multiple of 59, remove 1 second
		{
			stage.timersec--;

			if (stage.timersec < 0)
				stage.timersec += 60;
		}

		//minutes checker
		if (stage.timersec >= 59 && (stage.timepassed % 60) == 59)
			stage.timermin--;
	}

		
	//don't draw timer if "show timer" option not be enable
	if (stage.prefs.showtimer)
	{
		char text[0x80];

		//format string
		sprintf(text, "%d : %s%d", stage.timermin, (stage.timersec > 9) ?"" :"0", stage.timersec); //making this for avoid cases like 1:4

		//Draw text
		stage.font_cdr.draw(&stage.font_cdr,
		text,
		-18,
		(stage.prefs.downscroll) ? 103 : -111,
		FontAlign_Left,
		"stage"
	);

		//draw square length
		RECT square_black = {0, 251, 111, 4};
		RECT square_fill = {0, 251, (110 * (stage.timepassed) / (stage.timerlength * 60)), 4};

		RECT_FIXED square_dst = {
		FIXED_DEC(-56,1),
		FIXED_DEC(-110,1),
		0,
		FIXED_DEC(7,1)
	};

		if (stage.prefs.downscroll)
			square_dst.y = -square_dst.y - square_dst.h + FIXED_DEC(1,1);

		square_dst.w = square_fill.w << FIXED_SHIFT;
		Stage_DrawTex(&stage.tex_hud1, &square_fill, &square_dst, stage.bump);
		
		square_dst.w = square_black.w << FIXED_SHIFT;
		Stage_DrawTexCol(&stage.tex_hud1, &square_black, &square_dst, stage.bump,  0,  0,  0);
	}
}

//Stage SFX function
static void Stage_LoadSFX(void)
{
	//clean audio ram
	Audio_ClearAlloc();

	//load 
	char text[45];
	CdlFILE file;
	for (int i = 0; i < 4; i++)
	{													//intro weeb version										 intro normal version
	sprintf(text, "\\SOUNDS\\INTRO%d%s.VAG;1", i, (stage.stage_id >= StageId_6_1  && stage.stage_id <= StageId_6_3) ? "P" : "N");
	IO_FindFile(&file, text);
    u32 *data = IO_ReadFile(&file);
    stage.sounds[i] = Audio_LoadVAGData(data, file.size);
	Mem_Free(data);
	}
}

//Stage Intro Function
static void Stage_PlayIntro(void)
{
	//normal intro
	static const RECT intro_normal_src[3] = {
		{ 27, 67, 39, 30}, //"GO!"
		{125,  4, 88, 42}, //"Set!?"
		{ 11,  3, 95, 46} //"Ready?"
	};

	static const RECT_FIXED intro_normal_dst[3] = {
		{FIXED_DEC(-35,1), FIXED_DEC(-20,1), FIXED_DEC(78,1), FIXED_DEC(60,1)}, //"GO!"
		{FIXED_DEC(-80,1), FIXED_DEC(-30,1), FIXED_DEC(176,1), FIXED_DEC(84,1)}, //"Set!?"
		{FIXED_DEC(-85,1), FIXED_DEC(-30,1), FIXED_DEC(190,1), FIXED_DEC(92,1)} //"Ready?"
	};

	//week 6 intro
	static const RECT intro_weeb_src[3] = {
		{113,144, 92, 37}, //"DATE!"
		{ 15,160, 80, 37}, //"Set!?"
		{ 16,117, 87, 41} //"Ready?"
	};

	static const RECT_FIXED intro_weeb_dst[3] = {
		{FIXED_DEC(-100,1), FIXED_DEC(-30,1), FIXED_DEC(184,1), FIXED_DEC(72,1)}, //"DATE!"
		{FIXED_DEC(-80,1), FIXED_DEC(-30,1), FIXED_DEC(160,1), FIXED_DEC(72,1)}, //"Set!?"
		{FIXED_DEC(-85,1), FIXED_DEC(-30,1), FIXED_DEC(174,1), FIXED_DEC(82,1)} //"Ready?"
	};
	
	//draw intro's
	if (stage.stage_id >= StageId_6_1 && stage.stage_id <= StageId_6_3)
		Stage_DrawTex(&stage.tex_hude, &intro_weeb_src[-stage.song_step / 6], &intro_weeb_dst[-stage.song_step / 6], stage.bump);

	else
		Stage_DrawTex(&stage.tex_hude, &intro_normal_src[-stage.song_step / 6], &intro_normal_dst[-stage.song_step / 6], stage.bump);

	//play intro's sounds
	if (stage.flag & STAGE_FLAG_JUST_STEP && (stage.song_step % 0x6) == 0)
		Audio_PlaySound(stage.sounds[stage.song_step / 6 + 4], 80);

}

//Stage HUD functions
static void Stage_DrawHealth(s16 health, u8 i, s8 ox)
{
	//Check if we should use 'dying' frame or 'winning' frame
	s8 dying, winning;
	if (ox < 0)
	{
		winning = (health <= 2000) * 72;
		dying = (health >= 18000) * 36;
	}
	else
	{
		winning = (health >= 18000) * 72;
		dying = (health <= 2000) * 36;
	}
	
	//Get src and dst
	fixed_t hx = (128 << FIXED_SHIFT) * (10000 - health) / 10000;
	RECT src = {
		(i % 2) * 108 + dying + winning,
		(i / 2) * 36,
		36,
		36
	};
	RECT_FIXED dst = {
		hx + ox * FIXED_DEC(19,1) - FIXED_DEC(12,1),
		FIXED_DEC(70,1),
		src.w << FIXED_SHIFT,
		src.h << FIXED_SHIFT
	};
	if (stage.prefs.downscroll)
		dst.y = -dst.y - FIXED_DEC(44,1);

	//invert icon
	if (stage.mode == StageMode_Swap)
	{
	dst.x += dst.w;
	dst.w = -dst.w;
	}
	
	//Draw health icon
	Stage_DrawTex(&stage.tex_hud1, &src, &dst, FIXED_MUL(stage.bump, stage.sbump));
}

static void Stage_DrawHealthBar(s16 x, s32 color)
{	
	//colors for health bar
	u8 red = (color >> 16) & 0xFF;
	u8 blue = (color >> 8) & 0xFF;
	u8 green = (color) & 0xFF;
	//Get src and dst
	RECT src = {
		0,
	  251,
		x,
		4
	};
	RECT_FIXED dst = {
		FIXED_DEC(-128,1),
		FIXED_DEC(90,1),
		src.w << FIXED_SHIFT,
		src.h << FIXED_SHIFT
	};
	if (stage.prefs.downscroll)
		dst.y = -dst.y - dst.h;
	
	Stage_DrawTexCol(&stage.tex_hud1, &src, &dst, stage.bump, red >> 1, blue >> 1, green >> 1);
}

static void Stage_DrawStrum(u8 i, RECT *note_src, RECT_FIXED *note_dst)
{
	(void)note_dst;
	
	PlayerState *this = &stage.player_state[((i ^ stage.note_swap) & NOTE_FLAG_OPPONENT) != 0];
	i &= 0x3;
	
	if (this->arrow_hitan[i] > 0)
	{
		//Play hit animation
		u8 frame = ((this->arrow_hitan[i] << 1) / stage.step_time) & 1;
		note_src->x = (i + 1) << 5;
		note_src->y = 64 - (frame << 5);
		
		this->arrow_hitan[i] -= timer_dt;
		if (this->arrow_hitan[i] <= 0)
		{
			if (this->pad_held & note_key[i])
				this->arrow_hitan[i] = 1;
			else
				this->arrow_hitan[i] = 0;
		}
	}
	else if (this->arrow_hitan[i] < 0)
	{
		//Play depress animation
		note_src->x = (i + 1) << 5;
		note_src->y = 96;
		if (!(this->pad_held & note_key[i]))
			this->arrow_hitan[i] = 0;
	}
	else
	{
		note_src->x = 0;
		note_src->y = i << 5;
	}
}

//load notes position
static void Stage_LoadNotes(void)
{
	//Middle scroll
	if (stage.prefs.middlescroll)
	{
		//BF
		stage.note_x[0 ^ stage.note_swap] = FIXED_DEC(26 - 78,1) + FIXED_DEC(SCREEN_WIDEADD,4);
		stage.note_x[1 ^ stage.note_swap] = FIXED_DEC(60 - 78,1) + FIXED_DEC(SCREEN_WIDEADD,4); //+34
		stage.note_x[2 ^ stage.note_swap] = FIXED_DEC(94 - 78,1) + FIXED_DEC(SCREEN_WIDEADD,4);
		stage.note_x[3 ^ stage.note_swap] = FIXED_DEC(128 - 78,1) + FIXED_DEC(SCREEN_WIDEADD,4);
		//Opponent
		for (u8 i = 4; i < 8; i++)
		stage.note_x[i ^ stage.note_swap] = FIXED_DEC(-400,1) - FIXED_DEC(SCREEN_WIDEADD,4);
	}

	//Normal scroll
	else
	{
		//BF
		for (u8 i = 0; i < 4; i++)
		stage.note_x[i] =  FIXED_DEC(26 + i * 34,1) + FIXED_DEC(SCREEN_WIDEADD,4); //+34

		//Opponent
		for (u8 i = 4; i < 8; i++)
		stage.note_x[i] =  FIXED_DEC(-128 + (i - 4) * 34,1) + FIXED_DEC(SCREEN_WIDEADD,4); //-34
	}
	
	//note y
	for (u8 i = 0; i < 8; i++)
	stage.note_y[i] = FIXED_DEC(32 - SCREEN_HEIGHT2, 1);
}

static void Stage_DrawNotes(void)
{
	//Check if opponent should draw as bot
	u8 bot = (stage.mode >= StageMode_2P) ? 0 : NOTE_FLAG_OPPONENT;
	
	//Initialize scroll state
	SectionScroll scroll;
	scroll.start = stage.time_base;
	
	Section *scroll_section = stage.section_base;
	Stage_GetSectionScroll(&scroll, scroll_section);
	
	//Push scroll back until cur_note is properly contained
	while (scroll.start_step > stage.cur_note->pos)
	{
		//Look for previous section
		Section *prev_section = Stage_GetPrevSection(scroll_section);
		if (prev_section == NULL)
			break;
		
		//Push scroll back
		scroll_section = prev_section;
		Stage_GetSectionScroll(&scroll, scroll_section);
		scroll.start -= scroll.length;
	}
	
	//Draw notes
	for (Note *note = stage.cur_note; note->pos != 0xFFFF; note++)
	{
		//Update scroll
		while (note->pos >= scroll_section->end)
		{
			//Push scroll forward
			scroll.start += scroll.length;
			Stage_GetSectionScroll(&scroll, ++scroll_section);
		}
		
		//Get note information
		u8 i = ((note->type ^ stage.note_swap) & NOTE_FLAG_OPPONENT) != 0;
		PlayerState *this = &stage.player_state[i];
		
		fixed_t note_fp = (fixed_t)note->pos << FIXED_SHIFT;
		fixed_t time = (scroll.start - stage.song_time) + (scroll.length * (note->pos - scroll.start_step) / scroll.length_step);
		fixed_t y = stage.note_y[note->type & 0x7] + FIXED_MUL(stage.speed, time * 150);
		
		//Check if went above screen
		if (y < FIXED_DEC(-16 - SCREEN_HEIGHT2, 1))
		{
			//Wait for note to exit late time
			if (note_fp + stage.late_safe >= stage.note_scroll)
				continue;
			
			//Miss note if player's note
			if (!((note->type ^ stage.note_swap) & (bot | NOTE_FLAG_HIT | NOTE_FLAG_MINE)))
			{
					//Missed note
					Stage_CutVocal();
					Stage_MissNote(this);
					this->health -= 475;
					this->miss++;
					this->refresh_miss = true;

					this->max_accuracy += 20;
					this->refresh_accuracy = true;
			}
			
			//Update current note
			stage.cur_note++;
		}
		else
		{
			//Don't draw if below screen
			RECT note_src;
			RECT_FIXED note_dst;
			if (y > (FIXED_DEC(SCREEN_HEIGHT,2) + scroll.size) || note->pos == 0xFFFF)
				break;
			
			//Draw note
			if (note->type & NOTE_FLAG_SUSTAIN)
			{
				//Check for sustain clipping
				fixed_t clip;
				y -= scroll.size;
				if (((note->type ^ stage.note_swap) & (bot | NOTE_FLAG_HIT)) || ((this->pad_held & note_key[note->type & 0x3]) && (note_fp + stage.late_sus_safe >= stage.note_scroll)))
				{
					clip = FIXED_DEC(32 - SCREEN_HEIGHT2, 1) - y;
					if (clip < 0)
						clip = 0;
				}
				else
				{
					clip = 0;
				}
				
				//Draw sustain
				if (note->type & NOTE_FLAG_SUSTAIN_END)
				{
					if (clip < (24 << FIXED_SHIFT))
					{
						note_src.x = 160;
						note_src.y = ((note->type & 0x3) << 5) + (clip >> FIXED_SHIFT);
						note_src.w = 32;
						note_src.h = 28 - (clip >> FIXED_SHIFT);
						
						note_dst.x = stage.note_x[note->type & 0x7] - FIXED_DEC(16,1);
						note_dst.y = y + clip;
						note_dst.w = note_src.w << FIXED_SHIFT;
						note_dst.h = (note_src.h << FIXED_SHIFT);
						
						if (stage.prefs.downscroll)
						{
							note_dst.y = -note_dst.y;
							note_dst.h = -note_dst.h;
						}
						Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
					}
				}
				else
				{
					//Get note height
					fixed_t next_time = (scroll.start - stage.song_time) + (scroll.length * (note->pos + 12 - scroll.start_step) / scroll.length_step);
					fixed_t next_y = stage.note_y[note->type & 0x7] + FIXED_MUL(stage.speed, next_time * 150) - scroll.size;
					fixed_t next_size = next_y - y;
					
					if (clip < next_size)
					{
						note_src.x = 160;
						note_src.y = ((note->type & 0x3) << 5);
						note_src.w = 32;
						note_src.h = 16;
						
						note_dst.x = stage.note_x[note->type & 0x7] - FIXED_DEC(16,1);
						note_dst.y = y + clip;
						note_dst.w = note_src.w << FIXED_SHIFT;
						note_dst.h = (next_y - y) - clip;
						
						if (stage.prefs.downscroll)
							note_dst.y = -note_dst.y - note_dst.h;
						Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
					}
				}
			}
			else if (note->type & NOTE_FLAG_MINE)
			{
				//Don't draw if already hit
				if (note->type & NOTE_FLAG_HIT)
					continue;
				
				//Draw note body
				note_src.x = 192 + ((note->type & 0x1) << 5);
				note_src.y = (note->type & 0x2) << 4;
				note_src.w = 32;
				note_src.h = 32;
				
				note_dst.x = stage.note_x[note->type & 0x7] - FIXED_DEC(16,1);
				note_dst.y = y - FIXED_DEC(16,1);
				note_dst.w = note_src.w << FIXED_SHIFT;
				note_dst.h = note_src.h << FIXED_SHIFT;
				
				if (stage.prefs.downscroll)
					note_dst.y = -note_dst.y - note_dst.h;
				Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
				
				if (stage.stage_id == StageId_2_4)
				{
					//Draw note halo
					note_src.x = 160;
					note_src.y = 128 + ((animf_count & 0x3) << 3);
					note_src.w = 32;
					note_src.h = 8;
					
					note_dst.y -= FIXED_DEC(6,1);
					note_dst.h >>= 2;
					
					Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
				}
				else
				{
					//Draw note fire
					note_src.x = 192 + ((animf_count & 0x1) << 5);
					note_src.y = 64 + ((animf_count & 0x2) * 24);
					note_src.w = 32;
					note_src.h = 48;
					
					if (stage.prefs.downscroll)
					{
						note_dst.y += note_dst.h;
						note_dst.h = note_dst.h * -3 / 2;
					}
					else
					{
						note_dst.h = note_dst.h * 3 / 2;
					}
					Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
				}
			}
			else
			{
				//Don't draw if already hit
				if (note->type & NOTE_FLAG_HIT)
					continue;
				
				//Draw note
				note_src.x = 32 + ((note->type & 0x3) << 5);
				note_src.y = 0;
				note_src.w = 32;
				note_src.h = 32;
				
				note_dst.x = stage.note_x[note->type & 0x7] - FIXED_DEC(16,1);
				note_dst.y = y - FIXED_DEC(16,1);
				note_dst.w = note_src.w << FIXED_SHIFT;
				note_dst.h = note_src.h << FIXED_SHIFT;
				
				if (stage.prefs.downscroll)
					note_dst.y = -note_dst.y - note_dst.h;
				Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
			}
		}
	}
}

//Stage loads
static void Stage_LoadPlayer(void)
{
	//Load player character
	Character_Free(stage.player);
	stage.player = stage.stage_def->pchar.new(stage.stage_def->pchar.x, stage.stage_def->pchar.y);
}

static void Stage_LoadOpponent(void)
{
	//Load opponent character
	Character_Free(stage.opponent);
	stage.opponent = stage.stage_def->ochar.new(stage.stage_def->ochar.x, stage.stage_def->ochar.y);
}

static void Stage_LoadGirlfriend(void)
{
	//Load girlfriend character
	Character_Free(stage.gf);
	if (stage.stage_def->gchar.new != NULL)
		stage.gf = stage.stage_def->gchar.new(stage.stage_def->gchar.x, stage.stage_def->gchar.y);
	else
		stage.gf = NULL;
}

static void Stage_LoadStage(void)
{
	//Load back
	if (stage.back != NULL)
		stage.back->free(stage.back);
	stage.back = stage.stage_def->back();
}

static void Stage_LoadChart(void)
{
	//Load stage data
	char chart_path[64];
	if (stage.stage_def->week & 0x80)
	{
		//Use mod path convention
		static const char *mod_format[] = {
			"\\KAPI\\KAPI.%d%c.CHT;1", //Kapi
			"\\CLWN\\CLWN.%d%c.CHT;1" //Tricky
		};
		
		sprintf(chart_path, mod_format[stage.stage_def->week & 0x7F], stage.stage_def->week_song, "ENH"[stage.stage_diff]);
	}
	else
	{
		//Use standard path convention
		sprintf(chart_path, "\\WEEK%d\\%d.%d%c.CHT;1", stage.stage_def->week, stage.stage_def->week, stage.stage_def->week_song, "ENH"[stage.stage_diff]);
	}
	
	if (stage.chart_data != NULL)
		Mem_Free(stage.chart_data);
	stage.chart_data = IO_Read(chart_path);
	u8 *chart_byte = (u8*)stage.chart_data;


		//Directly use section and notes pointers
		stage.sections = (Section*)(chart_byte + 6);
		stage.notes = (Note*)(chart_byte + ((u16*)stage.chart_data)[2]);
		
		for (Note *note = stage.notes; note->pos != 0xFFFF; note++)
			stage.num_notes++;
	
	//Count max scores
	stage.player_state[0].max_score = 0;
	stage.player_state[1].max_score = 0;
	for (Note *note = stage.notes; note->pos != 0xFFFF; note++)
	{
		if (note->type & (NOTE_FLAG_SUSTAIN | NOTE_FLAG_MINE))
			continue;
		if (note->type & NOTE_FLAG_OPPONENT)
			stage.player_state[1].max_score += 35;
		else
			stage.player_state[0].max_score += 35;
	}
	if (stage.mode >= StageMode_2P && stage.player_state[1].max_score > stage.player_state[0].max_score)
		stage.max_score = stage.player_state[1].max_score;
	else
		stage.max_score = stage.player_state[0].max_score;
	
	stage.cur_section = stage.sections;
	stage.cur_note = stage.notes;
	
	stage.speed = *((fixed_t*)stage.chart_data);
	
	stage.step_crochet = 0;
	stage.time_base = 0;
	stage.step_base = 0;
	stage.section_base = stage.cur_section;
	Stage_ChangeBPM(stage.cur_section->flag & SECTION_FLAG_BPM_MASK, 0);
}

static void Stage_LoadMusic(void)
{
	//Offset sing ends
	stage.player->sing_end -= stage.note_scroll;
	stage.opponent->sing_end -= stage.note_scroll;
	if (stage.gf != NULL)
		stage.gf->sing_end -= stage.note_scroll;
	
	//Find music file and begin seeking to it
	Audio_SeekXA_Track(stage.stage_def->music_track);
	
	//Initialize music state
	stage.note_scroll = FIXED_DEC((-6 * 5) * 12,1);
	stage.song_time = FIXED_DIV(stage.note_scroll, stage.step_crochet);
	stage.interp_time = 0;
	stage.interp_ms = 0;
	stage.interp_speed = 0;
	
	//Offset sing ends again
	stage.player->sing_end += stage.note_scroll;
	stage.opponent->sing_end += stage.note_scroll;
	if (stage.gf != NULL)
		stage.gf->sing_end += stage.note_scroll;
}

static void Stage_LoadState(void)
{
	//Initialize stage state
	stage.flag = STAGE_FLAG_VOCAL_ACTIVE;

	//load notes position
	Stage_LoadNotes();

	//Get length of da song
	Stage_TimerGetLength();
	
	stage.gf_speed = 1 << 2;
	stage.select = 0;
	
	#ifdef STAGE_DEBUG
	stage.state = StageState_Debug;

	#else
	stage.state = StageState_Play;
	#endif
	
	if (stage.mode == StageMode_Swap)
	{
		stage.player_state[0].character = stage.opponent;
		stage.player_state[1].character = stage.player;
	}

	else
	{
	stage.player_state[0].character = stage.player;
	stage.player_state[1].character = stage.opponent;
	}

	for (int i = 0; i < 2; i++)
	{
		memset(stage.player_state[i].arrow_hitan, 0, sizeof(stage.player_state[i].arrow_hitan));
		
		stage.player_state[i].health = 10000;
		stage.player_state[i].combo = 0;
		
		stage.player_state[i].refresh_score = false;
		stage.player_state[i].score = 0;
		strcpy(stage.player_state[i].score_text, "SC: ?");

		stage.player_state[i].refresh_miss = false;
		stage.player_state[i].miss = 0;
		strcpy(stage.player_state[i].miss_text, "MS: 0");

		stage.player_state[i].refresh_accuracy = false;
		stage.player_state[i].min_accuracy = stage.player_state[i].max_accuracy = 0;
		strcpy(stage.player_state[i].accuracy_text, "AC: ?");
		
		stage.player_state[i].pad_held = stage.player_state[i].pad_press = 0;
	}
	
	ObjectList_Free(&stage.objlist_splash);
	ObjectList_Free(&stage.objlist_fg);
	ObjectList_Free(&stage.objlist_bg);
}

//Stage functions
void Stage_Load(StageId id, StageDiff difficulty, boolean story)
{
	//Get stage definition
	stage.stage_def = &stage_defs[stage.stage_id = id];
	stage.stage_diff = difficulty;
	stage.story = story;
	
	//Load HUD textures
	if (id >= StageId_6_1 && id <= StageId_6_3)
		Gfx_LoadTex(&stage.tex_hud0, IO_Read("\\STAGE\\HUD0WEEB.TIM;1"), GFX_LOADTEX_FREE);
	else
		Gfx_LoadTex(&stage.tex_hud0, IO_Read("\\STAGE\\HUD0.TIM;1"), GFX_LOADTEX_FREE);
	Gfx_LoadTex(&stage.tex_hud1, IO_Read("\\STAGE\\HUD1.TIM;1"), GFX_LOADTEX_FREE);
	Gfx_LoadTex(&stage.tex_hude, IO_Read("\\STAGE\\HUDEXTRA.TIM;1"), GFX_LOADTEX_FREE);

	//load debug struct
	Debug_Free();
	
	//Load stage background
	Stage_LoadStage();
	
	//Load characters
	Stage_LoadPlayer();
	Stage_LoadOpponent();
	Stage_LoadGirlfriend();
	
	//Load stage chart
	Stage_LoadChart();

	//Load Sound Effect
	Stage_LoadSFX();

	//Load fonts
	FontData_Load(&stage.font_bold, Font_Bold);
	FontData_Load(&stage.font_cdr, Font_CDR);

	//Initialize stage according to mode
	stage.note_swap = (stage.mode == StageMode_Swap) ? NOTE_FLAG_OPPONENT : 0;
	
	//Initialize stage state
	stage.story = story;
	
	Stage_LoadState();
	
	//Initialize camera
	if (stage.cur_section->flag & SECTION_FLAG_OPPFOCUS)
		Stage_FocusCharacter(stage.opponent, FIXED_UNIT);
	else
		Stage_FocusCharacter(stage.player, FIXED_UNIT);
	stage.camera.x = stage.camera.tx;
	stage.camera.y = stage.camera.ty;
	stage.camera.zoom = stage.camera.tz;
	
	stage.bump = FIXED_UNIT;
	stage.sbump = FIXED_UNIT;
	
	//Load music
	stage.note_scroll = 0;
	Stage_LoadMusic();
	
	//Test offset
	stage.offset = 0;
}

void Stage_Unload(void)
{
	//Unload stage background
	if (stage.back != NULL)
		stage.back->free(stage.back);
	stage.back = NULL;
	
	//Unload stage data
	Mem_Free(stage.chart_data);
	stage.chart_data = NULL;

	//free debug struct
	Debug_Free();
	
	//Free objects
	ObjectList_Free(&stage.objlist_splash);
	ObjectList_Free(&stage.objlist_fg);
	ObjectList_Free(&stage.objlist_bg);
	
	//Free characters
	Character_Free(stage.player);
	stage.player = NULL;
	Character_Free(stage.opponent);
	stage.opponent = NULL;
	Character_Free(stage.gf);
	stage.gf = NULL;
}

static boolean Stage_NextLoad(void)
{
	u8 load = stage.stage_def->next_load;
	if (load == 0)
	{
		//Do stage transition if full reload
		stage.trans = StageTrans_NextSong;
		Trans_Start();
		return false;
	}
	else
	{
		//Get stage definition
		stage.stage_def = &stage_defs[stage.stage_id = stage.stage_def->next_stage];
		
		//Load stage background
		if (load & STAGE_LOAD_STAGE)
			Stage_LoadStage();
		
		//Load characters
		if (load & STAGE_LOAD_PLAYER)
		{
			Stage_LoadPlayer();
		}
		else
		{
			stage.player->x = stage.stage_def->pchar.x;
			stage.player->y = stage.stage_def->pchar.y;
		}
		if (load & STAGE_LOAD_OPPONENT)
		{
			Stage_LoadOpponent();
		}
		else
		{
			stage.opponent->x = stage.stage_def->ochar.x;
			stage.opponent->y = stage.stage_def->ochar.y;
		}
		if (load & STAGE_LOAD_GIRLFRIEND)
		{
			Stage_LoadGirlfriend();
		}
		else if (stage.gf != NULL)
		{
			stage.gf->x = stage.stage_def->gchar.x;
			stage.gf->y = stage.stage_def->gchar.y;
		}
		
		//Load stage chart
		Stage_LoadChart();
		
		//Initialize stage state
		Stage_LoadState();

		//Load Sound Effect
		Stage_LoadSFX();
		
		//Load music
		Stage_LoadMusic();
		
		//Reset timer
		Timer_Reset();
		return true;
	}
}

static void Stage_LoadInfo(void)
{
	//making this a function because the playstate and pausestate use this

	//Draw timer
	Stage_TimerTick();

	//Tick note splashes
	ObjectList_Tick(&stage.objlist_splash);

	//Draw skill issue mode (botplay)
	RECT skill_issue_src = {129, 208, 67, 16};
	RECT_FIXED skill_issue_dst = {
	FIXED_DEC(-33,1), 
	FIXED_DEC(-60,1), 
	FIXED_DEC(67,1), 
	FIXED_DEC(16,1)
};
	if (stage.prefs.botplay)
		Stage_DrawTex(&stage.tex_hude, &skill_issue_src, &skill_issue_dst, stage.bump);
			
	//Draw stage notes
	Stage_DrawNotes();
			
	//Draw note HUD
	RECT note_src = {0, 0, 32, 32};
	RECT_FIXED note_dst = {0, 0, FIXED_DEC(32,1), FIXED_DEC(32,1)};
			
	for (u8 i = 0; i < 4; i++)
	{
		//BF
		note_dst.x = stage.note_x[i] - FIXED_DEC(16,1);
		note_dst.y = stage.note_y[i] - FIXED_DEC(16,1);
		if (stage.prefs.downscroll)
			note_dst.y = -note_dst.y - note_dst.h;

		Stage_DrawStrum(i, &note_src, &note_dst);
		Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
				
		//Opponent
		note_dst.x = stage.note_x[i | NOTE_FLAG_OPPONENT] - FIXED_DEC(16,1);
		note_dst.y = stage.note_y[i | NOTE_FLAG_OPPONENT] - FIXED_DEC(16,1);
		if (stage.prefs.downscroll)
			note_dst.y = -note_dst.y - note_dst.h;

		Stage_DrawStrum(i | 4, &note_src, &note_dst);
		Stage_DrawTex(&stage.tex_hud0, &note_src, &note_dst, stage.bump);
	}
			
	//Draw score
	for (u8 i = 0; i < ((stage.mode == StageMode_2P) ? 2 : 1); i++)
	{
		PlayerState *this = &stage.player_state[i];
				
		//Get string representing number
		if (this->refresh_score)
		{
			if (this->score != 0)
				sprintf(this->score_text, "SC: %d0", this->score * stage.max_score / this->max_score);
			else
				strcpy(this->score_text, "SC: ?");
			this->refresh_score = false;
		}
					
			//Draw text
			stage.font_cdr.draw(&stage.font_cdr,
				this->score_text,
				(stage.mode == StageMode_2P && i == 0) ? 10 : -130, 
				(stage.prefs.downscroll) ? -88 : 98,
				FontAlign_Left,
				"stage"
			);
		}

	//Draw Miss
	for (u8 i = 0; i < ((stage.mode == StageMode_2P) ? 2 : 1); i++)
	{
		PlayerState *this = &stage.player_state[i];
				
		//Get string representing number
		if (this->refresh_miss)
		{
			if (this->miss != 0)
				sprintf(this->miss_text, "MS: %d", this->miss);
			else
				strcpy(this->miss_text, "MS: 0");
			this->refresh_miss = false;
		}
					
			//Draw text
			stage.font_cdr.draw(&stage.font_cdr,
				this->miss_text,
				(stage.mode == StageMode_2P && i == 0) ? 90 : -50, 
				(stage.prefs.downscroll) ? -88 : 98,
				FontAlign_Left,
				"stage"
			);
		}

	//Draw accuracy
	for (u8 i = 0; i < ((stage.mode == StageMode_2P) ? 0 : 1); i++)
	{
		PlayerState *this = &stage.player_state[i];

		static const char *rating_text[] = {
		"You Suck!", // 0 - 9%
		"You Suck!", //10 - 19% repeating this because it's more easy LOL
		"Shit", //20 - 29%
		"Shit", //30 - 39% repeating this because it's more easy LOL
		"Bad", //40 - 49%
		"Bruh", //50 - 59%
		"Meh", //60 - 69%
		"Good", //70 - 79%
		"Great", //80 - 89%
		"Sick!", //90 - 99%
		"Perfect!!!", //100%
		"Nice!", //69% ( ͡° ͜ʖ ͡°) 
	};

		static const char *fc_text[] = {
		"- FC", // 0 - 79%
		"- GFC", //80 - 99%
		"- PFC", //100%
	};

		this->accuracy = (this->min_accuracy * 100) / (this->max_accuracy);

		//making this for in the case of special ratings,like Nice!
		static u8 rating;

		switch (this->accuracy)
		{
			case 69:
				rating = 11;
			break;
			
			default:
				rating = this->accuracy/10;
			break;
		}

		//fc rating
		static u8 fc;

		if (this->accuracy <= 79)
			fc = 0;

		else if (this->accuracy <= 99)
			fc = 1;

		else
			fc = 2;
				
		//Get string representing number
		if (this->refresh_accuracy)
		{
			if (this->accuracy != 0)
				sprintf(this->accuracy_text, "AC:	 (%d%%)  %s  %s", this->accuracy, rating_text[rating], (this->miss == 0) ? fc_text[fc] : '\0');
			else
				strcpy(this->accuracy_text, "AC: ?");
			this->refresh_accuracy = false;
		}
					
			//Draw text
			stage.font_cdr.draw(&stage.font_cdr,
				this->accuracy_text,
				15, 
				(stage.prefs.downscroll) ? -87 : 98,
				FontAlign_Left,
				"stage"
			);
	}
			
	//normal and swap healthbar
	if (stage.mode != StageMode_2P)
	{
		//Perform health checks
		if (stage.player_state[0].health <= 0)
		{
			//Player has died
			stage.player_state[0].health = 0;
			stage.state = StageState_Dead;
		}

		if (stage.player_state[0].health > 20000)
			stage.player_state[0].health = 20000;

		//Draw health heads
		Stage_DrawHealth(stage.player_state[0].health, stage.player_state[0].character->health_i,    1);
		Stage_DrawHealth(stage.player_state[0].health, stage.player_state[1].character->health_i, -1);
				
		//Draw health bar
		Stage_DrawHealthBar(254 - (254 * stage.player_state[0].health / 20000), stage.player_state[1].character->health_bar);
		Stage_DrawHealthBar(254, stage.player_state[0].character->health_bar);
	}
}
void Stage_Tick(void)
{
	SeamLoad:;
	
	  //Tick transition
		#ifdef STAGE_DEBUG
		if (pad_state.press & PAD_START)
		{
			stage.trans = StageTrans_Menu;
			Trans_Start();
		}

		#else
		if (pad_state.press & PAD_START && stage.state != StageState_Pause && stage.state != StageState_Play)
		{
			stage.trans = StageTrans_Reload;
			Trans_Start();
		}
		#endif
	
	if (Trans_Tick())
	{
		switch (stage.trans)
		{
			case StageTrans_Menu:
				//Load appropriate menu
				Stage_Unload();
				
				LoadScr_Start();
				if (stage.story)
					Menu_Load(MenuPage_Story);
				else
					Menu_Load(MenuPage_Freeplay);

				LoadScr_End();
				
				gameloop = GameLoop_Menu;
				return;
			case StageTrans_NextSong:
				//Load next song
				Stage_Unload();
				
				LoadScr_Start();
				Stage_Load(stage.stage_def->next_stage, stage.stage_diff, stage.story);
				LoadScr_End();
				break;
			case StageTrans_Reload:
				//Reload song
				Stage_Unload();
				
				LoadScr_Start();
				Stage_Load(stage.stage_id, stage.stage_diff, stage.story);
				LoadScr_End();
				break;
			default:
			break;
		}
	}
	
	switch (stage.state)
	{
		case StageState_Play:
		{
			//Clear per-frame flags
			stage.flag &= ~(STAGE_FLAG_JUST_STEP | STAGE_FLAG_SCORE_REFRESH);
			
			//Get song position
			boolean playing;
			fixed_t next_scroll;
			
				const fixed_t interp_int = FIXED_UNIT * 8 / 75;
				if (stage.note_scroll < 0)
				{
					//Play countdown sequence
					stage.song_time += timer_dt;
					
					//Update song
					if (stage.song_time >= 0)
					{
						//Song has started
						playing = true;
						Audio_PlayXA_Track(stage.stage_def->music_track, 0x40, stage.stage_def->music_channel, 0);
						
						//Update song time
						fixed_t audio_time = (fixed_t)Audio_TellXA_Milli() - stage.offset;
						if (audio_time < 0)
							audio_time = 0;
						stage.interp_ms = (audio_time << FIXED_SHIFT) / 1000;
						stage.interp_time = 0;
						stage.song_time = stage.interp_ms;
					}
					else
					{
						//Still scrolling
						playing = false;
					}
					
					//Update scroll
					next_scroll = FIXED_MUL(stage.song_time, stage.step_crochet);
				}
				else if (Audio_PlayingXA())
				{
					fixed_t audio_time_pof = (fixed_t)Audio_TellXA_Milli();
					fixed_t audio_time = (audio_time_pof > 0) ? (audio_time_pof - stage.offset) : 0;
					
						//Get playing song position
						if (audio_time_pof > 0)
						{
							stage.song_time += timer_dt;
							stage.interp_time += timer_dt;
						}
						
						if (stage.interp_time >= interp_int)
						{
							//Update interp state
							while (stage.interp_time >= interp_int)
								stage.interp_time -= interp_int;
							stage.interp_ms = (audio_time << FIXED_SHIFT) / 1000;
						}
						
						//Resync
						fixed_t next_time = stage.interp_ms + stage.interp_time;
						if (stage.song_time >= next_time + FIXED_DEC(25,1000) || stage.song_time <= next_time - FIXED_DEC(25,1000))
						{
							stage.song_time = next_time;
						}
						else
						{
							if (stage.song_time < next_time - FIXED_DEC(1,1000))
								stage.song_time += FIXED_DEC(1,1000);
							if (stage.song_time > next_time + FIXED_DEC(1,1000))
								stage.song_time -= FIXED_DEC(1,1000);
						}
					
					playing = true;
					
					//Update scroll
					next_scroll = ((fixed_t)stage.step_base << FIXED_SHIFT) + FIXED_MUL(stage.song_time - stage.time_base, stage.step_crochet);
				}
				else
				{
					//Song has ended
					playing = false;
					stage.song_time += timer_dt;
					
					//Update scroll
					next_scroll = ((fixed_t)stage.step_base << FIXED_SHIFT) + FIXED_MUL(stage.song_time - stage.time_base, stage.step_crochet);
					
					//Transition to menu or next song
					if (stage.story && stage.stage_def->next_stage != stage.stage_id && stage.state != StageState_Pause)
					{
						if (Stage_NextLoad())
							goto SeamLoad;
					}
					else if (stage.state != StageState_Pause)
					{
						stage.trans = StageTrans_Menu;
						Trans_Start();
					}
				}
			
			RecalcScroll:;
			//Update song scroll and step
			if (next_scroll > stage.note_scroll)
			{
				if (((stage.note_scroll / 12) & FIXED_UAND) != ((next_scroll / 12) & FIXED_UAND))
					stage.flag |= STAGE_FLAG_JUST_STEP;
				stage.note_scroll = next_scroll;
				stage.song_step = (stage.note_scroll >> FIXED_SHIFT);
				if (stage.note_scroll < 0)
					stage.song_step -= 11;
				stage.song_step /= 12;
			}
			
			//Update section
			if (stage.note_scroll >= 0)
			{
				//Check if current section has ended
				u16 end = stage.cur_section->end;
				if ((stage.note_scroll >> FIXED_SHIFT) >= end)
				{
					//Increment section pointer
					stage.cur_section++;
					
					//Update BPM
					u16 next_bpm = stage.cur_section->flag & SECTION_FLAG_BPM_MASK;
					Stage_ChangeBPM(next_bpm, end);
					stage.section_base = stage.cur_section;
					
					//Recalculate scroll based off new BPM
					next_scroll = ((fixed_t)stage.step_base << FIXED_SHIFT) + FIXED_MUL(stage.song_time - stage.time_base, stage.step_crochet);
					goto RecalcScroll;
				}
			}

			//intro sequence
			if (stage.song_step < 0)
			Stage_PlayIntro();

			//Go to pause state
			if (playing && pad_state.press & PAD_START)
			{
			Audio_PauseXA();
			stage.state = StageState_Pause;
			}
			
			//Handle bump
			if ((stage.bump = FIXED_UNIT + FIXED_MUL(stage.bump - FIXED_UNIT, FIXED_DEC(95,100))) <= FIXED_DEC(1003,1000))
				stage.bump = FIXED_UNIT;
			stage.sbump = FIXED_UNIT + FIXED_MUL(stage.sbump - FIXED_UNIT, FIXED_DEC(60,100));
			
			if (playing && (stage.flag & STAGE_FLAG_JUST_STEP))
			{
				//Check if screen should bump
				boolean is_bump_step = (stage.song_step & 0xF) == 0;
				
				//M.I.L.F bumps
				if (stage.stage_id == StageId_4_3 && stage.song_step >= (168 << 2) && stage.song_step < (200 << 2))
					is_bump_step = (stage.song_step & 0x3) == 0;
				
				//Bump screen
				if (is_bump_step)
					stage.bump = FIXED_DEC(103,100);
				
				//Bump health every 4 steps
				if ((stage.song_step & 0x3) == 0)
					stage.sbump = FIXED_DEC(103,100);
			}
			
			//Scroll camera
			if (stage.cur_section->flag & SECTION_FLAG_OPPFOCUS)
				Stage_FocusCharacter(stage.opponent, FIXED_UNIT / 24);
			else
				Stage_FocusCharacter(stage.player, FIXED_UNIT / 24);
			Stage_ScrollCamera();
			
			switch (stage.mode)
			{
				case StageMode_Normal:
				case StageMode_Swap:
				{
					//Handle player 1 inputs
					Stage_ProcessPlayer(&stage.player_state[0], &pad_state, playing);
					
					//Handle opponent notes
					u8 opponent_anote = CharAnim_Idle;
					u8 opponent_snote = CharAnim_Idle;
					
					for (Note *note = stage.cur_note;; note++)
					{
						if (note->pos > (stage.note_scroll >> FIXED_SHIFT))
							break;
						
						//Opponent note hits
						if (playing && ((note->type ^ stage.note_swap) & NOTE_FLAG_OPPONENT) && !(note->type & NOTE_FLAG_HIT))
						{
							//Opponent hits note
							stage.player_state[1].arrow_hitan[note->type & 0x3] = stage.step_time;
							Stage_StartVocal();
							if (note->type & NOTE_FLAG_SUSTAIN)
								opponent_snote = note_anims[note->type & 0x3][(note->type & NOTE_FLAG_ALT_ANIM) != 0];
							else
								opponent_anote = note_anims[note->type & 0x3][(note->type & NOTE_FLAG_ALT_ANIM) != 0];
							note->type |= NOTE_FLAG_HIT;
						}
					}
					
					if (opponent_anote != CharAnim_Idle)
						stage.player_state[1].character->set_anim(stage.player_state[1].character, opponent_anote);
					else if (opponent_snote != CharAnim_Idle)
						stage.player_state[1].character->set_anim(stage.player_state[1].character, opponent_snote);
					break;
				}
				case StageMode_2P:
				{
					//Handle player 1 and 2 inputs
					Stage_ProcessPlayer(&stage.player_state[0], &pad_state, playing);
					Stage_ProcessPlayer(&stage.player_state[1], &pad_state_2, playing);
					break;
				}
			}

			Stage_LoadInfo();
			
			//Hardcoded stage stuff
			switch (stage.stage_id)
			{
				case StageId_1_2: //Fresh GF bop
					switch (stage.song_step / 4)
					{
						case 16:
							stage.gf_speed = 2 << 2;
							break;
						case 48:
							stage.gf_speed = 1 << 2;
							break;
						case 80:
							stage.gf_speed = 2 << 2;
							break;
						case 112:
							stage.gf_speed = 1 << 2;
							break;
					}
					break;
				default:
					break;
			}
			
			//Draw stage foreground
			if (stage.back->draw_fg != NULL)
				stage.back->draw_fg(stage.back);
			
			//Tick foreground objects
			ObjectList_Tick(&stage.objlist_fg);
			
			//Tick characters
			stage.player->tick(stage.player);
			stage.opponent->tick(stage.opponent);
			
			//Draw stage middle
			if (stage.back->draw_md != NULL)
				stage.back->draw_md(stage.back);
			
			//Tick girlfriend
			if (stage.gf != NULL)
				stage.gf->tick(stage.gf);
			
			//Tick background objects
			ObjectList_Tick(&stage.objlist_bg);
			
			//Draw stage background
			if (stage.back->draw_bg != NULL)
				stage.back->draw_bg(stage.back);
			break;
		}
		case StageState_Dead: //Start BREAK animation and reading extra data from CD
		{
			//Stop music immediately
			Audio_StopXA();
			
			//Unload stage data
			Mem_Free(stage.chart_data);
			stage.chart_data = NULL;
			
			//Free background
			stage.back->free(stage.back);
			stage.back = NULL;
			
			//Free objects
			ObjectList_Free(&stage.objlist_fg);
			ObjectList_Free(&stage.objlist_bg);
			
			//Free opponent and girlfriend
			Character_Free(stage.opponent);
			stage.opponent = NULL;
			Character_Free(stage.gf);
			stage.gf = NULL;
			
			//Reset stage state
			stage.flag = 0;
			stage.bump = stage.sbump = FIXED_UNIT;
			
			//Change background colour to black
			Gfx_SetClear(0, 0, 0);
			
			//Run death animation, focus on player, and change state
			stage.player->set_anim(stage.player, PlayerAnim_Dead0);
			
			Stage_FocusCharacter(stage.player, 0);
			stage.song_time = 0;
			
			stage.state = StageState_DeadLoad;
		}
	//Fallthrough
		case StageState_DeadLoad:
		{
			//Scroll camera and tick player
			if (stage.song_time < FIXED_UNIT)
				stage.song_time += FIXED_UNIT / 60;
			stage.camera.td = FIXED_DEC(-2, 100) + FIXED_MUL(stage.song_time, FIXED_DEC(45, 1000));
			if (stage.camera.td > 0)
				Stage_ScrollCamera();
			stage.player->tick(stage.player);
			
			//Drop mic and change state if CD has finished reading and animation has ended
			if (IO_IsReading() || stage.player->animatable.anim != PlayerAnim_Dead1)
				break;
			
			stage.player->set_anim(stage.player, PlayerAnim_Dead2);
			stage.camera.td = FIXED_DEC(25, 1000);
			stage.state = StageState_DeadDrop;
			break;
		}
		case StageState_DeadDrop:
		{
			//Scroll camera and tick player
			Stage_ScrollCamera();
			stage.player->tick(stage.player);
			
			//Enter next state once mic has been dropped
			if (stage.player->animatable.anim == PlayerAnim_Dead3)
			{
				stage.state = StageState_DeadRetry;
				Audio_PlayXA_Track(XA_GameOver, 0x40, 1, true);
			}
			break;
		}
		case StageState_DeadRetry:
		{
			//Randomly twitch
			if (stage.player->animatable.anim == PlayerAnim_Dead3)
			{
				if (RandomRange(0, 29) == 0)
					stage.player->set_anim(stage.player, PlayerAnim_Dead4);
				if (RandomRange(0, 29) == 0)
					stage.player->set_anim(stage.player, PlayerAnim_Dead5);
			}
			
			//Scroll camera and tick player
			Stage_ScrollCamera();
			stage.player->tick(stage.player);
			break;
		}

	//pause state
		case StageState_Pause:
		{
			animf_count = 0;
			timer_dt = 0;

			static const char *stage_options[] = {
				"RESTART SONG",
				"QUIT TO MENU"
			};

				//Select option if cross is pressed
				if (pad_state.press & (PAD_CROSS | PAD_START))
				{
					switch (stage.select)
					{
						case 0: //Retry
							stage.trans = StageTrans_Reload;
							Trans_Start();
							break;
						case 1: //Quit
							stage.trans = StageTrans_Menu;
							Trans_Start();
							break;
					}
				}

				//Change option
				if (pad_state.press & PAD_UP)
				{
					if (stage.select > 0)
						stage.select--;
					else
						stage.select = COUNT_OF(stage_options) - 1;
				}
				if (pad_state.press & PAD_DOWN)
				{
					if (stage.select < COUNT_OF(stage_options) - 1)
						stage.select++;
					else
						stage.select = 0;
				}
				

			//draw options
			for (u8 i = 0; i < COUNT_OF(stage_options); i++)
			{
				//Get position on screen
				s32 y = (i * 24) - 8;
				if (y <= -SCREEN_HEIGHT2 - 8)
					continue;
				if (y >= SCREEN_HEIGHT2 + 8)
					break;
				
				//Draw text
				stage.font_bold.draw_col(&stage.font_bold,
					stage_options[i],
				    -150 + (y >> 2),
					y - 8,
					FontAlign_Left,
					//if the option is the one you are selecting, draw in normal color, else, draw gray
					(i == stage.select) ? 0x80 : 160 >> 1,
					(i == stage.select) ? 0x80 : 160 >> 1,
					(i == stage.select) ? 0x80 : 160 >> 1,
					"stage"
				);
			}
			//cool blend
			RECT screen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
			Gfx_BlendRect(&screen, 12, 12, 12, 0);

			Stage_LoadInfo();

			//Draw stage foreground
			if (stage.back->draw_fg != NULL)
				stage.back->draw_fg(stage.back);
			
			//Tick foreground objects
			ObjectList_Tick(&stage.objlist_fg);
			
			//Tick characters
			stage.player->tick(stage.player);
			stage.opponent->tick(stage.opponent);
			
			//Draw stage middle
			if (stage.back->draw_md != NULL)
				stage.back->draw_md(stage.back);
			
			//Tick girlfriend
			if (stage.gf != NULL)
				stage.gf->tick(stage.gf);
			
			//Tick background objects
			ObjectList_Tick(&stage.objlist_bg);
			
			//Draw stage background
			if (stage.back->draw_bg != NULL)
				stage.back->draw_bg(stage.back);
			break;
		}

		//debug state
		case StageState_Debug:
		{
			Debug_Tick();

			//Draw stage foreground
			if (stage.back->draw_fg != NULL)
				stage.back->draw_fg(stage.back);
			
			//Tick foreground objects
			ObjectList_Tick(&stage.objlist_fg);
			
			//Tick characters
			stage.player->tick(stage.player);
			stage.opponent->tick(stage.opponent);
			
			//Draw stage middle
			if (stage.back->draw_md != NULL)
				stage.back->draw_md(stage.back);
			
			//Tick girlfriend
			if (stage.gf != NULL)
				stage.gf->tick(stage.gf);
			
			//Tick background objects
			ObjectList_Tick(&stage.objlist_bg);
			
			//Draw stage background
			if (stage.back->draw_bg != NULL)
				stage.back->draw_bg(stage.back);
			break;
		}
	default:
		break;
	}
}
