#ifndef UTILITY_H
#define UTILITY_H

/* ════════════════════════════════════Librerie══════════════════════════════════════ */
#include "leaderboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ════════════════════════════════════Costanti══════════════════════════════════════ */
#define PENDING_CONNECTION_QUEUE_LENGHT 13
#define AVAILABLE_TIME 60
#define BUFFSIZE 4096
#define GAME_MALUS 85

/* ═════════════════════════════════════Struct═══════════════════════════════════════ */
typedef struct ThreadParameters{
  char client_username[25];     //lo username del client di cui sta gestendo le richieste
  int tid,                      //il thread id a cui è assegnato l'oggetto
      client_sd,                //il socket descriptore verso cui, il thread, inoltra i messaggi
      is_waiting,               //campo utilizzato per sincronizzare i thread
      is_asking;                //il numero della domanda che sta servendo al suo client
}ThreadParameters;

typedef struct NumberOfPlayers{
        int num_players,        //il numero di giocatori in partita
        updated_scores;         //campo utilizzato per sincronizzare i thread
    pthread_mutex_t* mutex;     //mutex per garantire un accesso mutuamente esclusivo all'oggetto
    pthread_cond_t* all_scores_are_updated;
}NumberOfPlayers;

/* ═════════════════════════════════════Funzioni═════════════════════════════════════ */
void setPlayerScore(Leaderboard* leaderboard, char* nickname, int spent_time);
int receiveInt(int fd, int* num);
int sendInt(int fd, int num);
int receiveString(int sd, char** string, int str_len);
int sendString(int fd, char* str, int str_length);
void sendLeaderboard(Leaderboard* leaderboard, int socket_addr);
NumberOfPlayers* initNumberOfPlayersStruct();

#endif