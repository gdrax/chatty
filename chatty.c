/*
 * chatterbox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include "utility.h"
#include "chat_parser.h"
#include "connections.h"
#include "queue.h"
#include "stats.h"
#include "users_table.h"
#include "string_list.h"

#define C_LOCKFD(fd)						\
		LOCK(&(fd_locks[fd%conf_data->fd_locks]));

#define C_UNLOCKFD(fd)						\
		UNLOCK(&(fd_locks[fd%conf_data->fd_locks]));

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
*/
/*struct statistics chattyStats = { 0,0,0,0,0,0,0 };*/

//parametri passati ai thread
struct thread_info {
	int fd_pipe[2];
	int fd_sk;
	int id;
};

//strutture delle richieste passate dal listener ai worker
typedef struct request {
	message_t *message;
	message_data_t *filedata;
	int fd;
} request_t;

//mutex per la sincornizzazione sulle statistiche
pthread_mutex_t stat_lock = PTHREAD_MUTEX_INITIALIZER;

//struttura contente le info del file di configurazione
info_server_t *conf_data;

//coda tramite la quale il thread dispatcher fornisce i file descriptor ai thread del pool
queue_t *pending_requests;

//struttura dati per memorizzare utenti gruppi e messaggi
users_table_t *users;

//intero per la terminazione dei thread
int quit = 0;

//lock per sincronizzarsi sulla variabile di terminazione
pthread_mutex_t quit_lock = PTHREAD_MUTEX_INITIALIZER;

//lock e cv per sincronizzarsi sulla coda
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t newRequest;

//array di mutex per sincronizzarsi sui descrittori
pthread_mutex_t *fd_locks;

pthread_mutex_t pipe_lock = PTHREAD_MUTEX_INITIALIZER;

//uso del server
static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}


//libera la memoria occupata da una richiesta
void freeRequest(void *data) {
	request_t *tmp = (request_t *)data;
	if (tmp) {
		freeMessage(tmp->message);
		if (tmp->filedata) free(tmp->filedata->buf);
		free(tmp->filedata);
	}
	free(tmp);
}

//corregge il nome di un file
char *make_name(char *name) {
	int len = strlen(name);
	for (int i=len-1; i >=0; i--)
		if (name[i] == '/')
			return &(name[i+1]);
	return name;
}

//legge il tipo di richiesta e produce un opportuna risposta
int make_reply (message_t *request, message_data_t *filedata, int fd, message_t *ack, message_t *reply, queue_t *fds, queue_t *list) {
	int ret = OP_FAIL;
	int ackData = 0;
	switch(request->hdr.op) {
		case REGISTER_OP: {
			char *buf;
			int n_users;
			ret = add_user(users, request->hdr.sender, fd);
			buf = users_list(users, &n_users);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setData(&(ack->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
				ackData = 1;
			}
		} break;

		case CONNECT_OP: {
			char *buf;
			int n_users;
			ret = set_online(users, request->hdr.sender, fd);
			buf = users_list(users, &n_users);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setData(&(ack->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
				ackData = 1;
			}
		} break;

		case POSTTXT_OP: {
			ret = send_text(users, request->hdr.sender, request->data.hdr.receiver, request->data.buf, fds);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setHeader(&(reply->hdr), TXT_MESSAGE, request->hdr.sender);
				setData(&(reply->data), "", request->data.buf, request->data.hdr.len);
			}
		} break;

		case POSTTXTALL_OP: {
			ret = send_text_all(users, request->hdr.sender, request->data.buf, fds);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setHeader(&(reply->hdr), TXT_MESSAGE, request->hdr.sender);
				setData(&(reply->data), "", request->data.buf, request->data.hdr.len);
			}
		} break;

		case POSTFILE_OP: {
			char *filename = make_name(request->data.buf);
			ret = send_file(users, request->hdr.sender, request->data.hdr.receiver, filename, filedata->buf, filedata->hdr.len, fds, conf_data->files_path);
			free(filedata->buf);
			free(filedata);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setHeader(&(reply->hdr), FILE_MESSAGE, request->hdr.sender);
				setData(&(reply->data), "", filename, strlen(filename)+1);
			}
		} break;

		case GETFILE_OP: {
			char *file = NULL;
			int filelen =0;
			ret = get_file(users, request->hdr.sender, request->data.buf, &file, &filelen, conf_data->files_path);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setData(&(ack->data), "", file, filelen);
				ackData = 1;
			}
		} break;

		case GETPREVMSGS_OP: {
			ret = get_history(users, request->hdr.sender, list);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				ackData = 1;
			}
		} break;

		case USRLIST_OP: {
			char *buf;
			int n_users;
			buf = users_list(users, &n_users);
			setHeader(&(ack->hdr), OP_OK, "CHATTY");
			setData(&(ack->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
			ackData = 1;
			} break;

		case UNREGISTER_OP: {
			ret = delete_user(users, request->hdr.sender);
			setHeader(&(ack->hdr), ret, "CHATTY");
			} break;

		case DISCONNECT_OP: {
			ret = set_offline(users, request->hdr.sender, fd);
			setHeader(&(ack->hdr), ret, "CHATTY");
			} break;

		case CREATEGROUP_OP: {
			ret = add_group(users, request->hdr.sender, request->data.hdr.receiver);
			setHeader(&(ack->hdr), ret, "CHATTY");
			} break;

		case ADDGROUP_OP: {
			ret = join_group(users, request->hdr.sender, request->data.hdr.receiver);
			setHeader(&(ack->hdr), ret, "CHATTY");
			} break;

		case DELGROUP_OP: {
			ret = leave_group(users, request->hdr.sender, request->data.hdr.receiver);
			setHeader(&(ack->hdr), ret, "CHATTY");
			} break;

		default: {
			PRINT("SERVER: Rischiesta sconosciuta")
			ackData = -1;
			} break;
	}
	return ackData;
}

