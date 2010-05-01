
#include <Eina.h>
#include <Ecore.h>
#include <Evas.h>
#include <Ecore_File.h>
#include <sqlite3.h>

#include "volume.h"
#include "database.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <time.h>

char* gen_file();

static char cur_path[4096];
static Eina_List* dirstack =0;
const char* vol_root = 0;
int debug = 0;

int main(int argc, char** argv)
{
	if (argc!=3)
		{
			fprintf(stderr, "rage_indexer <db file> <path>");
			return 1;
		}
	
	printf("db=%s;root=%s\n", argv[1], argv[2]);
	vol_root = argv[2];
	
	//snprintf(cur_path, sizeof(cur_path), "%s", vol_root);
	
	ecore_init();
	ecore_file_init();
	evas_init();
	
	database_init(argv[1]);
	Database* db = database_new();
	
	if (db)
		{
			const Eina_List* vol_files;
			time_t start_time, end_time;
			Volume_Item* db_item, *vol_item;
			DBIterator* db_it;
			
			start_time = time(0);
			
			volume_index((char*)vol_root);
			
			end_time = time(0);
/* 			printf("fs-query time=%lds\n", end_time - start_time); */
			vol_files = volume_items_get();
			
			{
				int res;
				struct timeval s_time, e_time;
				
				res = gettimeofday(&s_time, 0);
				db_it = database_video_files_path_search(db, vol_root);
				res = gettimeofday(&e_time, 0);
				
/* 				{ */
/* 					long sec = e_time.tv_sec - s_time.tv_sec; */
/* 					long usec = e_time.tv_usec - s_time.tv_usec; */
					
/* 					printf("%ld:%ld\n", sec, usec); */
/* 				} */
			}
			
			{
				int done = 0;
				int inserts = 0;
				int deletes = 0;
				int total =0;
				int db_done = 0;
				
				/* prime the item for the loop. */
				db_item = database_iterator_next(db_it);
				db_done = (db_item == 0);
				
				/* compare part. */
				while(!done)
					{
						if (!vol_files && db_done)
								{
									/* no more items to go through, we're done. */
									done = 1;
								}
							else
								{
									++total;
									
									if (vol_files && db_done)
										{
											vol_item = eina_list_data_get(vol_files);
											database_video_file_add(db, vol_item);
											++inserts;
											
											if (debug) { printf("vi:%s\n",  vol_item->path); }
											vol_files = vol_files->next;
										}
									else if (!vol_files && !db_done)
										{
											database_video_file_del(db, db_item->path);
											++deletes;
											
											volume_item_free(db_item);
											db_item = database_iterator_next(db_it);
											db_done = (db_item == 0);
										}
									else
										{
											/* both exist. */
											vol_item = eina_list_data_get(vol_files);
											
											//	printf("d:%s\nv:%s\n", db_item->path, vol_item->path);
											
											int cmp = strcmp(vol_item->path, db_item->path);
											if (cmp == 0)
												{
													/* they're the same, increment both lists. */
													volume_item_free(db_item);
													db_item = database_iterator_next(db_it);
													db_done = (db_item == 0);
													vol_files = vol_files->next;
												}
											else if (cmp < 0)
												{
													/* the fs item goes below the database item,
													 * insert the fs item and only increment the fs side.
													 */
													database_video_file_add(db, vol_item);
													++inserts;
													vol_files = vol_files->next;
												}
											else if (cmp > 0)
												{
													/* the fs item goes after the current db item,
													 * this indicates the db item is no longer needed.
													 * so delete it and move the db item forward.
													 */
													database_video_file_del(db, db_item->path);
													++deletes;
													volume_item_free(db_item);
													
													db_item = database_iterator_next(db_it);
													db_done = (db_item == 0);
												}
										}
								}
					}
				
				printf("total=%d\n", total);
				printf("inserts=%d\n", inserts);
				printf("deletes=%d\n", deletes);
			}
						
			volume_deindex((char*)vol_root);
			database_iterator_free(db_it);
			database_free(db);
			
			db = 0;
		}
	
	ecore_file_shutdown();
	ecore_shutdown();
	
	return 0;
}

char* gen_file(char* vol_path)
{
	DIR* dir = 0;
	struct dirent* de;
	int done =0;
	char buf[4096];
	char* file = 0;
	
	while(!done)
		{
			if (!dirstack)
				{
					if (vol_path)
						{
							snprintf(cur_path, sizeof(cur_path), "%s", vol_path);
						}
					
					dir = opendir(cur_path);
					dirstack = eina_list_append(dirstack, dir);
				}
			
			dir = eina_list_data_get(eina_list_last(dirstack));
			
			/* base case */
			if (!dirstack) { done = 1; break; }
			
			de = readdir(dir);
			if (de)
				{
					if (de->d_name[0] != '.')
						{
							/* link? */
							char* link;
							snprintf(buf, sizeof(buf), "%s/%s", cur_path, de->d_name);
							
							link = ecore_file_readlink(buf);
							if (link) { free(link); }
							else
								{
									if (ecore_file_is_dir(buf))
										{
											dir = opendir(buf);
											if (dir)
												{
													dirstack = eina_list_append(dirstack, dir);
													snprintf(cur_path, sizeof(cur_path), buf);
												}
											else
												{
													/* restore the original directory. */
													dir = eina_list_data_get(eina_list_last(dirstack));
												}
										}
									else
										{
											file = strdup(buf);
											done = 1;
										}
								}
						}
				}
			else
				{
					char* p;
					closedir(dir);
					dirstack = eina_list_remove(dirstack, dir);
					
					p = strrchr(cur_path, '/');
					if (p) { *p = 0; }
				}
			
			if (!dirstack)
				{
					/* we're done! */
					done = 1;
					break;
				}
		}
	
	return file;
}
