/*
	\file chat_parser.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef chat_parser_h
#define chat_parser_h

// struttura per memorizzare le opzioni di configurazione del server
typedef struct info_server {
	char *unix_path;
	int max_connections;
	int th_in_pool;
	int max_msg_size;
	int max_file_size;
	int max_history;
	char *files_path;
	char *stat_path;
	int user_locks;
	int group_locks;
	int fd_locks;
	int buckets;
	int check[12];
} info_server_t;

/*
Estrae da un file le info di configurazione del server

param:
config_path - Path del file di configurazione
infos - struttura nella quale salvare i dati

retval:
-1 - Errore
0 - Successo
*/
int get_config(char *config_path, info_server_t *infos);

/*
Libera la memoria occupata dalla struct

param:
infos: struct da liberare
*/
void free_config(info_server_t *infos);

#endif
