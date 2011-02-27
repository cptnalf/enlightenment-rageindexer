/* filename: rage_ipc.c
 *  chiefengineer
 *  Fri Feb 25 22:48:10 PST 2011
 */

#include "rage_ipc.h"

#define __UNUSED__

static Eina_List* _nodes = NULL;
static int debug = 0;

static int _version = 10; /* current proto version */
static int _version_magic1 = 0x14f8ec67; /* magic number for version check */
static int _version_magic2 = 0x3b45ef56; /* magic number for version check */
static int _version_magic3 = 0x8ea9fca0; /* magic number for version check */
static char *_key_private = "private";
/* auth key for anyone with read and write access - i.e. your own home and devices only - hardcoded for now */
static char *_key_public = "public"; /* public members of the network - read and borrow rights only - hardcoded for now */
static char *_ident_info = "user=x;location=y;"; /* send as ident in response to a who - for now hardcoded */

static Eina_Bool _client_cb_add(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool _client_cb_del(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool _client_cb_data(void *data __UNUSED__, int type __UNUSED__, void *event);

int rage_ipc_init(int n_debug)
{
	debug = n_debug;
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_ADD, _client_cb_add, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DEL,	_client_cb_del, NULL);
	ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DATA, _client_cb_data, NULL) ;
	
	return EINA_TRUE;
}

void rage_ipc_shutdown()
{
	Rage_Ipc* item;
	
	while(_nodes)
		{
			item = _nodes->data;
			rage_ipc_del(item);
		}
}

Rage_Ipc* rage_ipc_create(const char* addr, const int port)
{
	Rage_Ipc* data = calloc(1, sizeof(Rage_Ipc));
	
	if (data)
		{
			data->server = ecore_ipc_server_connect(ECORE_IPC_REMOTE_SYSTEM,
																							(char*)addr, port,
																							data);
			
			if (data->server) 
				{
					data->name = strdup(addr);
					data->key = strdup(_key_private);
					_nodes = eina_list_append(_nodes, data);
					
					ecore_ipc_server_send(data->server, OP_VERSION, _version,
																_version_magic1, _version_magic2, _version_magic3,
																NULL, 0);
				}
			else
				{
					free(data);
					data = NULL;
				}
		}
	
	return data;
}

void rage_ipc_del(Rage_Ipc* conn)
{
	_nodes = eina_list_remove(_nodes, conn);
	if (conn->server) ecore_ipc_server_del(conn->server);
	if (conn->name) free(conn->name);
	if (conn->key) free(conn->key);
	free(conn);
}

Eina_Bool rage_ipc_ready(Rage_Ipc* conn)
{
	if (conn) { return (conn->ready ? EINA_TRUE : EINA_FALSE); }
	return EINA_FALSE;
}

void rage_ipc_media_add(Rage_Ipc* conn, char* path, char* name, 
												const char* genre, const char* type, time_t created_date)
{
	struct _Rage_Ipc_VolItem item;
	
	strncpy(item.path, path, sizeof(item.path));
	strncpy(item.name, name, sizeof(item.name));
	strncpy(item.genre, genre, sizeof(item.genre));
	strncpy(item.type, type, sizeof(item.type));
	item.created_date = created_date;
	
	if (debug > 0) { printf("add %s\n", path); }
	
	ecore_ipc_server_send(conn->server, OP_MEDIA_PUT, 0, 0, 0, 0,
												&item, sizeof(item));
}

void rage_ipc_media_del(Rage_Ipc* conn, const char* path)
{
	if (debug > 0) { printf("delete %s\n", path); }
	ecore_ipc_server_send(conn->server, OP_MEDIA_DELETE, 0, 0, 0, 0,
												path, strlen(path) + 1);
}

/* struct _Rage_Ipc_Played */
/* { */
/* 	char path[1024]; */
/* 	time_t playedDate; */
/* }; */

/* void rage_ipc_media_played(Rage_Ipc* conn, const char* path, const time_t playedDate) */
/* { */
/* 	struct _Rage_Ipc_Played item; */
/* 	strncpy(item.path, path, sizeof(item.path)); */
/* 	item.playedDate = playedDate; */
	
/* 	ecore_ipc_server_send(conn->server, OP_MEDIA_PLAYED, 0, 0, 0, 0, */
/* 												&item, sizeof(item)); */
/* } */
/* void rage_ipc_thumb_gen(Rage_Ipc* conn, const char* path) */
/* { */
/* 	ecore_ipc_server_send(conn->server, OP_THUMB_GEN, 0, 0, 0, 0,  */
/* 												path, strlen(path)); */
/* } */

void rage_ipc_genre_list(Rage_Ipc* conn, const char* genre,
												 Eina_Bool (*callback)(Genre_Result* result, void* data),
												 void* data)
{
	if (debug > 0) { printf("list genres\n"); }
	conn->genre_list_callback = callback;
	conn->genre_list_data = data;
	ecore_ipc_server_send(conn->server, OP_GENRES_GET, 0, 0, 0, 0, 
												genre, strlen(genre) + 1);
}

