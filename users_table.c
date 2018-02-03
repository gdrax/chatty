/*
	\file users_table.c
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#include <pthread.h>
#include <string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include <sys/mman.h>


#include "users_table.h"
#include "msg_list.h"
#include "utility.h"
#include "icl_hash.h"
#include "config.h"
#include "ops.h"


//acquisisce la lock per un utente
#define LOCKUSER(u, n, locks)			\
		LOCK(&locks[djb2(u) % n])

//rilascia la lock per un utente
#define UNLOCKUSER(u, n, locks)			\
		UNLOCK(&locks[djb2(u) % n])

//acquisisce la lock per un gruppo
#define LOCKGROUP(g, n, m, locks)			\
		LOCK(&locks[(djb2(g) % m) + n])

//rilascia la lock per un gruppo
#define UNLOCKGROUP(g, n, m, locks)			\
		UNLOCK(&locks[(djb2(g) % m) + n])

//acquisisce la lock per un descrittore
#define LOCKFD(fd, n, m, f, locks)			\
		LOCK(&locks[(fd % f) + n + m])

//rilascia la lock per un descrittore
#define UNLOCKFD(fd, n, m, f, locks)			\
		UNLOCK(&locks[(fd % f) + n + m])

//acquisisce le lock per due utenti in ordine, se entrambi gli utenti sono sotto la stessa ne acquisisce una sola
#define LOCKINORDER(u1, u2, n, locks)				\
		if ((djb2(u1)%n) < (djb2(u2)%n)) {		\
			LOCKUSER(u1, n, locks)			\
			LOCKUSER(u2, n, locks)			\
		}						\
		else {						\
			if ((djb2(u1)%n) > (djb2(u2)%n)) {	\
				LOCKUSER(u2, n, locks)		\
				LOCKUSER(u1, n, locks)		\
			}					\
			else {					\
				LOCKUSER(u1, n, locks)		\
			}					\
		}

//rilascia le lock per due utenti in ordine, se entrambi sono sotto la stessa ne rilascia una sola
#define UNLOCKINORDER(u1, u2, n, locks)				\
		if ((djb2(u1)%n) > (djb2(u2)%n)) {		\
			UNLOCKUSER(u1, n, locks)		\
			UNLOCKUSER(u2, n, locks)		\
		}						\
		else {						\
			if ((djb2(u1)%n) < (djb2(u2)%n)) {	\
				UNLOCKUSER(u2, n, locks)	\
				UNLOCKUSER(u1, n, locks)	\
			}					\
			else {					\
				UNLOCKUSER(u1, n, locks)	\
			}					\
		}

//controlla se un nome è presente in una hash table
#define CHECKNAME(t, n, ele)	\
		(ele = icl_hash_find(t, (void *)n))


//funzione hash
unsigned int djb2(void *str) {
	unsigned int hash = 5381;
	unsigned char *s = (unsigned char *)str;
	int c;
	while ((c = *s++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash;
}


//libera la struttura di un user
void freeUser(void *data) {
	if (data) {
		chat_user_t *user = (chat_user_t *)data;
		delete_queue(user->msgs);
		free(user);
	}
}

//libera la struttura di un group
void freeGroup(void *data) {
	if (data) {
		chat_group_t *group = (chat_group_t *)data;
		deleteSList(group->members);
		free(group);
	}
}

//libera un chat_message
void freeChatMessage(void *data) {
	if (data) {
		chat_message_t *tmp = (chat_message_t *)data;
		freeMessage(tmp->message);
		free(tmp);
	}
}

//libera un message_t
void freeMessage(void *data) {
	if (data) {
		message_t *tmp = (message_t *)data;
		free(tmp->data.buf);
		free(tmp);
	}
}

//concatena un nome file con l'indirizzo di una directory
char *make_path(char *filename, char *dirpath) {
	int len = strlen(filename) + strlen(dirpath) + 2, i=0;
	char *filepath = malloc(len*sizeof(char));
	memset(filepath, 0, len);
	for(i=0; i<strlen(dirpath); i++)
		filepath[i] = dirpath[i];
	filepath[i] = '/';
	i++;
	for (int j=0; j<strlen(filename); j++, i++)
		filepath[i] = filename[j];
	filepath[i] = '\0';
	return filepath;
}

//scrive buffer su un file descriptor
int write_file(int fd, char *buf, int left) {
	int ret;
	while (left > 0) {
		ret = write(fd, buf, left);
		if (ret == -1) {
			return -1;
		}
		left -= ret;
		buf += ret;
	}
	return 0;
}

//salva un messaggio nella coda di un utente
void store_msg(users_table_t *table, char *sender, char *text, int type, queue_t *q, int status) {
	chat_message_t *new = malloc(sizeof(chat_message_t)), *tmp;
	memset(new, 0, sizeof(chat_message_t));
	new->message = malloc(sizeof(message_t));
	char *buf = malloc(sizeof(char)*(strlen(text)+1));
	memset(buf, 0, strlen(text)+1);
	strncpy(buf, text, strlen(text));
	setHeader(&(new->message->hdr), type, sender);
	setData(&(new->message->data), "", buf, strlen(text)+1);
	new->consegnato = status;
	insert_ele(q, new);
	//se supero il massimo elimino il messaggio più vecchio
	if (q->len > table->history) {
		tmp = take_ele(q);
		freeChatMessage(tmp);
	}
}


users_table_t *create_table(int u_locks, int g_locks, int fd_locks, int n_buckets, int history, int max_msg_size, int max_file_size) {
	if (u_locks <= 0) u_locks = 8;
	if (g_locks <= 0) g_locks = 8;
	if (fd_locks <= 0) fd_locks = 8;
	if (n_buckets <= 0) n_buckets = 16;

	users_table_t *table;
	TRY(table, malloc(sizeof(users_table_t)), NULL, "malloc", NULL, 1)
	table->users = icl_hash_create(n_buckets, djb2, NULL);
	table->groups = icl_hash_create(n_buckets, djb2, NULL);
	TRY(table->locks, malloc((u_locks+g_locks+fd_locks)*sizeof(pthread_mutex_t)), NULL, "malloc", NULL, 1)
	for (int i=0; i<(u_locks+g_locks+fd_locks); i++) {
		memset(&(table->locks[i]), 0, sizeof(pthread_mutex_t));
	}
	table->online_users = createSList();
	table->u_locks = u_locks;
	table->g_locks = g_locks;
	table->fd_locks = fd_locks;
	table->history = history;
	table->m_consegnati = 0;
	table->m_in_attesa = 0;
	table->f_consegnati = 0;
	table->f_in_attesa = 0;
	table->users_online = 0;
	table->errori = 0;
	table->max_msg_size = max_msg_size;
	table->max_file_size = max_file_size;
	return table;
}

int destroy_table(users_table_t *table) {
	int ret;
	if (table) {
		TRY(ret, icl_hash_destroy(table->users, NULL, &freeUser), -1, "icl_hash_destroy", -1, 0)
		TRY(ret, icl_hash_destroy(table->groups, NULL, &freeGroup), -1, "icl_hash_destroy", -1, 0)
		free(table->locks);
		deleteSList(table->online_users);
		free(table);
	}
	return 0;
}

int add_user(users_table_t *table, char *username, int fd) {
	if (!username || !table || fd < 0) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	if (CHECKNAME(table->users, username, user)) {
		//utente già presente
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks);
		return OP_NICK_ALREADY;
	}
	//creo nuovo utente
	chat_user_t *new;
	TRY(new, malloc(sizeof(chat_user_t)), NULL, "malloc", -1, 1)
	memset(new->username, 0, MAX_NAME_LENGTH+1);
	strncpy(new->username, username, strlen(username));
	new->online = -1;
	if ((new->msgs = create_queue(freeChatMessage)) == NULL) {
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks);
		return OP_FAIL;
	}
	if (!icl_hash_insert(table->users, new->username, new)) {
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks);
		return OP_FAIL;
	}
	UNLOCKUSER(username, table->u_locks, table->locks);
/*	icl_hash_dump(stdout, table->users);*/
/*	fflush(stdout);*/
	return OP_OK;
}

