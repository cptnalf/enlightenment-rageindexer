/* filename: unknown_eets.c
 *  chiefengineer
 *  Thu May 13 19:50:19 PDT 2010
 */

#include <Eina.h>
#include <Ecore.h>
#include <Evas.h>
#include <Ecore_File.h>

#include "volume.h"
#include "database.h"
#include "sha1.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <time.h>

//char* gen_file();

static char cur_path[4096];
static Eina_List* dirstack =0;
const char* vol_root = 0;
char* dir_prefix = 0;
int debug = 0;

char* gen_file(char* vol_path)
{
	return NULL;
}


int main(int argc, char* argv[])
{
	if (argc < 2)
		{
			printf("unknown_eets <media db> \n");
			return 1;
		}
	
	printf("db=%s\n", argv[1]);
	
	ecore_init();
	ecore_file_init();
	evas_init();
	
	database_init(argv[1]);
	Database* db = database_new();
	
	if (db)
		{
			time_t start_time, end_time;
			DBIterator* db_it;
			Volume_Item* db_item;
			int db_done = 0;
			const char* chmap = "0123456789abcdef";
			unsigned char sha[40];
			char buf[64];
			int i = 0;
			
			db_it = database_video_files_get(db, NULL);
			
			/* prime the item for the loop. */
			db_item = database_iterator_next(db_it);
			db_done = (db_item == 0);
			
			/* compare part. */
			while(!db_done)
				{
					
					db_item = database_iterator_next(db_it);
					db_done = (db_item == 0);
					
					if (db_item)
						{
							sha1_sum((const unsigned char*)db_item->path, strlen(db_item->path), sha);
							
							for (i = 0; i < 20; ++i)
								{
									buf[(i*2)+0] = chmap[(sha[i] >> 4) & 0xf];
									buf[(i*2)+1] = chmap[(sha[i]     ) & 0xf];
								}
							buf[(i*2)] = 0;
							
							printf("%s.eet\n", buf);
							
							volume_item_free(db_item);
						}
				}
		}
	
	return 0;
}