void *worker(void *data) {
	struct thread_info infos = *(struct thread_info *)data;
	int id = infos.id, fd_pipe = infos.fd_pipe[1];
	while (1) {
		LOCK(&queue_lock)
		LOCK(&quit_lock)
		while(pending_requests->len == 0 || quit) {
			if (quit) {
				UNLOCK(&quit_lock)
				UNLOCK(&queue_lock)
				fprintf(stdout, "WOKER %d: Ricevuto segnale di terminazione, chiudo\n", id);
				return NULL;
			}
			else {
				UNLOCK(&quit_lock)
			}
			fprintf(stdout, "WORKER %d: Mi sospendo su newRequest\n", id);
			CHECK((pthread_cond_wait(&newRequest, &queue_lock) != 0), "Pthread wait", NULL, 1)
			LOCK(&quit_lock)
		}
		//prendo una richiesta dalla coda
		UNLOCK(&quit_lock)
		request_t *work = (request_t *)take_ele(pending_requests);
		UNLOCK(&queue_lock)
		if (work == NULL) continue;
		fprintf(stdout, "WOKER %d: Trovata nuova richiesta su fd %d\n", id, work->fd);
		int fd = work->fd;
		message_t *request = work->message, *ack, *reply;
		message_data_t *filedata = work->filedata;
		free(work);
		int ret = -1, ackData = 0;
		queue_t *fds = create_queue(NULL), *list=create_queue(freeMessage);
		TRY(ack, malloc(sizeof(message_t)), NULL, "malloc", NULL, 0)
		TRY(reply, malloc(sizeof(message_t)), NULL, "malloc", NULL, 0)
		memset(ack, 0, sizeof(message_t));
		memset(reply, 0, sizeof(message_t));
		fprintf(stdout, "WORKER %d: Eseguo op %d su fd %d\n", id, request->hdr.op, fd);
		//eseguo la riichiesta
		ackData = make_reply(request, filedata, fd, ack, reply, fds, list);
		if (ackData == -1)
			//richiesta sconosciuta
			break;
		//invio ack
		C_LOCKFD(fd)
		if (ackData) {
			if (request->hdr.op == GETPREVMSGS_OP) {
				size_t n = list->len;
				setData(&(ack->data), "", (char *)&n, sizeof(n));
				fprintf(stdout, "%ld\n", *(size_t*)ack->data.buf);
			}
			if((ret = sendRequest(fd, ack)) == 0)
				break;
		}
		else {
			if((ret = sendHeader(fd, &(ack->hdr))) == 0)
				break;
		}
		C_UNLOCKFD(fd)
		fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, ack->hdr.op, fd);
		if (ret > 0) {
			//invio le risposte che servono dopo l'ack
			if (ack->hdr.op == OP_OK) {
				if(request->hdr.op == POSTTXT_OP || request->hdr.op == POSTTXTALL_OP || request->hdr.op == POSTFILE_OP) {
					int rec_fd, ret2;
					for (int i=0; i<fds->len; i++) {
						rec_fd = *(int *)take_ele(fds);
						C_LOCKFD(rec_fd)
						if ((ret2 = sendRequest(rec_fd, reply)) == 0) {
							set_offline(users, request->hdr.sender, rec_fd);
							close(rec_fd);
							fprintf(stdout, "WORKER %d: Connessione su fd %d chiusa\n", id, rec_fd);
						}
						else {
							fprintf(stdout, "WOKER %d: Invio messaggio %s su fd %d\n", id, reply->data.buf, rec_fd);
						}
						C_UNLOCKFD(rec_fd)
					}
				}
				else if(request->hdr.op == GETPREVMSGS_OP) {
					message_t *tmp = take_ele(list);
					while (tmp != NULL) {
						C_LOCKFD(fd)
						if ((ret = sendRequest(fd, tmp)) <= 0) {
							C_UNLOCKFD(fd)
							break;
						}
						else {
							fprintf(stdout, "WOKER %d: Invio vecchio messaggio %s su fd %d\n", id, tmp->data.buf, fd);
						}
						C_UNLOCKFD(fd)
						freeMessage(tmp);
						tmp = take_ele(list);
					}
				}
			}
		}
		delete_queue(fds);
		delete_queue(list);
		//se la connessione è chiusa chiudo il file descriptor...
		if (ret <= 0) {
			C_LOCKFD(fd)
			set_offline(users, request->hdr.sender, fd);
			close(fd);
			fprintf(stdout, "WORKER %d: Connessione su fd %d chiusa\n", id, fd);
			C_UNLOCKFD(fd)
		}
		//...altrimenti se non si sono verificati errori rimetto il file descriptor in coda
		else if (ack->hdr.op < 23) {
			LOCK(&pipe_lock)
			if ((write(fd_pipe, &fd, sizeof(int))) != sizeof(int))
				perror("write sulla pipe");
			fprintf(stdout, "WORKER %d: Restituisco fd %d al listener\n", id, fd);
			UNLOCK(&pipe_lock);
		}
		//libero l'ack
		if (request->hdr.op != GETPREVMSGS_OP && ackData) free(ack->data.buf);
		free(ack);
		freeMessage(request);
		free(reply);
		LOCK(&quit_lock)
		if (quit) {
			UNLOCK(&quit_lock)
			fprintf(stdout, "WOKER %d: Ricevuto segnale di terminazione, chiudo\n", id);
			return NULL;
		}
		UNLOCK(&quit_lock)
	}
	return NULL;
}

