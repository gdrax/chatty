/*
	\file chat_parser.c
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#define LINE_SIZE 501

#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<utility.h>
#include<chat_parser.h>

int get_config(char *config_path, info_server_t *infos) {

	int skipline, i=0, j=0, k=0,/* h=0,*/ uguale=0;
	char *buf, *nome_attr, *val_attr;
	FILE *fd;

	TRY(buf, malloc(LINE_SIZE*sizeof(char)), NULL, "Malloc", -1, 0)
	TRY(nome_attr, malloc(LINE_SIZE*sizeof(char)), NULL, "Malloc", -1, 0)
	TRY(val_attr, malloc(LINE_SIZE*sizeof(char)), NULL,  "Malloc", -1, 0)
	
	TRY(fd, fopen(config_path, "r"), NULL, "Fopen", -1, 0)

	//inizializzo la struct
	infos->max_connections = 0;
	infos->th_in_pool = 0;
	infos->max_msg_size = 0;
	infos->max_file_size = 0;
	infos->max_history = 0;
	infos->unix_path = NULL;
	infos->files_path = NULL;
	infos->stat_path = NULL;
	infos->user_locks = 0;
	infos->group_locks = 0;
	infos->fd_locks = 0;
	infos->buckets = 0;

	//inizializzo l'array delle info
	for (int f=0; f<12; f++) {
		infos->check[i] = 0;
	}

	while(fgets(buf, LINE_SIZE, fd) != NULL) {
		i=0; skipline=0, j=0, k=0, uguale=0;
		while (i<LINE_SIZE && skipline == 0) {
			switch(buf[i]) {
				/* salto la linea se è un commento o trovo un carattere di terminazione, salto spazi e tab*/
				case ('#'): skipline=1; break;
				case ('\0'): skipline=1; break;
				case ('\n'): skipline=1; break;
				case (EOF): skipline=1; break;
				case (' '): break;
				case ('	'): break;
				/* mi ricordo se ho trovato un uguale */
				case ('='): uguale=1; break;
				/* inserisco i caratteri in due array diversi a seconda del valore di 'uguale'*/
				default : if (uguale==0) {
						nome_attr[j] = buf[i];
						j++;
					  }
					  else {
						val_attr[k] = buf[i];
						k++;
					  }
					break;
			}
		i++;
		}
		nome_attr[j]='\0';
		val_attr[k]='\0';

		/* Seleziono i pattern che carrispondono alle variabili di configuazione che mi aspetto */
		if (strcmp(nome_attr, "UnixPath") == 0) {
			TRY(infos->unix_path, malloc((strlen(val_attr)+1)*sizeof(char)), NULL, "Malloc", -1, 0)
			memset(infos->unix_path, 0, strlen(val_attr)+1);
			strncpy(infos->unix_path, val_attr, strlen(val_attr));
			infos->check[0] = 1;
			continue;
		}
		if (strcmp(nome_attr, "MaxConnections") == 0) {
			infos->max_connections = atoi(val_attr);
			infos->check[1] = 1;
			continue;
		}
		if (strcmp(nome_attr, "ThreadsInPool") == 0) {
			infos->th_in_pool = atoi(val_attr);
			infos->check[2] = 1;
			continue;
		}
		if (strcmp(nome_attr, "MaxMsgSize") == 0) {
			infos->max_msg_size = atoi(val_attr);
			infos->check[3] = 1;
			continue;
		}
		if (strcmp(nome_attr, "MaxFileSize") == 0) {
			infos->max_file_size = atoi(val_attr);
			infos->check[4] = 1;
			continue;
		}
		if (strcmp(nome_attr, "MaxHistMsgs") == 0) {
			infos->max_history = atoi(val_attr);
			infos->check[5] = 1;
			continue;
		}
		if (strcmp(nome_attr, "DirName") == 0) {
			TRY(infos->files_path, malloc((strlen(val_attr)+1)*sizeof(char)), NULL, "Malloc", -1, 0)
			memset(infos->files_path, 0, strlen(val_attr)+1);
			strncpy(infos->files_path, val_attr, strlen(val_attr));
			infos->check[6] = 1;
			continue;
		}
		if (strcmp(nome_attr, "StatFileName") == 0) {
			TRY(infos->stat_path, malloc((strlen(val_attr)+1)*sizeof(char)), NULL, "Malloc", -1, 0)
			strncpy(infos->stat_path, val_attr, strlen(val_attr));
			infos->check[7] = 1;
			continue;
		}
		if (strcmp(nome_attr, "UserLocks") == 0) {
			infos->user_locks = atoi(val_attr);
			infos->check[8] = 1;
			continue;
		}
		if (strcmp(nome_attr, "GroupLocks") == 0) {
			infos->group_locks = atoi(val_attr);
			infos->check[9] = 1;
			continue;
		}
		if (strcmp(nome_attr, "FdLocks") == 0) {
			infos->fd_locks = atoi(val_attr);
			infos->check[10] = 1;
			continue;
		}
		if (strcmp(nome_attr, "Buckets") == 0) {
			infos->buckets = atoi(val_attr);
			infos->check[11] = 1;
			continue;
		}

	}
	free(buf);
	free(val_attr);
	free(nome_attr);
	TRY(i, fclose(fd), EOF, "Fclose", -1, 0)

	if (	infos->max_connections <= 0 || 
		infos->th_in_pool <= 0 || 
		infos->max_msg_size <= 0 || 
		infos->max_file_size <= 0 || 
		infos->max_history <= 0)
		return -1;

	if (	infos->unix_path == NULL || 
		infos->files_path == NULL || 
		infos->stat_path == NULL || 
		strcmp(infos->unix_path, "") == 0 || 
		strcmp(infos->files_path, "") == 0 || 
		strcmp(infos->stat_path, "") == 0)
		return -1;
	if (infos->user_locks < 1) infos->user_locks = 16;
	if (infos->group_locks < 1) infos->group_locks = 16;
	if (infos->fd_locks < 1) infos->fd_locks = 16;
	if (infos->buckets < 1) infos->buckets = 32;


	return 0;
}

void free_config(info_server_t *infos) {
	free(infos->unix_path);
	free(infos->files_path);
	free(infos->stat_path);
	free(infos);
}
