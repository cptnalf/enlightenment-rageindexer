
#include <time.h>
typedef struct _Volume_Item Volume_Item;

struct _Volume_Item
{
	long long id;
   char       *path;
   char       *rpath;
   char       *name;
   const char *genre;
   const char *type;
   double      last_played;
   int         play_count;
   double      last_pos;
   double      length;
   char       *artist;
   char       *album;
   int         track;
	time_t created;
};

void volume_init(const int n_debug, const char* n_vol_root, const char* n_translate);
void volume_shutdown();
void volume_update(void);
void volume_load(void);
void volume_add(char *vol);
void volume_del(char *vol);
int  volume_exists(char *vol);
void volume_index(const char* root, const char* tr);
void volume_deindex(const char* root);
int  volume_type_num_get(char *type);
const Eina_List *volume_items_get(void);
Volume_Item* volume_file_scan(const char* path);
Volume_Item* volume_item_new(const long long id, const char* path, const char* name, const char* genre, const char* type);
Volume_Item* volume_item_copy(const Volume_Item* item);
void volume_item_free(Volume_Item* item);
int volume_item_compare(const void* one, const void* two);

extern int VOLUME_ADD;
extern int VOLUME_DEL;
extern int VOLUME_TYPE_ADD;
extern int VOLUME_TYPE_DEL;
extern int VOLUME_SCAN_START;
extern int VOLUME_SCAN_STOP;
extern int VOLUME_SCAN_GO;