void *listener(void *data) {
	PRINT("SERVER: Thread listener avviato")
	struct thread_info infos = *(struct thread_info *)data;
	int fd_pipe = infos.fd_pipe[0], max = -1, n_ready = 0;
	fd_set active_set, ready_set;
	FD_ZERO(&active_set);
	FD_SET(fd_pipe, &active_set);
	if (fd_pipe > max) max = fd_pipe;
	while(1) {
		LOCK(&quit_lock)
		if (quit) {
			UNLOCK(&quit_lock)
			PRINT("LISTENER: Ricevuto segnale di terminazione, chiudo")
			break;
		}
		UNLOCK(&quit_lock)
		//reimposto il set prima di fare una nuova select
		n_ready = 0;
		ready_set = active_set;
		n_ready = select(max+1, &ready_set, NULL, NULL, NULL);
		if (n_ready < 0) {
			perror("select");
		}
		for (int i=0; i<max+1 && n_ready > 0; i++) {
			if (FD_ISSET(i, &ready_set)) {
				//se ci sono dati da leggere sulla pipe aggiorno il set
				if (i == fd_pipe) {
					int new_fd = -1;
					LOCK(&pipe_lock)
					//leggo dalla pipe
					if ((read(fd_pipe, &new_fd, sizeof(int))) != sizeof(int))
						perror("read sulla pipe");
					else {
						PRINT("LISTENER: Aggiorno set")
						if (new_fd > max) max = new_fd;
						FD_SET(new_fd, &active_set);
					}
					UNLOCK(&pipe_lock);
				}
				else {//se il descrittore di un client è pronto, leggo la richiesta e la inserisco in coda
					FD_CLR(i, &active_set);
					request_t *new = malloc(sizeof(request_t));
					memset(new, 0, sizeof(request_t));
					new->message = malloc(sizeof(message_t));
					memset(new->message, 0, sizeof(message_t));
					new->filedata = NULL;
					//leggo la nuova richiesta
					C_LOCKFD(i);
					if ((readMsg(i, new->message)) > 0) {
						//leggo eventuali dati aggiuntivi
						if (new->message->hdr.op == POSTFILE_OP) {
							new->filedata = malloc(sizeof(message_data_t));
							memset(new->filedata, 0, sizeof(message_data_t));
							if ((readData(i, new->filedata)) <= 0) {
								set_offline_fd(users, i);
								close(i);
								fprintf(stdout, "LISTENER: Connessione su fd %d chiusa\n", i);
								freeRequest(new);
							}
						}
						C_UNLOCKFD(i)
						//inserisco la rischiesta in coda
						new->fd = i;
						LOCK(&queue_lock)
						insert_ele(pending_requests, new);
						PRINT("LISTENER: Nuova richiesta trovata, inserisco in coda")
						pthread_cond_signal(&newRequest);
						UNLOCK(&queue_lock)
					}
					else {
						C_UNLOCKFD(i)
						set_offline_fd(users, i);
						close(i);
						fprintf(stdout, "LISTENER: Connessione su fd %d chiusa\n", i);
						freeRequest(new);
					}
				}
			n_ready--;
			}
		}
	}
	return NULL;
}

