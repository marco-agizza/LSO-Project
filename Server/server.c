#include "utility.h"

/* ═══════════════════════════Variabili globali═══════════════════════════════ */
// Identifica il numero di domande che verranno servite durante il gioco
int num_questions=-1;
pthread_mutex_t num_questions_mutex = PTHREAD_MUTEX_INITIALIZER;

// Indica la domanda che i thread stanno attualmente servendo
int we_are_asking = 0;
pthread_mutex_t we_are_asking_mutex= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t insert_in_game_condition= PTHREAD_COND_INITIALIZER;

// Al fine di consentire l'avvio della partita solo con un minimo di 2 giocatori
int num_players = 0;
pthread_mutex_t num_players_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_game_condition = PTHREAD_COND_INITIALIZER;

/*
Strumenti per la sincronizzazione dei thread starter (che gestiscono le richieste per i client che 
si sono uniti al gioco prima che iniziasse la partita)
*/
int i_am_a_starter = 0;
pthread_mutex_t i_am_a_starter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t countdown_ended_condition = PTHREAD_COND_INITIALIZER;

/*
Strumenti per la sincronizzazione dei thread sulla sveglia che "suona" ogni 60 secondi
*/
ThreadParameters* threads_array[PENDING_CONNECTION_QUEUE_LENGHT] = { NULL };
int threads_array_index = 0;
pthread_mutex_t threads_array_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t response_player_condition = PTHREAD_COND_INITIALIZER;

/*
Strumenti per tenere traccia dei thread che partecipano al gioco da prima che iniziasse la partita
*/
ThreadParameters* threads_starter[PENDING_CONNECTION_QUEUE_LENGHT] = { NULL };
int threads_starter_index = 0;
pthread_mutex_t threads_starter_mutex = PTHREAD_MUTEX_INITIALIZER;

// Rappresenta il numero di giocatori in partita (fintanto che il suo campo num_players è zero, la partita non è iniziata)
NumberOfPlayers* players_in_game;

// La classifica dei giocatori che partecipano al gioco
Leaderboard* players_leaderboard;

// Per la singola stampa a schermo del log di fine partita
int endgame_communication=0;
pthread_mutex_t endgame_comm_mutex = PTHREAD_MUTEX_INITIALIZER;

// Riferimento al socket descriptor su cui avviene la comunicazione
int sock_desc=-1;
pthread_mutex_t mutex_sock_d= PTHREAD_MUTEX_INITIALIZER;

/* ════════════════════════════Funzioni══════════════════════════════════════ */
void sigAlarmHandler(int received_signal);
void sigIntHandler(int received_signal);
int gameCannotStart();
int addThread(ThreadParameters* th_par);
int addStarterThread(ThreadParameters* th_par);
int takeMaxAsk();
void wakeThreadsUp();
int removeThread(ThreadParameters* th_par);
int removeStarterThread(ThreadParameters* th_par);
void *start(void *ptr);
void removePlayerFromGame(ThreadParameters *th_par);
int setOffset(int fd, int file_dim, int num, int *quiz_len);
int isStarterThread(char *username);
void endGame();

