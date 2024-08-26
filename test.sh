#!/bin/bash

# Script valido solo sul terminale di default di ubuntu!

# Compilazione del progetto
make

# Controllo se la compilazione è avvenuta con successo
if [ $? -ne 0 ]; then
    echo "Compilazione fallita. Verifica gli errori e riprova."
    exit 1
fi

# Menu per la scelta del test
echo "Seleziona il test da eseguire:"
echo "1) Server con due client umani"
echo "2) Server con un client umano e uno in modalità bot"
echo "3) Server con due client bot"
echo "4) Server con con un client bot che richiede fork del server e client umano"
echo "5) Server con un client bot che richiede fork del server e client bot"
read -p "Inserisci il numero del test da eseguire (1-2-3-4-5): " choice

case $choice in
    1)
        # Test 1: Server con due client umani
        echo "Avvio server e due client umani..."
        gnome-terminal -- bash -c "cd '$PWD'; ./TriServer 30 X O; exec bash"
        sleep 2
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Player1; exec bash"
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Player2; exec bash"
        ;;
    2)
        # Test 2: Server con un client umano e uno in modalità bot
        echo "Avvio server, un client normale e uno in modalità bot..."
        gnome-terminal -- bash -c "cd '$PWD'; ./TriServer 30 X O; exec bash"
        sleep 2
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Player1; exec bash"
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Bot BOTMODE; exec bash"
        ;;
    3)
        # Test 3: Server con due client bot
        echo "Avvio server, e due client bot..."
        gnome-terminal -- bash -c "cd '$PWD'; ./TriServer 30 X O; exec bash"
        sleep 2
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Bot BOTMODE; exec bash"
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Bot BOTMODE; exec bash"
        ;;
   4)
        # Test 4: Server con un client bot che richiede fork del server e client umano
        echo "Avvio server, e client(fork) e client umano nello stesso terminale..."
        gnome-terminal -- bash -c "cd '$PWD'; ./TriServer 30 X O; exec bash"
        sleep 2
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Bot '*'; sleep 2; ./TriClient Player1; exec bash"
        ;;
    5)
        # Test 5: Server con un client bot che richiede fork del server e client bot
        echo "Avvio server, e client(fork) e client bot..."
        gnome-terminal -- bash -c "cd '$PWD'; ./TriServer 30 X O; exec bash"
        sleep 2
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Bot '*'; exec bash"
        gnome-terminal -- bash -c "cd '$PWD'; ./TriClient Bot BOTMODE; exec bash"
        ;;

    *)
        echo "Scelta non valida. Uscita dallo script."
        exit 1
        ;;
esac