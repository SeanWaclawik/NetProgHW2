/**
 * Sean Waclawik & Garret Premo
 * Homework 2
 */

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <dns_sd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>


// need to make this selected by the operating system
#define PORT "5939"

#define ZCONF_NAME "waclas"
#define ZCONF_TYPE "_rps._tcp"
#define ZCONF_DOMAIN "local"

#define ROCK 0
#define PAPER 1
#define SCISSORS 2


void die(const char *msg) {
    perror("ERROR: ");
    perror(msg);
    exit(EXIT_FAILURE);
}

int isEmpty(const char *s) {
  while (*s != '\0') {
    if (!isspace(*s))
      return 0;
    s++;
  }
  return 1;
}

int isValidAction(char *action){
    // An item outside the set 
    //     {rock, paper, scissors} would also be considered invalid

    // lower case our input before comparing
    for(int i = 0; action[i]; i++){
      action[i] = tolower(action[i]);
    }

    if (strncmp(action, "rock", 4) == 0){
        return ROCK;
    } 
    else if (strncmp(action, "paper", 5) == 0){
        return PAPER;
    }
    else if (strncmp(action, "scissors", 8) == 0){
        return SCISSORS;
	}
    
    // else
	return -1;
}


void printResults (int fd, char * winnerAction, char * result, char * loserAction, char* winner, char* loser){
    send(fd, (const void *) winnerAction, strlen(winnerAction), 0);
    send(fd, (const void *) result, strlen(result), 0);
    send(fd, (const void *) loserAction, strlen(loserAction), 0);
    send(fd, (const void *) "! ", 2, 0);
    send(fd, (const void *) winner, strlen(winner), 0);
    send(fd, (const void *) result, strlen(result), 0);
    send(fd, (const void *) loser, strlen(loser), 0);
    send(fd, (const void *) "!\n", 2, 0);
}



