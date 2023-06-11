#ifndef LEADERBOARD_H
#define LEADERBOARD_H

/* ════════════════════════════════════Librerie══════════════════════════════════════ */
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

/* ═════════════════════════════════════Struct═══════════════════════════════════════ */
typedef struct Player {
    char username[25];
    int num_answ,       //numero di domande sottoposte al giocatore
        spent_time,     //tempo impiegato dal giocatore per rispondere alle domande a lui sottoposte
        is_ready;       //lo stato del giocatore (ready se in gioco o in attesa di giocare)
    struct Player *next;
} Player;

typedef struct Leaderboard {
    Player *head;
    int size;                   //numero di giocatori registrati
    int num_ready;              //numero di giocatori pronti (coloro che appariranno nella stampa della classifica)
    pthread_mutex_t *mutex;     //mutex per garantire un accesso mutuamente esclusivo all'oggetto
} Leaderboard;

/* ═════════════════════════════════════Funzioni═════════════════════════════════════ */
Leaderboard *createLeaderboard();
int isAlreadyIn(Player* curr_player, char* username);
int addUserToLeaderboard(Leaderboard **leaderboard, char* username);
void sortLeaderboard(Player** head, int size);
Player* swapPlayer(Player* player1, Player* player2);
void removeUserFromLeaderboard(Leaderboard **leaderboard, char* username);
void isReady(Leaderboard *leaderboard, char *username);

#endif
