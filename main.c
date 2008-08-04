
#include <Ecore.h>
#include <Evas.h>
#include <Evas_Data.h>
#include <sqlite3.h>

#include "volume.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

int main(int argc, char** argv)
{
	if (argc!=2)
		{
			fprintf(stderr, "need the path to the database file.\n");
			return 1;
		}
	
	ecore_init();
	evas_init();
	
	Database* db = database_new(argv[1]);
	
	if (db)
		{
			Evas_List* list = database_get_files(db, 0);
			
			while(list)
				{
					Volume_Item* item;
					
					list = evas_list_remove(list, list);
					item = (Volume_Item*)list->data;
					
					printf(item->path);
					
					free(item->path);
					free(item->name);
					if (item->genre) { evas_stringshare_del(item->genre); }
					free(item);
				}
			
			database_free(db);
			db = 0;
		}
	
	ecore_shutdown();
	
	return 0;
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
			for(; i < rows; ++i)
				{
					item = calloc(1, sizeof(Volume_Item));
					item->path = strdup(tbl_results[i + COL_PATH]);
					item->name = strdup(tbl_results[i + COL_TITLE]);
					
					item->genre = evas_stringshare_add(tbl_results[i +COL_GENRE]);
					
					item->play_count = atoi(tbl_results[i+COL_PLAYCOUNT]);
					item->length = atoi(tbl_results[i+COL_LENGTH]);
					item->last_played = atoi(tbl_results[i+COL_LASTPLAYED]);
					
					list = evas_list_append(list, item);
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
	if (result == SQLITE_OK)
		{
			fprintf(stderr, "db: insert error: \"%s\"; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	
	sqlite3_free(query);
}

