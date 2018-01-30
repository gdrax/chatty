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
		deleteMsgList(user->msgs);
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

char *make_path(char *filename, char *dirpath) {
	int len = strlen(filename) + strlen(dirpath) + 2, i=0;
	char *filepath = malloc(len*sizeof(char));
	for(i=0; i<strlen(dirpath); i++)
		filepath[i] = dirpath[i];
	filepath[i] = '/';
	i++;
	for (int j=0; j<strlen(filename); j++, i++)
		filepath[i] = filename[j];
	filepath[i] = '\0';
	return filepath;
}

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


users_table_t *create_table(int u_locks, int g_locks, int n_buckets, int history) {
	if (u_locks <= 0) u_locks = 8;
	if (g_locks <= 0) g_locks = 8;
	if (n_buckets <= 0) n_buckets = 16;

	users_table_t *table;
	TRY(table, malloc(sizeof(users_table_t)), NULL, "malloc", NULL, 1)
	table->users = icl_hash_create(n_buckets, djb2, NULL);
	table->groups = icl_hash_create(n_buckets, djb2, NULL);
	TRY(table->locks, malloc((u_locks+g_locks)*sizeof(pthread_mutex_t)), NULL, "malloc", NULL, 1)
	for (int i=0; i<(u_locks+g_locks); i++) {
		memset(&(table->locks[i]), 0, sizeof(pthread_mutex_t));
	}
	table->u_locks = u_locks;
	table->g_locks = g_locks;
	table->history = history;
	return table;
}

