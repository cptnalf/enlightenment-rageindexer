
#include <Eina.h>
#include <Ecore_File.h>
#include <stdio.h>
#include <stdlib.h>
#include "volume.h"
#include <string.h>
#include "extensions.h"
extern int debug;

extern const char* vol_root;
extern char* dir_prefix;
static Eina_List* items = 0;
static unsigned long items_count = 0;

static char* get_name(const char* path);
static const char* get_genre(const char* path);
static Volume_Item* volume_file_scan(const char* path);
static int volume_item_compare(const void* one, const void* two);

extern char* gen_file(char* vol_path);

static int _ext_comparer(const void* one, const void* two)
{
	const struct _ext* ptrOne = (const struct _ext*)one;
	const struct _ext* ptrTwo = (const struct _ext*)two;
	
	return strcasecmp(ptrOne->ext, ptrTwo->ext);
}

void volume_index(char* vol)
{
 	char* newfile = 0;
	Volume_Item* item;
	
	newfile = gen_file(vol);
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

void volume_deindex(char* vol)
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

static Volume_Item* volume_file_scan(const char* path)
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
						case (PHOTO_EXT): { type = eina_stringshare_add("audio"); break; }
						case (AUDIO_EXT): { type = eina_stringshare_add("photo"); break; }
							
						default: { type = NULL; break; }
						};
					
					if (type)
						{
							item = volume_item_new(0, path, 0, 0, type);
							item->name = get_name(path);
							item->genre = get_genre(path);
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
static int volume_item_compare(const void* one, const void* two)
{
	const Volume_Item* v1;
	const Volume_Item* v2;
	v1 = one;
	v2 = two;
	return strcmp(v1->path, v2->path);
}