int delete_user(users_table_t *table, char *username) {
	if (!username || !table) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	//se l'utente non è registrato non faccio nulla
	if (!CHECKNAME(table->users, username, user)) {
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
	int ret;
	if ((ret = icl_hash_delete(table->users, username, NULL, freeUser)) != 0) {
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks);
		return OP_FAIL;
	}
	UNLOCKUSER(username, table->u_locks, table->locks)
	return OP_OK;
}

int add_group(users_table_t *table, char *owner, char *groupname) {
	if (!owner || !groupname || !table) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(owner, table->u_locks, table->locks)
	LOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
	chat_user_t *user;
	chat_group_t *group;
	if (CHECKNAME(table->groups, groupname, group)) {
		//il gruppo esiste già
		table->errori++;
		UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
		UNLOCKUSER(owner, table->u_locks, table->locks)
		return OP_NICK_ALREADY;
	}
	//se l'utente è registrato creo il nuovo gruppo
	if (CHECKNAME(table->users, owner, user)) {
		chat_group_t *new;
		TRY(new, malloc(sizeof(chat_group_t)), NULL, "malloc", -1, 0)
		strncpy(new->groupname, groupname, strlen(groupname));
		if ((new->members = createSList(MAX_NAME_LENGTH+1)) == NULL) {
			table->errori++;
			UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
			UNLOCKUSER(owner, table->u_locks, table->locks)
			return OP_FAIL;
		}
		strncpy(new->owner, owner, strlen(owner));
		//aggiungo subito il proprietario tra i membri
		int ret;
		if ((ret = addString(new->members, new->owner, -1)) == -1) {
			table->errori++;
			UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
			UNLOCKUSER(owner, table->u_locks, table->locks)
			return OP_FAIL;
		}
		//aggiungo gruppo alla tabella hash
		if (!icl_hash_insert(table->groups, new->groupname, new)) {
			table->errori++;
			UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
			UNLOCKUSER(owner, table->u_locks, table->locks)
			return OP_FAIL;
		}
		UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
		UNLOCKUSER(owner, table->u_locks, table->locks)
		return OP_OK;
	}
	//l'owner non è registrato
	table->errori++;
	UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
	UNLOCKUSER(owner, table->u_locks, table->locks)
	return OP_NICK_UNKNOWN;
}

