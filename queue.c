#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <queue.h>

/**
 * @file queue.c
 * @brief File di implementazione dell'interfaccia per la coda
 */


/* ------------------- funzioni di utilita' -------------------- */

static Node_t *allocNode()         { return malloc(sizeof(Node_t));  }
static queue_t *allocQueue()       { return malloc(sizeof(queue_t)); }
static void freeNode(Node_t *node) { free((void*)node); }

/* ------------------- interfaccia della coda ------------------ */

queue_t *create_queue(void (*free_data)(void*)) {
    queue_t *q = allocQueue();
    if (!q) return NULL;
    q->head = allocNode();
    if (!q->head) return NULL;
    q->head->data = NULL; 
    q->head->next = NULL;
    q->tail = q->head;
    q->len = 0;
    q->free_data = free_data;
    return q;
}

void delete_queue(queue_t *q) {
	if (q) {
		while(q->head) {
			Node_t *p = (Node_t*)q->head;
			q->head = q->head->next;
			if (*q->free_data && p->data) (*q->free_data)(p->data);
			freeNode(p);
		}
	free(q);
	}
}

int insert_ele(queue_t *q, void *data) {
    Node_t *n = allocNode();
    n->data = data; 
    n->next = NULL;
    q->tail->next = n;
    q->tail       = n;
    q->len      += 1;
    return 0;
}

void *take_ele(queue_t *q) {
    if (!q->head->next) return NULL;
    Node_t *n  = (Node_t *)q->head;
    void *data = (q->head->next)->data;
    q->head->data = NULL;
    q->head    = q->head->next;
    q->len   -= 1;
    assert(q->len>=0);
    if (q->len == 0) {
	q->head->data = NULL;
	q->head->next = NULL;
	q->tail = q->head;
    }
    freeNode(n);
    return data;
} 

// accesso in sola lettura non in mutua esclusione
unsigned long length(queue_t *q) {
    unsigned long len = q->len;
    return len;
}

