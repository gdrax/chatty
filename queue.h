/*
	\file queue.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef queue_h
#define queue_h

#include"config.h"

typedef struct qnode {
	int fd;
	struct qnode *next;
} qnode_t;

typedef struct queue {
	int len;
	qnode_t *head;
	qnode_t *tail;
} queue_t;

/*
Crea una coda vuota

retval: puntatore alla coda appena creata
*/
queue_t *create_queue();

/*
Distrugge una coda

param:
q - puntatore alla coda da distruggere
*/
void delete_queue(queue_t *q);

/*
Inserisce elemento in coda

param:
q - coda già inizializzata
data - elemento da inserire in coda

retval: puntatore alla coda appene creata
*/
int insert_ele(queue_t *q, int fd);

/*
Restituisce ed elimina l'elemento in testa alla coda

param:
q - coda da cui prelevare l'elemento

retval:
elemento in testa alla coda
*/
int take_ele(queue_t *q);


#endif
