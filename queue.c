/*
	\file queue.c
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#include"queue.h"
#include"utility.h"
#include<string.h>

queue_t *create_queue() {
	queue_t *q;
	TRY(q, malloc(sizeof(queue_t)), NULL, "malloc", NULL, 1)
	TRY(q->head, malloc(sizeof(qnode_t)), NULL, "malloc", NULL, 1)
	q->head->fd = -1;
	q->head->next = NULL;
	q->tail = q->head;
	q->len = 0;
	return q;
}

void delete_queue(queue_t *q) {
	while (q->head !=NULL) {
		qnode_t *old = q->head;
		q->head = q->head->next;
		free(old);
	}
	free(q);
}

int insert_ele(queue_t *q, int fd) {
	if (!q || fd < 0)
		return -1;
	qnode_t *new;
	TRY(new, malloc(sizeof(qnode_t)), NULL, "malloc", -1, 1)
	new->fd = fd;
	new->next = NULL;
	q->tail->next = new;
	q->tail = new;
	q->len++;
	return 0;
}

int take_ele(queue_t *q) {
	if (!q) {
		return -1;
	}
	if (q->head == NULL) {
		return -1;
	}
	qnode_t *old = q->head;
	int ret = q->head->next->fd;
	q->head = q->head->next;
	q->len--;
	free(old);
	return ret;
}