void *dispatcher(void *data) {
	struct thread_info infos = *(struct thread_info *)data;
	int fd_sk = infos.fd_sk, fd_pipe = infos.fd_pipe[1];
	PRINT("SERVER: Thread dispatcher avviato")
	while(1) {
		LOCK(&quit_lock)
		if (quit) {
			UNLOCK(&quit_lock)
			PRINT("DISPATCHER: Ricevuto segnale di terminazione, chiudo")
			break;
		}
		UNLOCK(&quit_lock)
		int ret;
		//aspetto che un nuovo client si connetta
		ret = accept(fd_sk, NULL, 0);
		if (ret < 0) {
			perror("accept");
			break;
		}
		//ho trovato una nuova connessione, invio file descriptor sulla pipe
		else {
			LOCK(&pipe_lock)
			if ((write(fd_pipe, &ret, sizeof(int))) != sizeof(int))
				perror("write sulla pipe");
			else
				fprintf(stdout, "DISPATCHER: Nuovo client connesso, segnalo al listner fd %d\n", fd_pipe);

			UNLOCK(&pipe_lock);
		}
	}
	return NULL;
}

void *signal_handler(void *data) {

	char *stat_path = (char *)data;
	sigset_t segnali;
	int ret, segnale;
	TRY(ret, sigemptyset(&segnali), -1, "Sigemptyset", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGTERM), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGQUIT), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGINT), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGUSR1), -1, "Aggiunta segnale alla maschera", 0, 1)

	PRINT("SERVER: Thread gestore dei segnali avviato")
	while (1) {
		//aspetto un segnale
		ret = sigwait(&segnali, &segnale);
		if (ret > 0)
			perror("sigwait");
		if (segnale == SIGTERM || segnale == SIGQUIT || segnale == SIGINT) {
			PRINT("SERVER: Ricevuto segnale di terminazione, chiudo")
			LOCK(&quit_lock)
			quit = 1;
			UNLOCK(&quit_lock)
			pthread_cond_broadcast(&newRequest);
			pthread_cond_broadcast(&newRequest);
			break;
		}
		if (segnale == SIGUSR1) {
			PRINT("SERVER: Ricevuto segnale SIGUSR1, stampo statistiche")
			LOCK(&stat_lock)
			struct statistics *stats = get_stat(users);
			FILE *fd;
			if ((fd = fopen(stat_path, "a")) == NULL) {
				perror("fopen");
			}
			else {
				printStats(fd, stats);
				fclose(fd);
			}
			free(stats);
			UNLOCK(&stat_lock)
			continue;
		}
	}
	return NULL;
}

int main(int argc, char *argv[]) {

	extern char *optarg;
	int opt;

/*----------------RIDIREZIONE STDOUT E STDERR----------------*/
	freopen("/tmp/chatty_stdout.txt", "w+", stdout);
	freopen("/tmp/chatty_stderr.txt", "w+", stderr);

	int ret;
/*----------------GESTIONE SEGNALI----------------*/
	//maschero tutti i segnali
	sigset_t segnali;
	TRY(ret, sigfillset(&segnali), -1, "Sigfillset", 0, 1)
	TRY(ret, pthread_sigmask(SIG_SETMASK, &segnali, NULL), 1, "Sigmask", 0, 1)

	//imposto handler per ignorare SIGPIPE e SIGMASK
	TRY(ret, sigaddset(&segnali, SIGPIPE), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGSEGV), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, pthread_sigmask(SIG_SETMASK, &segnali, NULL), 1, "Sigmask", 0, 1)
	struct sigaction ignore;
	memset(&ignore, 0, sizeof(ignore));
	ignore.sa_handler = SIG_IGN;
	TRY(ret, sigaction(SIGPIPE, &ignore, NULL), -1, "Sigaction", 0, 1)
	TRY(ret, sigaction(SIGSEGV, &ignore, NULL), -1, "Sigaction", 0, 1)

	//maschero gli altri segnali che verranno gestiti da un thread apposito
	TRY(ret, sigemptyset(&segnali), -1, "Sigemptyset", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGINT), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGTERM), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGQUIT), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, sigaddset(&segnali, SIGUSR1), -1, "Aggiunta segnale alla maschera", 0, 1)
	TRY(ret, pthread_sigmask(SIG_SETMASK, &segnali, NULL), 1, "Sigmask", 0, 1)


