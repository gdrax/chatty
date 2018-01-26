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

int make_reply (message_t *reply, message_t *request, int fd, int id) {
	int ret = 0, out=-1;
	switch(request->hdr.op) {
		case REGISTER_OP: {
			LOCK(&fd_locks[fd%13]);
			char *buf;
			int n_users;
			ret = add_user(users, request->hdr.sender, fd);
			buf = users_list(users, &n_users);
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_ALREADY, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				setData(&(reply->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
				out = sendRequest(fd, reply);
			}
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
				fflush(stdout);
			free(buf);
			UNLOCK(&fd_locks[fd%13]);
		} break;

		case CONNECT_OP: {
			LOCK(&fd_locks[fd%13]);
			char *buf;
			int n_users;
			fprintf(stdout, "sono %s\n", request->hdr.sender);
			fflush(stdout);
			set_online(users, request->hdr.sender, fd);
				fprintf(stdout, "retschifo22222valwehwhwhehwoi\n");
			buf = users_list(users, &n_users);
				fprintf(stdout, "retschifo333332valwehwhwhehwoi\n");
			if (ret == 0){
				setHeader(&(reply->hdr), OP_OK, "");
				setData(&(reply->data), "", buf, n_users*(MAX_NAME_LENGTH+1));
				out = sendRequest(fd, reply);
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_NICK_UNKNOWN, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			free(buf);
			UNLOCK(&fd_locks[fd%13]);
		} break;

		case POSTTXT_OP: {
			LOCKALL(fd_locks, 13);
			int ret2;
			queue_t *fds = create_queue();
			ret = send_text(users, request->hdr.sender, request->data.hdr.receiver, request->data.buf, fds);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
				for (int i=0; i<fds->len; i++) {
					int rec_fd = take_ele(fds);
					fprintf(stdout, "Worker %d: Invio messaggio a fd %d\n", id, rec_fd);
					fflush(stdout);
					setHeader(&(reply->hdr), TXT_MESSAGE, request->hdr.sender);
					setData(&(reply->data), "", request->data.buf, strlen(request->data.buf));
					if( (ret2 = sendRequest(rec_fd, reply)) == 0) {
						set_offline(users, request->data.hdr.receiver);
						close(rec_fd);
						break;
					}
					fprintf(stdout, "Worker %d: Messaggio inviato a fd %d\n", id, rec_fd);
					fflush(stdout);
				}
			}
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_UNKNOWN, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			if (ret == 2) {
				setHeader(&(reply->hdr), OP_MSG_TOOLONG, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			delete_queue(fds);
			UNLOCKALL(fd_locks, 13);
		} break;

		case POSTTXTALL_OP: {
			LOCKALL(fd_locks, 13);
			int ret2;
			queue_t *fds = create_queue();
			ret = send_text_all(users, request->hdr.sender, request->data.buf, fds);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
				for (int i=0; i<fds->len; i++) {
					int rec_fd = take_ele(fds);
					fprintf(stdout, "Worker %d: Invio messaggio a fd %d\n", id, rec_fd);
					fflush(stdout);
					setHeader(&(reply->hdr), TXT_MESSAGE, request->hdr.sender);
					setData(&(reply->data), "", request->data.buf, strlen(request->data.buf));
					if( (ret2 = sendRequest(rec_fd, reply)) == 0) {
						close(rec_fd);
						break;
					}
				}
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_UNKNOWN, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			delete_queue(fds);
			UNLOCKALL(fd_locks, 13);
		} break;

		case POSTFILE_OP: {
			int ret2;
			LOCKALL(fd_locks, 13);
			message_data_t *filedata = malloc(sizeof(message_data_t));
			out = readData(fd, filedata);
			fprintf(stdout, "filelen 1 %d\n", filedata->hdr.len);
			fflush(stdout);
			queue_t *fds = create_queue();
			ret = send_file(users, request->hdr.sender, request->data.hdr.receiver, request->data.buf, filedata->buf, filedata->hdr.len, fds, conf_data->files_path, conf_data->max_file_size);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
				for (int i=0; i<fds->len; i++) {
					int rec_fd = take_ele(fds);
					fprintf(stdout, "Worker %d: Invio notifica file disponibile a fd %d\n", id, rec_fd);
					setHeader(&(reply->hdr), FILE_MESSAGE, request->hdr.sender);
					setData(&(reply->data), "", request->data.buf, strlen(request->data.buf));
					if( (ret2 = sendRequest(rec_fd, reply)) == 0) {
						set_offline(users, request->data.hdr.receiver);
						close(rec_fd);
						break;
					}
				}
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_UNKNOWN, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			if (ret == 2) {
				setHeader(&(reply->hdr), OP_MSG_TOOLONG, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			free(filedata);
			delete_queue(fds);
			UNLOCKALL(fd_locks, 13);
		} break;

		case GETFILE_OP: {
			LOCK(&fd_locks[fd%13]);
			char *filedata = NULL;
			int filelen;
			ret = get_file(users, request->hdr.sender, request->data.buf, filedata, &filelen, conf_data->files_path);
			fprintf(stdout, "filelen 2 %d\n", filelen);
			fflush(stdout);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				setData(&(reply->data), "", filedata, filelen);
				out = sendRequest(fd, reply);
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_UNKNOWN, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			free(filedata);
			UNLOCK(&fd_locks[fd%13]);
		} break;

		case GETPREVMSGS_OP: {
			LOCK(&fd_locks[fd%13]);
			int n_msgs=0, n_files=0, n=0;
			char *msgs=NULL, *files=NULL;
			ret = get_history(users, request->hdr.sender, &n_msgs, &n_files, &msgs, &files);
			n = n_msgs + n_files;
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				setData(&(reply->data), "server", (char *)&n, sizeof(n)); 
				out = sendRequest(fd, reply);
				for (int i=0; i<n_msgs; i++) {
					setHeader(&(reply->hdr), TXT_MESSAGE, "");
					setData(&(reply->data), "server", msgs+i*(MAX_MSG_LENGTH+1), MAX_MSG_LENGTH+1);
					if ( (out = sendRequest(fd, reply)) == 0) {
						break;
					}
				}
				for (int i=0 ; i<n_files; i++) {
					setHeader(&(reply->hdr), FILE_MESSAGE, "");
					setData(&(reply->data), "server", files+i*(MAX_MSG_LENGTH+1), MAX_MSG_LENGTH+1);
					if ( (out = sendRequest(fd, reply)) == 0) {
						break;
					}
				}
			}
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_UNKNOWN, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
				fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			}
			free(msgs);
			free(files);
			UNLOCK(&fd_locks[fd%13]);
			} break;

		case USRLIST_OP: {
			LOCK(&fd_locks[fd%13]);
			char *buf;
			int n_users;
			buf = users_list(users, &n_users);
			setHeader(&(reply->hdr), OP_OK, "");
			setData(&(reply->data), "server", buf, n_users*(MAX_NAME_LENGTH+1));
			out = sendRequest(fd, reply);
			free(buf);
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			UNLOCK(&fd_locks[fd%13]);
			} break;

		case UNREGISTER_OP: {
			LOCK(&fd_locks[fd%13]);
			ret = delete_user(users, request->hdr.sender);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			UNLOCK(&fd_locks[fd%13]);
			} break;

		case DISCONNECT_OP: {
			LOCK(&fd_locks[fd%13]);
			ret = set_offline(users, request->hdr.sender);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			UNLOCK(&fd_locks[fd%13]);
			} break;

		case CREATEGROUP_OP: {
			LOCK(&fd_locks[fd%13]);
			ret = add_group(users, request->hdr.sender, request->data.hdr.receiver);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
				}
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_ALREADY, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			UNLOCK(&fd_locks[fd%13]);
			} break;

		case ADDGROUP_OP: {
			LOCK(&fd_locks[fd%13]);
			ret = join_group(users, request->hdr.sender, request->data.hdr.receiver);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == 1) {
				setHeader(&(reply->hdr), OP_NICK_UNKNOWN, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			UNLOCK(&fd_locks[fd%13]);
			} break;

		case DELGROUP_OP: {
			LOCK(&fd_locks[fd%13]);
			ret = leave_group(users, request->hdr.sender, request->data.hdr.receiver);
			if (ret == 0) {
				setHeader(&(reply->hdr), OP_OK, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			if (ret == -1) {
				setHeader(&(reply->hdr), OP_FAIL, "");
				out = sendHeader(fd, &(reply->hdr));
			}
			fprintf(stdout, "WORKER %d: Risposta %d su fd %d\n", id, reply->hdr.op, fd);
			UNLOCK(&fd_locks[fd%13]);
			} break;

		default: {
			PRINT("SERVER: Rischiesta sconosciuta")
			} break;
	}
	return out;
}

void *worker(void *name) {
	int ret=0, id = *(int *)name;

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
		//prendo file desciptor dalla coda
		UNLOCK(&quit_lock)
		int fd = take_ele(pending_requests);
		UNLOCK(&queue_lock)
		message_t *reply = malloc(sizeof(message_t)), *request = malloc(sizeof(message_t));
		//leggo header della richiesta
//		LOCK(&fd_locks[fd%13]);
		ret = readMsg(fd, request);
		if(ret > 0) {
			//produco una risposta
			fprintf(stdout, "WORKER %d: Eseguo op %d su fd %d\n", id, request->hdr.op, fd);
			ret = make_reply(reply, request, fd, id);
			//se il client non ha finito, reinserisco il file descriptor nella coda
			if (ret > 0) {
				LOCK(&queue_lock);
				insert_ele(pending_requests, fd);
				UNLOCK(&queue_lock);
			}
			free(request->data.buf);
		}
		free(reply);
		//se la connessione è chiusa, disconnetto l'utente e chiudo il file descriptor
		if (ret == 0) {
			LOCK(&fd_locks[fd%13]);
			fprintf(stdout, "WORKER %d: connessione su fd %d chiusa\n", id, fd);
			set_offline(users, request->hdr.sender);
			close(fd);
			UNLOCK(&fd_locks[fd%13]);
		}
		free(request);
//		UNLOCK(&fd_locks[fd%13]);
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
