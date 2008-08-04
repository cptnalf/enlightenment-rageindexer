
#include <Ecore.h>
#include <Evas.h>
#include <Evas_Data.h>
#include <Ecore_File.h>
#include <sqlite3.h>

#include "volume.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

typedef struct _Database Database;
struct _Database
{
	sqlite3* db;
};

Database* database_new(const char* path);
void database_free(Database* db);
Evas_List* database_get_files(Database* db, const char* path);
void database_delete_file(Database* db, const char* path);
void database_insert_file(Database* db, const Volume_Item* item);

static char* gen_file();
static Volume_Item* volume_item_new(const char* path, const char* title, const char* genre);
static void volume_item_delete(Volume_Item* item);

static char cur_path[4096];
static Evas_List* dirstack =0;
static const char* vol_root = 0;
static int debug = 0;

int main(int argc, char** argv)
{
	if (argc!=3)
		{
			fprintf(stderr, "need the path to the database file.\n");
			return 1;
		}
	
	printf("db=%s;root=%s\n", argv[1], argv[2]);
	vol_root = argv[2];
	
	snprintf(cur_path, sizeof(cur_path), "%s", vol_root);
	
	ecore_init();
	ecore_file_init();
	evas_init();
	
	Database* db = database_new(argv[1]);
	
	if (db)
		{
			Evas_List* list;
			char* newfile = 0;
			Volume_Item* item;
			time_t start_time, end_time;
			start_time = time(0);
			
			newfile = gen_file();
			while(newfile)
				{
					const char* f;
					item = volume_item_new(newfile, 0, 0);
					
					/* set the name. */
					f = ecore_file_file_get(newfile);
					if (f)
						{
							char* c;
							
							item->name = strdup(f);
							c = strrchr(item->name, '.');
							
							if (c) *c = 0;
							for (c = item->name; *c; c++)
								{
									switch (*c)
										{
										case('.'):
										case('_'): { *c = ' '; break; }
										}
								}
						}
					
					f = ecore_file_dir_get(newfile);
					if (f)
							{
								int dir_len = strlen(vol_root);
								
								if (!strncmp(vol_root, f, dir_len))
									{
										if (strlen(f) == dir_len)
											{
												/* well, crap, we've got the same name.
												 * go back one dir.
												 */
												const char* genre_start = f + strlen(f);
												while (genre_start != f && *genre_start != '/') { --genre_start; }
												if (*genre_start == '/') { ++genre_start; }
												
												item->genre = evas_stringshare_add(genre_start);
											}
										else
											{
												/* we matched the directory to the first portion
												 * of the filename
												 * verify
												 */
												
												if (vol_root[dir_len -1] == f[dir_len -1])
													{
														char buf[4096];
														const char* it = f + dir_len;
														int i =0;
														
														while(*it == '/' && it != 0) { ++it; }
														
														for(; *it != 0 && *it != '/'; ++i, ++it)
															{
																switch(*it)
																	{
																	case '_':
																	case '.':
																		{ buf[i] = ' '; break; }
																	default: { buf[i] = *it; break; }
																	}
															}
														
														buf[i] = 0;
														
														item->genre = evas_stringshare_add(buf);
													}
											}
									}
								
								if (! item->genre)
									{
										item->genre = evas_stringshare_add("Unknown");
									}
							}
					
					database_insert_file(db, item);
					volume_item_delete(item);
					
					free(newfile);
					newfile = gen_file();
				}
			
			end_time = time(0);
			printf("%ld\n", end_time - start_time);
			start_time = time(0);
			list = database_get_files(db, 0);
			end_time = time(0);
			printf ("%ld\n", end_time - start_time);
			item = 0;
 			while(list) 
				{
					item = (Volume_Item*)list->data;
					list = evas_list_remove(list, item);
					
					if (item)
						{
							//printf("%s;%s;%s;\n", item->path, item->name, item->genre);
							printf (".");
							volume_item_delete(item);
						}
				}
			
			database_free(db);
			db = 0;
		}
	
	ecore_file_shutdown();
	ecore_shutdown();
	
	return 0;
}

