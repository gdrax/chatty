/*
	\file string_list.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef string_list_h
#define string_list_h


/*
Struttura di un elemento della lista
*/
typedef struct s_ele {
	char *data;
	int fd;
	struct s_ele *next;
} s_ele_t;

/*
Struttura della lista
*/
typedef struct string_list {
	int size;
	s_ele_t *head;
} string_list_t;


/*
Crea una nuova lista

param:

retval:
puntatore alla lista appena creata
*/
string_list_t *createSList();

/*
Distrugge una lista

param:
list - lista da distruggere
*/
void deleteSList(string_list_t *list);

/*
Aggiunge una stringa alla lista

param:
list - lista già inizializzata
string - string da aggiungere
fd - file descriptor assocoiato alla stringa

retval:
0 - stringa aggiunta
1 - stringa già presente
-1 - errore

*/
int addString(string_list_t *list, char *string, int fd);

/*
Elimina una stringa dalla lista

param:
list - lista già inizializzata
string - stringa da eliminare dalla lista

retval:
0 - stringa rimossa
-1 - errore
*/
int removeString(string_list_t *list, char *string);

/*
Cerca una stringa nella lista

param:
list: lista già inizializzata
string - stringa da cercare

retval:
1 - stringa trovata
0 - stringa non presente
*/
int searchString(string_list_t *list, char *string);

/*
Restituisce gli elementi per posizione

param:
list - lista già inizializzata
index - indice dell'elemento desiderato
string - puntatore a memoria già allocata (se non viene restituita la lista la memoria viene liberata)

retval:
-1 - errore
0 - successo
*/
int getByIndex(string_list_t *list, int index, char *string);

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
int update_fd(string_list_t *list, char *string, int fd);

/*
Rimuove la stringa associata ad un descrittore e la restituisce

param:
list - lista già inizializzata
fd - descrittore da rimuovere

retval:
NULL: errore
stringa associata al descrittore: successo
*/
char * disconnect_fd(string_list_t *list, int fd);

#endif
