#ifndef UNTITLED_UTILITY_H
#define UNTITLED_UTILITY_H

/* ════════════════════════════════Librerie════════════════════════════════════════ */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

/* ══════════════════════════════════Costanti══════════════════════════════════════ */
#define AVAILABLE_TIME 60
#define BUFFSIZE 4096
#define MAXLEN_USERNAME 25
#define MAXLEN_ANSWER 2

//Colori
#define YELLOW_COLOR "\033[0;33m"
#define WHITE_COLOR "\033[0;37m" 
#define CYAN_COLOR "\033[0;36m"
#define RED_COLOR "\033[0;31m"
#define GREEN_COLOR "\033[0;32m"
#define PURPLE_COLOR "\033[0;35m"
#define CYAN_BOLD_COLOR "\e[1;96m"

/* ═══════════════════════════════════Macro════════════════════════════════════════ */
#define printUpperBorder write(STDOUT_FILENO, CYAN_COLOR"\n\n\t╔════════════════════════════════════════════════ "CYAN_BOLD_COLOR" Aritmetica! "CYAN_COLOR" ════════════════════════════════════════════════╗\n"WHITE_COLOR, 341);
#define printLowerBorder write(STDOUT_FILENO, CYAN_COLOR"\n\t╚═══════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n"WHITE_COLOR, 356);
#define printAnswerUpperBorder write(STDOUT_FILENO, CYAN_COLOR"\t╔══════════════════════════════════════════════════════════════════════════════════════════════╗\n"WHITE_COLOR, 305);
#define printAnswerLowerBorder write(STDOUT_FILENO, CYAN_COLOR"\t╚══════════════════════════════════════════════════════════════════════════════════════════════╝\n"WHITE_COLOR, 305);

/* ══════════════════════════════════Funzioni══════════════════════════════════════ */
int isAPossibleAnswer(const char*);
int takeAnswer(int type);
int receiveInt(int sd, int *num);
int sendInt(int sd, int num);
int receiveString(int sd, char** string, int str_len);
int sendString(int sd, char* string, int str_length);
void takeSaneString(char *string, int str_len);

#endif