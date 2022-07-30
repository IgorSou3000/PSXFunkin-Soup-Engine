/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

//thanks to spicyjpeg for helping me understand this code

#include "save.h"

#include <libmcrd.h>
#include "../stage.h"
				  
	        //HAS to be BASCUS-scusid,somename
#define savetitle "bu00:BASCUS-49121soup"

//name who 'll be displayed in memory card
#define savegame "PSXFunkin Soup Engine"

static const u8 saveIconPalette[32] = {
	// Offset 0x00000014 to 0x00000033
	0x00, 0x00, 0x19, 0xE3, 0x08, 0xA1, 0x94, 0xD2, 0xF5, 0x88, 0x5B, 0x85,
	0x0B, 0x99, 0x1F, 0x82, 0x2B, 0x9D, 0xBD, 0xF7, 0x00, 0x80, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

SaveVar savevar;

static const u8 saveIconImage[128] = {
	// Offset 0x00000040 to 0x000000BF
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x01,
	0x00, 0x20, 0x22, 0x22, 0x22, 0x22, 0x11, 0x00, 0x00, 0x32, 0x33, 0x33,
	0x33, 0x13, 0x21, 0x00, 0x20, 0x43, 0x44, 0x44, 0x44, 0x11, 0x35, 0x02,
	0x32, 0x44, 0x44, 0x55, 0x15, 0x51, 0x55, 0x23, 0x46, 0x44, 0x55, 0x75,
	0x55, 0x75, 0x55, 0x65, 0x62, 0x54, 0x55, 0x55, 0x77, 0x57, 0x55, 0x28,
	0x12, 0x66, 0x56, 0x55, 0x55, 0x65, 0x66, 0x29, 0x12, 0x11, 0x61, 0x66,
	0x66, 0x16, 0x91, 0x29, 0x20, 0x11, 0x11, 0x11, 0x99, 0x11, 0x99, 0x02,
	0x00, 0x12, 0x11, 0x91, 0x19, 0x99, 0x29, 0x00, 0x00, 0xA0, 0x1A, 0x91,
	0x91, 0xA9, 0x0A, 0x00, 0x00, 0x00, 0xA0, 0xAA, 0xAA, 0x0A, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void Save_ReceiveInfo(void)
{
  savevar.prefs = stage.prefs;
}

static void Save_TransferInfo(void)
{
  stage.prefs = savevar.prefs;
}

static void toShiftJIS(u8 *buffer, const char *text)
{
    int pos = 0;
    for (u32 i = 0; i < strlen(text); i++) 
    {
        u8 c = text[i];
        if (c >= '0' && c <= '9') { buffer[pos++] = 0x82; buffer[pos++] = 0x4F + c - '0'; }
        else if (c >= 'A' && c <= 'Z') { buffer[pos++] = 0x82; buffer[pos++] = 0x60 + c - 'A'; }
        else if (c >= 'a' && c <= 'z') { buffer[pos++] = 0x82; buffer[pos++] = 0x81 + c - 'a'; }
        else if (c == '(') { buffer[pos++] = 0x81; buffer[pos++] = 0x69; }
        else if (c == ')') { buffer[pos++] = 0x81; buffer[pos++] = 0x6A; }
        else /* space */ { buffer[pos++] = 0x81; buffer[pos++] = 0x40; }
    }
}

static void initSaveFile(SaveFile *file, const char *name) 
{
	file->id = 0x4353;
 	file->iconDisplayFlag = 0x11;
 	file->iconBlockNum = 1;
  	toShiftJIS(file->title, name);
 	memcpy(file->iconPalette, saveIconPalette, 32);
 	memcpy(file->iconImage, saveIconImage, 128);
}

boolean ReadSave(void)
{
	int fd = open(savetitle, 0x0001);
	if (fd < 0) // file doesnt exist 
		return false;

	SaveFile file;
	if (read(fd, (void *) &file, sizeof(SaveFile)) == sizeof(SaveFile)) 
		printf("Readed MCRD!\n");
	else {
		printf("Read error\n");
		return false;
	}
	memcpy((void *) &savevar, (const void *) file.saveData, sizeof(savevar));
  Save_TransferInfo();
	close(fd);
	return true;
}

void WriteSave(void)
{	
	int fd = open(savetitle, 0x0002);

	if (fd < 0) // if save doesnt exist make one
		fd =  open(savetitle, 0x0202 | (1 << 16));

	SaveFile file;
	initSaveFile(&file, savegame);

  Save_ReceiveInfo();
  	memcpy((void *) file.saveData, (const void *) &savevar, sizeof(savevar));
	
	if (fd >= 0) {
	  	if (write(fd, (void *) &file, sizeof(SaveFile)) == sizeof(SaveFile)) 
	  		printf("Writed MCRD!\n");
	 	else 
	 		printf("write error\n");  // if save doesnt exist do a error
		close(fd);
	} 
	else 
		printf("open error %d\n", fd);  // failed to save
}

//initiliaze memory card
void MCRD_Init(void)
{
  InitCARD(1);
	StartCARD();
	_bu_init();	
	ChangeClearPAD(0);
}