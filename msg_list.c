/*
	\file msg_list.c
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/


#include "msg_list.h"
#include <string.h>
#include <stdlib.h>
#include "utility.h"
#include "ops.h"


msg_list_t *createMsgList(int history) {
	msg_list_t *list = malloc(sizeof(msg_list_t));
	if (!list)
		return NULL;
	list->history = history;
	list->msgs = 0;
	list->files = 0;
	list->head = NULL;
	list->tail = NULL;
	return list;
}

int addMsg(msg_list_t *list, char *text, char *sender, int type, int consegnato) {
	if (list == NULL || text == NULL || sender == NULL || (type!= 0 && type!=1))
		return -1;
	if (strlen(text) > MAX_MSG_LENGTH)
		return 2;
	//creo messaggio
	msg_t *tmp = malloc(sizeof(msg_t));
	memset(tmp->sender, 0, MAX_NAME_LENGTH+1);
	strncpy(tmp->sender, sender, strlen(sender));
	memset(tmp->text, 0, MAX_MSG_LENGTH+1);
	strncpy(tmp->text, text, strlen(text));
	tmp->type = type;
	tmp->consegnato = consegnato;
	tmp->next = NULL;
	//se la lista è vuota
	if (list->head == NULL) {
		list->head = tmp;
		list->tail = tmp;
		if (type == 1)
			list->msgs++;
		else
			list->files++;
	}
	//se ho ancora posto
	else if ((list->msgs+list->files) < list->history) {
		list->tail->next = tmp;
		list->tail = tmp;
		if (type == 1)
			list->msgs++;
		else
			list->files++;
	}
	//se ho la history piena elimino il messaggio più vecchio
	else if ((list->msgs+list->files) >= list->history) {
		int old_type = list->head->type;
		msg_t *tmp2 = list->head;
		list->head = list->head->next;
		free(tmp2);
		list->tail->next = tmp;
		list->tail = tmp;
		if (old_type == 1 && type == 0) {
			list->msgs--;
			list->files++;
		}
		else {
			if (old_type == 0 && type == 1) {
				list->msgs++;
				list->files--;
			}
		}
	}
	return 0;
}

void deleteMsgList(msg_list_t *list) {
	if (list == NULL)
		return;
	while(list->head != NULL) {
		msg_t *tmp = list->head;
		list->head = list->head->next;
		if(tmp->type == 1)
			list->msgs--;
		else
			list->files--;
		free(tmp);
	}
	free(list);
}

void printMsgList(msg_list_t *list) {
	if (!list)
		return;
	msg_t *tmp = list->head;
	while(tmp != NULL) {
		fprintf(stdout, "Sender: %s 	Text:%s\n", tmp->sender, tmp->text);
		fflush(stdout);
		tmp = tmp->next;
	}
}

int getMsgList(msg_list_t *src, msg_list_t *dest) {
	if (!src || !dest)
		return OP_FAIL;
	if ((src->msgs+src->files) == 0)
		return OP_OK;
	msg_t *tmp = src->head;
	while(tmp != NULL) {
		if (tmp->consegnato == 0) {
			addMsg(dest, tmp->text, tmp->sender, tmp->type, 1);
			tmp->consegnato = 1;
		}
		tmp = tmp->next;
	}
	return OP_OK;
}
