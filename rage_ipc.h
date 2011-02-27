/* filename: rage_ipc.h
 *  chiefengineer
 *  Fri Feb 25 22:48:20 PST 2011
 */

#include <Ecore.h>
#include <Ecore_Ipc.h>

#include "rage_ipc_structs.h"

typedef struct _Rage_VolItem Rage_VolItem;
typedef struct _Rage_Ipc Rage_Ipc;

struct _Rage_Ipc
{
	Ecore_Ipc_Server *server;
	char *name;
	char *key;
	char *ident;
	Eina_Bool (*_path_query_callback)(Query_Result* result, void* data);
	void* _path_query_data;
	Eina_Bool (*genre_list_callback)(Genre_Result* result, void* data);
	void* genre_list_data;
	
	unsigned char connected : 1;
	unsigned char version_ok : 1;
	unsigned char who_ok : 1;
	unsigned char ready : 1;
};

int rage_ipc_init();
void rage_ipc_shutdown();

Rage_Ipc* rage_ipc_create(const char* addr, const int port);
void rage_ipc_del(Rage_Ipc* conn);

Eina_Bool rage_ipc_ready(Rage_Ipc* conn);

void rage_ipc_media_add(Rage_Ipc* conn, char* path, char* name, 
												const char* genre, const char* type, time_t created_date);
void rage_ipc_media_del(Rage_Ipc* conn, const char* path);
void rage_ipc_media_played(Rage_Ipc* conn, const char* path, time_t playedDate);
void rage_ipc_thumb_gen(Rage_Ipc* conn, const char* path);

void rage_ipc_genre_list(Rage_Ipc* conn, const char* genre,
												 Eina_Bool (*callback)(Genre_Result* result, void* data),
												 void* data);
void rage_ipc_media_path_query(Rage_Ipc* conn, const char* path, 
															 Eina_Bool (*callback)(Query_Result* result, void* data), 
															 void* data);
/*
void rage_ipc_thumb_get(
*/
