
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

#ifndef VERSION
#define VERSION "_unknown_"
#endif

typedef struct _Indexer_Data Indexer_Data;
struct _Indexer_Data
{
	Rage_Ipc* conn;
	int done;
	int inserts;
	int deletes;
	int total;
	int db_done;

	const Eina_List* vol_files;
	const Eina_List* db_files;
	Eina_List* db_src;
	Volume_Item* db_item, *vol_item;
};

char* gen_file();

static char cur_path[4096];
static Eina_List* dirstack =NULL;
const char* vol_root = NULL;
char* dir_prefix = 0;
int debug = 0;

extern int sha1_sum(const unsigned char *data, int size, unsigned char *dst);
void thumb_del(const char* filename);
static void prefix_setup(char* dir_root);
static Eina_Bool _idler(void* data);
static Eina_Bool _dbQuery(void* data);

int main(int argc, char** argv)
{
	const char* addr;
	int port;
	
	if (argc!=3)
		{
			fprintf(stderr, "rage_indexer <server> <port> <path>\n");
			fprintf(stderr, "(c) saa %s\n", VERSION);

			return 1;
		}
	
	addr = argv[1];
	port = atoi(argv[2]);
	vol_root = argv[3];
	
	printf("rage_indexer (c) saa %s\n", VERSION);
	printf("rageipc://%s:%d;root=%s\n", addr, port, vol_root);
	
	prefix_setup(argv[3]);
	
	//snprintf(cur_path, sizeof(cur_path), "%s", vol_root);
	
	ecore_init();
	ecore_file_init();
	ecore_con_init();
	ecore_ipc_init();
	rage_ipc_init();
	
	{
		time_t start_time, end_time;
		Indexer_Data idxData;
		
		memset(&idxData, 0, sizeof(Indexer_Data));
		
		start_time = time(0);
		
		volume_index((char*)vol_root);
		
		end_time = time(0);
		/* 			printf("fs-query time=%lds\n", end_time - start_time); */
		idxData.vol_files = volume_items_get();
		
		{
			Ecore_Idler* idler;
			
			idxData.conn = rage_ipc_create("localhost", 9889);
			idler = ecore_idler_add( _dbQuery, &idxData);
			
			ecore_main_loop_begin();
			
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
	
	idxData->db_files = idxData->db_src;

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
	
	rage_ipc_media_path_query(idxData->conn, vol_root, _path_query_finished, data);
	return EINA_FALSE;
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
																 idxData->vol_item->path, idxData->vol_item->name, 
																 idxData->vol_item->genre, idxData->vol_item->type,
																 created);
							++ idxData->inserts;
							
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
																		 idxData->vol_item->path, idxData->vol_item->name, 
																		 idxData->vol_item->genre, idxData->vol_item->type,
																		 created);
									++ idxData->inserts;
									
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
