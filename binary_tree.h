/*
	\file binary_tree.h
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#ifndef binary_tree_h
#define binary_tree_h

#include <config.h>

//struttura dati nodo
typedef struct node {
	char *username;
	struct node *left;
	struct node *right;
} node_t;

//struttura dell'albero binario, contiene radice e dimensione (numero di nodi)
typedef struct binary_tree {
	node_t *root;
	int size;
} binary_tree_t;

/*
Crea e inizializza un nuovo albero

return:
- NULL se si è verificato un errore
- putatore all'albero creato
*/
binary_tree_t *createTree();

/*
Cancella un albero

param:
-tree: puntatore all'albero da cancellare
*/
void deleteTree(binary_tree_t *tree);

/*
Inserisce un nodo nell'albero binario

param:
-tree: albero in cui inserire il nodo
-username: stringa contenente il nome del nodo (utente)

return:
- 0 successo
- -1 fallimento
- 1 nick già presente
*/
int insertUser(binary_tree_t *tree, char *username);


/*
Elimina un nodo dall'abero

param:
-tree: albero da cui eliminare il nodo
-username: nome del nodo da eliminare

return:
- 0 successo
- -1 fallimento
- 1 utente non trovato
*/
int deleteUser(binary_tree_t *tree, char *username);


/*
Cerca il nodo corrispondere all'username dato

paran:
-tree: albero dove cercare il nodo
-username: nome da cercare

return:
- 0 successo
- -1 fallimento
- 1 username non presente
*/

int searchUser(binary_tree_t *tree, char *username);

/*
Stampa il contenuto dell'albero

param:
-root: radice dell'albero da stampare
*/

void printTree(node_t *root);

/*
Restituisce l'i-esimo elemento dell'albero in ordine alfabetico

param:
-tree: albero già inizializzato
-index: indice dell'elemento da prendere

retval:
- NULL: indice maggiore del numero di nodi o minore di 1
- stringa corrispondente all'elemento 
*/

char *getInOrder(binary_tree_t *tree, int index);

#endif
