#include "leaderboard.h"

/**
 * Istanzia e inizializza la classifica dei giocatori
*/
Leaderboard* createLeaderboard() {
    Leaderboard* new_leaderboard = malloc(sizeof(Leaderboard));
    new_leaderboard->size = 0;
    new_leaderboard->num_ready=0;
    new_leaderboard->head = NULL;
    new_leaderboard->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(new_leaderboard->mutex, NULL);

    return new_leaderboard;
}

/**
 * Controlla che lo username in ingresso non sia già utilizzato da un altro giocatore
 * 
 * @param leaderboard la classifica
 * @param username il nickname con cui verrà identificato il giocatore
 * @return 1 se il giocatore è presente, 0 altrimenti
*/
int isAlreadyIn(Player* curr_player, char* username){
    if(curr_player==NULL)
        return 0;
    else{
        if(strcmp(username,curr_player->username)==0)
            return 1;
        else
            return isAlreadyIn(curr_player->next, username);
    }
}

/**
 * Aggiunge un nuovo giocatore, identificato mediante il suo nickname, in testa alla classifica
 * 
 * @param leaderboard la classifica
 * @param username il nickname del giocatore che si intende registrare
 * @return 1 se il giocatore è stato aggiunto, 0 se il giocatore è già presente
 */
int addUserToLeaderboard(Leaderboard **leaderboard, char* username) {
    pthread_mutex_lock((*leaderboard)->mutex);
    if(isAlreadyIn((*leaderboard)->head, username)){
        pthread_mutex_unlock((*leaderboard)->mutex);
        return 0;
    }
    Player* new_player = malloc(sizeof(Player));
    strcpy(new_player->username, username);
    new_player->spent_time=0;
    new_player->num_answ=0;
    new_player->is_ready=0;
    new_player->next = (*leaderboard)->head;
    (*leaderboard)->head=new_player;
    (*leaderboard)->size++;
    pthread_mutex_unlock((*leaderboard)->mutex);
    
    return 1;
}

/**
 * Funzione di supporto a sortLeaderboard che fa lo swap dei nodi della lista
 * 
 * @param player1 @param player2 i nodi protagonisti dell'operazione
 * @return il secondo nodo in ingresso
 * */
Player* swapPlayer(Player* player1, Player* player2) {
    Player* tmp = player2->next;
    player2->next = player1;
    player1->next = tmp;
    return player2;
}

/**
 * Funzione che ordina la classifica in modo che in testa vi sia il giocatore con
 * tempo medio inferiore
 * 
 * @param head nodo in testa alla lista
 * @param size il numero di nodi della lista
 * */
void sortLeaderboard(Player** head, int size) {
    struct Player** h;
    int i, j, swapped;
  
    for (i = 0; i <= size; i++) {
        h = head;
        swapped = 0;
        for (j = 0; j < size-i-1; j++) {
            Player* p1 = *h;
            Player* p2 = p1->next;
            float avg_spent_time_1, 
                  avg_spent_time_2;
            if(p1->num_answ==0)
                avg_spent_time_1=0;
            else
                avg_spent_time_1=p1->spent_time/p1->num_answ;
            if(p2->num_answ==0)
                avg_spent_time_2=0;
            else
                avg_spent_time_2=p2->spent_time/p2->num_answ;

            if (avg_spent_time_1 > avg_spent_time_2) {
                //aggiorno il prossimo giocatore da confrontare con gli altri (p2)
                *h = swapPlayer(p1, p2);
                swapped = 1;
            }
            h = &(*h)->next;
        }
        if (swapped == 0)
            break;
    }
}

/**
 * Rimuove l'utente identificato da @param username dalla classifica @param leaderboard
 * 
 * @param leaderboard la classifica da cui si vuole rimuovere il giocatore
 * @param username lo username del giocatore
*/
void removeUserFromLeaderboard(Leaderboard **leaderboard, char* username){
    Player *head=(*leaderboard)->head;
    Player *prev=NULL, *temp=NULL;
    
    pthread_mutex_lock((*leaderboard)->mutex);
    if(strcmp(head->username,username)==0){
        temp=head;
        (*leaderboard)->head=head->next;
        if(temp->is_ready==1)
            (*leaderboard)->num_ready--;
        (*leaderboard)->size--;
        free(temp);
        pthread_mutex_unlock((*leaderboard)->mutex);
        return;
    }
    while(head){
        if(strcmp(head->username, username)==0){
            temp=head;
            head=head->next;
            if(prev)
                prev->next=head;
            if(temp->is_ready==1)
                (*leaderboard)->num_ready--;
            (*leaderboard)->size--;
            free(temp);
            pthread_mutex_unlock((*leaderboard)->mutex);
            return;
        }
        prev=head;
        head=head->next;
    }
    pthread_mutex_unlock((*leaderboard)->mutex);
}

/**
 * Setta il campo is_ready del giocatore identificato da @param username a 1
 * 
 * @param username: il giocatore di cui si vuole settare il campo is_ready a 1
 * @param leaderboard: la classifica
*/
void isReady(Leaderboard *leaderboard, char *username){
    Player *curr=leaderboard->head;

    while(curr){
        if(strcmp(curr->username, username)==0){
            curr->is_ready=1;
            leaderboard->num_ready++;
            return;
        }
        curr=curr->next;
    }
    return;
}