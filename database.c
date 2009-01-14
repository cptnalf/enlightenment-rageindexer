
#include <Evas.h>
#include <stdlib.h>
#include <stdio.h>
#include "volume.h"
#include "database.h"
#include <string.h>
#include <time.h>

static void* _video_files_next(DBIterator* it);
static char db_path[4096];

void database_init(const char* path)
{
	snprintf(db_path, sizeof(db_path), "%s", path);
}

/** create a connection to a database at path.
 *  @return pointer to the database, or NULL on failure.
 */
Database* database_new()
{
	int result;
	Database* db = calloc(1, sizeof(Database));
	
	//printf("db=%s\n", path);
	
	result = sqlite3_open(db_path, &db->db);
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
														"f_type TEXT,"            // type of file (video, audio, photo
														"playcount INTEGER,"      // number of times its played.
														"length INTEGER,"         // length in seconds.
														"lastplayed INTEGER)"     // time_t it was last played
														, NULL, NULL, &errmsg);
			
			if (result != SQLITE_OK)
				{
					fprintf(stderr, "unable to create table! :%s\n", errmsg);
					sqlite3_free(errmsg);
					/* don't care about the error message.
					*/
				}
		}
	return db;
}

/** free a database connected to with 'database_new'
 */
void database_free(Database* db)
{
	//printf("closing the db.\n");
	sqlite3_close(db->db);
	free(db);
}

static DBIterator* _database_iterator_new(NextItemFx next_item_fx, FreeFx free_fx, char** tbl_results,
																					const int rows, const int cols)
{
	DBIterator* it = malloc(sizeof(DBIterator));
	
	it->next_fx = next_item_fx;
	it->free_fx = free_fx;
	
	it->tbl_results = tbl_results;
	it->rows = rows;
	it->cols = cols;
	it->pos = 0; 
	/* point to one before the first item. 
	 * the result sets include the header rows, so we need to include this in our row count.
	 */
	
	return it;
}
/** delete an iterator.
 */
void database_iterator_free(DBIterator* it)
{
	if (it)
		{
			if (it->free_fx) { it->free_fx(it); }
			if (it->tbl_results) { sqlite3_free_table(it->tbl_results); }
			free(it);
			it = 0;
		}
}

/** move the iterator to the next item.
 *  so:
 *     /-before this function is done.
 *  ...[record][record]...
 *             \- here after this function executes.
 *
 *  @return the item which exists at the next position
 */
void* database_iterator_next(DBIterator* it)
{
	void* result = 0;
	
	if (database_iterator_move_next(it))
		{
			result = database_iterator_get(it);
		}
	
	return result;
}

void* database_iterator_get(DBIterator* it)
{
	if (it && it->next_fx) { return it->next_fx(it); }
	return 0;
}

int database_iterator_move_next(DBIterator* it)
{
	int result = 0;
	
	/* only push to the next item if:
	 *  there is an iterator,
	 *  there is a next function,
	 *  there is another record to point to.
	 */
	if (it && it->next_fx)
		{
			it->pos += it->cols;
			/* not zero rows and 
			 * the new position is less than or equal to the number of rows
			 * get the next result.
			 */
			if((it->rows != 0) && (it->pos <= (it->rows * it->cols)))
				{
					result = 1;
				}
		}
	return result;
}

/** retrieve all the files in the database.
 *  or filters the files with the given part of the query.
 *
 *  @param query_part2  the query after the 'from' clause.
 *  @return list or NULL if error (or no files)
 */
DBIterator* database_video_files_get(Database* db, const char* query_part2)
{
	char* error_msg;
	int result;
	int rows, cols;
	char** tbl_results=0;
	DBIterator* it = 0;
	char query[4096];
	
	const char* query_base = 
		"SELECT path, title, genre, f_type, playcount, length, lastplayed "
		"FROM video_files ";
	
	if (! query_part2)
		{
			query_part2 = "ORDER BY title, path ";
		}
	
	snprintf(query, sizeof(query), "%s %s", query_base, query_part2);
	
	result = sqlite3_get_table(db->db, query, &tbl_results, &rows, &cols, &error_msg);
	if (SQLITE_OK == result)
		{
			it =_database_iterator_new(_video_files_next, 0, /* no extra data to free. */
																 tbl_results, rows, cols);
		}
	else
		{
			printf("error: %s", error_msg);
			sqlite3_free(error_msg);
		}
	
	return it;
}

/** retrieve files from the database given part of a path.
 *  
 *  @param path  the front part of the path to search for.
 *  @return      an iterator pointing to before the first row, or null if no records.
 */
DBIterator* database_video_files_path_search(Database* db, const char* path)
{
	DBIterator* it;
	char* query;
	
	query = sqlite3_mprintf("WHERE path like '%q%s' "
													"ORDER BY title, path ",
													path, "%");
	
	it = database_video_files_get(db, query);
	sqlite3_free(query);
	
	return it;
}

/** retrieves all files from the database given the genre.
 *  
 *  @param genre  genre to filter the file list for
 *  @return       an iterator or null if no files with the genre exist.
 */
