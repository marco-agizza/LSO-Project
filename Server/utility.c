#include "utility.h"

/**
 * Attribuisce il "punteggio" al giocatore identificato da @param nickname 
 * 
 * @param leaderboard: la classifica di gioco
 * @param nickname: lo username del giocatore a cui si vuole attribuire il "punteggio"
 * @param spent_time: indica i secondi impiegati dal giocatore per fornire la risposta:
 *      - è pari al temppo impigato dal giocatore per rispondere alla domanda
 *      - è pari ad 85 se il tempo è scaduto oppure la risposta data è risultata sbagliata
 *      - è pari a -1 se il giocatore ha scelto di rigiocare e quindi va resettato il suo score in classifica
 */
void setPlayerScore(Leaderboard* leaderboard, char* nickname, int spent_time) {
    pthread_mutex_lock(leaderboard->mutex);
    Player* current_player = leaderboard->head;
    while(current_player) {
        if(strcmp(current_player->username, nickname) == 0) {
            if(spent_time!=-1){
                current_player->spent_time+=spent_time;
                current_player->num_answ++;
            }else{
                current_player->spent_time=0;
                current_player->num_answ=0;
                current_player->is_ready=0;
                leaderboard->num_ready--;
            }
            pthread_mutex_unlock(leaderboard->mutex);
            return;
        }
        current_player = current_player->next;
    }
    pthread_mutex_unlock(leaderboard->mutex);
}

/**
 * Lettura di un intero dal socket
 * 
 * @param sd socket descriptor da cui leggere l'intero
 * @param num puntatore a intero su cui verrà salvato il valore intero letto
 * @return 0 se ha successo, -1 altrimenti
*/
int receiveInt(int sd, int *num){
    int32_t ret;
    char* data = (char*) &ret;
    int bytes_left = sizeof(ret);
    int bytes_read;
    
    while (bytes_left > 0) {
        if((bytes_read = read(sd, data, bytes_left))<0)
            return -1;
        data += bytes_read;
        bytes_left -= bytes_read;
    }
    *num = ntohl(ret);
    return 0;
}

/**
 * Scrive un intero sul socket
 * 
 * @param sd il socket descriptor su cui scrivere l'intero
 * @param num il valore numerico che si intende scrivere sul socket
 * @return 0 se ha successo, -1 altrimenti
 */
int sendInt(int sd, int num) {
    int32_t conv = htonl(num);
    char* data = (char*) &conv;
    int bytes_left = sizeof(conv);
    int bytes_written;

    while (bytes_left > 0){
        if ((bytes_written = write(sd, data, bytes_left)) < 0)
            return -1;
        data += bytes_written;
        bytes_left -= bytes_written;
    } 
    return 0;
}

/**
 * Scrive una stringa sul socket 
 * 
 * @param sd il socket descriptor su cui scrivere la stringa
 * @param string il doppio puntatore a stringa in verrà salvara la stringa lett
 * @param str_len la lunghezza della stringa da leggere
*/
int receiveString(int sd, char** string, int str_len){
    *string = calloc(str_len, sizeof(char));
    int bytes_left = str_len;
    int received_bytes;

    do {
        received_bytes = read(sd, *string, bytes_left);
        if(received_bytes < 0)
            return -1;
        bytes_left -= received_bytes;
        string+=received_bytes;
    } while(bytes_left > 0);
    string-=received_bytes;
    
    return 0;
}

/**
 * Scrive una stringa sul socket
 * 
 * @param sd il file descriptor del socket su cui scrivere
 * @param string la stringa che si intende scrivere sul socket
 * @param str_length la lunghezza della stringa
 * @return 0 se ha successo, -1 altrimenti
 */
int sendString(int sd, char* string, int str_length) {
    int bytes_left = str_length;
    int bytes_written;

    while(bytes_left > 0){        
        if ((bytes_written = write(sd, string, bytes_left)) < 0)
            return -1;
        string += bytes_written;
        bytes_left -= bytes_written;
    }
    return 0;
}

/**
 * Invia la classifica @param leaderboard al cliente @param socket_addr
 * 
 * @param leaderboard: identifica la classifica da stampare
 * @param socket_addr: indirizzo del socket su cui comunicare la classifica
 * */
void sendLeaderboard(Leaderboard* leaderboard, int socket_addr) {
    pthread_mutex_lock(leaderboard->mutex);
    sendInt(socket_addr, leaderboard->num_ready);
    Player* current_player = leaderboard->head;
    while(current_player != NULL) {
        if(current_player->is_ready!=0){
            sendInt(socket_addr, (int)strlen(current_player->username));
            sendString(socket_addr, current_player->username, (int) strlen(current_player->username));
            if(current_player->num_answ!=0){
                float avg_spent_time=current_player->spent_time/current_player->num_answ;
                //avg_spent_time avrà al più due cifre prima e due dopo la virgola
                char str_avg_spent_time[5];
                gcvt(avg_spent_time, 4, str_avg_spent_time);
                sendInt(socket_addr, 5);
                sendString(socket_addr, str_avg_spent_time, 5);
            }else{
                sendInt(socket_addr, 3);
                sendString(socket_addr, "0.0", 3);
            }
        }
        current_player = current_player->next;
    }
    pthread_mutex_unlock(leaderboard->mutex);
}

/**
 * Inizializza la struct che contiene i campi numPlayers e mutex
 * 
 * @return la struttura inizializzata
 */
NumberOfPlayers* initNumberOfPlayersStruct(){
    NumberOfPlayers* fp = malloc(sizeof(NumberOfPlayers));
    if(fp == NULL)
        return NULL;

    fp->num_players = 0;
    fp->updated_scores=0;
    fp->mutex = malloc(sizeof(pthread_mutex_t));
    fp->all_scores_are_updated = malloc(sizeof(pthread_cond_t));
    return fp;
}