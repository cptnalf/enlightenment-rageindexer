
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

#ifndef VERSION
#define VERSION "_unknown_"
#endif

char* gen_file();

static char cur_path[4096];
static Eina_List* dirstack =NULL;
const char* vol_root = NULL;
char* dir_prefix = 0;
int debug = 0;

extern int sha1_sum(const unsigned char *data, int size, unsigned char *dst);
void thumb_del(const char* filename);
static void prefix_setup(char* dir_root);

int main(int argc, char** argv)
{
	if (argc!=3)
		{
			fprintf(stderr, "rage_indexer <db file> <path>\n");
			fprintf(stderr, "(c) saa %s\n", VERSION);

			return 1;
		}
	
	printf("rage_indexer (c) saa %s\n", VERSION);
	printf("db=%s;root=%s\n", argv[1], argv[2]);
	vol_root = argv[2];
	
	prefix_setup(argv[2]);
	
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
											
											printf("%%add|%s|%s|%s\n",
														 vol_item->path, vol_item->name, 
														 vol_item->genre);

											if (debug) { printf("vi:%s\n",  vol_item->path); }
											vol_files = vol_files->next;
										}
									else if (!vol_files && !db_done)
										{
											database_video_file_del(db, db_item->path);
											
											/* we also want to delete the thumb file */
											thumb_del(db_item->path);
											++deletes;
											
											printf("%%del|%s|%s|%s|%.0f|%d\n",
														 db_item->path, db_item->name, 
														 db_item->genre, 
														 db_item->last_played, db_item->play_count);
											
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
													
													printf("%%add|%s|%s|%s\n",
																 vol_item->path, vol_item->name, 
																 vol_item->genre);
													
													vol_files = vol_files->next;
												}
											else if (cmp > 0)
												{
													/* the fs item goes after the current db item,
													 * this indicates the db item is no longer needed.
													 * so delete it and move the db item forward.
													 */
													database_video_file_del(db, db_item->path);
													thumb_del(db_item->path);
													++deletes;
													
													printf("%%del|%s|%s|%s|%.0f|%d\n",
																 db_item->path, db_item->name, 
																 db_item->genre, 
																 db_item->last_played, db_item->play_count);
													
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

/** generate the file list.
 */
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

/** delete the thumb eet associated with image we're moving/deleting.
 */
void thumb_del(const char* filename)
{
	const char cmap[] = "0123456789abcdef";
	unsigned char sha[40];
	char buf[4096];
	char name[4096];
	int i=0;
	
	sha1_sum((const unsigned char*)filename, strlen(filename), sha);
	
	/* convert the sha to a string */
	for(i=0; i < 20; ++i)
		{
			buf[(i*2) + 0] = cmap[(sha[i] >> 4) & 0xf];
			buf[(i*2) + 1] = cmap[(sha[i]     ) & 0xf];
		}
	buf[i*2] = 0;
	
	snprintf(name, sizeof(name), "%s/.rage/thumbs/%s.eet", getenv("HOME"), buf);
	
	if (ecore_file_exists(name))
		{
			ecore_file_unlink(name);
		}
}

/** setup the genre depending on the root directory.
 *  eg: movies => movies/<folder>/filename
 *      anime  => anime/<folder>/filename
 */
void prefix_setup(char* dir_root)
{
	int idx = strlen(dir_root);
	const char* last_dir_part;
	
	/* normalize the path to not be postfixed with a '/' */
	--idx;
	if (vol_root[idx] == '/') { dir_root[idx]='\0'; --idx; }
	
	last_dir_part = vol_root + idx;
	/* find the next '/' starting from the back. */
	while(last_dir_part != vol_root && (*last_dir_part) != '/')
		{ --last_dir_part; }
	if (*last_dir_part == '/') { ++last_dir_part; }
	/* either at the beginning of the path, or somewhere in between. */
	
	/* now, we want to see if we have a magic prefixer... */
	if (strncmp(last_dir_part, "anime", 5) == 0)
		{
			/* determine if we're dealing with anime... */
			dir_prefix = "anime/";
		}
	else
		{
			if (strncmp(last_dir_part, "movies", 6) == 0)
				{ dir_prefix = "movies/"; }
		}
	
	if (dir_prefix) { printf("%s!\n", dir_prefix); }
}