int remove_group(users_table_t *table, char *groupname) {
	if (!table || !groupname) {
		table->errori++;
		return -1;
	}
	LOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
	chat_group_t *group;
	if (CHECKNAME(table->groups, groupname, group)) {
		int ret;
		if ((ret = icl_hash_delete(table->groups, groupname, NULL, freeGroup)) != 0) {
			table->errori++;
			UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
			return -1;
		}
	}
	UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
	return 0;
}

int join_group(users_table_t *table, char *username, char *groupname) {
	if (!table || !username || !groupname) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	LOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
	chat_user_t *user;
	chat_group_t *group;
	if (!CHECKNAME(table->groups, groupname, group) || !CHECKNAME(table->users, username, user)) {
		//user o gruppo non registrati
		table->errori++;
		UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_NICK_UNKNOWN;
	}
	else {
		int ret;
		if ((ret = addString(group->members, user->username, -1)) != 0) {
			table->errori++;
			UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
			UNLOCKUSER(username, table->u_locks, table->locks)
			return OP_FAIL;
		}
		UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
}

int leave_group(users_table_t *table, char *username, char *groupname) {
	if (!table || !username || !groupname) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	LOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
	chat_user_t *user;
	chat_group_t *group;
	if (!CHECKNAME(table->groups, groupname, group) || !CHECKNAME(table->users, username, user)) {
		//user o gruppo non registrati
		UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
	else {
		int ret;
		if((ret = removeString(group->members, username)) != 0) {
			table->errori++;
			UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
			UNLOCKUSER(username, table->u_locks, table->locks)
			return OP_FAIL;
		}
		return OP_OK;
	}
}

int set_online(users_table_t *table, char *username, int fd) {
	if (!table || !username || fd < 0) {
		table->errori++;
		return OP_FAIL;
	}
	int ret=0;
	LOCKUSER(username, table->u_locks, table->locks)
	LOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
	chat_user_t *user;
	if (CHECKNAME(table->users, username, user)) {
		user->online = fd;
		//aggiungo utente alla lista degli online
		ret = addString(table->online_users, username, fd);
		//se la stringa è già presente aggiorno il descrittore
		if (ret == 1) {
			update_fd(table->online_users, username, fd);
		}
		table->users_online++;
		UNLOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
	//user non registrato
	table->errori++;
	UNLOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
	UNLOCKUSER(username, table->u_locks, table->locks)
	return OP_NICK_UNKNOWN;
}

int set_offline(users_table_t *table, char *username, int fd) {
	if (!table) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	LOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
	chat_user_t *user;
	if (CHECKNAME(table->users, username, user)) {
		user->online = -1;
		table->users_online--;
		//rimuovo il descriptor dalla lista online
		removeString(table->online_users, username);
		UNLOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
	//user non registrato
	table->errori++;
	UNLOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
	UNLOCKUSER(username, table->u_locks, table->locks)
	return OP_NICK_UNKNOWN;
}

int set_offline_fd(users_table_t *table, int fd) {
	if (!table) {
		table->errori++;
		return OP_FAIL;
	}
	char *username = NULL;
	LOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
	username = disconnect_fd(table->online_users, fd);
	UNLOCKFD(fd, table->u_locks, table->g_locks, table->fd_locks, table->locks)
	if (username == NULL) {
		return OP_OK;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	//se l'utente è registrato agggiorno le informazioni
	if (CHECKNAME(table->users, username, user)) {
		user->online = -1;
		table->users_online--;
	}
	UNLOCKUSER(username, table->u_locks, table->locks)
	free(username);
	return OP_OK;
}

int send_text(users_table_t *table, char *sender, char *receiver, char *text, queue_t *fds) {
	if (!table || !sender || !receiver || !text || !fds) {
		table->errori++;
		return OP_FAIL;
	}
	if (strlen(text) > table->max_msg_size) {
		table->errori++;
		return OP_MSG_TOOLONG;
	}
	//controllo se il receiver è un gruppo
	LOCKGROUP(receiver, table->u_locks, table->g_locks, table->locks)
	chat_group_t *group;
	if (CHECKNAME(table->groups, receiver, group)) {
		UNLOCKGROUP(receiver, table->u_locks, table->g_locks, table->locks)
		//controllo se il mittente è registrato
		LOCKUSER(sender, table->u_locks, table->locks)
		chat_user_t *usender;
		if (!CHECKNAME(table->users, sender, usender)) {
			//mittente non registrato
			table->errori++;
			UNLOCKUSER(sender, table->u_locks, table->locks)
			return OP_NICK_UNKNOWN;
		}
		if (searchString(group->members, sender) == 0) {
			//mittente non iscritto al gruppo
			table->errori++;
			UNLOCKUSER(sender, table->u_locks, table->locks)
			return OP_NICK_UNKNOWN;
		}
		UNLOCKUSER(sender, table->u_locks, table->locks)
		//mittente e gruppo sono registrati memorizzo messaggi e prendo i file descriptor degli utenti online
		LOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
		char *username = malloc((MAX_NAME_LENGTH+1)*sizeof(char));
		for (int i=0; i<group->members->size; i++) {
			memset(username, 0, (MAX_NAME_LENGTH+1)*sizeof(char));
			//prendo uno a uno i nomi dei membri del gruppo
			getByIndex(group->members, i, username);
			if (username != NULL) {
				chat_user_t *user;
				//se l'utente è ancora registrato gli invio il messaggio
				if(CHECKNAME(table->users, username, user)) {
					if (user->online != -1) {
						table->m_consegnati++;
						insert_ele(fds, &(user->online));
						store_msg(table, sender, text, TXT_MESSAGE, user->msgs, 1);
					}
					else {
						table->m_in_attesa++;
						store_msg(table, sender, text, TXT_MESSAGE, user->msgs, 0);
					}
				}
				//se un utente non è più registrato lo elimino dal gruppo
				else
					removeString(group->members, username);
			}
		}
		free(username);
		UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks);
		return OP_OK;
	}
	//invio messaggio a utente singolo
	else {
		UNLOCKGROUP(receiver, table->u_locks, table->g_locks, table->locks)
		if (strncmp(sender, receiver, MAX_NAME_LENGTH+1) == 0)
			//non è possibile mandare messaggi a se stessi
			return OP_OK;
		LOCKINORDER(sender, receiver, table->u_locks, table->locks);
		chat_user_t *usender, *ureceiver;
		if (!CHECKNAME(table->users, sender, usender) || !CHECKNAME(table->users, receiver, ureceiver)) {
			//mittente o destinatario non registrati
			table->errori++;
			UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
			return OP_NICK_UNKNOWN;
		}
		else {
			if (ureceiver->online != -1) {
				table->m_consegnati++;
				insert_ele(fds, &(ureceiver->online));
				store_msg(table, sender, text, TXT_MESSAGE, ureceiver->msgs, 1);
			}
			else {
				table->m_in_attesa++;
				store_msg(table, sender, text, TXT_MESSAGE, ureceiver->msgs, 0);
			}
			UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
			return OP_OK;
		}
	}
}

int send_text_all(users_table_t *table, char *sender, char *text, queue_t *fds) {
	if (!table || !sender || !text || !fds) {
		table->errori++;
		return OP_FAIL;
	}
	if (strlen(text) > table->max_msg_size) {
		table->errori++;
		return OP_MSG_TOOLONG;
	}
	chat_user_t *usender;
	LOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
	if (!CHECKNAME(table->users, sender, usender)) {
		//mittente non registrato
		table->errori++;
		UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
		return OP_NICK_UNKNOWN;
	}
	int i;
	icl_entry_t *tmp;
	char *key;
	chat_user_t *user;
	icl_hash_foreach(table->users, i, tmp, key, user) {
		//invio messaggio a tutti tranne che al mittente
		if (user != NULL) {
			if ((strncmp(user->username, sender, MAX_NAME_LENGTH+1)) != 0) {
				if (user->online != -1) {
					table->m_consegnati++;
					insert_ele(fds, &(user->online));
					store_msg(table, sender, text, TXT_MESSAGE, user->msgs, 1);
				}
				else {
					table->m_in_attesa++;
					store_msg(table, sender, text, TXT_MESSAGE, user->msgs, 0);
				}
			}
		}
	}
	UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
	return OP_OK;
}

int send_file(users_table_t *table, char *sender, char *receiver, char *filename, char *data, int writelen, queue_t *fds, char *dirpath) {
	if (!table || !sender || !receiver || !filename || !data || !dirpath) {
		table->errori++;
		return OP_FAIL;
	}
	if (writelen > table->max_file_size*1000 || strlen(filename) > table->max_msg_size ) {
		table->errori++;
		return OP_MSG_TOOLONG;
	}
	//controllo se il receiver è un gruppo
	LOCKGROUP(receiver, table->u_locks, table->g_locks, table->locks)
	chat_group_t *group;
	if (CHECKNAME(table->groups, receiver, group)) {
		UNLOCKGROUP(receiver, table->u_locks, table->g_locks, table->locks)
		//controllo se il mittente è registrato
		LOCKUSER(sender, table->u_locks, table->locks)
		chat_user_t *usender;
		if (!CHECKNAME(table->users, sender, usender)) {
			//mittente non registrato
			table->errori++;
			UNLOCKUSER(sender, table->u_locks, table->locks)
			return OP_NICK_UNKNOWN;
		}
		if (searchString(group->members, sender) == 0) {
			//mittente non iscritto al gruppo
			table->errori++;
			UNLOCKUSER(sender, table->u_locks, table->locks)
			return OP_NICK_UNKNOWN;
		}
		UNLOCKUSER(sender, table->u_locks, table->locks)
		//mittente e gruppo sono registrati memorizzo messaggi e prendo i file descriptor degli utenti online
		LOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
		char *username = malloc((MAX_NAME_LENGTH+1)*sizeof(char));
		//creo messaggio
		char *filepath = make_path(filename, dirpath);
		int fd=-1, ret=-1;
		TRY(fd, open(filepath, O_CREAT | O_RDWR, 0666), -1, "open", -1, 0);
		ret = write_file(fd, data, writelen);
		if (ret == -1) {
			table->errori++;
			UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
			return OP_FAIL;
		}
		for (int i=0; i<group->members->size; i++) {
			memset(username, 0, (MAX_NAME_LENGTH+1)*sizeof(char));
			//prendo uno a uno i nomi dei membri del gruppo
			getByIndex(group->members, i, username);
			if (username != NULL) {
				chat_user_t *user;
				//se l'utente è ancora registrato gli invio la notifica del messaggio
				if(CHECKNAME(table->users, username, user)) {
					//se l'utente è online lo metto in coda (per inviargli la notifica)
					if (user->online != -1) {
						insert_ele(fds, &(user->online));
					}
					//aggiungo comunque il messaggio nella history
					store_msg(table, sender, filename, FILE_MESSAGE, user->msgs, 0);
				}
				//se un utente non è più registrato lo elimino dal gruppo
				else
					removeString(group->members, username);
			}
		}
		free(filepath);
		free(username);
		UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks);
		return ret;
	}
	else {
		UNLOCKGROUP(receiver, table->u_locks, table->g_locks, table->locks)
		if (strncmp(sender, receiver, MAX_NAME_LENGTH+1) == 0)
			//non è possibile mandare messaggi a se stessi
			return OP_OK;
		LOCKINORDER(sender, receiver, table->u_locks, table->locks);
		chat_user_t *usender, *ureceiver;
		if (!CHECKNAME(table->users, sender, usender) || !CHECKNAME(table->users, receiver, ureceiver)) {
			//mittenete o destinatario non registrati
			table->errori++;
			UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
			return OP_NICK_UNKNOWN;
		}
		else {
			//creo messaggio
		fprintf(stdout, "filename %s\n", filename);
		fflush(stdout);
			char *filepath = make_path(filename, dirpath);
			int fd=-1, ret=-1;
			TRY(fd, open(filepath, O_CREAT | O_RDWR, 0666), -1, "open", -1, 0);
			ret = write_file(fd, data, writelen);
			if (ret == -1) {
				table->errori++;
				UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
				return OP_FAIL;
			}
			if (ureceiver->online != -1) {
				insert_ele(fds, &(ureceiver->online));
			}
			store_msg(table, sender, filename, FILE_MESSAGE, ureceiver->msgs, 0);
			table->f_in_attesa++;
			close(fd);
			free(filepath);
			UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
			return OP_OK;
		}
	}
}

int get_file(users_table_t *table, char *username, char *filename, char **datadest, int *filelen, char *dirpath) {
	if (!table || !username || !filename || !dirpath) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	if (!CHECKNAME(table->users, username, user)) {
		//user non registrato
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_NICK_UNKNOWN;
	}
	int ret=-1, fd=-1;
	char *filepath = make_path(filename, dirpath);
	struct stat info;
	ret = stat(filepath, &info);
	if (ret == -1) {
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_FAIL;
	}
	if (S_ISREG(info.st_mode)) {
		if ((fd = open(filepath, O_RDONLY)) != -1) {
			size_t size = info.st_size;
			char *mappedfile;
			mappedfile = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
			*datadest = malloc(size*sizeof(char));
			memset(*datadest, 0, size);
			strncpy(*datadest, mappedfile, size);
			*filelen = size;
			if (mappedfile == MAP_FAILED) {
				perror("mmap");
				close(fd);
				table->errori++;
				UNLOCKUSER(username, table->u_locks, table->locks)
				return OP_FAIL;
			}
			table->f_consegnati++;
			close(fd);
			free(filepath);
			UNLOCKUSER(username, table->u_locks, table->locks)
			return OP_OK;
		}
		else {//open fail
			table->errori++;
			UNLOCKUSER(username, table->u_locks, table->locks)
			free(filepath);
			return OP_FAIL;
		}
	}
	//file non reg
	else {
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks)
		free(filepath);
		return OP_FAIL;
	}
}


int get_history(users_table_t *table, char *username, queue_t *list) {
	if (!table || !username) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	if (CHECKNAME(table->users, username, user)) {
		//prendo i messaggi (nomi di file e testuali) che non sono stati già spediti
		chat_message_t *msg = take_ele(user->msgs);
		while(msg) {
			if (msg->consegnato == 0) {
				insert_ele(list, msg->message);
				msg->message = NULL;
			}
			freeChatMessage(msg);
			msg = take_ele(user->msgs);
		}
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
	//user non registrato
	table->errori++;
	UNLOCKUSER(username, table->u_locks, table->locks)
	return OP_NICK_ALREADY;
}

char *users_list(users_table_t *table, int *n) {
	if (!table) {
		table->errori++;
		return NULL;
	}
	LOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
	if (table->users->nentries < 0) {
		table->errori++;
		UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
		return NULL;
	}
	int i, count=0;
	icl_entry_t *tmp;
	char *key, *list = malloc((table->users->nentries*(MAX_NAME_LENGTH+1))*sizeof(char));
	memset(list, 0, table->users->nentries*(MAX_NAME_LENGTH+1));
	chat_user_t *user;
	icl_hash_foreach(table->users, i, tmp, key, user) {
		if (user != NULL && user->username != NULL) {
			strncat(list + count*(MAX_NAME_LENGTH+1), user->username, strlen(user->username));
			count++;
		}
	}
	*n = table->users->nentries;
	UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
	return list;
}

struct statistics *get_stat(users_table_t *table) {
	if (!table)
		return NULL;
	LOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
	struct statistics *stats = malloc(sizeof(struct statistics));
	stats->nusers = table->users->nentries;
	stats->nonline = table->users_online;
	stats->ndelivered = table->m_consegnati;
	stats->nnotdelivered = table->m_in_attesa;
	stats->nfiledelivered = table->f_consegnati;
	stats->nfilenotdelivered = table->f_in_attesa;
	stats->nerrors = table->errori;
	UNLOCKALL(table->locks, table->u_locks+table->g_locks+table->fd_locks)
	return stats;
}
