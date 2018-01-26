#
# chatterbox Progetto del corso di LSO 2017 
# 
# Dipartimento di Informatica Università di Pisa
# Docenti: Prencipe, Torquati
#
#

##########################################################
# IMPORTANTE: completare la lista dei file da consegnare
# 
FILE_DA_CONSEGNARE=Makefile chatty.c message.h ops.h stats.h config.h \
		   DATA/chatty.conf1 DATA/chatty.conf2 connections.h 
# inserire il nome del tarball: es. NinoBixio
TARNAME=NomeCognome
# inserice il corso di appartenenza: CorsoA oppure CorsoB
CORSO=CorsoX
#
###########################################################

###################################################################
# NOTA: Il nome riportato in UNIX_PATH deve corrispondere al nome 
#       usato per l'opzione UnixPath nel file di configurazione del 
#       server (vedere i file nella directory DATA).
#       Lo stesso vale per il nome riportato in STAT_PATH e DIR_PATH
#       che deveno corrispondere con l'opzione StatFileName e 
#       DirName, rispettivamente.
#
# ATTENZIONE: se il codice viene sviluppato sulle macchine del 
#             laboratorio utilizzare come nomi, nomi unici, 
#             ad esempo /tmp/chatty_sock_<numero-di-matricola> e
#             /tmp/chatty_stats_<numero-di-matricola>.
#
###################################################################
UNIX_PATH       = /tmp/chatty_socket
STAT_PATH       = /tmp/chatty_stats.txt
DIR_PATH        = /tmp/chatty

CC		=  gcc
AR              =  ar
CFLAGS	        += -std=c99 -Wall -pedantic -g -DMAKE_VALGRIND_HAPPY
ARFLAGS         =  rvs
INCLUDES	= -I.
LDFLAGS 	= -L.
OPTFLAGS	= #-O3 
LIBS            = -pthread

# aggiungere qui altri targets se necessario
TARGETS		= chatty        \
		  client	\
		  test_users


# aggiungere qui i file oggetto da compilare
OBJECTS		= chat_parser.o	\
		  connections.o	\
		  queue.o	\
		  msg_list.o	\
		  string_list.o	\
		  users_table.o	\
		  icl_hash.o	\
		  listener.o


# aggiungere qui gli altri include 
INCLUDE_FILES   = connections.h \
		  message.h     \
		  ops.h	  	\
		  stats.h       \
		  config.h	\
		  utility.h	\
		  chat_parser.h	\
		  queue.h	\
		  msg_list.h	\
		  string_list.h	\
		  users_table.h	\
		  icl_hash.h



.PHONY: all clean cleanall test1 test2 test3 test4 test5 test6 consegna
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) 

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all		: $(TARGETS)


