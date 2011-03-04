
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>

#include "volume.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <time.h>

#include <Ecore_Con.h>
#include <Ecore_Ipc.h>
#include "rage_ipc.h"
#include "fs_mon.h"

#ifndef VERSION
#define VERSION "_unknown_"
#endif

typedef struct _Indexer_Data Indexer_Data;
struct _Indexer_Data
{
	Rage_Ipc* conn;
	const char* vol_root;
	int done;
	int inserts;
	int deletes;
	int total;
	int db_done;

	const Eina_List* vol_files;
	const Eina_List* db_files;
	Eina_List* db_src;
	Volume_Item* db_item, *vol_item;
	Eina_Bool monitor;
};

char* gen_file();

static int debug = 0;

static Eina_Bool _idler_start(void* data);
extern int sha1_sum(const unsigned char *data, int size, unsigned char *dst);
void thumb_del(const char* filename);
static Eina_Bool _idler(void* data);
static Eina_Bool _dbQuery(void* data);

int main(int argc, char** argv)
{
	int i=1;
	Indexer_Data idxData;
	const char* addr;
	int port;
	const char* vol_root;
	const char* translate = NULL;

	memset(&idxData, 0, sizeof(Indexer_Data));
	
	if (argc < 4)
		{
			fprintf(stderr, "rage_indexer -[m|i] <server> <port> <path> [translate path]\n");
			fprintf(stderr, "(c) saa %s\n", VERSION);

			return 1;
		}
	
	if (!strcmp(argv[i], "-m")) { idxData.monitor = EINA_TRUE; }
	else if (!strcmp(argv[i], "-i")) { idxData.monitor = EINA_FALSE; }
	else
		{
			fprintf(stderr, "unknown mode %s\n", argv[i]);
			return 1;
		}
	++i;
	
	addr = argv[i];
	++i;
	port = atoi(argv[i]);
	++i;
	vol_root = argv[i];
	idxData.vol_root = vol_root;
	++i;
	
	if (i < argc)
		{
			translate = argv[i];
		}
	
	printf("rage_indexer (c) saa %s\n", VERSION);
	printf("rageipc://%s:%d;root=%s\n", addr, port, vol_root);
	
	//snprintf(cur_path, sizeof(cur_path), "%s", vol_root);
	
	ecore_init();
	ecore_file_init();
	ecore_con_init();
	ecore_ipc_init();
	rage_ipc_init(1);

	if (! idxData.monitor)
		{
			time_t start_time, end_time;
			volume_init(debug, vol_root, translate);
			
			start_time = time(0);
			volume_index((char*)vol_root, translate);
			end_time = time(0);
			/* 			printf("fs-query time=%lds\n", end_time - start_time); */
			idxData.vol_files = volume_items_get();
		}

	idxData.conn = rage_ipc_create(addr, 9889);
	if (idxData.monitor) { fs_mon_init(vol_root, translate, idxData.conn); }
	{
		Ecore_Idler* idler = ecore_idler_add(_idler_start, &idxData);
	}
	
	ecore_main_loop_begin();
	
	if (idxData.monitor)
		{
			fs_mon_shutdown();
		}
	else
		{
			//ecore_idler_del(idler);
			rage_ipc_del(idxData.conn);
			idxData.conn = NULL;
			
			printf("total=%d\n", idxData.total);
			printf("inserts=%d\n", idxData.inserts);
			printf("deletes=%d\n", idxData.deletes);
			
			if (idxData.db_src)
				{
					/* these items have been freed as the comparison has progressed. */
					eina_list_free(idxData.db_src);
				}
			volume_deindex((char*)vol_root);
		}
	
	rage_ipc_shutdown();
	ecore_ipc_shutdown();
	ecore_con_shutdown();
	ecore_file_shutdown();
	ecore_shutdown();
	
	return 0;
}

static Eina_Bool _path_query_finished(Query_Result* result, void* data)
{
	Indexer_Data* idxData = data;
	Volume_Item* volitem;
	unsigned int i;
	Ecore_Idler* idle;
	
	printf("got %d records from the host\n", result->count);
	
	if (result)
		{
			for(i=0; i < result->count; ++i)
				{
					volitem = 
						volume_item_new(result->recs[i].id,
														result->recs[i].path,
														result->recs[i].name,
														result->recs[i].genre,
														result->recs[i].type
														);
					idxData->db_src = eina_list_append(idxData->db_src, volitem);
				}
			
			idxData->db_src = eina_list_sort(idxData->db_src, result->count, 
																			 volume_item_compare);			
			idxData->db_files = idxData->db_src;
		}

	idle = ecore_idler_add(_idler, data);
	
	return EINA_TRUE;
}

