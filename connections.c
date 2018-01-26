/*
 * chatterbox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


#include <connections.h>
#include <utility.h>

int openConnection(char* path, unsigned int ntimes, unsigned int secs) {
	if (!path) return -1;
	if (ntimes < 1) ntimes = MAX_RETRIES;
	if (secs < 1) secs = MAX_SLEEPING;

	int fd = 0;
	TRY(fd, socket(AF_UNIX, SOCK_STREAM, 0), -1, "client socket", -1, 0)
	struct sockaddr_un indirizzo;
	memset(&indirizzo, 0, sizeof(indirizzo));
	indirizzo.sun_family = AF_UNIX;
	strncpy(indirizzo.sun_path, path, UNIX_PATH_MAX);
	int ret;
	for (int i=0; i<ntimes; i++) {
		PRINT("CLIENT: Provo a connettermi")
		ret = connect(fd, (struct sockaddr *)&indirizzo, UNIX_PATH_MAX);
		if (ret != 0)
			sleep(secs);
		else {
			PRINT("CLIENT: Connessione riuscita")
			return fd;
		}
	}
	PRINT("CLIENT: Tutti i tentativi di connessione falliti")
	return -1;
}


// -------- server side ----- 
int readHeader(long connfd, message_hdr_t *hdr) {
	if (!hdr) {
		errno = EINVAL;
		return -1;
	}
	memset(hdr, 0, sizeof(message_hdr_t));
	ssize_t ret;
	int left = sizeof(message_hdr_t);
	char *buf = (char *)hdr;
	while(left > 0) {
		ret = read(connfd, buf, left);
		if (ret == 0)
			return 0;
		if (ret == -1)
			return -1;
		left -= ret;
		buf += ret;
	}
	return 1;
}


int readData(long fd, message_data_t *data) {
	if (!data) {
		errno = EINVAL;
		return -1;
	}
	memset(data, 0, sizeof(message_data_t));
	//effettuo una read per leggere l'header dei dati
	ssize_t ret;
	int left = sizeof(message_data_hdr_t);
	char *buf = (char *)data;
	while(left > 0) {
		ret = read(fd, buf, left);
		if (ret == 0)
			return 0;
		if (ret == -1)
			return -1;
		left -= ret;
		buf += ret;
	}
	//effettuo una read per leggere la parte dati
	left = data->hdr.len;
	TRY(data->buf, malloc(left*sizeof(char)), NULL, "malloc", -1, 0)
	buf = data->buf;
	while (left > 0) {
		ret = read(fd, buf, left);
		if (ret == 0)
			return 0;
		if (ret == -1)
			return -1;
		left -= ret;
		buf += ret;
	}
	return 1;
}



int readMsg(long fd, message_t *msg) {
	if (!msg) {
		errno = EINVAL;
		return -1;
	}
	int reth = -1, retd = -1;
	reth = readHeader(fd, &msg->hdr);
	retd = readData(fd, &msg->data);
	switch(reth+retd) {
		case 2: return 1; break;
		case 0: return 0; break;
		default: return -1; break;
	}
}



int sendRequest(long fd, message_t *msg) {
	if (!msg)
		return -1;
	int reth = -1, retd = -1;
	reth = sendHeader(fd, &msg->hdr);
	retd = sendData(fd, &msg->data);
	switch(reth+retd) {
		case 2: return 1; break;
		case 0: return 0; break;
		default: return -1; break;
	}
}

int sendHeader(long fd, message_hdr_t *hdr) {
	if (!hdr)
		return -1;
	ssize_t ret;
	int left = sizeof(message_hdr_t);
	char *buf = (char *)hdr;
	while (left > 0) {
		ret = write(fd, buf, left);
		if (ret == 0)
			return 0;
		if (ret == -1)
			return -1;
		left -= ret;
		buf += ret;
	}
	return 1;
}


int sendData(long fd, message_data_t *msg) {
	if (!msg)
		return -1;
	//scrivo header dati
	ssize_t ret;
	int left = sizeof(message_data_hdr_t);
	char *buf = (char *)msg;
	while(left > 0) {
		ret = write(fd, buf, left);
		if (ret == 0)
			return 0;
		if (ret == -1)
			return -1;
		left -= ret;
		buf += ret;
	}
	//scrivo buffer dati
	left = msg->hdr.len;
	buf = msg->buf;
	while (left > 0) {
		ret = write(fd, buf, left);
		if (ret == 0)
			return 0;
		if (ret == -1)
			return -1;
		left -= ret;
		buf += ret;
	}
	return 1;
}

