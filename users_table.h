/*
	\file users_table.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef users_table_h
#define users_table_h

#include "icl_hash.h"
#include "queue.h"
#include "config.h"
#include "utility.h"
#include <pthread.h>
#include "string_list.h"
#include "stats.h"
#include "message.h"

//acquisisce tutte le lock
#define LOCKALL(locks, n)			\
		for(int i=0; i<n; i++)		\
			LOCK(&locks[i])

//rilascia tutte le lock
#define UNLOCKALL(locks, n)			\
		for(int i=n-1; i>=0; i--)		\
			UNLOCK(&locks[i])

/*
Struttura per memorizzare i dati di un utente
*/
typedef struct chat_user {
	char username[MAX_NAME_LENGTH+1];		//nome utente
	int online;					//-1 = offline, >0 = fd su cui è connesso il client
	queue_t *msgs;				//lista degli ultimi messaggi ricevuti dall'utente
} chat_user_t;

/*
Struttura per memorizzare i dati di un gruppo
*/
typedef struct chat_group {
	char groupname[MAX_NAME_LENGTH+1];	//nome del gruppo
	string_list_t *members;			//lista contenente i nomi dei membri del gruppo
	char owner[MAX_NAME_LENGTH+1];		//nome del proprietario del gruppo
} chat_group_t;

/*
Struttura per memorizzare un messaggio della history di un utente
*/
typedef struct chat_message {
	char sender[MAX_NAME_LENGTH+1];		//nickname del mittente
	char *text;				//contenuto del messaggio
	int type;				//FILE_MESSAGE o TXT_MESSAGE
	int consegnato;				//stato di consegna (0 = non consegnato, 1 = già consegnato)
} chat_message_t;

/*
Struttura della tabella utenti
*/
typedef struct users_table {
	icl_hash_t *users;		//tabella hash contenente gli utenti iscritti (vedi struct sopra)
	icl_hash_t *groups;		//tabella hash contenente i gruppi registrati (vedi struct sopra)
	pthread_mutex_t *locks;		//array di lock per la sincronizzazione
	int u_locks;			//numero di lock utilizzate per la sincronizzazione sugli utenti
	int g_locks;			//numero di lock utilizzate per la sincornizzazione sui gruppi
	int fd_locks;			//numero di lock utilizzate per la sincornizzazione sui descrittori
	string_list_t *online_users;	//lista dei nomi degli utenti attualmente connessi e il descrittori associati
	int history;			//massimo numero di messaggi tenuti in memoria per ogni utente
	int max_msg_size;		//lunghezza massima di un messaggio
	int max_file_size;		//lunghezza massima del nome di un messaggio
	int m_consegnati;		//messaggi consegnati
	int m_in_attesa;		//messaggi da consegnare
	int f_consegnati;		//file consegnati
	int f_in_attesa;		//file da consegnare
	int errori;			//numero di messaggi di errore del server
} users_table_t;

/*
Libera la memoria occupata da un chat_message_t

param:
data - puntatore da liberare
*/
void freeChatMessage(void *data);

/*
Libera la memoria occupata da un message_t

param:
data - puntatore da liberare
*/
void freeMessage(void *data);

/*
Crea una nuova tabella vuota

param:
u_locks - numero di mutex da utilizzare per sincronizzarsi sugli user
g_locks - numero di mutex da utilizzare per sincornizzarsi sui gruppi
fd_locks - numero di mutex da utilizzare per sincornizzarsi sui descrittori
n_buckets - numero di buckets per le tabelle hash
history - numero massimo di messaggi memorizzati
max_msg_size - lunghezza massima di un messaggio
max_file_size - lunghezza massima del nome di un messaggio

retval: puntatore alla tabella appena creata
*/
users_table_t *create_table(int u_locks, int g_locks, int fd_locks, int n_buckets, int hisotry, int max_msg_size, int max_file_size);

/*
Distrugge una tabella

param:
table - tabella da distruggere

retval:
0 - successo
-1 - errore
*/
int destroy_table(users_table_t *table);

/*
Aggiunge un nuovo utente

param:
table - tabella inizializzata
username - nome dell'utente
fd - file descriptor su cui è connesso il client

retval:
OP_OK - successo
OP_NICK_ALREADY - utente già presente
OP_FAIL - errore
*/
int add_user(users_table_t *table, char *username, int fd);

/*
Elimina un utente

param:
table - tabella inizializzata
username - nome dell'utente

retval:
OP_OK - successo
OP_FAIL - errore
*/
int delete_user(users_table_t *table, char *username);

/*
Crea un nuovo gruppo aggiungendoci il suo creatore

param:
table - tabella inizializzata
owner - nome del creatore del gruppo
groupname - nome del gruppo da creare

retval:
OP_OK - successo
OP_NICK_ALREADY - gruppo o username già presente
OP_FAIL - errore
*/
int add_group(users_table_t *table, char *owner, char *groupname);