void rage_ipc_media_path_query(Rage_Ipc* conn, const char* path, 
															 Eina_Bool (*callback)(Query_Result* result, void* data),
															 void* data)
{
	if (debug > 0) { printf("list files for path %s\n", path); }
		
	conn->_path_query_callback = callback;
	conn->_path_query_data = data;
	
	ecore_ipc_server_send(conn->server, OP_MEDIA_QUERY, MQ_TYPE_PATH, 0, 0, 0,
												path, strlen(path) + 1);
}

static Eina_Bool _client_cb_add(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Server_Add *e;
	Rage_Ipc *nd;
	
	e = event;
	nd = ecore_ipc_server_data_get(e->server);
	if (!nd) 
		{
			printf("Warning: unknown server connected!\n");
			return EINA_TRUE;
		}
	nd->connected = 1;
	return EINA_TRUE;
}

static Eina_Bool _client_cb_del(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Server_Del *e;
	Rage_Ipc *nd;
   
	e = event;
	nd = ecore_ipc_server_data_get(e->server);
	if (!nd)
		{
			printf("Warning: unknown server deleted.\n");
			return EINA_TRUE;
		}
	
	nd->server = NULL;
	//rage_ipc_del(nd);
	return EINA_TRUE;
}

static Eina_Bool _client_cb_data(void *data __UNUSED__, int type __UNUSED__, void *event)
{
	Ecore_Ipc_Event_Server_Data *e;
	Rage_Ipc *nd;
  
	e = event;
	nd = ecore_ipc_server_data_get(e->server);
	
	if (!nd) return EINA_TRUE;
	
	switch (e->major)
		{
		case OP_VERSION: /* version info from client */
			if ((e->minor != _version) ||
					(e->ref != _version_magic1) ||
					(e->ref_to != _version_magic2) ||
					(e->response != _version_magic3))
				/* client version not matching ours or magic wrong */
				{
					ecore_ipc_server_send(nd->server, OP_VERSION_ERROR, _version,
																0, 0, 0, NULL, 0);
					ecore_ipc_server_flush(nd->server);
					rage_ipc_del(nd);
				}
			else
				{
					nd->version_ok = 1;
					ecore_ipc_server_send(nd->server, OP_USER_AUTH, 0,
																0, 0, 0, nd->key, strlen(nd->key) + 1);
					ecore_ipc_server_send(nd->server, OP_USER_WHO, 0,
																0, 0, 0, NULL, 0);
				}
			break;
		case OP_VERSION_ERROR: /* client does not like our version */
			ecore_ipc_server_flush(nd->server);
			rage_ipc_del(nd);
			break;
		case OP_SYNC: /* client requested a sync - reply with sync in e->minor */
			ecore_ipc_server_send(nd->server, OP_SYNC, e->minor,
														0, 0, 0, NULL, 0);
			break;
		case OP_USER_AUTH_ERROR:
			{
				printf("Authentication error!\n");
				rage_ipc_del(nd);
				break;
			}
			
		case OP_USER_WHO:
			{
				ecore_ipc_server_send(nd->server, OP_USER_IDENT, 0,
															0, 0, 0, _ident_info, strlen(_ident_info) + 1);
				break;
			}
			
		case OP_USER_IDENT:
			{
				if (debug > 1) { printf("ident?\n"); }
				if ((e->data) && (e->size > 1) && (((char *)e->data)[e->size - 1] == 0))
					{
						if (nd->ident) free(nd->ident);
						nd->ident = strdup(e->data);
					}
				nd->ready = 1;
				break; 
			}
			
			
		case OP_MEDIA_ADD:
			if (debug > 0) { printf("Got media add!\n"); }
			break;
		case OP_MEDIA_DEL:
			if (debug > 0) { printf("got media delete!\n"); }
			break;
		case (OP_MEDIA_LIST):
			{
				Query_Result* result = e->data;
				
				if (result->size == e->size)
					{
						/* this is the query response, so call the callback. */
						if (nd->_path_query_callback)
							nd->_path_query_callback(result, nd->_path_query_data);
					}
				
				nd->_path_query_callback = NULL;
				nd->_path_query_data = NULL;
				break;
			}
			
		case (OP_GENRE_LIST):
			{
				Genre_Result* result = e->data;
				if ((e->size == 0) || (result->size == e->size))
					{
						if (nd->genre_list_callback)
							nd->genre_list_callback(result, nd->genre_list_data);
					}
				
				nd->genre_list_callback = NULL;
				nd->genre_list_data = NULL;
				break;
			}

		case OP_MEDIA_LOCK_NOTIFY:
			break;
		case OP_MEDIA_UNLOCK_NOTIFY:
			break;
		case OP_MEDIA_UNLOCK:
			break;
		case OP_MEDIA_GET_DATA:
			break;
		case OP_THUMB_GET_DATA:
			break;
		default:
			break;
		}
	return EINA_TRUE;
}