static char* gen_file()
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
					dir = opendir(cur_path);
					dirstack = evas_list_append(dirstack, dir);
				}
			
			dir = evas_list_data(evas_list_last(dirstack));
			
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
													dirstack = evas_list_append(dirstack, dir);
													snprintf(cur_path, sizeof(cur_path), buf);
												}
											else
												{
													/* restore the original directory. */
													dir = evas_list_data(evas_list_last(dirstack));
												}
										}
									else
										{
											char* ext = strrchr(buf, '.');
											
											if (ext)
												{
													ext++;
													if ((!strcasecmp(ext, "avi"))   || (!strcasecmp(ext, "mov")) ||
															(!strcasecmp(ext, "mpg"))   || (!strcasecmp(ext, "mpeg"))||
															(!strcasecmp(ext, "vob"))   || (!strcasecmp(ext, "wmv")) ||
															(!strcasecmp(ext, "asf"))   || (!strcasecmp(ext, "mng")) ||
															(!strcasecmp(ext, "3gp"))   || (!strcasecmp(ext, "wmx")) ||
															(!strcasecmp(ext, "wvx"))   || (!strcasecmp(ext, "mp4")) ||
															(!strcasecmp(ext, "mpe"))   || (!strcasecmp(ext, "qt"))  ||
															(!strcasecmp(ext, "fli"))   || (!strcasecmp(ext, "dv"))  ||
															(!strcasecmp(ext, "wm"))    || (!strcasecmp(ext, "asx")) ||
															(!strcasecmp(ext, "movie")) || (!strcasecmp(ext, "lsf")) ||
															(!strcasecmp(ext, "mkv")) )
														{
															file = strdup(buf);
															done = 1;
														}
												}
										}
								}
						}
				}
			else
				{
					char* p;
					closedir(dir);
					dirstack = evas_list_remove(dirstack, dir);
					
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

/** create a connection to a database at path.
 *  @return pointer to the database, or NULL on failure.
 */
Database* database_new(const char* path)
{
	int result;
	Database* db = calloc(1, sizeof(Database));
		
	result = sqlite3_open(path, &db->db);
	if (result)
		{
			fprintf(stderr, "error: %s\n", sqlite3_errmsg(db->db));
			sqlite3_close(db->db);
			free(db);
			db = 0;
		}
	
	if (db)
		{
			char* errmsg;
			result = sqlite3_exec(db->db,
														"CREATE TABLE video_files("
														"path TEXT PRIMARY KEY,"  // sha hash of the path
														"genre TEXT,"             // genre of the file
														"title TEXT,"             // title of the file.
														"playcount INTEGER,"      // number of times its played.
														"length INTEGER,"         // length in seconds.
														"lastplayed INTEGER)"     // time_t it was last played
														, NULL, NULL, &errmsg);
			
			if (result != SQLITE_OK)
				{
					fprintf(stderr, "unable to create table! :%s\n", errmsg);
					sqlite3_free(errmsg);
				}
		}
	return db;
}

/** free a database connected to with 'database_new'
 */
void database_free(Database* db)
{
	printf("closing the db.\n");
	sqlite3_close(db->db);
	free(db);
}

/** retrieve all the files in the database.
 *  @return list or NULL if error (or no files)
 */
Evas_List* database_get_files(Database* db, const char* path)
{
#define COL_PATH       0
#define COL_TITLE      1
#define COL_GENRE      2
#define COL_PLAYCOUNT  3
#define COL_LENGTH     4
#define COL_LASTPLAYED 5

 	Volume_Item* item =0;
	const char* query = 
		"SELECT path, title, genre, playcount, length, lastplayed "
		"FROM video_files "
		"ORDER BY path";
	char* error_msg;
	int result;
	Evas_List* list = 0;
	int rows, cols;
	char** tbl_results=0;
	
	result = sqlite3_get_table(db->db, query, &tbl_results, &rows, &cols, &error_msg);
	if (SQLITE_OK == result)
		{
			int i = 0;
			const int max_item = rows*cols;
			
			printf("%dx%d\n", rows, cols);
			
			for(i=cols; i < max_item; i += cols)
				{
					//printf ("%s;%s;%s\n", tbl_results[i+COL_PATH], tbl_results[i + COL_TITLE],
					//					tbl_results[i + COL_GENRE]);
					item = volume_item_new(tbl_results[i + COL_PATH], tbl_results[i + COL_TITLE],
																 tbl_results[i +COL_GENRE]);
					
					item->play_count = atoi(tbl_results[i+COL_PLAYCOUNT]);
					item->length = atoi(tbl_results[i+COL_LENGTH]);
					item->last_played = atoi(tbl_results[i+COL_LASTPLAYED]);
					
					list = evas_list_append(list, item);
					item = 0;
				}
			
			sqlite3_free_table(tbl_results);
		}
	
	return list;
}

/** delete a file from the database.
 */
void database_delete_file(Database* db, const char* path)
{
	int result;
	char* error_msg;
	char* query = sqlite3_mprintf("DELETE FROM video_files WHERE path = %Q",
																path);
	
	printf("%s\n", query);
	result = sqlite3_exec(db->db, query, NULL, NULL, &error_msg);
	if (result != SQLITE_OK)
		{
			fprintf(stderr, "db: delete error: %s; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	
	sqlite3_free(query);
}

/** add a new file to the database
 */
void database_insert_file(Database* db, const Volume_Item* item)
{
	int result;
	char* error_msg =0;
	char* query = sqlite3_mprintf(
		 "INSERT INTO video_files (path, title, genre, playcount, length, lastplayed) "
		 "VALUES(%Q, %Q, %Q, %d, %d, %d)",
		 item->path, item->name, item->genre, 
		 item->play_count, item->length, item->last_played);
	
	result = sqlite3_exec(db->db, query, NULL, NULL, &error_msg);
	if (result != SQLITE_OK)
		{
			fprintf(stderr, "db: insert error: \"%s\"; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	
	sqlite3_free(query);
}

Volume_Item* volume_item_new(const char* path, const char* name, const char* genre)
{
	Volume_Item* item = calloc(1, sizeof(Volume_Item));
	
	if (debug){ printf("%s;%s;%s\n", path, name, genre); }
	item->path = strdup(path);
	
	if (name) { item->name = strdup(name); }
	if (genre) { item->genre = evas_stringshare_add(genre); }
	
	return item;
}

void volume_item_delete(Volume_Item* item)
{
	if (item)
		{
			if (debug) 
				{	printf("0x%X;0x%X;0x%X;0x%X\n", item, item->path, item->name, item->genre); }
			free(item->path);
			free(item->name);
			if (item->genre) { evas_stringshare_del(item->genre); }
			
			free(item);
		}
}
