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

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
*/
struct statistics chattyStats = { 0,0,0,0,0,0,0 };

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

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

queue_t *make_reply (message_t *request, message_t *ack, message_t *reply, message_data_t *filedata, int *ackData, int fd) {
	int ret = OP_FAIL;
	queue_t *fds = create_queue();
	*ackData = 0;
	switch(request->hdr.op) {
		case REGISTER_OP: {
			char *buf;
			int n_users;
			ret = add_user(users, request->hdr.sender, fd);
			buf = users_list(users, &n_users);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setData(&(ack->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
				*ackData = 1;
			}
			free(buf);
		} break;

		case CONNECT_OP: {
			char *buf;
			int n_users;
			ret = set_online(users, request->hdr.sender, fd);
			buf = users_list(users, &n_users);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setData(&(ack->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
				*ackData = 1;
			}
			free(buf);
		} break;

		case POSTTXT_OP: {
			ret = send_text(users, request->hdr.sender, request->data.hdr.receiver, request->data.buf, fds);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setHeader(&(reply->hdr), TXT_MESSAGE, request->hdr.sender);
				setData(&(reply->data), "", request->data.buf, strlen(request->data.buf));
			}
		} break;

		case POSTTXTALL_OP: {
			ret = send_text_all(users, request->hdr.sender, request->data.buf, fds);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == OP_OK) {
				setHeader(&(reply->hdr), TXT_MESSAGE, request->hdr.sender);
				setData(&(reply->data), "", request->data.buf, strlen(request->data.buf));
			}
		} break;

		case POSTFILE_OP: {
			ret = send_file(users, request->hdr.sender, request->data.hdr.receiver, request->data.buf, filedata->buf, filedata->hdr.len, fds, conf_data->files_path, conf_data->max_file_size);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == 0) {
				setHeader(&(reply->hdr), FILE_MESSAGE, request->hdr.sender);
				setData(&(reply->data), "", request->data.buf, strlen(request->data.buf));
			}
		} break;

		case GETFILE_OP: {
			char *file;
			int filelen;
			ret = get_file(users, request->hdr.sender, request->data.buf, file, &filelen, conf_data->files_path);
			setHeader(&(ack->hdr), ret, "CHATTY");
			if (ret == 0) {
				setData(&(ack->data), "", file, filelen);
				insert_ele(fds, fd);
				*ackData = 1;
			}
			free(file);
		} break;

		case GETPREVMSGS_OP: {
			int n_msgs=0, n_files=0, n=0;
			char *msgs=NULL, *files=NULL;
			ret = get_history(users, request->hdr.sender, &n_msgs, &n_files, &msgs, &files);
			setHeader(&(ack->hdr), ret, "CHATTY");
			n = n_msgs + n_files;
			free(reply);
			reply = malloc(n*sizeof(message_t));
			if (ret == 0) {
				setData(&(ack->data), "", (char *)&n, sizeof(n)); 
				*ackData = 1;
				for (int i=0; i<n_msgs; i++) {
					setHeader(&(reply[i].hdr), TXT_MESSAGE, "");
					setData(&(reply[i].data), "server", msgs+i*(MAX_MSG_LENGTH+1), MAX_MSG_LENGTH+1);
				}
				for (int j=0, i=n_msgs ; j<n_files; i++, j++) {
					setHeader(&(reply[i].hdr), FILE_MESSAGE, "");
					setData(&(reply[i].data), "server", files+j*(MAX_MSG_LENGTH+1), MAX_MSG_LENGTH+1);
				}
			}
			free(msgs);
			free(files);
			} break;

		case USRLIST_OP: {
			char *buf;
			int n_users;
			buf = users_list(users, &n_users);
			setHeader(&(ack->hdr), OP_OK, "CHATTY");
			setData(&(ack->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
			*ackData = 1;
			free(buf);
			} break;

		case UNREGISTER_OP: {
			ret = delete_user(users, request->hdr.sender);
			setHeader(&(ack->hdr), ret, "CHATTY");
			} break;

		case DISCONNECT_OP: {
			ret = set_offline(users, request->hdr.sender);
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
			} break;
	}
	return fds;
}

void *worker(void *name) {
	int id = *(int *)name;

	while (1) {
		LOCK(&queue_lock)
		LOCK(&quit_lock)
		while(pending_requests->len == 0 || quit) {
			if (quit) {
				UNLOCK(&quit_lock)
				UNLOCK(&queue_lock)
				fprintf(stdout, "WOKER %d: Ricevuto segnale di terminazione, chiudo\n", id);
				fflush(stdout);
				return NULL;
			}
			else {
				UNLOCK(&quit_lock)
				fprintf(stdout, "WORKER %d: Mi sospendo su newRequest\n", id);
				fflush(stdout);
				CHECK((pthread_cond_wait(&newRequest, &queue_lock) != 0), "Pthread wait", NULL, 1)
			}
		LOCK(&quit_lock)
		}
		//prendo un file desciptor dalla coda
		UNLOCK(&quit_lock)
		int fd = take_ele(pending_requests);
		UNLOCK(&queue_lock)
		message_t *request, *ack, *reply;
		message_data_t *filedata;
		int ret = 0, ackData = 0;
		queue_t *fds;
		TRY(request, malloc(sizeof(message_t)), NULL, "malloc", NULL, 0)
		TRY(ack, malloc(sizeof(message_t)), NULL, "malloc", NULL, 0)
		TRY(reply, malloc(sizeof(message_t)), NULL, "malloc", NULL, 0)
		TRY(filedata, malloc(sizeof(message_data_t)), NULL, "malloc", NULL, 0)
		//leggo header della richiesta
		if((ret = readMsg(fd, request)) == 0) {
			fprintf(stdout, "SERVER: Connessione su fd %d chiusa", fd);
			close(fd);
		}
		else if(ret == 1) {
			if (request->hdr.op == POSTFILE_OP)
				//devo leggere il contenuto del file
				ret = readData(fd, filedata);
			fprintf(stdout, "WORKER %d: Eseguo op %d su fd %d\n", id, request->hdr.op, fd);
			//eseguo la riichiesta
			fds = make_reply(request, ack, reply, filedata, &ackData, fd);
			//invio ack
			if (ackData == 1)
				ret = sendRequest(fd, ack);
			else
				ret = sendHeader(fd, &(ack->hdr));
			//se il client ha chiuso la connessione chiudo il file descriptor
			if (ret == 0) {
				fprintf(stdout, "SERVER: Connessione su fd %d chiusa", fd);
				close(fd);
				set_offline(users, request->hdr.sender);
				continue;
			}

			//invio le risposte che servono dopo l'ack
			if (ack->hdr.op == OP_OK) {
				if(request->hdr.op == POSTTXT_OP || request->hdr.op == POSTTXTALL_OP || request->hdr.op == POSTFILE_OP) {
					int rec_fd;
					while((rec_fd = take_ele(fds)) != -1) {
						if ((ret = sendRequest(rec_fd, reply)) == 0) {
							fprintf(stdout, "SERVER: Connessione su fd %d chiusa", rec_fd);
							close(rec_fd);
						}
					}
				}
				else if(request->hdr.op == GETPREVMSGS_OP) {
					int n = sizeof(reply)/sizeof(message_t);
					for (int i=0; i<n; i++) {
						if ((ret = sendRequest(fd, reply)) == 0) {
							fprintf(stdout, "SERVER: Connessione su fd %d chiusa", fd);
							close(fd);
						}
					}
					
				}
			}
		}
		free(request);
		free(reply);
		free(filedata);
		delete_queue(fds);
		LOCK(&quit_lock)
		if (quit) {
			UNLOCK(&quit_lock)
			fprintf(stdout, "WOKER %d: Ricevuto segnale di terminazione, chiudo\n", id);
			fflush(stdout);
			return NULL;
		}
		UNLOCK(&quit_lock)
		pthread_cond_signal(&newRequest);
	}
	return NULL;
}

void *dispatcher(void *data) {
	int fd = *(int *)data;
	fprintf(stdout, "SERVER: Thread dispatcher avviato\n");
	while(1) {
		LOCK(&quit_lock)
		if (quit) {
			UNLOCK(&quit_lock)
			fprintf(stdout, "DISPATCHER: Ricevuto segnale di terminazione, chiudo\n");
			break;
		}
		UNLOCK(&quit_lock)
		int ret;
		ret = accept(fd, NULL, 0);
		if (ret < 0) {
			break;
		}
		else {
			LOCK(&queue_lock);
			fprintf(stdout, "DISPATCHER: Nuovo client connesso, inserisco in coda\n");
			insert_ele(pending_requests, ret);
			pthread_cond_signal(&newRequest);
			UNLOCK(&queue_lock);
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
		sigwait(&segnali, &segnale);

		if (segnale == SIGTERM || segnale == SIGQUIT || segnale == SIGINT) {
			PRINT("SERVER: Ricevuto segnale di terminazione, chiudo")
			fflush(stdout);
			LOCK(&quit_lock)
			quit = 1;
			UNLOCK(&quit_lock)
			pthread_cond_broadcast(&newRequest);
			break;
		}
		if (segnale == SIGUSR1) {
			PRINT("SERVER: Ricevuto segnale SIGUSR1, stampo statistiche")
			LOCK(&stat_lock)
			get_stat(users, &chattyStats);
			FILE *fd;
			if ((fd = fopen(stat_path, "a")) == NULL) {
				//fopen fail
				perror("OPEN FAIL");
			}
			else {
				printStats(fd, &chattyStats);
				fclose(fd);
			}
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
	strncpy(indirizzo.sun_path, conf_data->unix_path, strlen(conf_data->unix_path)+1);

	// faccio il bind con l'indirizzo estratto dal file di conf
	TRY(ret, bind(fd_sk, (struct sockaddr *)&indirizzo, sizeof(indirizzo)), -1, "bind", 0, 1)
	PRINT("SERVER: Indirizzo associato al socket")

	// metto socket in listen
	TRY(ret, listen(fd_sk, SOMAXCONN), -1, "trying to set the socket as listening", 0, 1)
	PRINT("SERVER: Socket in listening")

/*----------------INIZIALIZZO STRUTTURE----------------*/

	users = create_table(16, 8, 32, conf_data->max_history);

	fd_locks = malloc(13*sizeof(pthread_mutex_t));
	for (int i=0; i<13; i++) {
		memset(&(fd_locks[i]), 0, sizeof(pthread_mutex_t));
	}

/*----------------AVVIO I THREAD----------------*/
	//creo coda delle connessioni
	pending_requests = create_queue();

	// lancio i thread worker
	pthread_t *workers;
	TRY(workers, malloc(conf_data->th_in_pool*sizeof(pthread_t)), NULL, "main chatty malloc", 0, 1)
	for (int i=0, k=0; i<conf_data->th_in_pool; i++, k++) {
		CHECK((pthread_create(&workers[i], NULL, worker, &k) != 0), "Pthread create", 0, 1)
	}
	PRINT("SERVER: Thread worker avviati")

	//lancio il thread dispatcher
	pthread_t th_dispatcher;
	CHECK((pthread_create(&th_dispatcher, NULL, dispatcher, &fd_sk) != 0), "Pthread create", 0, 1)

	//lancio thread gestore dei segnali
	pthread_t th_signal_handler;
	CHECK((pthread_create(&th_signal_handler, NULL, signal_handler, conf_data->stat_path) != 0), "Pthread create", 0, 1)

/*----------------TERMINAZIONE DEI THREAD----------------*/
	pthread_join(th_signal_handler, NULL);
	pthread_cancel(th_dispatcher);
	for (int i=0; i>conf_data->th_in_pool; i++) {
		pthread_detach(workers[i]);
		pthread_join(workers[i], NULL);
		PRINT("SERVER: Thread worker terminato")
	}
	fprintf(stdout, "SERVER: Thread workers temrinati\n");
	free(workers);

/*----------------CANCELLAZIONE STRUTTURE DATI----------------*/
	destroy_table(users);
	delete_queue(pending_requests);
	free(fd_locks);
	fflush(stdout);
	return 0;
}