int destroy_table(users_table_t *table) {
	int ret;
	if (table) {
		TRY(ret, icl_hash_destroy(table->users, NULL, &freeUser), -1, "icl_hash_destroy", -1, 0)
		TRY(ret, icl_hash_destroy(table->groups, NULL, &freeGroup), -1, "icl_hash_destroy", -1, 0)
		free(table->locks);
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
	strncpy(new->username, username, strlen(username)+1);
	new->online = -1;
	if ((new->msgs = createMsgList(table->history)) == NULL) {
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
		strncpy(new->groupname, groupname, strlen(groupname)+1);
		if ((new->members = createSList(MAX_NAME_LENGTH+1)) == NULL) {
			table->errori++;
			UNLOCKGROUP(groupname, table->u_locks, table->g_locks, table->locks)
			UNLOCKUSER(owner, table->u_locks, table->locks)
			return OP_FAIL;
		}
		strncpy(new->owner, owner, strlen(owner)+1);
		//aggiungo subito il proprietario tra i membri
		int ret;
		if ((ret = addString(new->members, new->owner)) == -1) {
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
		if ((ret = addString(group->members, user->username)) != 0) {
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
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	if (CHECKNAME(table->users, username, user)) {
		user->online = fd;
		table->users_online++;
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
	//user non registrato
	table->errori++;
	UNLOCKUSER(username, table->u_locks, table->locks)
	return OP_NICK_UNKNOWN;
}

int set_offline(users_table_t *table, char *username) {
	if (!table || !username) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	if (CHECKNAME(table->users, username, user)) {
		user->online = -1;
		table->users_online--;
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_OK;
	}
	//user non registrato
	table->errori++;
	UNLOCKUSER(username, table->u_locks, table->locks)
	return OP_NICK_UNKNOWN;
}

int send_text(users_table_t *table, char *sender, char *receiver, char *text, queue_t *fds) {
	if (!table || !sender || !receiver || !text || !fds) {
		table->errori++;
		return OP_FAIL;
	}
	if (strlen(text) > MAX_MSG_LENGTH) {
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
		LOCKALL(table->locks, table->u_locks+table->g_locks)
		char *username = malloc((MAX_NAME_LENGTH+1)*sizeof(char));
		int ret;
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
						insert_ele(fds, user->online);
						if ((ret = addMsg(user->msgs, text, sender, 1, 1) != 0)) {
							table->errori++;
							UNLOCKALL(table->locks, table->u_locks+table->g_locks)
							return OP_FAIL;
						}
					}
					else {
						table->m_in_attesa++;
						if ((ret = addMsg(user->msgs, text, sender, 1, 0) != 0)) {
							table->errori++;
							UNLOCKALL(table->locks, table->u_locks+table->g_locks)
							return OP_FAIL;
						}
					}
				}
				//se un utente non è più registrato lo elimino dal gruppo
				else
					removeString(group->members, username);
			}
		}
		free(username);
		UNLOCKALL(table->locks, table->u_locks+table->g_locks);
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
		else {//fprintf(stdout, "devo mandare a %s con fd %d, io sono %s con df %d\n", ureceiver->username, ureceiver->online, usender->username, usender->online);
				//fflush(stdout);
			int ret;
			if (ureceiver->online != -1) {
				table->m_consegnati++;
				insert_ele(fds, ureceiver->online);
				//fprintf(stdout, "fd %d\n", ureceiver->online);
				if ((ret = addMsg(ureceiver->msgs, text, sender, 1, 1)) != 0) {
					table->errori++;
					UNLOCKINORDER(sender, receiver, table->u_locks, table->locks)
					return OP_FAIL;
				}
			}
			else {
				table->m_in_attesa++;
				if ((ret = addMsg(ureceiver->msgs, text, sender, 1, 0)) != 0) {
					table->errori++;
					UNLOCKINORDER(sender, receiver, table->u_locks, table->locks)
					return OP_FAIL;
				}
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
	if (strlen(text) > MAX_MSG_LENGTH) {
		table->errori++;
		return OP_MSG_TOOLONG;
	}
	chat_user_t *usender;
	LOCKALL(table->locks, table->u_locks+table->g_locks)
	if (!CHECKNAME(table->users, sender, usender)) {
		//mittente non registrato
		table->errori++;
		UNLOCKALL(table->locks, table->u_locks+table->g_locks)
		return OP_NICK_UNKNOWN;
	}
	int i, ret=-1;
	icl_entry_t *tmp;
	char *key;
	chat_user_t *user;
	icl_hash_foreach(table->users, i, tmp, key, user) {
		//invio messaggio a tutti tranne che al mittente
		if (user != NULL/* && strncmp(user->username, sender, MAX_NAME_LENGTH+1) != 0*/) {
			if (user->online != -1) {
				table->m_consegnati++;
				insert_ele(fds, user->online);
				if ((ret == addMsg(user->msgs, text, sender, 1, 1)) != 0) {
					table->errori++;
					UNLOCKALL(table->locks, table->u_locks+table->g_locks)
					return OP_FAIL;
				}
			}
			else {
				table->m_in_attesa++;
				if ((ret == addMsg(user->msgs, text, sender, 1, 1)) != 0) {
					table->errori++;
					UNLOCKALL(table->locks, table->u_locks+table->g_locks)
					return OP_FAIL;
				}
			}
			UNLOCKALL(table->locks, table->u_locks+table->g_locks)
			return OP_OK;
		}
	}
	UNLOCKALL(table->locks, table->u_locks+table->g_locks)
	return OP_OK;
}

int send_file(users_table_t *table, char *sender, char *receiver, char *name, char *data, int writelen, queue_t *fds, char *dirpath, int max_size) {
	if (!table || !sender || !receiver || !name || !data || !dirpath) {
		table->errori++;
		return OP_FAIL;
	}
	if (strlen(data) > max_size) {
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
		LOCKALL(table->locks, table->u_locks+table->g_locks)
		char *username = malloc((MAX_NAME_LENGTH+1)*sizeof(char));
		//creo messaggio
		char *filepath = make_path(name, dirpath);
		int fd=-1, ret=-1;
		TRY(fd, open(filepath, O_CREAT | O_RDWR, 0666), -1, "open", -1, 0);
		ret = write_file(fd, (char *)data, writelen);
		if (ret == -1) {
			table->errori++;
			UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
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
						insert_ele(fds, user->online);
					}
					//aggiungo comunque il messaggio nella history
					if ((ret = addMsg(user->msgs, name, sender, 0, 0) != 0)) {
						table->errori++;
						UNLOCKALL(table->locks, table->u_locks+table->g_locks)
						return OP_FAIL;
					}
					UNLOCKALL(table->locks, table->u_locks+table->g_locks)
					return OP_OK;
				}
				//se un utente non è più registrato lo elimino dal gruppo
				else
					removeString(group->members, username);
			}
		}
		free(filepath);
		free(username);
		UNLOCKALL(table->locks, table->u_locks+table->g_locks);
		return OP_OK;
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
			char *filepath = make_path(name, dirpath);
			int fd=-1, ret=-1;
			TRY(fd, open(filepath, O_CREAT | O_RDWR, 0666), -1, "open", -1, 0);
			ret = write_file(fd, (char *)data, writelen);
			if (ret == -1) {
				table->errori++;
				UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
				return OP_FAIL;
			}
			if (ureceiver->online != -1) {
				insert_ele(fds, ureceiver->online);
			}
			addMsg(ureceiver->msgs, name, sender, 0, 0);
			table->f_in_attesa++;
			close(fd);
			free(filepath);
			UNLOCKINORDER(sender, receiver, table->u_locks, table->locks);
			return OP_OK;
		}
	}
}