void runGame () {
    int sockfd, new, maxfd, on = 1;
    int nready, i, serverAction;
    int client[50];
    int clientSize = 50;

    // order of this array is important due to #defines of ROCK, ...
    char actionStrs[3][10] = {"ROCK", "PAPER", "SCISSORS"};

    struct addrinfo *res0, *res, server;
    char buffer[BUFSIZ];
    char playerName[BUFSIZ];

    fd_set master, readfds;
    int error;
    ssize_t nbytes;
    (void)memset(&server, '\0', sizeof(struct addrinfo));

    server.ai_family = AF_INET;
    server.ai_socktype = SOCK_STREAM;
    server.ai_protocol = IPPROTO_TCP;
    server.ai_flags = AI_PASSIVE;

    srand(time(NULL));   // seed the random value
    int randSeed = 0;


    if (0 != (error = getaddrinfo(NULL, PORT, &server, &res0))) {
        errx(EXIT_FAILURE, "%s", gai_strerror(error));
    }

    for (res = res0; res; res = res->ai_next) {
        if (-1 == (sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol))) {
            perror("socket()");
            continue;
        }

        if (-1 == (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(int)))) {
            perror("setsockopt()");
            continue;
        }

        if (-1 == (bind(sockfd, res->ai_addr, res->ai_addrlen))) {
            perror("bind");
            continue;
        }

        break;
    }

    if (-1 == sockfd) {
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res0);

    if (-1 == (listen(sockfd, 32))) {
        die("listen()");
    }

    if (-1 == (fcntl(sockfd, F_SETFD, O_NONBLOCK))) {
        die("fcntl()");
    }

    DNSServiceRef serviceRef;
    DNSServiceErrorType dnserror = DNSServiceRegister(&serviceRef, 0, 0,
        ZCONF_NAME, ZCONF_TYPE, ZCONF_DOMAIN, NULL, htons(atoi(PORT)), 0, NULL,
        NULL, NULL);

    if (dnserror != kDNSServiceErr_NoError) {
        die("DNSServiceRegister()");
    }

    ///int dns_sd_fd = DNSServiceRefSockFD(serviceRef);

    // start
    maxfd = sockfd;


    // zero out the clients with -1 for not used
    int j;
    for (j=0; j<clientSize; j++){
        client[j] = -1;
    }


    FD_ZERO(&master);
    FD_SET(sockfd, &master);



    // end
    // FD_ZERO(&master);
    // FD_SET(dns_sd_fd, &master);

    // FD_ZERO(&readfds);
    // FD_SET(sockfd, &master);
    

    // maxfd = sockfd;

    while (1) {

        memcpy(&readfds, &master, sizeof(master));

        if (-1 == (nready = select(maxfd+1, &readfds, NULL, NULL, NULL))) {
            die("select()");
        }

        if(FD_ISSET(sockfd, &readfds)) {
            if (-1 == (new = accept(sockfd, NULL, NULL))) {
                if (EWOULDBLOCK != errno) {
                    die("accept()");
                }
                break;
            }
            printf("Received a connection\n");
            char *msg = "What is your name?\n";
            nbytes = write(new, msg, strlen(msg));

            j=0;
            while (j<clientSize && client[j] != -1){
                j++;
            }

            if (j >= clientSize){
                printf("j >= clientSize\n");

                printf("Too many connections\n");
                close(new);
            }

            client[j] = new;
            

            FD_SET(new, &master);
            if(new > maxfd)
                maxfd = new;
            if(--nready <= 0)
                continue;
        }

        for (i=0; i<=maxfd && nready>0; i++) {
            if (FD_ISSET(i, &readfds)) {
                nready--;

                if (i == sockfd) {
                    // printf("Received a new connection\n");

                    if (-1 == (new = accept(sockfd, NULL, NULL))) {
                        if (EWOULDBLOCK != errno) {
                            die("accept()");
                        }
                        break;
                    } else {
                        if (-1 == (fcntl(new, F_SETFD, O_NONBLOCK))) {
                            die("fcntl()");
                        }

                        FD_SET(new, &master);

                        if (maxfd < new) {
                            maxfd = new;
                        }

                        

                    }
                } else {
                    // (void)printf("recv() data from one of descriptors(s)\n");

                    // get the name of connecting player
                    int invalid=1;
                    do {
                        memset(&buffer, 0, sizeof(buffer));
                        send(i, (const void *)"What is your name?\n", 19, 0);
                        nbytes = recv(i, buffer, sizeof(buffer), 0);
                        if (nbytes <= 0) {
                            if (EWOULDBLOCK != errno) {
                                die("recv()");
                            }

                            break;
                        } 

                        buffer[nbytes-1] = '\0';
                        //printf("string recv has %ld bytes: '%s'\n", nbytes, buffer);

                        invalid = isEmpty(buffer);
                    } while(invalid);
                    printf("other player's name:\n");

                    // copy over to the player's name to upper case
                    // lower case our input before comparing
                    int k;
                    for(k = 0; buffer[k]; k++){
                      playerName[k] = toupper(buffer[k]);
                    }
                    playerName[BUFSIZ] = '\0';
                    if (k<BUFSIZ){
                        playerName[k] = '\0';
                    }
                    printf("is %s\n", playerName);

                    // set our action 
                    serverAction = (rand() * randSeed) % 3;
                    randSeed+= serverAction+i;

                    // get the player's action
                    int playerAction = -1;
                    while(playerAction < 0) {
                        send(i, (const void *)"Rock, paper, or scissors?\n", 26, 0);
                        nbytes = recv(i, buffer, sizeof(buffer), 0);
                        if (nbytes <= 0) {
                            if (EWOULDBLOCK != errno) {
                                die("recv()");
                            }

                            break;
                        }

                        buffer[nbytes] = '\0';
                        printf("%s", buffer);

                        playerAction = isValidAction(buffer);
                    }

                    // figure out who wins

                    // TODO this should be a switchcase
                    
                    // Case: tie
                    if (serverAction == playerAction) {
                        printResults (i, actionStrs[serverAction], " ties ", actionStrs[playerAction], ZCONF_NAME, playerName);
                        
                        // send(i, (const void *) actionStrs[serverAction], strlen(actionStrs[serverAction]), 0);
                        // send(i, (const void *) " ties ", 6, 0);
                        // send(i, (const void *) actionStrs[playerAction], strlen(actionStrs[playerAction]), 0);
                        // send(i, (const void *) "! ", 2, 0);
                        // send(i, (const void *) ZCONF_NAME, strlen(ZCONF_NAME), 0);
                        // send(i, (const void *) " ties ", 6, 0);
                        // send(i, (const void *) playerName, strlen(playerName), 0);
                        // send(i, (const void *) "!\n", 2, 0);
                        
                	} 
                    // case rock vs paper
                    else if (serverAction == ROCK && playerAction == PAPER) {
                        // player wins
                        printResults (i, actionStrs[playerAction], " covers ", actionStrs[serverAction], playerName, ZCONF_NAME);
                    }

                    // case paper vs rock
                    else if (serverAction == PAPER && playerAction == ROCK) {
                        // server wins
                        printResults (i, actionStrs[serverAction], " covers ", actionStrs[playerAction], ZCONF_NAME, playerName);
                    }

                    // case rock vs scissors
                    else if (serverAction == ROCK && playerAction == SCISSORS) {
                        printResults(i, actionStrs[serverAction], " crushes ", actionStrs[playerAction], ZCONF_NAME, playerName);
                    }

                    // case scissors vs rock
                    else if (serverAction == SCISSORS && playerAction == ROCK) {
                        printResults(i, actionStrs[playerAction], " crushes ", actionStrs[serverAction], playerName, ZCONF_NAME);
                    }

                    // case paper vs scissors
                    else if (serverAction == PAPER && playerAction == SCISSORS) {
                        printResults(i, actionStrs[playerAction], " cuts ", actionStrs[serverAction], playerName, ZCONF_NAME);
                    }

                    // case scissors vs paper
                    else if (serverAction == SCISSORS && playerAction == PAPER) {
                        printResults(i, actionStrs[serverAction], " cuts ", actionStrs[playerAction], ZCONF_NAME, playerName);
                    }

                    else { // error
                        die("ERROR: outside of 'rock', 'paper', 'scissors'!\n");
                    }

                    // cleanup
                    close(i);
                    FD_CLR(i, &master);

                }
            
            }


        }

    }
    DNSServiceRefDeallocate(serviceRef);
}


int main(int argc, char **argv) {
    runGame();
    return 0;
}

