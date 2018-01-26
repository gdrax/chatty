#include <utility.h>
#include <users_table.h>
#include <string_list.c>
#include <msg_list.h>
#include <stdlib.h>
#include "icl_hash.h"
#include "queue.h"

int main() {
/*			CREAZIONE TABELLA			*/
	users_table_t *table = create_table(8, 2, 32, 10);
	char *s1 = "pippo", *s2 = "pluto", *s3 = "biagio", *s4 = "DD", *s5 = "bens";
	
	char *g1 = "gruppo 1", *g2 = "gruppo 2", *g3 = "gruppo 3", *g4 = "gruppo 4", *g5 = "gruppo 5";
	
	char *t1 = "text 1", *t2 = "text 2", *t3 = "text 3", *t4 = "text 4", *t5 = "text 5";

	int ret;

	queue_t *queue = create_queue();
/*			INSERIMENTO USER			*/
	ret = add_user(table, s1, 0);
	fprintf(stdout, "Aggiunta user: %d\n", ret);

	ret = add_user(table, s2, 0);
	fprintf(stdout, "Aggiunta user: %d\n", ret);

	int n;
	char *listone = users_list(table,&n);
	fprintf(stdout, "User list: %s\n", listone);
	free(listone);

	ret = add_user(table, s3, 0);
	fprintf(stdout, "Aggiunta user: %d\n", ret);

	ret = add_user(table, s4, 0);
	fprintf(stdout, "Aggiunta user: %d\n", ret);

	ret = add_user(table, s5, 0);
	fprintf(stdout, "Aggiunta user: %d\n", ret);

	ret = add_user(table, s2, 0);
	fprintf(stdout, "Aggiunta user già presente: %d\n", ret);

/*			RIMOZIONE USER			*/
	ret = delete_user(table, s3);
	fprintf(stdout, "Rimozione user: %d\n", ret);

	ret = delete_user(table, "mhanz");
	fprintf(stdout, "Rimozione user non presente: %d\n", ret);

/*			INVIO MESSAGGI PRIVATI			*/
	ret = send_text(table, s2, s1, t1, queue);
	fprintf(stdout, "Invio testo s2-s1: %d\n", ret);

	ret = send_text(table, s2, s3, t1, queue);
	fprintf(stdout, "Invio testo senza destinatario s2-s3: %d\n", ret);

	ret = send_text(table, s3, s5, t2, queue);
	fprintf(stdout, "Invio testo senza mittente s3-s5: %d\n", ret);

	ret = send_text(table, s2, s4, t1, queue);
	fprintf(stdout, "Invio testo s2-s4: %d\n", ret);

	ret = send_text(table, s4, s2, t1, queue);
	fprintf(stdout, "Invio testo s4-s2: %d\n", ret);

	for (int i=0; i<6; i++) {
		ret = send_text(table, s1, s4, t5, queue);
		fprintf(stdout, "Invio testo s1-s4: %d\n", ret);
	}

/*			INVIO MESSAGGI BROADCAST			*/
	for (int i=0; i<5; i++) {
		ret = send_text_all(table, s1, t3, queue);
		fprintf(stdout, "Invio testo s1-all %d\n", ret);
	}

/*			CREAZIONE GRUPPI			*/
	ret = add_group(table, s1, g1);
	fprintf(stdout, "Creazione gruppo s1-g1: %d\n", ret);

	ret = add_group(table, s5, g2);
	fprintf(stdout, "Creazione gruppo s5-g2: %d\n", ret);

	ret = add_group(table, s1, g1);
	fprintf(stdout, "Creazione gruppo già esistente s1-g1: %d\n", ret);

	ret = add_group(table, s3, g4);
	fprintf(stdout, "Creazione gruppo senza owner s3-g4: %d\n", ret);

/*			REGISTRAZIONE USER A GRUPPI			*/
	ret = join_group(table, s1, g1);
	fprintf(stdout, "Aggiunta membro già presente a gruppo s1-g1: %d\n", ret);

	ret = join_group(table, s3, g1);
	fprintf(stdout, "Aggiutna membro inesistente a gruppo s3-g1: %d\n", ret);

	ret = join_group(table, s1, g5);
	fprintf(stdout, "Aggiunta membro a gruppo inesistente s1-g5: %d\n", ret);

	ret = join_group(table, s4, g1);
	fprintf(stdout, "Aggiunta membro a gruppo s4-g1: %d\n", ret);

	ret = join_group(table, s2, g1);
	fprintf(stdout, "Aggiunta membro a gruppo s2-g5: %d\n", ret);

	ret = join_group(table, s5, g5);
	fprintf(stdout, "Aggiunta membro a gruppo inesistente s5-g5: %d\n", ret);

	ret = join_group(table, s2, g2);
	fprintf(stdout, "Aggiunta membro a gruppo s2-g2: %d\n", ret);

	ret = join_group(table, s1, g2);
	fprintf(stdout, "Aggiunta membro a gruppo s1-g2: %d\n", ret);

	ret = join_group(table, s4, g2);
	fprintf(stdout, "Aggiunta membro a gruppo s4-g2: %d\n", ret);

/*			DEREGISTRAZIONE USER DA GRUPPI			*/
	ret = leave_group(table, s1, g5);
	fprintf(stdout, "Rimozione membro da gruppo inesistente s1-g5: %d\n", ret);

	ret = leave_group(table, s1, g5);
	fprintf(stdout, "Rimozione membro inesistente da gruppo s3-g1: %d\n", ret);

	ret = leave_group(table, s1, g5);
	fprintf(stdout, "Rimozione membro non presente nel gruppo s4-g2: %d\n", ret);

	ret = leave_group(table, s3, g4);
	fprintf(stdout, "Rimozione membro da gruppo s3-g4: %d\n", ret);

/*			ONLINE / OFFLINE			*/
	ret = set_online(table, "mhanz", 4);
	fprintf(stdout, "Set online user non prensete: %d\n", ret);

	ret = set_online(table, s1, 3);
	fprintf(stdout, "Set online user s1: %d\n", ret);

	ret = set_offline(table, s1);
	fprintf(stdout, "Set offline user s1: %d\n", ret);

	ret = set_offline(table, "mhanz");
	fprintf(stdout, "Set offline user non presente: %d\n", ret);


/*			INVIO MESSAGGIO BROADCAST A GRUPPO			*/
	ret = send_text(table, s1, g2, t4, queue);
	fprintf(stdout, "Invio messaggio broadcast a gruppo s1-g2: %d\n", ret);

/*			DISTRUZIONE GRUPPO			*/

	ret = remove_group(table, g2);
	fprintf(stdout, "Rimozione gruppo g2: %d\n", ret);


/*			STAMPE VARIE			*/
	fprintf(stdout, "User iscritti = %d\n", table->users->nentries);
	fprintf(stdout, "Gruppi iscritti = %d\n", table->groups->nentries);


	int i;
	char *key;
	icl_entry_t *tmp;
	chat_user_t *user;
	icl_hash_foreach(table->users, i, tmp, key, user) {
		fprintf(stdout, "USER-------->%s\n", user->username);
	}

	icl_hash_foreach(table->users, i, tmp, key, user) {
		fprintf(stdout, "\n	%s	\n", user->username);
		printMsgList(user->msgs);
	}

/*			DISTRUZIONE TABELLA			*/
	ret = destroy_table(table);
	fprintf(stdout, "Distruzione della tabella %d\n", ret);

	
/*	queue_t *queue = create_queue();*/

/*	insert_ele(queue, "sargio");*/
/*	insert_ele(queue, "edo");*/
/*	char *s = take_ele(queue);*/
/*	s = take_ele(queue);*/
	delete_queue(queue);
	return 0;
}
