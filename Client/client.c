#include "utility.h"

void sighandler(int sig_num);
int interactWithServer(int server_sd, char *username);
int printLeaderboard(int server_sd, char *nickname);
void printQuestion(int server_sd);
void printMenu();
int connection_var=-1;

int main(int argc, char* argv[]){
	int sd, server_port;
	struct sockaddr_in server_address;
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGABRT, sighandler);
    signal(SIGTSTP, sighandler);

	if(argc!=3 || (server_port= atoi(argv[2]))<0){
		fprintf(stderr, "\033[0;31merror\033[0;37m, usage: %s <ip_addr> <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	server_address.sin_family= AF_INET;
	server_address.sin_port=htons(server_port);
	inet_aton(argv[1], &server_address.sin_addr);
	memset(&(server_address.sin_zero), '\0', 8);
	
	if((sd=socket(PF_INET, SOCK_STREAM, 0))<0){
        perror("\033[0;31merror\033[0;37m, socket creation error: ");
		exit(EXIT_FAILURE);
	}
	
	int connection= connect(sd, (struct sockaddr*) &server_address, sizeof(server_address));
	if(connection<0){
        perror("\033[0;31merror\033[0;37m, socket connection error: ");
        exit(EXIT_FAILURE);
	}
    connection_var=sd;
    
    char nickname[MAXLEN_USERNAME]={'\0'};
    int wants_to_keep_playing=1,    // indica se l'utente vuole rigiocare o meno
        nickname_len=0,             // la lunghezza dello username del client    
        is_not_a_replay=1;          // indica se non è la prima partita giocata dal client
    do{
        printMenu();
        if(is_not_a_replay){
            int username_registered=0; // l'esito dell'aggiunta del giocatore alla classifica
            write(STDOUT_FILENO, "\tInserire il nickname con cui vuoi che gli altri giocatori ti riconoscano: ", 75);
            is_not_a_replay=0;
            do{
                takeSaneString(nickname, 25);
                nickname_len=strlen(nickname);
                if(nickname_len<3){
                    write(STDOUT_FILENO, YELLOW_COLOR"\tattenzione"WHITE_COLOR", inserire un nickname di almeno tre caratteri: ", 73);
                    for(int i=0;i<nickname_len;i++)
                        nickname[i]='\0';
                }else{
                    if(sendInt(sd, nickname_len)<0){
                        perror("\033[0;31merror\033[0;37m, send username length");
                        exit(1);
                    }
                    if(sendString(sd, nickname, nickname_len)<0){
                        perror("\033[0;31merror\033[0;37m, send username");
                        exit(1);
                    }
                    username_registered=0;
                    if(receiveInt(sd, &username_registered)<0){
                        perror("\033[0;31merror\033[0;37m, server communication");
                        exit(1);
                    }
                    if(username_registered==0){
                        write(STDOUT_FILENO, YELLOW_COLOR"\tattenzione"WHITE_COLOR", inserire uno username non gia' utilizzato da un altro giocatore: ",91);
                    }
                }
            }while(nickname_len<3 || username_registered==0);
        }
        char response[MAXLEN_ANSWER]={'\0'};
        write(STDOUT_FILENO, "\tSei pronto? "CYAN_COLOR"Y "WHITE_COLOR"per iniziare, "CYAN_COLOR"N"WHITE_COLOR" per uscire dal gioco"CYAN_COLOR"[Y/N]"WHITE_COLOR": ", 101);
        do{
            takeSaneString(response, MAXLEN_ANSWER);
            if(strcmp(response,"N")!=0 && strcmp(response,"Y")!=0)
                write(STDOUT_FILENO, YELLOW_COLOR"\tattenzione"WHITE_COLOR", inserire Y per iniziare, N per uscire dal gioco: ", 76);
        }while(strcmp(response,"N")!=0 && strcmp(response,"Y")!=0);
        if(strcmp(response,"N")==0){
            if(sendInt(sd, -1)<0){
                perror("\033[0;31merror\033[0;37m, server communication");
                exit(1);
            }
            break;
        }else{
            if(sendInt(sd, 1)<0){
                perror("\033[0;31merror\033[0;37m, server communication");
                exit(1);
            }
            int is_in_the_lobby=0;
            if(receiveInt(sd, &is_in_the_lobby)==0){
                if(is_in_the_lobby==1){
                    write(STDOUT_FILENO, GREEN_COLOR"\tIn attesa di altri giocatori...\n"WHITE_COLOR, 48);
                    if(receiveInt(sd, &is_in_the_lobby)==0)
                        write(STDOUT_FILENO, GREEN_COLOR"\tIl gioco avrà inizio tra 30 secondi...\n"WHITE_COLOR, 55);
                    else{
                        perror("\033[0;31merror\033[0;37m, server communication");
                        exit(1);
                    }
                }else if(is_in_the_lobby==2)
                    write(STDOUT_FILENO, GREEN_COLOR"\tIl gioco avrà inizio tra 30 secondi...\n"WHITE_COLOR, 55);
                else if (is_in_the_lobby==3)
                    write(STDOUT_FILENO, GREEN_COLOR"\tIl gioco avrà presto inizio...\n"WHITE_COLOR, 47);
                else if(is_in_the_lobby==4)
                    write(STDOUT_FILENO, "\n\tIl gioco e' in corso. Potrai unirti non appena verra' lanciata una nuova domanda!\n", 84);
                else
                    write(STDOUT_FILENO, "\tTroppo tardi per giocare! Riprova tra cinque minuti\n", 53);
                
                if(is_in_the_lobby!=5)
                    wants_to_keep_playing= interactWithServer(sd, nickname);
                else
                    wants_to_keep_playing= 0;
            }else{
                perror("\033[0;31merror\033[0;37m, server communication\n");
                exit(1);
            }
        }
    }while(wants_to_keep_playing);
    return 0;
}
/**
 * Gestisce l'interazione tra il client e il server e gli scambi dei messaggi.
 * 
 * @param server_sd socket descriptor del server
 * @param nickname lo username del client
*/
int interactWithServer(int server_sd, char *nickname){
    int leaderboard_size = -1,      // la lunghezza della leaderboard
        wants_to_keep_playing=0,    // memorizza messaggio da parte del server per sincronizzare il replay delle partite
        winner_username_length,     // lunghezza dello username del vincitore della partita
        response;                   // indica se il client intende rigiocare o meno
    char *winner=NULL,              // lo username del vincitore
         printer[160];

    // Gestione delle risposte alle domande
    while(printLeaderboard(server_sd, nickname)>=0){
        int answer,     // risposta data
            outcome=0;  // esito della risposta

        printQuestion(server_sd);
        write(STDOUT_FILENO, "Inserisci risposta: \n", 19);
        answer = takeAnswer(1);
        sendInt(server_sd, answer);
        if(answer!=-1)
            write(STDOUT_FILENO, "Risposta inviata con successo! Aspettare il termine dei 60 secondi\n", 67);
        if(receiveInt(server_sd,&outcome)<0){
            perror("error communication with server");
            exit(1);
        }
        if (outcome==-1)
            write(STDOUT_FILENO, RED_COLOR"\nPeccato"WHITE_COLOR", il tempo a disposizione per rispondere alla domanda e' scaduto!\n\b", 89);
        else if(outcome==0)
            write(STDOUT_FILENO, RED_COLOR"\nPeccato"WHITE_COLOR", la risposta e' errata!\n\b", 48);
        else
            write(STDOUT_FILENO, GREEN_COLOR"\nBravo"WHITE_COLOR", hai risposto correttamente alla domanda!\n\b", 64);
        sleep(2);
        if(sendInt(server_sd, 1)<0){
            perror("error communication with server (end of show result)");
            exit(1);
        }
    }
    printLeaderboard(server_sd, nickname);
    printAnswerLowerBorder

    if(receiveInt(server_sd, &winner_username_length)<0){
        perror("error, communication with server (winner username len)");
        exit(1);
    }
    if(receiveString(server_sd, &winner, winner_username_length)<0){
        perror("error, communincation with server (winner username)");
        exit(1);
    }
    if(strcmp(winner, nickname)==0)
        write(STDOUT_FILENO, "\tComplimenti, hai vinto!\n", 26);
    else
        write(STDOUT_FILENO, "\tPeccato, hai perso!\n", 21);
    
    write(STDOUT_FILENO, "\n\n\tSei interessato a rigiocare?[Y/N]: ", 38);

    response=takeAnswer(2);
    if(response==-1){
        if(sendInt(server_sd, -1)<0){
            perror("\033[0;31merror\033[0;37m, server communication error deny replay");
            exit(1);
        }
        write(STDOUT_FILENO, "Uscita dal gioco in corso...\n", 29);
        sleep(1);
        return 0;
    }else{
        write(STDOUT_FILENO, "\n\tIn attesa che gli altri giocatori rispondano o che passino 60 secondi...\n", 75);
        if(sendInt(server_sd, 1)<0){
            perror("\033[0;31merror\033[0;37m, server communication accept replay");
            exit(1);
        }
        if(receiveInt(server_sd, &wants_to_keep_playing)<0){
            perror("\033[0;31merror\033[0;37m, server communication error for replay");
            exit(1);
        }
        if(wants_to_keep_playing==10)
            return 1;
        else
            return 0;
    }
}

/**
 * Stampa a schermo la leaderboard permettendo un' immediata visualizzazione della
 * posizione del client, colorando il suo username
 * 
 * @param sd socket descriptor del server
 * @param nickname lo username del client
*/
int printLeaderboard(int sd, char *nickname){
    int leaderboard_size=0, 
        username_len = 0, 
        avg_spent_time_len = 0,
        points = -1;
    char *username,
         *str_avg_spent_time,
         printer[160];
    float avg_spent_time=0;

    if (receiveInt(sd, &leaderboard_size) < 0) {
        perror("\033[0;31mReading the number of players");
        exit(1);
    }
    
    if(leaderboard_size<0)
        return leaderboard_size; // la partita è finita
    
    system("clear");
    write(STDOUT_FILENO, CYAN_BOLD_COLOR"\t\t\t\t\t\t  CLASSIFICA\n"WHITE_COLOR, 33);
    for (int i = 0; i < leaderboard_size; i++) {
        receiveInt(sd, &username_len);
        receiveString(sd, &username, username_len);
        receiveInt(sd, &avg_spent_time_len); 
        receiveString(sd, &str_avg_spent_time, avg_spent_time_len);
        avg_spent_time=atof(str_avg_spent_time);
        if(strcmp(username, nickname)==0)
            sprintf(printer, "\t\t\tPosizione %d\t- "PURPLE_COLOR"Username"WHITE_COLOR": "GREEN_COLOR"%s"WHITE_COLOR" - "PURPLE_COLOR"tempo_medio"WHITE_COLOR": %.2f\n", i+1, username, avg_spent_time);
        else
            sprintf(printer, "\t\t\tPosizione %d\t- "PURPLE_COLOR"Username"WHITE_COLOR": %s - "PURPLE_COLOR"tempo_medio"WHITE_COLOR": %.2f\n", i+1, username, avg_spent_time);
        
        write(STDOUT_FILENO, printer, strlen(printer));
        free(username);
        free(str_avg_spent_time);
    }
    return 0;
}

/**
 * Stampa a schermo la domanda ricevuta dal server
 * 
 * @param sd socket descriptor del server
*/
void printQuestion(int sd){
    int question_len=0;

    if (receiveInt(sd, &question_len) < 0) {
        perror("\033[0;31merror\033[0;37m, reading question lenght\n");
        exit(1);
    }
    char *question = NULL;
    if (receiveString(sd, &question, question_len) < 0) {
        perror("\033[0;31merror\033[0;37m, reading question");
        exit(1);
    }
    printAnswerUpperBorder
    write(STDOUT_FILENO, question, question_len - 1);
    printAnswerLowerBorder
}

/**
 * Stampa a schermo il menu iniziale
*/
void printMenu(){
    system("clear");
    printUpperBorder
    write(STDOUT_FILENO, CYAN_COLOR"\t║\t"WHITE_COLOR"Benvenuto in \"Aritmetica!\", il gioco a quiz per mettere alla prova le tue abilita' matematiche!\t\t"CYAN_COLOR"║\n", 127);
    write(STDOUT_FILENO, "\t║\t"WHITE_COLOR"Ti verranno poste delle domande di aritmetica e avrai un minuto di tempo per rispondere ad ognuna.\t"CYAN_COLOR"║\n", 122);
    write(STDOUT_FILENO, "\t║\t"WHITE_COLOR"Rispondi piu' in fretta che puoi e non dimenticare di rispondere correttamente! :)\t\t\t"CYAN_COLOR"║"WHITE_COLOR, 115);
    printLowerBorder
    write(STDOUT_FILENO, "\tIl gioco avra' inizio quando ci saranno almeno due giocatori connessi\n\n", 72);
}

/**
 * Handler dei segnali SIGINT, SIGQUIT, SIGABRT, SIGTSTP
*/
void sighandler(int sig_num){
    write(STDOUT_FILENO, "\n\nE' stata richiesta un'uscita forzata\n", 39);
    if(connection_var!=-1)
        close(connection_var);
    sleep(1);
    exit(0);
}