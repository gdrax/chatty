/*
	\file binary_tree.c
	\author Bertoncini Gioele 
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
*/

#include<binary_tree.h>
#include<utility.h>
#include<string.h>
#include<stdlib.h>


/*			Funzioni ausiliarie			*/

/*	OKOK	*/
/*Funzione che libera tutti i nodi collegati a una radice, usata in deleteTree*/
void deleteNodes(node_t *n) {
	if (n->left == NULL && n->right == NULL) free(n);
	if (n->left == NULL && n->right != NULL) deleteNodes(n->right);
	if (n->left != NULL && n->right == NULL) deleteNodes(n->left);
	if (n->left != NULL && n->right != NULL) {
		deleteNodes(n->right);
		deleteNodes(n->left);
	}
}

/*	OKOK	*/
/*Funzione per inserire un nuovo nodo, usata da insertUser*/
node_t *insertNode(node_t *node, char *username, int *retval) {
	if (node == NULL) {
		node_t *tmp = (node_t *)malloc(sizeof(node_t));
		strncpy(tmp->username, username, strlen(username)+1);
		tmp->left = NULL;
		tmp->right = NULL;
		*retval = 0;
		return tmp;
	}
	else {
		if (strcmp(node->username, username) == 0) {
			*retval = 2;
		}
		if (strcmp(node->username, username) < 0) {
			node->right = insertNode(node->right, username, retval);
		}
		if (strcmp(node->username, username) > 0) {
			node->left = insertNode(node->left, username, retval);
		}
		return node;
	}

}

/*	OKOK	*/
/*Funzione che ricerca un nodo a partire dalla radice, usata in searchUser*/
node_t *searchNode(node_t *node, char *username, int *retval) {
	if (node == NULL) {
		*retval = 1;
		return NULL;
	}
	else {
		if (strcmp(node->username, username) == 0) {
		*retval = 0;
		return node;
		}
		if (strcmp(node->username, username) < 0)
			return searchNode(node->right, username, retval);
		if (strcmp(node->username, username) > 0)
			return searchNode(node->left, username, retval);
	}
	return NULL;
}

/*	OKOK	*/
/*Funzione che trova il minimo in albero binario di ricerca, usata in deleteNode*/
node_t *findTreeMin(node_t *node) {
	if (node == NULL) return NULL;
	if (node->left) return findTreeMin(node->left);
	else return node;
}


/*	OKOK	*/
/*Funzione che cancella un nodo, partendo dalla radice dell'albero, usata in deleteUser*/
node_t *deleteNode(node_t *node, char *username, int *retval) {
	node_t *tmp;
	//username da cancellare non trovato
	if (node == NULL) *retval = 1;
	//raggiungo il nodo da cancellare
	else if (strcmp(username, node->username) < 0) node->left = deleteNode(node->left, username, retval);
	else if (strcmp(username, node->username) > 0) node->right = deleteNode(node->right, username, retval);
	else {
		if (node->right && node->left) {
			//trovo il nodo che rimpiazzerà quello cancellato
			tmp = findTreeMin(node->right);
			strncpy(tmp->username, node->username, strlen(tmp->username));
			//cancello il nodo che ho spostato
			node->right = deleteNode(node->right, tmp->username, retval);
		}
		else {
			tmp = node;
			if (node->left == NULL)
				node = node->right;
			else if (node->right == NULL)
				node = node->left;
			free(tmp);
		}
		retval = 0;
	}
	return node;
}

/*			Funzioni di interfaccia			*/

/*		*/
binary_tree_t *createTree(int max_msg_length) {
	binary_tree_t *t = malloc(sizeof(binary_tree_t));
	if (!t) return NULL;
	t->root = NULL;
	t->size = 0;
	return t;
}

/*	OKOK	*/
void deleteTree(binary_tree_t *tree) {
	if (tree->root == NULL)
		free(tree);
	else
		deleteNodes(tree->root);
}

/*	OKOK	*/
int insertUser(binary_tree_t *tree, char *username) {
	if (tree == NULL)
		return -1;
	int retval = -1;
	tree->root = insertNode(tree->root, username, &retval);
	if (retval == 0) {
		tree->size++;
		return retval;
	}
	if (retval == 1) return 1;
	return -1;
}

/*	OKOK	*/
int deleteUser(binary_tree_t *tree, char *username) {
	int retval = -1;
	tree->root = deleteNode(tree->root, username, &retval);
	return retval;
}

/*	OK	*/
int searchUser(binary_tree_t *tree, char *username) {
	if (tree == NULL)
		return -1;
	int retval = -1;
	node_t *found = malloc(sizeof(node_t));
	found = searchNode(tree->root, username, &retval);
	if (!found) return -1;
	return retval;
}


/*	OKOK	*/
void printTree(node_t *root) {
	if (root == NULL)
		return;
	if (root->left != NULL)
		printTree(root->left);
	fprintf(stdout, "%s\n", root->username);
	if (root->right !=NULL)
		printTree(root->right);
}

char *getInOrder(binary_tree_t *tree, int index) {
	if (!tree)
		return NULL;
	if (tree->root->size < index || size < 1)
		return NULL;




