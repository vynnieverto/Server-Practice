#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Port is Keenan's student number currently
#ifndef PORT
  #define PORT 58487
#endif

struct client{
    char name[250];
    char buffer[256];
    int inbuf;
    int fd;
    int available;
    struct in_addr ipaddr;
    struct client *next;
    struct client *opponent;
    int hp;
    int power;
    int read_input;
    int speak;
    int winstreak;
};
static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
//static void broadcast(struct client *top, char *s, int size);
static void broadcast_entry(struct client *top, struct client *chall, int status);
int handleclient(struct client *p, struct client *top);
int bindandlisten(void);
int matchmake(struct client *p1, struct client *p2);
void end_battle(struct client *p1, struct client *p2, int status);
void print_info(struct client *p1, struct client *p2);
void find_opponent(struct client *head);


int main(void){
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    //struct client *bot = NULL;
    //struct client *new = NULL;

    socklen_t len;
    struct sockaddr_in q;
    fd_set allset;
    fd_set rset;
    //struct client *p1 = NULL;
    //struct client *p2 = NULL;
    
    int i;

    int listenfd = bindandlisten(); // this binds this fd to a particular ip address, and makes it an incoming socket.
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd;
    
    while (1){
        rset = allset;
        // MIGHT NEED TIMEOUT HERE!

        nready = select(maxfd + 1, &rset, NULL, NULL, NULL); // TIMEOUT WOULD REPLACE THE 3RD NULL

        // TIMEOUT WOULD NEED
        // if (nready == 0) {
        //     printf("No response from clients in %d seconds\n", SECONDS);
        //     continue;
        // }

        if (nready == -1){
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0){
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd){
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));

            head = addclient(head, clientfd, q.sin_addr);

            // new = addclient(clientfd, q.sin_addr);
            // if (!head){
            //     head = new;
            //     bot = new;
            // } else {
            //     bot->next = new;
            //     bot = bot->next;
            // }
        }

        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