DBIterator* database_video_files_genre_search(Database* db, const char* genre)
{
	DBIterator* it;
	char* query;
	
	query = sqlite3_mprintf("WHERE genre = '%q' "
													"ORDER BY title, path ",
													genre);
	
	it = database_video_files_get(db, query);
	sqlite3_free(query);
	
	return it;
}

/** retrieves the first 25 files with a playcount greater than 0.
 *  orders the list by playcount, title then path.
 *
 *  @return an iterator or null if no files have been played.
 */
DBIterator* database_video_favorites_get(Database* db)
{
 	const char* where_clause = 
		"WHERE playcount > 0 "
		"ORDER BY playcount DESC, lastplayed DESC, title, path "
		"LIMIT 25";
	
	return database_video_files_get(db, where_clause);
}

/** retrieve the first 25 files with a recent lastplayed date.
 *  orders by the last play date.
 *
 *  @return an iterator.
 */
DBIterator* database_video_recents_get(Database* db)
{
	const char* where_clause =
		"ORDER BY lastplayed DESC, title, path "
		" LIMIT 25";
	return database_video_files_get(db, where_clause);
}

static void* _genre_next(DBIterator* it)
{
	Genre* genre = malloc(sizeof(Genre));
			
	genre->label = evas_stringshare_add(it->tbl_results[it->pos + 0]);
	genre->count = atoi(it->tbl_results[it->pos +1]);
	
	return genre;
}

/** get a list of the genres in the database.
 */
DBIterator* database_video_genres_get(Database* db)
{
	DBIterator* it = 0;
	char** tbl_results = 0;
	int rows, cols;
	int result;
	char* error_msg;
	char* query = 
		"SELECT genre, count(path) "
		"FROM video_files "
		"GROUP BY genre "
		"ORDER BY genre";
	
	result = sqlite3_get_table(db->db, query, &tbl_results, &rows, &cols, &error_msg);
	if (SQLITE_OK == result)
		{
			if (rows > 0)
				{
					int max_item = rows * cols;
					
					it = _database_iterator_new(_genre_next, 0, /* nothing to free */
																			tbl_results, rows, cols);
				}
		}
	
	return it;
}

/** delete a file from the database.
 */
void database_video_file_del(Database* db, const char* path)
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
void database_video_file_add(Database* db, const Volume_Item* item)
{
	int result;
	char* error_msg =0;
	char* query = sqlite3_mprintf(
		 "INSERT INTO video_files (path, title, genre, f_type, playcount, length, lastplayed) "
		 "VALUES(%Q, %Q, %Q, %Q, %d, %d, %d)",
		 item->path, item->name, item->genre, item->type,
		 item->play_count, item->length, item->last_played);
	
	result = sqlite3_exec(db->db, query, NULL, NULL, &error_msg);
	if (result != SQLITE_OK)
		{
			fprintf(stderr, "db: insert error: \"%s\"; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	
	sqlite3_free(query);
}

/* played the file.
 */
void database_video_file_update(Database* db, Volume_Item* item)
{
	int result;
	char* error_msg;
	char* query;
	time_t lp = time(0);
	
	/* update the values so they can be written to the database. */
	item->play_count ++;
	item->last_played = 0.0 + lp;
	
	query = sqlite3_mprintf("UPDATE video_files "
													"SET playcount = %d, lastplayed = %d "
													"WHERE path = '%q' ",
													item->play_count, lp, item->path);
	printf ("%s;%s;%s\n", query, item->path, item->name);
	
	result = sqlite3_exec(db->db, query, NULL, NULL, &error_msg);
	if (SQLITE_OK != result)
		{
			fprintf(stderr, "db: update error:\"%s\"; %s\n", query, error_msg);
			sqlite3_free(error_msg);
		}
	sqlite3_free(query);
}

/* you HAVE TO SELECT AT LEAST (IN THIS ORDER)
 * path, title, genre, f_type, playcount, length, lastplayed
 */
static void* _video_files_next(DBIterator* it)
{
#define COL_PATH       0
#define COL_TITLE      1
#define COL_GENRE      2
#define COL_FTYPE      3
#define COL_PLAYCOUNT  4
#define COL_LENGTH     5
#define COL_LASTPLAYED 6

	Volume_Item* item;

	/*
	printf("%dx%d\n", it->rows, it->cols);
	printf ("%s;%s;%s\n", it->tbl_results[it->pos + COL_PATH], it->tbl_results[it->pos + COL_TITLE],
					it->tbl_results[it->pos + COL_GENRE]);
	*/
	item = volume_item_new(it->tbl_results[it->pos + COL_PATH], it->tbl_results[it->pos + COL_TITLE],
												 it->tbl_results[it->pos +COL_GENRE], it->tbl_results[it->pos + COL_FTYPE]);
	
	item->play_count = atoi(it->tbl_results[it->pos+COL_PLAYCOUNT]);
	item->length = atoi(it->tbl_results[it->pos+COL_LENGTH]);
	item->last_played = atoi(it->tbl_results[it->pos+COL_LASTPLAYED]);
	
	return item;
}
