/* filename: test_client.c
 *  chiefengineer
 *  Sat Feb 26 18:29:14 PST 2011
 */

#include "rage_ipc.h"
#include <Ecore.h>
#include <Ecore_Con.h>

Rage_Ipc* conn;
const char* query = NULL;

static Eina_Bool list_callback(Genre_Result* result, void* data)
{
	int i=0;
	printf("got answer\n");
	if (result)
		{
			for(; i< result->count; ++i)
				{
					printf("%s - %d\n", result->recs[i].label, result->recs[i].count);
				}
		}
	return EINA_TRUE;
}

static Eina_Bool _idle(void* data)
{
	Rage_Ipc* conn = data;
	printf("hello.\n");
	if (rage_ipc_ready(conn))
		{
			rage_ipc_genre_list(conn, query, list_callback, NULL);
			return EINA_FALSE;
		}
	
	return EINA_TRUE;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
		{
			printf("test_client <query>\n");
			exit(1);
		}
	
	query = argv[1];
	
	ecore_init();
	ecore_con_init();
	ecore_ipc_init();
	rage_ipc_init();
	
	conn = rage_ipc_create("localhost", 9889);
	
	ecore_idler_add(_idle, conn);
	ecore_main_loop_begin();
	
	rage_ipc_del(conn);

	rage_ipc_shutdown();
	ecore_ipc_shutdown();
	ecore_con_shutdown();
	ecore_shutdown();
	
	return 0;
}