/*
Rimuove un gruppo

param:
table - tabella inizializzata
groupname - nome del gruppo da rimuovere

retval:
OP_OK - successo
OP_FAIL - errore
*/
int remove_group(users_table_t *table, char *groupname);

/*
Aggiunge un utente ad un gruppo

param:
table - tabella inizializzata
username - nome dell'utente
groupname - nome del gruppo

retval:
OP_OK - successo
OP_NICK_UNKNOWN - gruppo o username non registrati
OP_FAIL - errore
*/
int join_group(users_table_t *table, char *username, char *groupname);

/*
Disiscrive un utente da un gruppo

param:
table - tabella inizializzata
username - nome dell'utente
groupname - nome del gruppo

retval:
OP_OK - successo
OP_FAIL - errore
*/
int leave_group(users_table_t *table, char *username, char *groupname);

/*
Contrassegna un utente come online

param:
table - tabella inizializzata
username - nome dell'utente
fd - file descriptor su cui si è connesso il client

retval:
OP_OK - successo
OP_FAIL - errore
*/
int set_online(users_table_t *table, char *username, int fd);

/*
Contrassegna un utente come offline

param:
table - tabella inizializzata
username - nome dell'utente
fd - file descriptor su cui l'utente è attualmente connesso

retval:
OP_OK - successo
OP_FAIL - errore
*/
int set_offline(users_table_t *table, char *username, int fd);

/*
Rimuove un file descriptor dalla lista online e contrassegna l'utente associato come offline

param:
table - tabella inizializzata
fd - descrittore da rimuovere

retval:
OP_OK - successo
OP_FAIL - errore
*/
int set_offline_fd(users_table_t *table, int fd);

/*
Memorizza un messaggio diretto a un utente o ai membri di un gruppo

param:
table - tabella inizializzata
sender - mittente del messaggio
receiver - destinatario del messaggio (utente o gruppo)
text - contenuto del messaggio
fds - coda di file descriptor a cui inviare il messaggio (uno solo se receiver è un utente, più di uno se è un gruppo)

retval:
OP_OK - messaggio inviato correttamente
OP_NICK_UNKNOWN - sender o receiver non registrati
OP_FAIL - errore
OP_MSG_TOOLONG - messaggio troppo lungo
*/
int send_text(users_table_t *table, char *sender, char *receiver, char *text, queue_t *fds);

/*
Memorizza un messaggio diretto a tutti gli utenti registrati

param:
table - tabella inizializzata
sender - nome del mittente
text - contenuto del messaggio
fds - coda di decsrittori a cui inviare il messaggio

retval:
OP_OK - messaggio inviato correttamente
OP_NICK_UNKNOWN - sender non registrato
OP_FAIL - errore
OP_MSG_TOOLONG - messaggio troppo lungo
*/
int send_text_all(users_table_t *table, char *sender, char *text, queue_t *fds);

/*
Memorizza un file diretto a un utente o ai membri di un gruppo

param:
table - tabella inizializzata
sender - nome del mittente
receiver - destinatario del file (utente o gruppo)
name - nome del file
data - contenuto del file
writelen - lunghezza del file
fds - coda di file descriptor a cui inviare il messaggio (uno solo se il receiver è un utente, più di uno se è un gruppo)
dirpath - directory nella quale memorizzare il file

retval:
OP_OK - messaggio inviato correttamente
OP_NICK_UNKNOWN - sender o receiver non registrati
OP_FAIL - errore
OP_MSG_TOOLONG - file troppo lungo
*/
int send_file(users_table_t *table, char *sender, char *receiver, char *name, char *data, int writelen, queue_t *fds, char *dirpath);

/*
Prepara il file da spedire all'utente che lo ha richiesto

param:
table - tabella inizializzata
username - nome dell'utente che ha richiesto il file
name - nome del file
datadest - puntatore dove verrà restituito il contenuto del file
filelen - puntatore dove verrà restituita lal lunghezza del file
dirpath - directory dove il server ha memorizzato il file

retval:
OP_OK - successo
OP_NICK_UNKNOWN - username non registrato
OP_FAIL - errore
*/
int get_file(users_table_t *table, char *username, char *name, char **datadest, int *filelen, char *dirpath);

/*
Restituisce una lista contenente i messaggi e i nomi di file non ancora consegnati a un certo utente

param:
table - tabella inizializzata
username - nome dell'utente
msgs - coda dove verranno restituiti i messaggi da consengare (già inizializzata)

retval:
OP_OK - successo
OP_FAIL - errore
OP_NICK_UNKNOWN - username non registrato
*/
int get_history(users_table_t *table, char *username, queue_t *list);

/*
Inserisce in sequenza i nomi degli utenti registrati in una stringa

param:
table - tabella già inizializzata
n - numero di user restitiuiti

retval:
striga contenente i nomi degli user registrati
NULL se la lista è vuota*/
char *users_list(users_table_t *table, int *n);

/*
Restitiuisce la struttura delle statistiche

param:
table - tabella inizializzata

retval:
struttura contenente le statistiche aggiornate
*/
struct statistics *get_stat(users_table_t *table);

#endif
