
#include <Eina.h>
#include <Ecore_File.h>
#include <stdio.h>
#include <stdlib.h>
#include "volume.h"
#include <string.h>
#include "extensions.h"

static int debug;
static const char* vol_root;
static const char* translate;
static Eina_List* dirstack =NULL;
static char cur_path[4096];

static char* dir_prefix;
static Eina_List* items = 0;
static unsigned long items_count = 0;

static char* get_name(const char* path);
static const char* get_genre(const char* path);

static char* gen_file(const char* vol_path);
static void _set_dir_prefix(char* dir_root);

static int _ext_comparer(const void* one, const void* two)
{
	const struct _ext* ptrOne = (const struct _ext*)one;
	const struct _ext* ptrTwo = (const struct _ext*)two;
	
	return strcasecmp(ptrOne->ext, ptrTwo->ext);
}

void volume_init(const int n_debug, const char* n_vol_root, const char* n_translate)
{
	debug = n_debug;
	vol_root = strdup(n_vol_root);
	if (n_translate) 
		{
			translate = strdup(n_translate);
		}
	
	_set_dir_prefix((char*)vol_root);
}

void volume_shutdown()
{
	free((char*)vol_root);
	free((char*)translate);
}

void volume_index(const char* root, const char* tr)
{
 	char* newfile = 0;
	Volume_Item* item;
	
	newfile = gen_file(vol_root);
	while(newfile)
		{
			item = volume_file_scan(newfile);
			if (item)
				{
					items = eina_list_append(items, item);
					item = 0;
					++items_count;
				}
			
			free(newfile);
			newfile = gen_file(0);
		}
	
	items = eina_list_sort(items, items_count, volume_item_compare);
}

void volume_deindex(const char* root)
{
	if(items)
		{
			Eina_List* list = items;
			while(list)
				{
					Volume_Item* ptr = eina_list_data_get(list);
					list = eina_list_remove_list(list, list);
					
					volume_item_free(ptr);
					--items_count;
				}
		}
}

const Eina_List* volume_items_get()
{
	return items;
}

Volume_Item* volume_file_scan(const char* path)
{
	Volume_Item* item =0;
	char* ext = strrchr(path, '.');
	const char* type =0;
	struct _ext* found_item = NULL;
	struct _ext search_item;
	
	if (ext)
		{
			ext++;
			
			/* binary search through the extensions list
			 * then use the type .
			 */
			search_item.ext = ext;
			search_item.type = 0;
			
			void* key = bsearch(&search_item, 
													extensions, extensions_size, sizeof(struct _ext),
													_ext_comparer);
			
			found_item = (struct _ext*) key;
			
			if (found_item)
				{
					switch (found_item->type)
						{
						case (VIDEO_EXT): { type = eina_stringshare_add("video"); break; }
						case (PHOTO_EXT): { type = eina_stringshare_add("photo"); break; }
						case (AUDIO_EXT): { type = eina_stringshare_add("audio"); break; }
							
						default: { type = NULL; break; }
						};
					
					if (type)
						{
							char buf[1024];
							
							if (translate)
								{
									int vr_len = strlen(vol_root);
									int tr_len = strlen(translate);
									const char* fname_start = path + vr_len;
									const char* tmp = translate + (tr_len -1);
									
									if (*tmp != '/')
										{
											/* need to keep the '/' from the path. */
											if (*fname_start != '/') { ++fname_start; }
										}
									else
										{
											/* i don't need a '/'. */
											if (*fname_start == '/') { ++fname_start; }
										}
									
									snprintf(buf, sizeof(buf), "%s%s", translate, fname_start);
								}
							else { snprintf(buf, sizeof(buf), "%s", path); }
							
							item = volume_item_new(0, buf, 0, 0, type);
							item->name = get_name(path);
							item->genre = get_genre(path);
							item->created = ecore_file_mod_time(path);
						}
				}
		}
	
	return item;
}

static char* get_name(const char* path)
{
	const char* f;
	char* name =0;
	
	/* set the name. */
	f = ecore_file_file_get(path);
	if (f)
		{
			char* c;
			
			name = strdup(f);
			c = strrchr(name, '.');
			
			if (c) *c = 0;
			for (c = name; *c; c++)
				{
					switch (*c)
						{
						case('.'):
						case('_'): { *c = ' '; break; }
						}
				}
		}
	
	return name;
}
	
static const char* get_genre(const char* path)
{
	const char* genre;
	const char* f;
	
	f = ecore_file_dir_get(path);
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
							char buf[4096];
							int i =0;
							
							while (genre_start != f && *genre_start != '/') { --genre_start; }
							if (*genre_start == '/') { ++genre_start; }
							
							for(; *genre_start != 0 && *genre_start != '/'; 
									++i, ++genre_start)
								{
									switch(*genre_start)
										{
										case '_':
										case '.':
											{ buf[i] = ' '; break; }
										default: { buf[i] = *genre_start; break; }
										}
								}
							
							buf[i] = 0;
							
							genre = eina_stringshare_add(buf);
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
									
									/* anime needs a prefix... */
									if (dir_prefix)
										{
											snprintf(buf, sizeof(buf), dir_prefix);
											i=strlen(dir_prefix);
										}
									
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
									
									genre = eina_stringshare_add(buf);
								}
						}
				}
			
			if (! genre)
				{
					genre = eina_stringshare_add("Unknown");
				}
		}
	
	return genre;
}

Volume_Item* volume_item_new(const long long id, const char* path, const char* name, const char* genre, const char* type)
{
	Volume_Item* item = calloc(1, sizeof(Volume_Item));
	
	item->id = id;
	item->path = strdup(path);
	item->rpath = ecore_file_realpath(item->path);
	if (name) { item->name = strdup(name); }
	if (genre) { item->genre = eina_stringshare_add(genre); }
	if (type) { item->type = eina_stringshare_add(type); }
	
	return item;
}

Volume_Item* volume_item_copy(const Volume_Item* item)
{
	Volume_Item* item_copy = calloc(1, sizeof(Volume_Item));
	
	item_copy->id = item->id;
	item_copy->path = strdup(item->path);
	item_copy->rpath = ecore_file_realpath(item->rpath);
	if (item->name)  { item_copy->name = strdup(item->name); }
	if (item->genre) { item_copy->genre = eina_stringshare_add(item->genre); }
	if (item->type)  { item_copy->type = eina_stringshare_add(item->type); }
	
	item_copy->last_played = item->last_played;
	item_copy->play_count = item->play_count;
	item_copy->last_pos = item->last_pos;
	item_copy->length = item->length;

	return item_copy;
}

void volume_item_free(Volume_Item* item)
{
	if (item)
		{
			if (debug) 
				{	printf("0x%X;0x%X;0x%X;0x%X\n", 
								 (unsigned int)item, (unsigned int)item->path, 
								 (unsigned int)item->name, (unsigned int)item->genre); }
			
			free(item->path);
			free(item->rpath);
			free(item->name);
			if (item->genre) { eina_stringshare_del(item->genre); }
			if (item->type) { eina_stringshare_del(item->type); }
			
			free(item);
		}
}

/** compares two volume items,
 */
int volume_item_compare(const void* one, const void* two)
{
	const Volume_Item* v1;
	const Volume_Item* v2;
	v1 = one;
	v2 = two;
	return strcmp(v1->path, v2->path);
}

/** setup the genre depending on the root directory.
 *  eg: movies => movies/<folder>/filename
 *      anime  => anime/<folder>/filename
 */
static void _set_dir_prefix(char* dir_root)
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

/** generate the file list.
 */
static char* gen_file(const char* vol_path)
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