int handleclient(struct client *p, struct client *top) {

    char buf[400];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    //size_t length;
    
    if (len > 0) {
        // Append data to buffer
        if (p->read_input){
            if (p->inbuf > 250) {return 0;}
            strncat(p->buffer, buf, len);
            p->inbuf += len;
        } else {
            return 0;
        }
        // Check if buffer contains a newline character

        if (strlen(p->name) <= 0){
            char *newline_pos = strchr(p->buffer, '\n');
            if (newline_pos != NULL){
                p->read_input = 0;
                *newline_pos = '\0';
                // Extract the name from the buffer
                strncpy(p->name, p->buffer, sizeof(p->name) - 1);
                p->name[sizeof(p->name) - 1] = '\0';  // Ensure null-termination
                p->available = 1;
                
                // Clear the buffer
                memset(p->buffer, 0, sizeof(p->buffer));
                p->inbuf = 0;
                
                // Send welcome message
                char welcome[282];
                snprintf(welcome, sizeof(welcome), "Welcome, %s! Awaiting opponent...\n", p->name);
                send(p->fd, welcome, strlen(welcome), 0);
                broadcast_entry(top, p, 1);
                find_opponent(top);
                return 0;
            } else {
                return 0;
            }
        } else if (p->speak){
            char msg[650];
            char *newline_pos = strchr(p->buffer, '\n');
            struct client *enemy = p->opponent;
            if (newline_pos != NULL){
                *newline_pos = '\0';
                sprintf(msg, "You speak: %s\n", p->buffer);
                if (write(p->fd, msg, strlen(msg)) < 0) {
                    perror("write");
                    exit(1);
                }
                
                sprintf(msg, "%s takes a break to tell you: %s\n", p->name, p->buffer);
                if (write(enemy->fd, msg, strlen(msg)) < 0) {
                    perror("write");
                    exit(1);
                }

                print_info(p, enemy);
                memset(p->buffer, 0, sizeof(p->buffer));
                p->speak = 0;
            }
            return 0;

        }
        else if (!p->available){
            char msg[400];
            char act = p->buffer[0];
            struct client *enemy = p->opponent;
            if (act == 'a'){
                int attack = (rand() % 5) + 2;
                enemy->hp -= attack;
                sprintf(msg, "\nYou hit %s for %d damage!\n", enemy->name, attack);
                if (write(p->fd, msg, strlen(msg)) < 0){
                    perror("write");
                    exit(1);
                }

                sprintf(msg, "%s hits you for %d damage!\n", p->name, attack);
                if (write(enemy->fd, msg, strlen(msg)) < 0){
                    perror("write");
                    exit(1);
                }
                if (enemy->hp <= 0){
                    end_battle(p, enemy, 1);
                    find_opponent(top);
                    find_opponent(top);

                    return 0;
                }
                print_info(enemy, p);
                p->read_input = 0;
                enemy->read_input = 1;

                // sprintf(msg, "(a)ttack\n(p)owermove\n(s)peak something\n");
                // write(enemy->fd, msg, strlen(msg));

                // sprintf(msg, "Waiting for %s to strike...\n", p->name);
                // write(p->fd, msg, strlen(msg));
                memset(p->buffer, 0, sizeof(p->buffer));
                return 0;
            }
            
                // need to check if enemy hp is <= 0

            else if (act == 'p'){
                if (p->power <= 0){
                    memset(p->buffer, 0, sizeof(p->buffer));
                    return 0;

                }
                // ENHANCEMENT greater chance of powerhits hitting.
                int powertoss = rand() % 100;
                if (powertoss <= 50 + p->winstreak){
                    p->power -= 1;
                    int attack = ((rand() % 5) + 2) * 3;
                    enemy->hp -= attack;
                    sprintf(msg, "\nYou hit %s for %d damage!\n", enemy->name, attack);
                    if (write(p->fd, msg, strlen(msg)) < 0){
                        perror("write");
                        exit(1);
                    }

                    sprintf(msg, "\n%s powermoves you for %d damage!\n", p->name, attack);
                    if (write(enemy->fd, msg, strlen(msg)) < 0){
                        perror("write");
                        exit(1);
                    }
                    if (enemy->hp <= 0){
                        end_battle(p, enemy, 1);
                        find_opponent(top);
                        find_opponent(top);
                    } else{
                        print_info(enemy, p);
                    }
                    
                    p->read_input = 0;
                    enemy->read_input = 1;
                    // need to check if enemy hp is <= 0

                } else {
                    p->power -= 1;
                    sprintf(msg, "\nYou missed!\n");
                    if (write(p->fd, msg, strlen(msg)) < 0){
                        perror("write");
                        exit(1);
                    }

                    sprintf(msg, "\n%s missed you!\n", p->name);
                    if (write(enemy->fd, msg, strlen(msg)) < 0){
                        perror("write");
                        exit(1);
                    }
                    print_info(enemy, p);
                    p->read_input = 0;
                    enemy->read_input = 1;
                    
                }
                memset(p->buffer, 0, sizeof(p->buffer));
                return 0;
            } else if (act == 's') {
                p->speak = 1;
                sprintf(msg, "\nSpeak: ");
                if (write(p->fd, msg, strlen(msg)) < 0){
                    perror("write");
                    exit(1);
                }
                // should be done similarly to the name setting part of the method.
                // however i need to change that so do this later. 
                memset(p->buffer, 0, sizeof(p->buffer));
            }
            else{
                memset(p->buffer, 0, sizeof(p->buffer));
            }
            return 0;            
        } 
    } else if (len == 0) {
        // Connection closed by client
        return -1;
    } else {
        // Error reading from socket
        perror("read");
        return -1;
    }
    return 0;
}
    
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    send(fd, "What is your name?", strlen("What is your name?"), 0);

    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    p->available = 0;
    p->read_input = 1;
    p->opponent = NULL;
    p->winstreak = 0;
    memset(p->buffer, 0, sizeof(p->buffer));
    memset(p->name, 0, sizeof(p->name));
    top = p;

    return top;
}



static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next);
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        if ((*p)->available == 0){
            end_battle((*p)->opponent, *p, 0);
        }
        broadcast_entry(top, *p, 0);
        free(*p);
        *p = t;
        find_opponent(top);
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}

/*
static void broadcast(struct client *top, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        write(p->fd, s, size);
    }
    // should probably check write() return value and perhaps remove client 
} */

