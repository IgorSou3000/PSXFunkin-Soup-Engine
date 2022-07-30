/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "main.h"

#include "engine/timer.h"
#include "engine/io.h"
#include "engine/gfx.h"
#include "engine/audio.h"
#include "engine/pad.h"

#include "menu/menu.h"
#include "engine/save.h"
#include "stage.h"
#include "debug.h"

//Game loop
GameLoop gameloop;

//Error handler
char error_msg[0x200];

void ErrorLock(void)
{
	while (1)
	{
		#ifdef PSXF_PC
			MsgPrint(error_msg);
			exit(1);
		#else
			FntPrint("A fatal error has occured\n~c700%s\n", error_msg);
			Gfx_Flip();
		#endif
	}
}

//Memory heap
//#define MEM_STAT //This will enable the Mem_GetStat function which returns information about available memory in the heap

#define MEM_IMPLEMENTATION
#include "engine/mem.h"
#undef MEM_IMPLEMENTATION

#ifndef PSXF_STDMEM
static u8 malloc_heap[0x1A0000];
#endif

//Entry point
int main(int argc, char **argv)
{
	//Remember arguments
	my_argc = argc;
	my_argv = argv;
	
	//Initialize system
	PSX_Init();
	
	Mem_Init((void*)malloc_heap, sizeof(malloc_heap));
	
	IO_Init();
	Audio_Init();
	Gfx_Init();
	Pad_Init();
	MCRD_Init();
	
	Timer_Init();

	//if not found a save, enable some options
	if (ReadSave() == false)
	{
		//options that's already enable for be more easy
		stage.prefs.showtimer = true;
	}
	
	//Start game
	gameloop = GameLoop_Menu;
	Menu_Load(MenuPage_Opening);
	
	//Game loop
	while (PSX_Running())
	{
		//Prepare frame
		Timer_Tick();
		Audio_ProcessXA();
		Pad_Update();
		
		#ifdef MEM_STAT
			//Memory stats
			size_t mem_used, mem_size, mem_max;
			Mem_GetStat(&mem_used, &mem_size, &mem_max);
			#ifndef MEM_BAR
				FntPrint("mem: %08X/%08X (max %08X)\n", mem_used, mem_size, mem_max);
			#endif
		#endif
		
		//Tick and draw game
		switch (gameloop)
		{
			case GameLoop_Menu:
				Menu_Tick();
				break;
			case GameLoop_Stage:
				Stage_Tick();
				break;
		}
		
		//Flip gfx buffers
		Gfx_Flip();
	}
	
	//Deinitialize system
	Pad_Quit();
	Gfx_Quit();
	Audio_Quit();
	IO_Quit();
	
	PSX_Quit();
	return 0;
}
