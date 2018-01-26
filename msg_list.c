/*
	\file msg_list.c
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/


#include "msg_list.h"
#include <string.h>
#include <stdlib.h>
#include "utility.h"


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

/*int appendMsgList(msg_list_t *list, int *n_msgs, int *n_files, char *msg_list, char *file_list) {*/
/*	if (!list)*/
/*		return -1;*/
/*	if ((list->msgs+list->files) == 0)*/
/*		return -1;*/
/*	int m_count=0, f_count=0;*/
/*	msg_t *tmp = list->head;*/
/*	msg_list = malloc(list->msgs*(MAX_MSG_LENGTH+1)*sizeof(char));*/
/*	file_list = malloc(list->files*(MAX_MSG_LENGTH+1)*sizeof(char));*/
/*	memset(msg_list, 0, list->msgs*(MAX_MSG_LENGTH+1)*sizeof(char));*/
/*	memset(file_list, 0, list->files*(MAX_MSG_LENGTH+1)*sizeof(char));*/
/*	while(tmp != NULL) {*/
/*		if (tmp->consegnato == 0) {*/
/*			if (tmp->type == 1) {*/
/*				strncat(msg_list + m_count*(MAX_MSG_LENGTH+1), tmp->text, MAX_MSG_LENGTH+1);*/
/*				tmp->consegnato = 1;*/
/*				m_count++;*/
/*			}*/
/*			else {*/
/*				strncat(file_list + f_count*(MAX_MSG_LENGTH+1), tmp->text, MAX_MSG_LENGTH+1);*/
/*				tmp->consegnato = 1;*/
/*				f_count++;*/
/*			}*/
/*		}*/
/*		tmp = tmp->next;*/
/*	}*/
/*	*n_msgs = m_count;*/
/*	*n_files = f_count;*/
/*	return 0;*/
/*}*/
char *appendMsgList(msg_list_t *list, int *nm, int *nf) {
	if (!list)
		return NULL;
	if ((list->msgs+list->files) == 0)
		return NULL;
	char *msgs, *files, *string;
	int m_count=0, f_count=0;
	msg_t *tmp = list->head;
	msgs = malloc(list->msgs*(MAX_MSG_LENGTH+1)*sizeof(char));
	memset(msgs, 0, list->msgs*(MAX_MSG_LENGTH+1)*sizeof(char));
	files = malloc(list->files*(MAX_MSG_LENGTH+1)*sizeof(char));
	memset(files, 0, list->files*(MAX_MSG_LENGTH+1)*sizeof(char));
	while(tmp != NULL) {
		if (tmp->consegnato == 0) {
			if (tmp->type == 1) {
				strncat(msgs + m_count*(MAX_MSG_LENGTH+1), tmp->text, MAX_MSG_LENGTH+1);
				tmp->consegnato = 1;
				m_count++;
			}
			else {
				strncat(files + f_count*(MAX_MSG_LENGTH+1), tmp->text, MAX_MSG_LENGTH+1);
				tmp->consegnato = 1;
				f_count++;
			}
		}
		tmp = tmp->next;
	}
	string = malloc((list->msgs+list->files)*(MAX_MSG_LENGTH+1)*sizeof(char));
	memset(string, 0, (list->msgs+list->files)*(MAX_MSG_LENGTH+1));
	strncat(string, msgs, m_count*(MAX_MSG_LENGTH+1));
	strncat(string+m_count*(MAX_NAME_LENGTH+1), files, f_count*(MAX_MSG_LENGTH+1));
	*nm = m_count;
	*nf = f_count;
	free(msgs);
	free(files);
	return string;
}
