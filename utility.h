/*
	\file utility.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

/*
Se il comando chiamato 'cmd' restituisce 'check' viene stampato il messaggio 'msg' con errno,
se 'exit' non è 0 termina il programma, altrimenti ritorna il valore 'out'
*/

#define TRY(ret, cmd, check, msg, out, quit)							\
		if ((ret = cmd) == check) { 						\
			fprintf(stderr, "File: %s, line: %d\n", __FILE__, __LINE__);	\
			perror(msg);							\
			fflush(stderr);							\
			if (quit)							\
				exit(EXIT_FAILURE);					\
			else								\
				return out;						\
		}

#define CHECK(condiz, msg, out, quit)							\
		if (condiz) {								\
			fprintf(stderr, "File: %s, line: %d\n", __FILE__, __LINE__);	\
			perror(msg);							\
			if (quit)							\
				exit(EXIT_FAILURE);					\
			else								\
				return out;						\
		}

/*
Stampa un messaggio sullo standard out con nome del file e numero di riga
*/

#define PRINT(msg)									\
		fflush(stdout);							\
		fflush(stderr);							\
		fprintf(stdout, "File: %s, line: %d ---> %s\n", __FILE__, __LINE__, msg);

/*sincronizzazione*/
#define LOCK(lock)									\
		if (pthread_mutex_lock(lock) != 0) {					\
			fprintf(stderr, "File: %s, line: %d\n", __FILE__, __LINE__);	\
			perror("Pthread lock");						\
			exit(EXIT_FAILURE);						\
		}

#define UNLOCK(lock)									\
		if (pthread_mutex_unlock(lock) != 0) {					\
			fprintf(stderr, "File: %s, line: %d\n", __FILE__, __LINE__);	\
			perror("Pthread unlock");						\
			exit(EXIT_FAILURE);						\
		}


#endif
