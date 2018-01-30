/*
	\file msg_list.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef msg_list_h
#define msg_list_h

#include "config.h"
#include "string_list.h"

/*
Struttura di un messaggio
*/
typedef struct msg {
	char text[MAX_MSG_LENGTH+1];	//contenuto del messaggio
	char sender[MAX_NAME_LENGTH+1];	//mittente del messaggio
	int type;			//tipo del messaggio: 0 = nome di file, 1 = messaggio di testo
	int consegnato;			//stato di consegna: 0 = da conegnare, 1 = consegnato
	struct msg *next;
} msg_t;

/*
Struttura della lista di messaggi
*/
typedef struct msg_list {
	int history;	//massimo numero di messaggi da memorizzare
	int msgs;	//numero di messaggi testuali nella lista
	int files;	//numero di nomi di file nella lista
	msg_t *head;
	msg_t *tail;
} msg_list_t;


/*
Crea una nuova lista

param:
history - massima lunghezza che la lista potrà avere

retval:
puntatore alla lista appena creata
*/
msg_list_t *createMsgList(int history);

/*
Aggiunge messaggio nella lista (nel caso sia piena, elimina quello più vecchio)

param:
list - lista già inizializzata
text - messaggio da aggiungere
sender - mittente del messaggio
type - tipo di messaggio
consegnato - stato del messaggio

retval:
OP_MSG_TOOLONG - messaggio troppo lungo
OP_FAIL - errore
OP_OK - successo
*/
int addMsg(msg_list_t *list, char *text, char *sender, int type, int consegnato);

/*
Distrugge una lista

param:
list - lista da distruggere
*/
void deleteMsgList(msg_list_t *list);

/*
Stampa sullo standard out il contenuto di una lista

param:
list - lista già inizializzata
*/
void printMsgList(msg_list_t *list);

/*
Riempe una nuova lista con i messaggi non ancora consegnati

param:
src - lista contenente i messaggi di un utente
dest - lista che conterrà i messaggi non inviati

retval:
OP_FAIL: fallimento
OP_OK: successo
*/
int getMsgList(msg_list_t *src, msg_list_t *dest);

#endif
