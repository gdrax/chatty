/*
	\file string_list.c
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/


#include "string_list.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "utility.h"

void printSList(string_list_t *list) {
	if (!list)
		return;
	s_ele_t *tmp = list->head;
	while(tmp != NULL) {
		fprintf(stdout, "fd: %d 	nick: %s\n", tmp->fd, tmp->data);
		fflush(stdout);
		tmp = tmp->next;
	}
}

string_list_t *createSList(int history) {
	string_list_t *list = malloc(sizeof(string_list_t));
	if (!list)
		return NULL;
	list->size = 0;
	list->head = NULL;
	return list;
}

void deleteSList(string_list_t *list) {
	if (list == NULL)
		return;
	while(list->head != NULL) {
		s_ele_t *tmp = list->head;
		list->head = list->head->next;
		free(tmp->data);
		free(tmp);
	}
	free(list);
}

int addString(string_list_t *list, char *string, int fd) {
	if (!list || !string)
		return -1;
	//se la stringa è già presente nella lista non la inserisco
	int ret = searchString(list, string);
	if (ret == 1)
		return 1;
	//creo un nuovo elemento
	s_ele_t *tmp;
	TRY(tmp, malloc(sizeof(s_ele_t)), NULL, "malloc", -1, 0)
	TRY(tmp->data, malloc((MAX_NAME_LENGTH+1)*sizeof(char)), NULL, "malloc", -1, 0)
	strncpy(tmp->data, string, MAX_NAME_LENGTH+1);
	tmp->fd = fd;
	tmp->next = NULL;
	//se la lista è vuota
	if (list->head == NULL) {
		list->head = tmp;
		list->size++;
	}
	else {
		s_ele_t *corr = list->head;
		while(corr->next != NULL)
			corr = corr->next;
		corr->next = tmp;
		list->size++;
	}
	return 0;
}

int removeString(string_list_t *list, char *string) {
	if (list == NULL || string == NULL)
		return -1;
	if (!list->head)
		return -1;
	s_ele_t *corr;
	corr = list->head;
	if (strcmp(list->head->data, string) == 0) {
		//la stringa da eliminare è in testa
		list->head = list->head->next;
		list->size--;
		free(corr);
	}
	else {
		s_ele_t *tmp;
		while(corr != NULL) {
			if (!corr->next)
				return 0;
			if (strcmp(corr->next->data, string) == 0)
				break;
		corr = corr->next;
		}
		tmp = corr->next;
		corr->next = tmp->next;
		tmp->next = NULL;
		list->size--;
		free(tmp->data);
		free(tmp);
	}
			fprintf(stdout, "after all 22\n");
			fflush(stdout);
printSList(list);

	return 0;
}


int searchString(string_list_t *list, char *string) {
	if (list == NULL || string == NULL)
		return -1;
	s_ele_t *tmp;
	tmp = list->head;
	while(tmp != NULL) {
		if (strcmp(tmp->data, string) == 0)
			return 1;
		else
			tmp = tmp->next;
	}
	return 0;
}

int getByIndex(string_list_t *list, int index, char *string) {
	if (list == NULL) {
		free(string);
		return -1;
	}
	if (index > list->size || index < 0) {
		free(string);
		return -1;
	}
	s_ele_t *tmp = list->head;
	while (tmp != NULL && index > 0) {
		tmp = tmp->next;
		index--;
	}
	if (tmp != NULL) {
		strncpy(string, tmp->data, MAX_NAME_LENGTH+1);
		return 0;
	}
	free(string);
	return -1;
}

/*
Cambia il descrittore di file associato a una stringa

param:
list - lista già inizializzata
string - stringa asociata
fd - nuovo descrittore

retval:
-1 - errore
0 - successo
*/
int update_fd(string_list_t *list, char *string, int fd) {
	if (list == NULL || string == NULL)
		return -1;
	s_ele_t *tmp;
	tmp = list->head;
	while(tmp != NULL) {
		if (strcmp(tmp->data, string) == 0) {
			tmp->fd = fd;
			break;
		}
		tmp = tmp->next;
	}
	return 0;
}
/*
Rimuove la stringa associata ad un descrittore e la restituisce

param:
list - lista già inizializzata
fd - descrittore da rimuovere

retval:
NULL: errore
stringa associata al descrittore: successo
*/


char * disconnect_fd(string_list_t *list, int fd) {
	if (list == NULL)
		return NULL;
	if (!list->head)
		//lista vuota
		return NULL;
	char *name = NULL;
	printSList(list);
	s_ele_t *corr;
	corr = list->head;
	if (list->head->fd == fd) {
		//la stringa da eliminare è in testa
		name = malloc(MAX_NAME_LENGTH+1*sizeof(char));
		memset(name, 0, MAX_NAME_LENGTH+1);
		strncpy(name, list->head->data, strlen(list->head->data));
		list->head = list->head->next;
		list->size--;
		free(corr->data);
		free(corr);
	}
	else {
		while(corr != NULL) {
			if (!corr->next)
				return 0;
			if (corr->next->fd == fd)
				break;
			corr = corr->next;
		}
		s_ele_t *tmp = corr->next;
		name = malloc(MAX_NAME_LENGTH+1*sizeof(char));
		memset(name, 0, MAX_NAME_LENGTH+1);
		strncpy(name, tmp->data, strlen(tmp->data));
		corr->next = tmp->next;
		tmp->next = NULL;
		list->size--;
		free(tmp->data);
		free(tmp);
	}
	return name;
}

