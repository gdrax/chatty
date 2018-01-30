/*
	\file users_table.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef users_table_h
#define users_table_h

#include "icl_hash.h"
#include "queue.h"
#include "msg_list.h"
#include "config.h"
#include "utility.h"
#include <pthread.h>
#include "string_list.h"
#include "stats.h"

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
	msg_list_t *msgs;				//lista degli ultimi messaggi ricevuti dall'utente
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
Struttura della tabella utenti
*/
typedef struct users_table {
	icl_hash_t *users;	//tabella hash contenente gli utenti iscritti (vedi struct sopra)
	icl_hash_t *groups;	//tabella hash contenente i gruppi registrati (vedi struct sopra)
	pthread_mutex_t *locks;	//array di lock per la sincronizzazione
	int u_locks;		//numero di lock utilizzate per la sincronizzazione sugli utenti
	int g_locks;		//numero di lock utilizzate per la sincornizzazione sui gruppi
	int history;		//massimo numero di messaggi tenuti in memoria per ogni utente
	int m_consegnati;	//messaggi consegnati
	int m_in_attesa;	//messaggi da consegnare
	int f_consegnati;	//file consegnati
	int f_in_attesa;	//file da consegnare
	int users_online;	//utenti online
	int errori;		//numero di messaggi di errore del server
} users_table_t;

/*
Crea una nuova tabella vuota

param:
u_locks - numero di mutex da utilizzare per sincronizzarsi sugli user
g_locks - numero di mutex da utilizzare per sincornizzarsi sui gruppi
n_buckets - numero di buckets per le tabelle hash
history - numero massimo di messaggi memorizzati

retval: puntatore alla tabella appena creata
*/
users_table_t *create_table(int u_locks, int g_locks, int n_buckets, int hisotry);

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
0 - successo
1 - utente già presente
-1 - errore
*/
int add_user(users_table_t *table, char *username, int fd);

/*
Elimina un utente

param:
table - tabella inizializzata
username - nome dell'utente

retval:
0 - successo
-1 - errore
*/
int delete_user(users_table_t *table, char *username);

/*
Crea un nuovo gruppo aggiungendoci il suo creatore

param:
table - tabella inizializzata
username - nome del creatore del gruppo
groupname - nome del gruppo da creare

retval:
0 - successo
1 - gruppo o username già presente
-1 - errore
*/
int add_group(users_table_t *table, char *owner, char *groupname);

/*
Rimuove un gruppo

param:
table - tabella inizializzata
groupname - nome del gruppo da rimuovere

retval:
0 - successo
-1 - errore
*/
int remove_group(users_table_t *table, char *groupname);

/*
Aggiunge un utente ad un gruppo

param:
table - tabella inizializzata
username - nome dell'utente
groupname - nome del gruppo

retval:
0 - successo
1 - gruppo o username già presente
-1 - errore
*/
int join_group(users_table_t *table, char *username, char *groupname);

/*
Disiscrive un utente da un gruppo

param:
table - tabella inizializzata
username - nome dell'utente
groupname - nome del gruppo

retval:
0 - successo
1 - user o gruppo non registrati, o user già iscritto al gruppo
-1 - errore
*/
int leave_group(users_table_t *table, char *username, char *groupname);

/*
Contrassegna un utente come online

param:
table - tabella inizializzata
username - nome dell'utente
fd - file descriptor su cui si è connesso il client

retval:
0 - successo
-1 - errore
*/
int set_online(users_table_t *table, char *username, int fd);

/*
Contrassegna un utente come offline

param:
table - tabella inizializzata
username - nome dell'utente

retval:
0 - successo
-1 - errore
*/
int set_offline(users_table_t *table, char *username);

/*
Memorizza un messaggio diretto a un utente o ai membri di un gruppo

param:
table - tabella inizializzata
sender - mittente del messaggio
receiver - destinatario del messaggio (utente o gruppo)
text - contenuto del messaggio
fds - coda di file descriptor a cui inviare il messaggio (uno solo se receiver è un utente, più di uno se è un gruppo)

retval:
0 - messaggio inviato correttamente
1 - sender o receiver non registrati
-1 - errore
2 - messaggio troppo lungo
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
0 - successo
1 - sender non registrato
-1 - errore
2 - messaggio troppo lungo
*/
int send_text_all(users_table_t *table, char *sender, char *text, queue_t *fds);

/*
Memorizza un file diretto a un utente o ai membri di un gruppo

param:
table - tabella inizializzata
sender - nome del mittente
receiver - destinatario del file (utente o gruppo)
data - contenuto del file
writelen - lunghezza del file
fds - coda di file descriptor a cui inviare il messaggio (uno solo se il receiver è un utente, più di uno se è un gruppo)
dirpath - directory nella quale memorizzare il file
max_size - lunghezza massima di file accettato dal server

retval:
0: successo
1 - sender o receiver non registrati
2 - file troppo lungo
-1 - errore
*/
int send_file(users_table_t *table, char *sender, char *receiver, char *name, char *data, int writelen, queue_t *fds, char *dirpath, int max_size);

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
0: successo
1 - username non registrato
-1: errore
*/
int get_file(users_table_t *table, char *username, char *name, char *datadest, int *filelen, char *dirpath);

/*
Restituisce due stringhe contenenti le liste dei messaggi testuali e dei nomi dei file  da spedire

param:
table - tabella inizializzata
username - nome dell'utente
msgs - lista dove verranno restituiti i messaggi da consengare

retval:
0: successo
-1: errore
1: username non registrato
*/
int get_history(users_table_t *table, char *username, msg_list_t *msgs);

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
Aggiorna la struttura delle statistiche

param:
table - tabella inizializzata
stats - struttura delle statistiche
*/
void get_stat(users_table_t *table, struct statistics *stats);

#endif
