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



string_list_t *createSList(int history) {
	string_list_t *list = malloc(sizeof(string_list_t));
	if (!list)
		return NULL;
	list->size = 0;
	list->head = NULL;
	list->tail = NULL;
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

int addString(string_list_t *list, char *string) {
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
	tmp->next = NULL;
	//se la lista è vuota
	if (list->head == NULL) {
		list->head = tmp;
		list->tail = tmp;
		list->size++;
	}
	//se ci sono già altri elementi
	else {
		list->tail->next = tmp;
		list->tail = tmp;
		list->size++;
	}
	return 0;
}

int removeString(string_list_t *list, char *string) {
	if (list == NULL || string == NULL)
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
		s_ele_t *tmp, *prec;
		prec = list->head;
		corr = list->head->next;
		while(corr->next != NULL && prec != NULL) {
			if (strcmp(corr->data, string) == 0) {
				tmp = corr;
				prec->next = corr->next;
				list->size--;
				free(tmp);
				break;
			}
			else {
				corr = corr->next;
				prec = prec->next;
			}
		}
	}
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