int main(int argc, char *argv[]){
	players_leaderboard = createLeaderboard();
    players_in_game = initNumberOfPlayersStruct();
	signal(SIGALRM, sigAlarmHandler);
    signal(SIGINT, sigIntHandler);
    signal(SIGQUIT,SIG_IGN);
    signal(SIGTERM,SIG_IGN);
    signal(SIGABRT,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);

	int listen_sd, connect_sd; //socket descriptors
    
    if(argc!=2){
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
	}
	int my_port= atoi(argv[1]);
	if(my_port<0){
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
	}
	struct sockaddr_in my_address;
	my_address.sin_family = AF_INET;
	my_address.sin_port = htons(my_port);
	my_address.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(my_address.sin_zero), '\0', 8); 
	
	if((listen_sd=socket(PF_INET, SOCK_STREAM, 0))<0){
		fprintf(stderr, "Socket creation error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
    pthread_mutex_lock(&mutex_sock_d);
    sock_desc=listen_sd;
    pthread_mutex_unlock(&mutex_sock_d);
	if(bind(listen_sd, (struct sockaddr*)&my_address, sizeof(my_address))<0){
		fprintf(stderr, "Socket binding error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(listen(listen_sd, PENDING_CONNECTION_QUEUE_LENGHT)<0){
		fprintf(stderr, "Socket listening error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
    system("clear");
    write(STDOUT_FILENO, "Server in attesa di connessioni...\n\n", 36);
    while(1){
        int err;
        struct sockaddr_in client_addr;
        socklen_t client_len=sizeof(client_addr);
        pthread_t curr_thread;
        if((connect_sd=accept(listen_sd, (struct sockaddr*)&client_addr, &client_len))<0){
            fprintf(stderr, "Client connection error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        write(STDOUT_FILENO, "Un giocatore si e' connesso;\n", 29);
        ThreadParameters *to_pass=malloc(sizeof(ThreadParameters));
        to_pass->client_sd=connect_sd;
        to_pass->is_waiting=0;
        to_pass->is_asking=1;
        addThread(to_pass);
        if((err=pthread_create(&curr_thread, NULL, start, (void*)to_pass))!=0){
            fprintf(stderr, "Thread creation error: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }
        pthread_detach(curr_thread);
	}
	return 0;
}

//funzione di avvio dei peer thread 
void *start(void *ptr) {
	ThreadParameters* th_params = (ThreadParameters*)ptr;
    char *client_username=NULL,
         printer[456];
    int wants_to_keep_playing=1,
        username_len,
        added_player=0;
    th_params->tid=pthread_self();
    // Registrazione dell'utente
    do{
        if(receiveInt(th_params->client_sd, &username_len)==-1){
            perror("Receive username length");
            pthread_exit(NULL);
        }
        if(receiveString(th_params->client_sd, &client_username, username_len)<0){
            perror("Receive username");
            pthread_exit(NULL);
        }
        strcpy(th_params->client_username, client_username);
        
        added_player=addUserToLeaderboard(&players_leaderboard, th_params->client_username);
        if(sendInt(th_params->client_sd, added_player)<0){
            perror("Confirm username accepted");
            pthread_exit(NULL);
        }
    }while(added_player==0);
    sprintf(printer, "L'utente %s è registrato al gioco\n", client_username);
    write(STDOUT_FILENO, printer, strlen(printer));
    // Ciclo di gioco
    do{
        int i_am_the_first=0,   // pari ad uno per il primo thread, a due per il secondo, tre altrimenti
            is_ready,
            questions_fd;       // file descriptor del file in cui sono salvate le domande del quiz
        
        if(receiveInt(th_params->client_sd, &is_ready)==-1){
            perror("Receive ready message");
            pthread_exit(NULL);
        }
        if(is_ready==-1){
            write(STDOUT_FILENO, "Il giocatore ha abbandonato la connessione\n", 43);
            removePlayerFromGame(th_params);
            wants_to_keep_playing=0;
            break;
        }else{
            int sended_message=0; // indica se è già stato inviato, o meno, il messaggio che notifica il client quando inizierà la partita
            isReady(players_leaderboard, client_username);
            sprintf(printer, "L'utente %s è pronto a giocare!\n", client_username);
            write(STDOUT_FILENO, printer, strlen(printer));
            pthread_mutex_lock(players_in_game->mutex);
            if(players_in_game->num_players==0)
                addStarterThread(th_params);
            pthread_mutex_unlock(players_in_game->mutex);
            pthread_mutex_lock(&num_players_mutex);
            num_players++;
            pthread_cond_broadcast(&start_game_condition);
            while(gameCannotStart()==1){
                pthread_mutex_lock(&i_am_a_starter_mutex);
                i_am_a_starter++;
                pthread_mutex_unlock(&i_am_a_starter_mutex);
                if(sended_message==0){
                    if(sendInt(th_params->client_sd, 1)<0){
                        write(STDOUT_FILENO, "Send player is in the lobby", 27);
                        pthread_exit(NULL);
                    }
                    sended_message++;
                }
                pthread_cond_wait(&start_game_condition, &num_players_mutex);
                i_am_the_first=1;
            }
            if(num_players<3){
                if(i_am_the_first==0){
                    i_am_the_first=2;
                    pthread_mutex_lock(&i_am_a_starter_mutex);
                    i_am_a_starter++;
                    pthread_mutex_unlock(&i_am_a_starter_mutex);
                }
                // Invio tramite messaggio il valore 2 al primo e al secondo giocatore
                if(sendInt(th_params->client_sd, 2)<0){
                    write(STDOUT_FILENO, "Send player that the game will start after 30 sec", 27);
                    pthread_exit(NULL);
                }
                sended_message++;
            }else{
                i_am_the_first=3;
            }
            pthread_mutex_lock(players_in_game->mutex);
            if(sended_message==0 && players_in_game->num_players==0){
                // Invio messaggio a tutti gli starter che non sono il primo e il secondo loggati
                if(sendInt(th_params->client_sd, 3)<0){
                    write(STDOUT_FILENO, "Send player that the game will start soon", 27);
                    pthread_exit(NULL);
                }
                sended_message++;
            }
            pthread_mutex_unlock(players_in_game->mutex);
            pthread_mutex_unlock(&num_players_mutex);
            // Apro il file
            if((questions_fd = open("questions.txt", O_RDONLY))<0){
                perror("Open questions file: ");
                pthread_exit(NULL);
            }
            int file_dim=lseek(questions_fd, 0, SEEK_END);
            if(lseek(questions_fd, 0, SEEK_SET)==-1){
                perror("lseek seek set file");
                pthread_exit(NULL);
            }

            char buff[BUFFSIZE], s_answer;
            int nbytes, read_chars,  correct_answer, file_offset = 0;
            // Il primo thread aspetta 30 secondi
            if(i_am_the_first==1){
                write(STDOUT_FILENO, "Inizio conto alla rovescia per l'avvio della partita\n", 53);
                sleep(30);
                write(STDOUT_FILENO, "\nPartita avviata!\n", 18);
            }
            
            pthread_mutex_lock(&i_am_a_starter_mutex);
            if(i_am_the_first<3){
                i_am_a_starter--;
                pthread_cond_broadcast(&countdown_ended_condition);
            }
            // Tutti i thread si mettono in attesa del primo (solo quest'ultimo e il secondo possono decrementare i_am_a_starter)
            while(i_am_a_starter!=0)
                pthread_cond_wait(&countdown_ended_condition, &i_am_a_starter_mutex);
            pthread_mutex_unlock(&i_am_a_starter_mutex);

            pthread_mutex_lock(players_in_game->mutex);
            players_in_game->num_players++;
            players_in_game->updated_scores++;
            pthread_mutex_unlock(players_in_game->mutex);

            int first_answ=1, new_in_game=1;
            while(file_offset < file_dim) {
                if(!isStarterThread(th_params->client_username)&&new_in_game){
                    // Il thread non gestisce uno starter client (un client che gioca dall'inizio della partita)
                    th_params->is_waiting=1;
                    if(sended_message==0){
                        pthread_mutex_lock(&num_questions_mutex);
                        if(num_questions==takeMaxAsk()){
                            // Se si sta già chiedendo l'ultima domanda
                            sprintf(printer, "Il giocatore %s non può unirsi al gioco perché sta terminando\n", client_username);
                            write(STDOUT_FILENO, printer, strlen(printer));
                            if(sendInt(th_params->client_sd, 5)<0){
                                perror("Send player that he cannot play");
                                pthread_exit(NULL);
                            }
                            removePlayerFromGame(th_params);
                            wants_to_keep_playing=0;
                            pthread_mutex_lock(players_in_game->mutex);
                            players_in_game->updated_scores--;
                            players_in_game->num_players--;
                            pthread_mutex_unlock(players_in_game->mutex);
                            pthread_mutex_unlock(&num_questions_mutex);
                            pthread_exit(NULL);
                        }else{
                            if(sendInt(th_params->client_sd, 4)<0){
                                perror("Send player that he will play next quest");
                                pthread_exit(NULL);
                            }
                        }
                        pthread_mutex_unlock(&num_questions_mutex);
                        sended_message++;
                    }
                    pthread_mutex_lock(players_in_game->mutex);
                    players_in_game->updated_scores--;
                    pthread_cond_broadcast(players_in_game->all_scores_are_updated);
                    pthread_mutex_unlock(players_in_game->mutex);
                    pthread_mutex_lock(&we_are_asking_mutex);
                    while(th_params->is_waiting) {
                        pthread_cond_wait(&insert_in_game_condition, &we_are_asking_mutex);
                    }
                    pthread_mutex_unlock(&we_are_asking_mutex);
                }else
                    new_in_game=0;
                /*
                Aspetto che tutti i thread che gestiscono le richieste dei giocatori in partita abbiano aggiornato
                il loro punteggio, per permettere ad ogni thread di inviare una classifica con gli stessi punteggi e giocatori ad ogni client
                */
                pthread_mutex_lock(players_in_game->mutex);
                if(new_in_game==0){ // Se non sono nuovo, ho il mio updated_scores da decrementare
                    players_in_game->updated_scores--;
                    pthread_cond_broadcast(players_in_game->all_scores_are_updated);
                }else
                    new_in_game=0;
                while(players_in_game->updated_scores!=0)
                    pthread_cond_wait(players_in_game->all_scores_are_updated, players_in_game->mutex);
                pthread_mutex_unlock(players_in_game->mutex);

                /*
                Per ogni nuovo giocatore, viene controllata la domanda da cui esso dovrà iniziare a rispondere e
                settato il corretto valore dell'offset
                */
                if(first_answ==1){
                    int quiz_len=0;
                    if((file_offset=setOffset(questions_fd, file_dim, th_params->is_asking, &quiz_len))==-1){
                        perror("error, reading file");
                        pthread_exit(NULL);
                    }else if(file_offset==-2){
                        write(STDOUT_FILENO, "Troppo tardi per giocare\n", 25);
                        removePlayerFromGame(th_params);
                        break;
                    }
                    pthread_mutex_lock(&num_questions_mutex);
                    // Controllo se num_questions necessita di essere inizializzato
                    if(num_questions==-1)
                        num_questions= quiz_len;
                    pthread_mutex_unlock(&num_questions_mutex);
                    first_answ--;
                    alarm(AVAILABLE_TIME);
                }else{
                    first_answ--;
                    alarm(AVAILABLE_TIME+2);
                }
                memset(buff, '\0', sizeof(char) * read_chars);
                read_chars = 0;
                correct_answer = 0;
                // Leggo la domanda dal file 
                do {
                    nbytes = read(questions_fd, &buff[read_chars], 1);
                    if(nbytes <= 0){
                        break;
                    }
                    if (buff[read_chars] == '~') {
                        file_offset++;
                        buff[read_chars]='\0';
                        read(questions_fd, &s_answer, 1);
                        correct_answer = atoi(&s_answer);
                        file_offset++;
                    }
                    file_offset++; read_chars++;
                } while (correct_answer == 0);
                lseek(questions_fd, file_offset, SEEK_SET);
                if(read_chars == 0)
                    break;

                sortLeaderboard(&(players_leaderboard->head),players_leaderboard->size);
                sendLeaderboard(players_leaderboard,th_params->client_sd);
                if(sendInt(th_params->client_sd, read_chars) < 0){
                    perror("Send characters to read");
                    pthread_exit(NULL);
                }
                if(sendString(th_params->client_sd, buff, read_chars) < 0){
                    perror("Send question");
                    pthread_exit(NULL);
                }
                // Imposto il valore 1 al parametro is_waiting al fine di permettere l'attesa di SIGALRM
                th_params->is_waiting= 1;

                // Raccolgo dal socket la risposta data dal client con la variabile received_answer
                int received_answer = -1;
                receiveInt(th_params->client_sd, &received_answer);
                int spent_seconds=alarm(0);
                alarm(spent_seconds);
                spent_seconds=AVAILABLE_TIME-spent_seconds;
                if(first_answ<0){
                    spent_seconds++;
                    sprintf(printer, "Ho ricevuto la risposta %d dopo %d secondi\n", received_answer, spent_seconds);
                }else{
                    sprintf(printer, "Ho ricevuto la risposta %d dopo %d secondi\n", received_answer, spent_seconds);
                }
                write(STDOUT_FILENO, printer, strlen(printer));
                
                pthread_mutex_lock(players_in_game->mutex);
                players_in_game->updated_scores++;
                pthread_mutex_unlock(players_in_game->mutex); 
                // Attesa del thread finché non riceve SIGALRM (l'handler imposterà il campo is_waiting di ogni thread a zero)
                pthread_mutex_lock(&threads_array_mutex);
                while(th_params->is_waiting)
                    pthread_cond_wait(&response_player_condition, &threads_array_mutex);
                pthread_mutex_unlock(&threads_array_mutex);
                // Calcolo dell'esito della risposta
                int outcome=0;
                if (received_answer==correct_answer){
                    outcome=1;  
                    setPlayerScore(players_leaderboard, th_params->client_username, spent_seconds);
                }else{
                    if (received_answer==-1)
                        outcome=-1;
                    setPlayerScore(players_leaderboard, th_params->client_username, GAME_MALUS);
                }
                if (sendInt(th_params->client_sd, outcome)<0){
                    perror("error communication of result to the client");
                    pthread_exit(NULL);
                }
                if (receiveInt(th_params->client_sd, &outcome)<0){
                    perror("error communication of show result end");
                    pthread_exit(NULL);
                }
            }
            // La partita è terminata
            pthread_mutex_lock(&endgame_comm_mutex);
            if(endgame_communication==0){
                write(STDOUT_FILENO, "Partita terminata!\n", 19);
                endgame_communication++;
            }
            pthread_mutex_unlock(&endgame_comm_mutex);
            alarm(0);
            close(questions_fd);
            if(sendInt(th_params->client_sd, -2)<0){
                perror("error, communication end game");
                pthread_exit(NULL);
            }
            sortLeaderboard(&(players_leaderboard->head),players_leaderboard->size);
            sendLeaderboard(players_leaderboard,th_params->client_sd);
            if(sendInt(th_params->client_sd, strlen(players_leaderboard->head->username))<0){
                perror("error, communication with client (winner username len)");
                pthread_exit(NULL);
            }
            if(sendString(th_params->client_sd, players_leaderboard->head->username, strlen(players_leaderboard->head->username))<0){
                perror("error, communication with client (winner username)");
                pthread_exit(NULL);
            }
            if(receiveInt(th_params->client_sd, &wants_to_keep_playing)<0){
                perror("error, communication with client(continue to play)");
                pthread_exit(NULL);
            }
            
            if(wants_to_keep_playing==1){
                // Il client ha scelto di rigiocare
                sprintf(printer, "L'utente %s vuole rigiocare!\n", client_username);
                write(STDOUT_FILENO, printer, strlen(printer));
                setPlayerScore(players_leaderboard, th_params->client_username, -1);
                th_params->is_asking=1;
                pthread_mutex_lock(&num_players_mutex);
                num_players--;
                pthread_mutex_unlock(&num_players_mutex);
                removeStarterThread(th_params);
            }else{
                // Il client ha scelto di abbandonare il gioco
                sprintf(printer, "L'utente %s abbandona il gioco\n", client_username);
                write(STDOUT_FILENO, printer, strlen(printer));
                removePlayerFromGame(th_params);
            }
            pthread_mutex_lock(&endgame_comm_mutex);
            if(endgame_communication==1){
                endgame_communication++;
                endGame();
            }
            pthread_mutex_unlock(&endgame_comm_mutex);

            pthread_mutex_lock(players_in_game->mutex);
            players_in_game->updated_scores--;
            pthread_cond_broadcast(players_in_game->all_scores_are_updated);
            while(players_in_game->updated_scores!=0)
                pthread_cond_wait(players_in_game->all_scores_are_updated, players_in_game->mutex);
            players_in_game->num_players--;
            pthread_mutex_unlock(players_in_game->mutex);

            pthread_mutex_lock(&endgame_comm_mutex);
            if(endgame_communication!=0)
                endgame_communication=0;
            pthread_mutex_unlock(&endgame_comm_mutex);

            if(wants_to_keep_playing){
                // Comunico al client che tutti i giocatori hanno risposto e può aggiornare la schermata
                if(sendInt(th_params->client_sd, 10)<0){
                    perror("error, communication replay");
                    pthread_exit(NULL);
                }
            }
        }
    }while(wants_to_keep_playing==1);
}
/**
 * Imposta il valore dell'offset così da consentire la lettura della domanda @param num -esima dal file
 * 
 * @param file descriptor del file da cui vengono recuperate le domande
 * @param file_dim la dimensione in byte del file
 * @param num il numero di domanda che si vuole poter leggere in seguito (l'offset viene posto a inizio riga della domanda)
 * @param quiz_len parametro in cui verrà salvato il numero di domande presenti nel file (posto nella prima riga)
 * @return 0 se ha successo, -1 se si è verificato un errore, -2 se la domanda richiesta non esiste nel file
*/
int setOffset(int fd, int file_dim, int num, int *quiz_len){
    int file_offset=lseek(fd, 0, SEEK_CUR),
        read_chars = 0,
        nbytes,
        curr_quest=1,
        num_quest,
        read_ints=0;    
    char buff[10],
         str_num_quest[30];
    
    do{
        nbytes=read(fd, &str_num_quest[read_ints], 1);
        if(nbytes == -1)
            return -1;
            
        read_ints++;
        file_offset++;
    }while(str_num_quest[read_ints-1] != '\n' && file_offset<file_dim);
    str_num_quest[read_ints-1]='\0';
    num_quest=atoi(str_num_quest);
    lseek(fd, file_offset, SEEK_SET);
    if(num_quest==0){
        perror("non vi sono domande disponibili");
        pthread_exit(NULL);
    }
    *quiz_len=num_quest;
    if(num>1){
        if(num<=num_quest){
            do {
                nbytes = read(fd, &buff[read_chars], 1);
                if(nbytes == -1)
                    return -1;
                
                if (buff[read_chars] == '~') {
                    file_offset+=2;
                    curr_quest++;
                    lseek(fd, file_offset, SEEK_SET);
                }else
                    file_offset++;
            } while (curr_quest!=num && file_offset<file_dim);
        }else
            return -2;
    }
    return file_offset;
}

/**
 * Controlla se il thread che gestisce il client identificato da @param username è uno starter della
 * partita oppure no.
 * 
 * @return 0 se non è uno starter, 1 altrimenti
*/
int isStarterThread(char *username){
    int ret=0;
    pthread_mutex_lock(&threads_starter_mutex);
    for(int i=0; i<PENDING_CONNECTION_QUEUE_LENGHT; i++){
        if(threads_starter[i]!=NULL){
            if(strcmp(username, threads_starter[i]->client_username)==0)
                ret++;
        }
    }
    pthread_mutex_unlock(&threads_starter_mutex);
    return ret;
}

/**
 * Rimuove ogni traccia del giocatore identificato dai campi in th_par dal gioco
 * 
 * @param th_par i parametri del thread che gestisce le richieste del giocatore 
 * che deve essere rimosso dal gioco
*/
void removePlayerFromGame(ThreadParameters *th_par){
    char printer[256];
    sprintf(printer, "Rimuovo %s dal gioco\n", th_par->client_username);
    write(STDOUT_FILENO, printer, strlen(printer));
    removeUserFromLeaderboard(&players_leaderboard , th_par->client_username);
    removeThread(th_par);
    removeStarterThread(th_par);
    pthread_mutex_lock(&num_players_mutex);
    num_players--;
    pthread_mutex_unlock(&num_players_mutex);
    free(th_par);
}

/**
 * Verifica della condizione necessaria affinché possa iniziare la partita:
 * 	- vi sono almeno due giocatori in attesa di giocare;
 * 
 * @return 0 se il gioco può essere avviato, 1 altrimenti
 */
int gameCannotStart() {
	int ret=0;

    if(num_players < 2)
		ret++;
	return ret;
}

/**
 * Per ognuno dei threads parameters, imposta il campo is_waiting a zero e la
 * prossima domanda che dovranno servire ai client.
 */
void wakeThreadsUp() {
    pthread_mutex_lock(&threads_array_mutex);
    int curr_quest=takeMaxAsk();
    for (int i = 0; i < PENDING_CONNECTION_QUEUE_LENGHT; i++) {
        if(threads_array[i] != NULL) {
            threads_array[i]->is_waiting = 0;
            threads_array[i]->is_asking=curr_quest+1;
        }
    }
    pthread_mutex_unlock(&threads_array_mutex);
}

/** 
 * Aggiunge un thread all'array dei thread parameters in gioco
 * 
 * @param th_par la struct thread parameters contenente i parametri necessari al thread;
 * @return 1 se il thread è stato aggiunto con successo, 0 se non vi sono posti disponibili;
*/
int addThread(ThreadParameters* th_par) {
	int ret=0;
    pthread_mutex_lock(&threads_array_mutex);
    int index = threads_array_index % PENDING_CONNECTION_QUEUE_LENGHT;
    if(threads_array[index] == NULL) {
        threads_array[index] = th_par;
        threads_array_index++;
		ret++;
    }
    pthread_mutex_unlock(&threads_array_mutex);
    return ret;
}

/**
 * Recupero della domanda che si sta attualmente servendo ai client
 * 
 * @return numero della doamanda che si sta servendo ai client
*/
int takeMaxAsk(){
    int ret=0;

    for(int i=0; i<PENDING_CONNECTION_QUEUE_LENGHT; i++){
        if(threads_array[i]!=NULL && threads_array[i]->is_asking >ret)
            ret=threads_array[i]->is_asking;
    }
    return ret;
}

/**
 * Aggiunta di un nuovo ThreadParameter all'array dei thread parameters in gioco
 * 
 * @param th_par il thread parameter che si vuole registrare
*/
int addStarterThread(ThreadParameters* th_par) {
	int ret=0;

    pthread_mutex_lock(&threads_starter_mutex);
    int index = threads_starter_index % PENDING_CONNECTION_QUEUE_LENGHT;
    if(threads_starter[index] == NULL) {
        threads_starter[index] = th_par;
        threads_starter_index++;
		ret++;
    }
    pthread_mutex_unlock(&threads_starter_mutex);
    return ret;
}

/**
 * Rimuove il thread parameters in ingresso dall'array dei thread parameters in gioco
 * 
 * @param th_par il thread da rimuovere dall'array
 * @return 1 se il thread parameters è stato rimosso con successo, 0 altrimenti
*/
int removeThread(ThreadParameters* th_par) {
	int ret=0;

    pthread_mutex_lock(&threads_array_mutex);
    for (int i = 0; i < PENDING_CONNECTION_QUEUE_LENGHT; i++) {
        if(threads_array[i] != NULL && strcmp(threads_array[i]->client_username, th_par->client_username) == 0) {
            threads_array[i] = NULL;
            ret++;
        }
    }
    pthread_mutex_unlock(&threads_array_mutex);
    return ret;
}

/**
 * Rimuove il thread parameters in ingresso dall'array dei thread parameters starter
 * 
 * @param th_par il thread da rimuovere dall'array
 * @return 1 se il thread parameters è stato rimosso con successo, 0 altrimenti
*/
int removeStarterThread(ThreadParameters* th_par){
    int ret=0;

    pthread_mutex_lock(&threads_starter_mutex);
    for (int i = 0; i < PENDING_CONNECTION_QUEUE_LENGHT; i++) {
        if(threads_starter[i] != NULL && strcmp(threads_starter[i]->client_username, th_par->client_username) == 0) {
            threads_starter[i] = NULL;
            ret++;
        }
    }
    pthread_mutex_unlock(&threads_starter_mutex);
    return ret;
}

/**
 * Handler di SIGALRM; Sveglia i thread in attesa e imposta la prossima sveglia tra TIME_AVAILABLE secondi
 * 
 * @param received_signal il segnale ricevuto dall'handler
 * */
void sigAlarmHandler(int received_signal){
    if(received_signal == SIGALRM) {
        wakeThreadsUp(); //oltre a svegliare i thread aggiorno il numero della domanda che devono servire
        pthread_cond_broadcast(&response_player_condition); //sveglia i thread che si sono messi in attesa della sveglia
        pthread_cond_broadcast(&insert_in_game_condition);  //sveglio i thread che si sono messi in attesa di unirsi alla partita dopo che questa è iniziata
    }
}

/**
 * Handler di SIGINT; mostra a schermo che il segnale è stato catturato e termina il processo con i suoi peer thread
 * 
 * @param received_signal ili segnale ricevuto dall'handler
*/
void sigIntHandler(int received_signal){
    if(received_signal == SIGINT){
        write(STDOUT_FILENO, "\n\nChiusura in corso...", 22);
        pthread_mutex_lock(&mutex_sock_d);
        if(sock_desc!=-1)
            close(sock_desc);
        pthread_mutex_unlock(&mutex_sock_d);
        sleep(1);
        write(STDOUT_FILENO, "\n", 1);
        exit(0);
    }
}

/**
 * Gestisce la fine della partita, resettando i valori delle variabili globali 
 * per la prossima partita
*/
void endGame(){
    pthread_mutex_lock(&num_questions_mutex);
    num_questions=-1;
    pthread_mutex_unlock(&num_questions_mutex);
    pthread_mutex_lock(&we_are_asking_mutex);
    we_are_asking=0;
    pthread_mutex_unlock(&we_are_asking_mutex);
    pthread_mutex_lock(&i_am_a_starter_mutex);
    i_am_a_starter = 0;
    pthread_mutex_unlock(&i_am_a_starter_mutex);
}