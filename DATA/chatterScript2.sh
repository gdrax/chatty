#! /bin/bash

##### Commentare i remove dei file tmp per debug

if [ $# == 0 ]; then
	echo "Usage: chatterScript [FILE] [MINUTES]"
	exit 1
elif [ $# == 1 ]; then
	echo "chatterScipt: Too few arguments"
	exit 1
elif ! [ -f $1 ]; then
	echo "chatterScipt: Bad file"
	exit 1
elif [ $2 -lt 0 ]; then
	echo "chatterScipt: TIME cannot be negative"
	exit 1
else

	word=DirName

#seleziono le linee che contengono DirName, =, /
	directory=$(grep "^[^#]" $1 | grep $word | tr -d " "| tr -d "	" | cut -f 2 -s -d "=")


#	echo $directory

#se TIME = 0 stampo il file listing
#se TIME > 0 salvo tutti i nomi di file e directory piu vecchie di TIME minuti e le rimuovo
	if ! [ -d $directory ]; then
		echo "chatterScipt: Not a directory"
	elif [ -z $directory ]; then
		echo "chatterScipt: Directory not found"
	fi

	cd $directory;

	if [ $2 == 0 ] ; then
		ls
		exit 0
	else
		exec 4>tmp.txt
		find  -mmin +$2 >&4
		exec 4<tmp.txt
#controllo che il file che sto per rimuovere non sia proprio $target
		while read -u 4 linea; do
			if [[ $linea != $directory ]] ; then
				rm -r $linea
			fi
		done
		rm tmp.txt
		echo "chatterScipt: Done."
		exit 0
	fi
fi
#FUNZIONANTE
