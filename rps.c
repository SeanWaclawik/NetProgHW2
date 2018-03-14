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


// need to make this selected by the operating system
#define PORT "9421"

#define ZCONF_NAME "rps_hw2"
#define ZCONF_TYPE "_gtn._tcp"
#define ZCONF_DOMAIN "local"


void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}


int takeUserInput(char *name, int *invalid) {
    int current_guess;

    if(strstr(name, "guess ") != NULL) {
    	int i;
    	char guess_string[3];

    	for (i = 6; i < sizeof(name)/sizeof(name[0]); ++i) {
    		guess_string[i-6] = name[i];
    	}

    	if ((current_guess = atoi(guess_string)) == 0) {
            *invalid = 1;
    		return -1;
    	}

	} else {
        *invalid = 1;
		return -1;
	}

    return current_guess;
}

void runGame () {
    int sockfd, new, maxfd, on = 1
    int nready, i, number_to_guess, guess_count;

    struct addrinfo *res0, *res, hints;
    char buffer[BUFSIZ];
    fd_set master, readfds;
    int error;
    ssize_t nbytes;
    (void)memset(&hints, '\0', sizeof(struct addrinfo));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (0 != (error = getaddrinfo(NULL, PORT, &hints, &res0))) {
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

    int dns_sd_fd = DNSServiceRefSockFD(serviceRef);

    FD_ZERO(&master);
    FD_ZERO(&readfds);

    FD_SET(sockfd, &master);
    FD_SET(dns_sd_fd, &master);

    maxfd = sockfd;

    while (1) {
        memcpy(&readfds, &master, sizeof(master));

        if (-1 == (nready = select(maxfd+1, &readfds, NULL, NULL, NULL))) {
            die("select()");
        }

        for (i=0; i<=maxfd && nready>0; i++) {
            if (FD_ISSET(i, &readfds)) {
                nready--;

                if (i == sockfd) {
                    printf("Received a new connection\n");

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

                        number_to_guess = rand() % 100 + 1;
                        guess_count = 0;
                    }
                } else {
                    (void)printf("recv() data from one of descriptors(s)\n");

                    nbytes = recv(i, buffer, sizeof(buffer), 0);
                    if (nbytes <= 0) {
                        if (EWOULDBLOCK != errno) {
                            die("recv()");
                        }

                        break;
                    }

                    buffer[nbytes] = '\0';
                    printf("%s", buffer);

                    int invalid;
                    int current_guess = takeUserInput(buffer, &invalid);

                    if(current_guess == -1 && invalid == 1) {
                        send(i, (const void *)"???\n", 4, 0);
                    } else if (number_to_guess == current_guess) {
                        ++guess_count;
                        double high, low;
                        high = (log((double)100)/log((double)2)) + 1;
                        low = (log((double)100)/log((double)2)) + 1;

                        if (guess_count > high) {
                            send(i, (const void *)"CORRECT\nBETTER LUCK NEXT TIME\n", 30, 0);
                        } else if (guess_count < low) {
                            send(i, (const void *)"CORRECT\nGREAT GUESSING\n", 23, 0);
                        } else {
                            send(i, (const void *)"CORRECT\nAVERAGE\n", 16, 0);
                        }

                        close(i);
                        FD_CLR(i, &master);
                	} else if (number_to_guess > current_guess) {
                        ++guess_count;
                        send(i, (const void *)"HIGHER\n", 7, 0);
                		printf("HIGHER\n");
                	} else if (number_to_guess < current_guess) {
                        ++guess_count;
                        send(i, (const void *)"LOWER\n", 6, 0);
                		printf("LOWER\n");
                	}

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