static Eina_Bool _dbQuery(void* data)
{
	/* int res; */
	/* struct timeval s_time, e_time; */
	
	/* res = gettimeofday(&s_time, 0); */
	/* idxData.db_it = database_video_files_path_search(db, vol_root); */
	/* res = gettimeofday(&e_time, 0); */
	
	/* 				{ */
	/* 					long sec = e_time.tv_sec - s_time.tv_sec; */
	/* 					long usec = e_time.tv_usec - s_time.tv_usec; */
	
	/* 					printf("%ld:%ld\n", sec, usec); */
	/* 				} */
	
	/* prime the item for the loop. */
	/* idxData.db_item = database_iterator_next(idxData.db_it); */
	/* idxData.db_done = (idxData.db_item == 0); */
	
	Indexer_Data* idxData = data;
	
	printf("querying for %s.\n", idxData->vol_root);
	rage_ipc_media_path_query(idxData->conn, idxData->vol_root, _path_query_finished, data);
	return EINA_FALSE;
}

static Eina_Bool _idler_start(void* data)
{
	Indexer_Data* idx_data = data;
	
	if (rage_ipc_ready(idx_data->conn))
		{
			printf("start!\n");

			if (idx_data->monitor) { fs_mon_startup(); }
			else
				{
					Ecore_Idler* idler = ecore_idler_add( _dbQuery, idx_data);
					idler = NULL;
				}
			return EINA_FALSE;
		}
	
	return EINA_TRUE;
}

/* 
 * ask for all of the items under path x.
 */
static Eina_Bool _idler(void* data)
{
	Indexer_Data* idxData = data;
	time_t created;
	
	/* compare part. */
	if (idxData->done) { ecore_main_loop_quit(); return EINA_FALSE; }
	else
		{
			if (!idxData->vol_files && !idxData->db_files)
				{
					/* no more items to go through, we're done. */
					idxData->done = 1;
				}
			else
				{
					++ idxData->total;
					
					if (idxData->vol_files && !idxData->db_files)
						{
							idxData->vol_item = eina_list_data_get(idxData->vol_files);
							
							created = time(0);
							rage_ipc_media_add(idxData->conn, 
																 idxData->vol_item->path, 
																 idxData->vol_item->name, 
																 idxData->vol_item->genre, 
																 idxData->vol_item->type,
																 created);
							++ idxData->inserts;
							
							if (debug)
								printf("%%add|%s|%s|%s\n",
											 idxData->vol_item->path, idxData->vol_item->name, 
											 idxData->vol_item->genre);
							
							if (debug) { printf("vi:%s\n",  idxData->vol_item->path); }
							idxData->vol_files = idxData->vol_files->next;
						}
					else if (! idxData->vol_files && idxData->db_files)
						{
							idxData->db_item = eina_list_data_get(idxData->db_files);
							
							rage_ipc_media_del(idxData->conn, idxData->db_item->path);
							
							/* we also want to delete the thumb file */
							thumb_del(idxData->db_item->path);
							++ idxData->deletes;
									
							if (debug)
								printf("%%del|%s|%s|%s|%.0f|%d\n",
											 idxData->db_item->path, idxData->db_item->name, 
											 idxData->db_item->genre, 
											 idxData->db_item->last_played, idxData->db_item->play_count);
							
							volume_item_free(idxData->db_item);
							idxData->db_files = idxData->db_files->next;
						}
					else
						{
							/* both exist. */
							idxData->vol_item = eina_list_data_get(idxData->vol_files);
							idxData->db_item = eina_list_data_get(idxData->db_files);
											
							//	printf("d:%s\nv:%s\n", db_item->path, vol_item->path);
											
							int cmp = strcmp(idxData->vol_item->path, idxData->db_item->path);
							if (cmp == 0)
								{
									/* they're the same, increment both lists. */
									volume_item_free(idxData->db_item);
									idxData->db_files = idxData->db_files->next;
									idxData->vol_files = idxData->vol_files->next;
								}
							else if (cmp < 0)
								{
									/* the fs item goes below the database item,
									 * insert the fs item and only increment the fs side.
									 */
									created = time(0);
									rage_ipc_media_add(idxData->conn, 
																		 idxData->vol_item->path, 
																		 idxData->vol_item->name, 
																		 idxData->vol_item->genre, 
																		 idxData->vol_item->type,
																		 created);
									++ idxData->inserts;
									
									if (debug)
										printf("%%add|%s|%s|%s\n",
													 idxData->vol_item->path, idxData->vol_item->name, 
													 idxData->vol_item->genre);
									
									idxData->vol_files = idxData->vol_files->next;
								}
							else if (cmp > 0)
								{
									/* the fs item goes after the current db item,
									 * this indicates the db item is no longer needed.
									 * so delete it and move the db item forward.
									 */
									rage_ipc_media_del(idxData->conn, idxData->db_item->path);
									thumb_del(idxData->db_item->path);
									++ idxData->deletes;
									
									if (debug)
										printf("%%del|%s|%s|%s|%.0f|%d\n",
													 idxData->db_item->path, idxData->db_item->name, 
													 idxData->db_item->genre, 
													 idxData->db_item->last_played, idxData->db_item->play_count);
									
									volume_item_free(idxData->db_item);
									idxData->db_files = idxData->db_files->next;
								}
						}
				}
		}
	return EINA_TRUE;
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