static void broadcast_entry(struct client *top, struct client *chall, int status){
    struct client *p;
    char s[272];
    size_t len;
    if (status == 1){
        sprintf(s, "**%s enters the arena**\n", chall->name);
        for (p = top; p != NULL; p = p->next){
            if ((!(p == chall)) && (strlen(p->name) > 0)){
                len = strlen(s);
                if (write(p->fd, s, len) < 0){
                    perror("write");
                    exit(1);
                }
            }
        }
        return;
    } else {
        sprintf(s, "**%s leaves**\n", chall->name);
        for (p = top; p!= NULL; p = p->next){
            len = strlen(s);
            if (write(p->fd, s, len) < 0){
                    perror("write");
                    exit(1);
                }
        }
        return;
    }
}
void find_opponent(struct client *head){
    struct client *p;
    struct client *p1 = NULL;
    struct client *p2 = NULL;
    for (p = head; p != NULL; p = p->next){
        if (p->available){
            if (p1 == NULL){
                p1 = p;
            } else if(p2 == NULL && p1->opponent != p && p1 != p){
                p2 = p;
                matchmake(p1, p2);
                return;
            }
        }

    }
}
int matchmake(struct client *p1, struct client *p2){
    char msg[300];
    int cointoss;
    size_t len;
    p1->available = 0;
    p2->available = 0;

    p1->opponent = p2;
    p2->opponent = p1;

    p1->hp = (rand() % 11) + 20;
    p2->hp = (rand() % 11) + 20;

    p1->power = 1 + (rand() % 3);
    p2->power = 1 + (rand() % 3);

    sprintf(msg, "You engage %s on a winsteak of %d!\n", p2->name, p2->winstreak);
    len = strlen(msg);
    if (write(p1->fd, msg, len) < 0){
        perror("write");
        exit(1);
    }
    sprintf(msg, "You engage %s on a winsteak of %d!\n", p1->name, p1->winstreak);
    len = strlen(msg);
    if (write(p2->fd, msg, len) < 0){
        perror("write");
        exit(1);
    }

    cointoss = rand() % 2;
    if (cointoss == 0){
        p1->read_input = 1;
        print_info(p1, p2);

    } else {
        p2->read_input = 1;
        print_info(p2, p1);
    }
    return 1;
}

void end_battle(struct client *p1, struct client *p2, int status){
    char msg[400];
    if (status){
        sprintf(msg, "%s gives up. You win!\n", p2->name);
        if (write(p1->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }

        sprintf(msg, "You are no match for %s. You scurry away...\n", p1->name);
        if (write(p2->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }

        sprintf(msg, "Awaiting next opponent...\n");
        if (write(p1->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }

        sprintf(msg, "Awaiting next opponent...\n");
        if (write(p2->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }

        // WINSTREAK ++
        p1->winstreak++;
        p2->winstreak = 0;

        p1->available = 1;
        p1->read_input = 0;

        p2->available = 1;
        p2->read_input = 0;

    } else {
        sprintf(msg, "--%s dropped. You win!\n\n", p2->name);
        if (write(p1->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }

        sprintf(msg, "Awaiting next opponent...\n");
        if (write(p1->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }
        p1->winstreak++;
        p1->available = 1;
        p1->opponent = NULL;
        p1->read_input = 0;
        

    }
    
}
void print_info(struct client *p1, struct client *p2){
    char msg[300];

    sprintf(msg, "Current Winstreak: %d\nYour hitpoints: %d\nYour powermoves: %d\n", p1->winstreak, p1->hp, p1->power);
    if (write(p1->fd, msg, strlen(msg)) < 0){
        perror("write");
        exit(1);
    }
    
    sprintf(msg, "\n%s's hitpoints: %d\n\n", p2->name, p2->hp);
    if (write(p1->fd, msg, strlen(msg)) < 0){
        perror("write");
        exit(1);
    }

    if (p1->power <= 0){
        sprintf(msg, "(a)ttack\n(s)peak something\n");
        if (write(p1->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }
    } else{
        sprintf(msg, "(a)ttack\n(p)owermove\n(s)peak something\n");
        if (write(p1->fd, msg, strlen(msg)) < 0){
            perror("write");
            exit(1);
        }
    }



    sprintf(msg, "Your hitpoints: %d\nYour powermoves: %d\n", p2->hp, p2->power);
    if (write(p2->fd, msg, strlen(msg)) < 0){
        perror("write");
        exit(1);
    }
    
    sprintf(msg, "\n%s's hitpoints: %d\n", p1->name, p1->hp);
    if (write(p2->fd, msg, strlen(msg)) < 0){
        perror("write");
        exit(1);
    }

    sprintf(msg, "Waiting for %s to strike...\n", p1->name);
    if (write(p2->fd, msg, strlen(msg)) < 0){
        perror("write");
        exit(1);
    }
}