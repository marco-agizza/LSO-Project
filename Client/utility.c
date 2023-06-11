#include "utility.h"

/**
 * Verifica che la stringa in ingresso sia costituita da un numero compreso tra 1 e 4 
 * 
 * @param answ la stringa da controllare
 * @return true se si tratta di un numero compreso tra uno e quattro
 */
int isAPossibleAnswer(const char* answ) {
    int str_len=strlen(answ);
    if(str_len!=1)
        return 0;
    if(!((int)answ[0] >= 49 && (int)answ[0] <= 52))
        return 0;

    return 1;
}

/**
 * Gestisce l'inserimento, da parte dell'utente, di una risposta in un certo lasso di tempo
 * 
 * @param type indica il tipo di risposta che si intende gestire:
 *      - è pari a 1, gestisce l'inserimento di una risposta ad una domanda (valori ammessi : 1 a 4)
 *      - è diverso da 1, gestisce l'inserimento di una risposta del tipo "Y o N"
 * @return la risposta dell'utente oppure, allo scadere del timeout della select, -1
 */
int takeAnswer(int type) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    struct timeval timeout;
    timeout.tv_sec = AVAILABLE_TIME+1;
    timeout.tv_usec = 0;
    int givenAns = -1,
        ok;

    if (select(STDIN_FILENO+1, &read_fds, NULL, NULL, &timeout) != 0) {
        char buff[MAXLEN_ANSWER] = { '\0' };
        do{
            ok=1;
            takeSaneString(buff, MAXLEN_ANSWER);
            if(type==1){
                if(!isAPossibleAnswer(buff)){
                    ok=0;
                    write(STDOUT_FILENO, YELLOW_COLOR"Attenzione"WHITE_COLOR", inserire un numero compreso tra 1 e 4: ", 65);
                }
            }else{
                if(strcmp(buff, "Y")!=0 && strcmp(buff, "N")!=0){
                    ok=0;
                    write(STDOUT_FILENO, YELLOW_COLOR"\tAttenzione"WHITE_COLOR", inserire Y o N: ", 43);
                }else{
                    if(strcmp(buff, "Y")==0)
                        return 1;
                    else
                        return -1;
                }
            }
	    }while(ok==0);
        givenAns = atoi(buff);
    }
    return givenAns;
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
 * @param string il doppio puntatore a stringa in cui verrà salvata la stringa letta
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
 * Legge da stdin una stringa di lunghezza massima @param len e non vuota
 * 
 * @param string la string ottenuta da stdin
 * @param len la lunghezza massima della stringa che si vuole recuperare da stdin
*/
void takeSaneString(char *string, int str_len){
    int n, 
        read_bytes = 0, 
        ok = 0;
    char buff[BUFFSIZE]={'\0'};
    do{
        if((n=read(STDIN_FILENO, buff, BUFFSIZE))>0){
            if(n > str_len+1){
                char printer[BUFFSIZE];
                for(int i; i<n; i++)
                    buff[i]='\0';
                sprintf(printer, YELLOW_COLOR"\tattenzione"WHITE_COLOR", immettere una stringa con meno di %d caratteri: ", str_len);
                write(STDOUT_FILENO, printer, strlen(printer));
            }else if(buff[0]=='\0'){
                for(int i; i<n; i++)
                    buff[i]='\0';
                write(STDOUT_FILENO, RED_COLOR"\terrore"WHITE_COLOR", non sono ammessi campi vuoti: ", 53);
            }
        }
    }while(n > str_len+1 || buff[0]=='\0');
    buff[n-1]='\0';
    strcpy(string, buff);
}