/*----------------GESTIONE OPZIONI DI LANCIO DEL SERVER----------------*/
	// controllo opzioni di lancio del server
	opt = getopt(argc, argv, "f:");
	switch(opt) {
		case 'f': {
			//ottengo le info dal file di configurazione
			TRY(conf_data, malloc(sizeof(info_server_t)), NULL, "malloc", 0, 1)
			ret = get_config(optarg, conf_data);
			if (ret == -1) exit(EXIT_FAILURE);
			break;
		}
		default : usage(argv[0]); break;
	}

	unlink(conf_data->unix_path);

/*----------------INIZIALIZZO CONNESSIONE----------------*/
	int fd_sk;
	TRY(fd_sk, socket(AF_UNIX, SOCK_STREAM, 0), -1, "socket", 0, 1)
	struct sockaddr_un indirizzo;
	memset(&indirizzo, 0, sizeof(indirizzo));
	indirizzo.sun_family = AF_UNIX;
	strncpy(indirizzo.sun_path, conf_data->unix_path, strlen(conf_data->unix_path));

	// faccio il bind con l'indirizzo estratto dal file di conf
	TRY(ret, bind(fd_sk, (struct sockaddr *)&indirizzo, sizeof(indirizzo)), -1, "bind", 0, 1)
	PRINT("SERVER: Indirizzo associato al socket")

	// metto socket in listen
	TRY(ret, listen(fd_sk, SOMAXCONN), -1, "trying to set the socket as listening", 0, 1)
	PRINT("SERVER: Socket in listening")

/*----------------INIZIALIZZO STRUTTURE----------------*/

	users = create_table(conf_data->user_locks, conf_data->group_locks, conf_data->fd_locks, conf_data->buckets, conf_data->max_history, conf_data->max_msg_size, conf_data->max_file_size);

	fd_locks = malloc(conf_data->fd_locks*sizeof(pthread_mutex_t));
	for (int i=0; i<conf_data->fd_locks; i++) {
		memset(&(fd_locks[i]), 0, sizeof(pthread_mutex_t));
	}

/*----------------AVVIO I THREAD----------------*/
	//creo coda delle connessioni
	pending_requests = create_queue(freeRequest);

	//imposto gli attributi dei thread
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	//creo struttura da passare ai thread e inizializzo la pipe
	struct thread_info infos;
	CHECK((pipe(infos.fd_pipe) == -1), "pipe", 0, 1);
	infos.fd_sk = fd_sk;

	//lancio il thread dispatcher
	pthread_t th_dispatcher;
	CHECK((pthread_create(&th_dispatcher, &attr, dispatcher, &infos) != 0), "Pthread create", 0, 1)

	//lancio il thread listener
	pthread_t th_listener;
	CHECK((pthread_create(&th_listener, &attr, listener, &infos) != 0), "Pthread create", 0, 1)

	// lancio i thread worker
	pthread_t *workers;
	TRY(workers, malloc(conf_data->th_in_pool*sizeof(pthread_t)), NULL, "main chatty malloc", 0, 1)
	for (int i=0, k=0; i<conf_data->th_in_pool; i++, k++) {
		infos.id = k;
		CHECK((pthread_create(&workers[i], &attr, worker, &infos) != 0), "Pthread create", 0, 1)
	}
	PRINT("SERVER: Threads avviati")

	//lancio thread gestore dei segnali
	pthread_t th_signal_handler;
	CHECK((pthread_create(&th_signal_handler, NULL, signal_handler, conf_data->stat_path) != 0), "Pthread create", 0, 1)

/*----------------TERMINAZIONE DEI THREAD----------------*/
	pthread_join(th_signal_handler, NULL);

	for (int i=0; i>conf_data->th_in_pool; i++) {
//		pthread_detach(workers[i]);
		pthread_join(workers[i], NULL);
		PRINT("SERVER: Thread worker terminato")
	}
	pthread_join(th_listener, NULL);
	pthread_cancel(th_dispatcher);

	fprintf(stdout, "SERVER: Thread temrinati\n");
	free(workers);

/*----------------CANCELLAZIONE STRUTTURE DATI----------------*/
	destroy_table(users);
	delete_queue(pending_requests);
	free(fd_locks);
	free_config(conf_data);
	fflush(stdout);
	return 0;
}