int get_file(users_table_t *table, char *username, char *name, char *datadest, int *filelen, char *dirpath) {
	if (!table || !username || !name || !dirpath) {
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
	char *filepath = make_path(name, dirpath);
	struct stat info;
	ret = stat(filepath, &info);
	if (ret == -1) {
		table->errori++;
		UNLOCKUSER(username, table->u_locks, table->locks)
		return OP_FAIL;
	}
	if (S_ISREG(info.st_mode)) {
		if ((fd = open(filepath, O_RDONLY)) != -1) {
			char *mappedfile = NULL;
			mappedfile = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
			datadest = mappedfile;
			*filelen = (int)info.st_size;
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


int get_history(users_table_t *table, char *username, msg_list_t *msgs) {
	if (!table || !username || !msgs) {
		table->errori++;
		return OP_FAIL;
	}
	LOCKUSER(username, table->u_locks, table->locks)
	chat_user_t *user;
	int ret;
	if (CHECKNAME(table->users, username, user)) {
		//prendo i messaggi (nomi di file e testuali) che non sono stati già spediti
		ret = getMsgList(user->msgs, msgs);
		UNLOCKUSER(username, table->u_locks, table->locks)
		return ret;
	}
	//user non registrato
	table->errori++;
	UNLOCKUSER(username, table->u_locks, table->locks)
	return OP_NICK_UNKNOWN;
}

char *users_list(users_table_t *table, int *n) {
	if (!table) {
		table->errori++;
		return NULL;
	}
	LOCKALL(table->locks, table->u_locks+table->g_locks)
	if (table->users->nentries < 0) {
		table->errori++;
		UNLOCKALL(table->locks, table->u_locks+table->g_locks)
		return NULL;
	}
	int i, count=0;
	icl_entry_t *tmp;
	char *key, *list = malloc(table->users->nentries*(MAX_NAME_LENGTH+1)*sizeof(char));
	memset(list, 0, table->users->nentries*(MAX_NAME_LENGTH+1)*sizeof(char));
	chat_user_t *user;
	icl_hash_foreach(table->users, i, tmp, key, user) {
		if (user != NULL && user->username != NULL) {
			strncat(list + count*(MAX_NAME_LENGTH+1), user->username, MAX_NAME_LENGTH+1);
			count++;
		}
	}

	*n = table->users->nentries;
	UNLOCKALL(table->locks, table->u_locks+table->g_locks)
	return list;
}

void get_stat(users_table_t *table, struct statistics *stats) {
	if (!table)
		return;
	LOCKALL(table->locks, table->u_locks+table->g_locks)
	stats->nusers = table->users->nentries;
	stats->nonline = table->users_online;
	stats->ndelivered = table->m_consegnati;
	stats->nnotdelivered = table->m_in_attesa;
	stats->nfiledelivered = table->f_consegnati;
	stats->nfilenotdelivered = table->f_in_attesa;
	stats->nerrors = table->errori;
	UNLOCKALL(table->locks, table->u_locks+table->g_locks)
}
