# tic-tac-toe-System-call 
# Gioco del Tris Multiplayer

Questo è un semplice gioco del Tris multiplayer implementato in C, in cui due giocatori possono connettersi e giocare l'uno contro l'altro utilizzando memoria condivisa e semafori per la sincronizzazione.

## Caratteristiche

- **Multiplayer**: Due giocatori possono connettersi al server e giocare a turni.
- **Memoria Condivisa**: Lo stato del gioco viene mantenuto in memoria condivisa, permettendo a entrambi i giocatori di accedere e modificare la griglia di gioco.
- **Sincronizzazione con Semafori**: I semafori sono utilizzati per garantire che i giocatori prendano i turni correttamente.

## Struttura del Progetto

- **`TrisServer.c`**: Il codice del server che gestisce il gioco, le connessioni dei giocatori e coordina i turni di gioco.
- **`TrisClient.c`**: Il codice del client che i giocatori usano per connettersi al server, fare mosse e visualizzare lo stato del gioco.
- **`Makefile`**: Un makefile per semplificare il processo di compilazione.



