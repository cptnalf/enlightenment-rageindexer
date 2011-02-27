/* filename: fs_mon.c
 *  chiefengineer
 *  Sun Feb 27 11:09:17 PST 2011
 */

#include <Eina.h>
#include <Ecore_File.h>

#include "fs_mon.h"
#include "rage_ipc.h"
#include "volume.h"

static Eina_List* efms = NULL;
static const char* monitor_root = NULL;
static const char* remote_root = NULL;
static Rage_Ipc* conn = NULL;

static Eina_List* gen_list(char* vol_path);

static void monitor_cb(void* data, Ecore_File_Monitor* em, 
								Ecore_File_Event event, 
								const char* path);

void fs_mon_init(const char* mon_root, const char* r_root, Rage_Ipc* c)
{
	monitor_root = mon_root;
	remote_root = r_root;
	conn = c;
	
	volume_init(0, mon_root, r_root);
}

void fs_mon_shutdown()
{
	Eina_List* l;
	Ecore_File_Monitor* efm = NULL;
	
	conn = NULL;
	
	EINA_LIST_FOREACH(efms, l, efm)
		{ ecore_file_monitor_del(efm); }
	
	eina_list_free(efms);
}

void fs_mon_startup()
{
	Ecore_File_Monitor* efm = NULL;
	Eina_List* dir_list ;
	Eina_List* l;
	char* d;
	
	printf("starting fs monitoring of %s\n", monitor_root);
	
	dir_list = gen_list((char*)monitor_root);
	EINA_LIST_FOREACH(dir_list, l, d)
		{
			printf("%s\n", d);
			
			efm = ecore_file_monitor_add(d, monitor_cb, NULL);
			efms = eina_list_append(efms, efm);
			free (d);
		}
	
	eina_list_free(dir_list);
}

static void monitor_cb(void* data, Ecore_File_Monitor* em, 
											 Ecore_File_Event event, 
											 const char* path)
{
	if (! conn) { return; }
	
	switch (event)
		{
		case ECORE_FILE_EVENT_CREATED_FILE:
			{
				Volume_Item* vi = volume_file_scan(path);
				
				if (vi)
					{
						rage_ipc_media_add(conn, 
															 vi->path, vi->path, vi->genre, vi->type, vi->created);
						volume_item_free(vi);
					}
				
				break;
			}
		case (ECORE_FILE_EVENT_CREATED_DIRECTORY):
			{
				Ecore_File_Monitor* efm;
				printf("created dir:%s\n", path);
				
				efm = ecore_file_monitor_add(path, monitor_cb, NULL);
				efms = eina_list_append(efms, efm);
				break;
			}
			
		case (ECORE_FILE_EVENT_DELETED_FILE):
			{
				rage_ipc_media_del(conn, path);
				break;
			}
		case (ECORE_FILE_EVENT_DELETED_DIRECTORY):
			{
				printf("deleted dir: %s\n", path);
				{
					Eina_List* l;
					Ecore_File_Monitor* efm;
					
					EINA_LIST_FOREACH(efms, l, efm)
						{
							const char* efmpath = ecore_file_monitor_path_get(efm);
							if (!strcmp(efmpath, path))
								{
									efms = eina_list_remove(efms, efm);
									ecore_file_monitor_del(efm);
									break;
								}
						}
				}
				
				break;
			}
		case (ECORE_FILE_EVENT_DELETED_SELF):
			{
				printf("deleted the root monitor directory?\n");
				break;
			}
		default:
			break;
		}
}

/** generate the file list.
 */
static Eina_List* gen_list(char* vol_path)
{
	Eina_List* dirs_to_watch = NULL;
	Eina_List* dirstack = NULL;

	char cur_path[4096];
 
	DIR* dir = 0;
	struct dirent* de;
	int done =0;
	char buf[4096];
	char* dirpath = NULL;
	
	while(!done)
		{
			if (!dirstack)
				{
					if (vol_path)
						{
							int len;
							snprintf(cur_path, sizeof(cur_path), "%s", vol_path);
							
							len = strlen(cur_path) - 1;
							if (cur_path[len] == '/') { cur_path[len] = '\0'; }
						}
					
					dir = opendir(cur_path);
					
					dirpath = strdup(cur_path);
					dirs_to_watch = eina_list_append( dirs_to_watch, dirpath);
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
													dirpath = strdup(cur_path);
													dirs_to_watch = eina_list_append( dirs_to_watch, dirpath);
												}
											else
												{
													/* restore the original directory. */
													dir = eina_list_data_get(eina_list_last(dirstack));
												}
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
	
	eina_list_free(dirstack);
	
	return dirs_to_watch;
}