chatty: chatty.o libchatty.a $(INCLUDE_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

client: client.o connections.o message.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

test_users: test_users.c libchatty.a $(INCLUDE_FILES)
	$(CC) $(FLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

############################ non modificare da qui in poi

libchatty.a: $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

clean		: 
	rm -f $(TARGETS)

cleanall	: clean
	\rm -f *.o *~ libchatty.a valgrind_out $(STAT_PATH) $(UNIX_PATH)
	\rm -fr  $(DIR_PATH)

killchatty:
	killall -9 chatty

# da cancellare
start:
	gedit chatty.c connections.c ops.h client.c message.h utility.h users_table.c Makefile &

valgrind:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	valgrind --leak-check=full -v -q ./chatty -f DATA/chatty.conf1&
	./client -l $(UNIX_PATH) -c pippo
	./client -l $(UNIX_PATH) -c pluto
	./client -l $(UNIX_PATH) -c minni
	./client -l $(UNIX_PATH) -k pippo -S "Ciao pluto":pluto -S "come stai?":pluto
	./client -l $(UNIX_PATH) -k pluto -p -S "Ciao pippo":pippo -S "bene e tu?":pippo -S "Ciao minni come stai?":minni
	./client -l $(UNIX_PATH) -k pippo -p
	./client -l $(UNIX_PATH) -k pluto -p
	./client -l $(UNIX_PATH) -k minni -p
#	killall -QUIT -w chatty
	@echo "********** Test1 superato!"

my_test_user:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
#	./test_users
	valgrind --leak-check=full -v -q ./test_users

# test base
test10: 
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./NEclient&
	./chatty -f DATA/chatty.conf1
	@echo "********* Test10 superato!"

test00: 
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./chatty -f DATA/chatty.conf1&
	./client -l $(UNIX_PATH) -c sargione4
	./client -l $(UNIX_PATH) -c sargione5
	./client -l $(UNIX_PATH) -k sargione4 -S "Ciao sargione5":sargione5
	./client -l $(UNIX_PATH) -k sargione5 -S "Ciao":sargione4 -S "Ciao ":sargione4


test0: 
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	valgrind --leak-check=full -v -q ./chatty -f DATA/chatty.conf1&
	./client -l $(UNIX_PATH) -c sargione
	sleep 5
	killall -QUIT -w chatty

test6mini:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	valgrind --leak-check=full -v -q ./chatty -f DATA/chatty.conf1&
	./client -l $(UNIX_PATH) -c pippo &
	./client -l $(UNIX_PATH) -c pluto &
	./client -l $(UNIX_PATH) -c minni &
	./client -l $(UNIX_PATH) -c topolino &
	./client -l $(UNIX_PATH) -c paperino &
	./client -l $(UNIX_PATH) -c qui &
	./client -l $(UNIX_PATH) -k pippo -g gruppo1
	./client -l $(UNIX_PATH) -k pluto -g gruppo2
	./client -l $(UNIX_PATH) -k minni -a gruppo1
	./client -l $(UNIX_PATH) -k topolino -a gruppo1
	./client -l $(UNIX_PATH) -k paperino -a gruppo2
	./client -l $(UNIX_PATH) -k minni -a gruppo2
	./client -l $(UNIX_PATH)  -k pippo -S "Ciao a tutti da pippo":gruppo1 -R 1
#	./client -l $(UNIX_PATH)  -k minni -S "Ciao a tutti da minni":gruppo1 -R 1 -S "sargio":topolino

test20:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./chatty -f DATA/chatty.conf1&
	./testfilemini.sh $(UNIX_PATH) $(DIR_PATH)
	killall -QUIT -w chatty
	@echo "********** Test2 superato!"

test21:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	valgrind --leak-check=full -v -q ./chatty -f DATA/chatty.conf1&
	./client -l $(UNIX_PATH) -c pippo
	./client -l $(UNIX_PATH) -c pluto
	./client -l $(UNIX_PATH) -c minni
	./client -l $(UNIX_PATH) -k pluto -R 2&
	./client -l $(UNIX_PATH) -k pippo -S "Ti mando un file":pluto -s ./client:pluto



test1: 
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./chatty -f DATA/chatty.conf1&
	./client -l $(UNIX_PATH) -c pippo
	./client -l $(UNIX_PATH) -c pluto
	./client -l $(UNIX_PATH) -c minni
	./client -l $(UNIX_PATH) -k pippo -S "Ciao pluto":pluto -S "come stai?":pluto
	./client -l $(UNIX_PATH) -k pluto -p -S "Ciao pippo":pippo -S "bene e tu?":pippo -S "Ciao minni come stai?":minni
	./client -l $(UNIX_PATH) -k pippo -p
	./client -l $(UNIX_PATH) -k pluto -p
	./client -l $(UNIX_PATH) -k minni -p
#	killall -QUIT -w chatty
	@echo "********** Test1 superato!"

# test scambio file 
test2:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./chatty -f DATA/chatty.conf1&
	./testfile.sh $(UNIX_PATH) $(DIR_PATH)
	killall -QUIT -w chatty
	@echo "********** Test2 superato!"

# test parametri di configurazione e statistiche
test3:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./chatty -f DATA/chatty.conf2&
	./testconf.sh $(UNIX_PATH) $(STAT_PATH)
	killall -QUIT -w chatty
	@echo "********** Test3 superato!"


# verifica di memory leaks 
test4:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./testleaks.sh $(UNIX_PATH)
	@echo "********** Test4 superato!"

# stress test
test5:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./chatty -f DATA/chatty.conf1&
	./teststress.sh $(UNIX_PATH)
	killall -QUIT -w chatty
	@echo "********** Test5 superato!"

# gestione gruppi
test6:
	make cleanall
	\mkdir -p $(DIR_PATH)
	make all
	./chatty -f DATA/chatty.conf1&
	./testgroups.sh $(UNIX_PATH)
	killall -QUIT -w chatty
	@echo "********** Test6 superato!"



# target per la consegna
consegna:
	make test1
	sleep 3
	make test2
	sleep 3
	make test3
	sleep 3
	make test4
	sleep 3
	make test5
	sleep 3
	make test6
	sleep 3
	tar -cvf $(TARNAME)_$(CORSO)_chatty.tar $(FILE_DA_CONSEGNARE) 
	@echo "*** TAR PRONTO $(TARNAME)_$(CORSO)_chatty.tar "
	@echo "Per la consegna seguire le istruzioni specificate nella pagina del progetto:"
	@echo " http://didawiki.di.unipi.it/doku.php/informatica/sol/laboratorio17/progetto"
	@echo 
