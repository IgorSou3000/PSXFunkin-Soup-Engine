/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "archive.h"
#include "../main.h"
	
//Archive structure
typedef struct
{
	char path[12];
	u32 pos;
} ArchiveFile;

//Archive functions
IO_Data Archive_Find(IO_Data arc, const char *path)
{
	//Check against all archive files
	for (const ArchiveFile *file = (const ArchiveFile*)arc; file->path[0] != '\0'; file++)
	{
		if (strncmp(file->path, path, 12))
			continue;
		return (IO_Data)((u8*)arc + file->pos);
	}
	
	//Failed to find the requested file
	sprintf(error_msg, "[Archive_Find] Failed to find %s in %p", path, (void*)arc);
	ErrorLock();
	return NULL;
}
