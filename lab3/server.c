#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdarg.h>

#define MAX 1024
#define IP_FOUND "IP_FOUND"
#define IP_FOUND_ACK "IP_FOUND_ACK"
#define IP_REGISTER "IP_REGISTER"
#define MSG "MSG"
#define ACK "ACK"
#define SA struct sockaddr

static char realLogin[16] = {0};
static char realPassword[16] = {0};
static int p[2];
static int forkCount = -1;
static pid_t pid[30] = {0};
static int gConnfd;
static int workEnd = 1;
pthread_t tid[4];

struct servers {
    char address[16];
    int port;
    struct servers* next;
} servers;

void log(char *s_log, ...){
	char hc_logtext_tmp[10000] = {0};
	va_list va_var;
	va_start(va_var, s_log);	
    vsnprintf(hc_logtext_tmp,10000,s_log,va_var);
	fprintf(stdout,"%d | %s", getpid(), hc_logtext_tmp);
    fflush(stdout);
    va_end(va_var);
}

void addServer(const char* address, int port, struct servers** head) {
    struct servers* tmp = (struct servers*)malloc(sizeof(struct servers));
    strcpy(tmp->address, address);
    tmp->port = port;
    tmp->next = NULL;
    
    if (*head == NULL) {
        *head = tmp;
        return;
    }
    
    struct servers* p = *head;
    while (p->next != NULL) {
        p = p->next;
    }
    
    p->next = tmp;
}

void removeServer(const char* address, struct servers** head) {
    struct servers* p = *head;
    struct servers* prev = NULL;

    while (p != NULL) {
        if (strcmp(p->address, address) == 0) {
            if (prev == NULL) {
                // Removing the first element
                *head = p->next;
            } else {
                // Removing a middle element
                prev->next = p->next;
            }

            free(p);
            return;
        }

        prev = p;
        p = p->next;
    }
}

void printList(struct servers** head) {
    struct servers* p = *head;
    while (p != NULL) {
        log("%s:%d \n", p->address, p->port);
        p = p->next;
    }
    log("\n");
}


struct servers* getHead(struct servers** list) {
    return *list;
}

void *controlThread(void *forkId){
    char pBuff[1];
    while(1){
        read(p[0], pBuff, 1);
        if(pBuff[0] == '1') {
            write(p[1], "1", 1);
            if (forkId != 0) {
                for (; forkCount > 0; forkCount--) {
                    write(p[1], "1", 1);
                    waitpid(pid[forkCount], NULL, 0);
                }
                log("process stop %d\n", getpid());
                exit(0);
            }

            if (forkId == 0) {
                log("process stop %d\n", getpid());
                while(1) {
                    if (workEnd) {
                        write(gConnfd, "Bye", 3);
                        exit(0);
                    }
                }
            }
        }
        sleep(1);
    }
}

double calc(char actor, int num1, int num2){
    switch(actor){
        case '+':
            return (double)(num1 + num2);
        case '*':
            return (double)(num1 * num2);
        case '/':
            return (double)(num1) / num2;
        case '-':
            return (double)(num1 - num2);
        case '%':
            return (double)(num1 % num2);
        default:
            break;
    }
    return -1;
}

uint64_t calcFact(int num){
    uint64_t fact = 1;
    for (int i = 1; i <= num; ++i)
        fact *= i;
    return fact;
}

void sendMSG(char *msg){
    int sockfd;
    struct sockaddr_in servaddr;
    struct servers* p = getHead(&servers);
    char buff[MAX];
    while (p != NULL) {
        bzero(&servaddr, sizeof(servaddr));
        log("NetChecker: check node %s:%d \n", p->address, p->port);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &(int){2}, sizeof(int));
        if (sockfd == -1) {
            log("MSGSender: socket creation failed...\n");
            exit(0);
        }
        else
            log("MSGSender: Socket successfully created..\n");

        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(p->address);
        servaddr.sin_port = htons(p->port);

        if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
            log("MSGSender: connection with the server %s:%d failed...\n", p->address, p->port);
        }
        else{
            log("NetChecker: connected to the server %s:%d\n", p->address, p->port);
            write(sockfd, msg, sizeof(msg));
            log("MSGSender: message was send!");
        }
        p = p->next;
    }
}

void worker(int connfd, int forkId)
{
    char buff[MAX];
    int session = 0;
    char login[16] = {0};
    char password[16] = {0};
    gConnfd = connfd;

    bzero(buff, MAX);
    strcpy(buff, "Please log in: \n");
    write(connfd, buff, sizeof(buff));

    pthread_t tid;
    pthread_create(&tid, NULL, controlThread, (void *) forkId);
    pthread_detach(tid);

    while (1) {
        bzero(buff, MAX);
        read(connfd, buff, sizeof(buff));
        if(strncmp(buff, "ping", 4) == 0){
            close(connfd);
            log("NetChecker ACK: pong\n");
            exit(0);
        }
        if (strncmp("remsg", buff, 5) == 0) {
            log("MSGReceiver - %s\n", buff+strlen("remsg"));
            close(connfd);
            exit(0);
        }
        if(!session){
            strncpy(login, buff, strlen(realLogin));
            strncpy(password, &buff[strlen(realLogin)+1], strlen(realPassword));
            log("Client creds: %s %s \n", login, password);
            log("%d %d \n", strcmp(realLogin, login), strcmp(realPassword, password));
            if(strcmp(realLogin, login) == 0 && strcmp(realPassword, password) == 0) {
                session = 1;

                bzero(buff, MAX);
                strcpy(buff, "Success log in!\n Send operation: \n");
                write(connfd, buff, sizeof(buff));
            }else{
                bzero(buff, MAX);
                strcpy(buff, "Log in failed! Try again: \n");
                write(connfd, buff, sizeof(buff));
            }
        }else{
            if (strncmp("stop", buff, 4) == 0) {
                close(connfd);
                log("Server stopped %d\n", getpid());
                write(p[1], "1", 1);
                exit(0);
            }
            if (strncmp("exit", buff, 4) == 0) {
                close(connfd);
                log("Client disconnected\n");
                exit(0);
            }
            if (strncmp("list", buff, 4) == 0) {
                write(connfd, "startList", 10);
                read(connfd, buff, sizeof(buff));
                struct servers* p = getHead(&servers);
                while (p != NULL) {
                    sprintf(buff, "%s:%d", p->address, p->port);
                    write(connfd, buff, sizeof(buff));
                    read(connfd, buff, sizeof(buff));
                    p = p->next;
                }
                write(connfd, "endList", 8);
                continue;
            }
            if (strncmp("msg", buff, 3) == 0) {
                char tmpBuff[MAX+3];
                sprintf(tmpBuff, "re%s", buff);
                sendMSG(tmpBuff);
                sprintf(buff, "MSGSender: message was send to all servers\n");
                write(connfd, buff, sizeof(buff));
                continue;
            }
            workEnd = 0;
            log("From client: %s", buff);
            int num1 = 0, num2 = 0;
            int cursor = 0;
            char actor = 0;
            char current[32] = {0};
            int j = 0;
            errno = 0;
            for(int i = 0; i < strlen(buff)+1; i++){
                if(buff[i] != ' ' && i+1 != strlen(buff)+1)
                    current[j++] = buff[i];
                else{
                    switch (cursor) {
                        case 0:
                            if(current[0] == '!')
                                actor = '!';
                            else
                                num1 = strtol(current, NULL, 10);

                            break;
                        case 1:
                            if(actor == '!')
                                num1 = strtol(current, NULL, 10);
                            else
                                actor = current[0];
                            break;
                        case 2:
                            if(actor != '!')
                                num2 = strtol(current, NULL, 10);
                            break;
                    }
                    cursor++;
                    j=0;
                }
            }

            log("Parsed: %d %c %d \n", num1, actor, num2);

            if(errno != 0){
                bzero(buff, MAX);
                strcpy(buff, "Wrong format!! NUMBER ACTOR NUMBER or ! NUMBER \n");
                write(connfd, buff, sizeof(buff));
                workEnd = 1;
                continue;
            }

            bzero(buff, MAX);
            if(actor != '!')
                sprintf(buff, "Answer is: %f \n", calc(actor, num1, num2));
            else
                sprintf(buff, "Answer is: %llu \n", calcFact(num1));
            write(connfd, buff, sizeof(buff));
            workEnd = 1;
            log("work ended %d\n", workEnd);
        }
    }
}

void getServers(){
    int sock;
    struct sockaddr_in broadcast_addr;
    struct sockaddr_in server_addr;
    int yes = 1;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int addr_len;
    int ret, ret2;
    fd_set readfd;
    char buffer[1024];
    time_t timeStart;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("sock error");
        return;
    }
    ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes));
    ret2 = setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    if (ret == -1 || ret2 == -1) {
        perror("setsockopt error");
        return;
    }

    addr_len = sizeof(struct sockaddr_in);

    memset((void*)&broadcast_addr, 0, addr_len);
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(8000);

    log("Start initial find servers \n");
    sprintf(buffer, "%s%s", IP_REGISTER, getenv("PORT"));
    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*) &broadcast_addr, addr_len);
    timeStart = time(NULL);
    while((unsigned long)time(NULL) - timeStart < 3){
        FD_ZERO(&readfd);
        FD_SET(sock, &readfd);

        ret = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&server_addr, &addr_len);
                    
        if (ret > 0) {
            if (FD_ISSET(sock, &readfd)) {
                if (strstr(buffer, IP_FOUND_ACK)) {
                    char port[6];
                    strcpy(port, buffer+strlen(IP_FOUND_ACK));
                    log("\tFound server IP is %s, Port is %s\n", inet_ntoa(server_addr.sin_addr), port);
                    addServer(inet_ntoa(server_addr.sin_addr),atoi(port), &servers);
                }
            }
        }
    }
}

void *connectionThread(void* arg){
    int connfd, len;
    struct sockaddr_in cli;
    len = sizeof(cli);
    int sockfd = (int) arg;

    while(1) {
        connfd = accept(sockfd, (SA *) &cli, &len);
        if (connfd < 0) {
            log("server accept failed...\n");
            exit(0);
        } else
            log("server accept the client...\n");

        pid[++forkCount] = fork();
        if (pid[forkCount] < 0) {
            log("ERROR in new process creation");
        }
        if (pid[forkCount] == 0) {
            close(sockfd);
            worker(connfd, pid[forkCount]);
        } else {
            close(connfd);
        }
    }
}

void *netCheckerThread(void* arg){
    int sockfd;
    struct sockaddr_in servaddr;
    while(1){
        sleep(10);
        struct servers* p = getHead(&servers);
        while (p != NULL) {
            bzero(&servaddr, sizeof(servaddr));
            log("NetChecker: check node %s:%d \n", p->address, p->port);
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &(int){2}, sizeof(int));
            if (sockfd == -1) {
                log("NetChecker: socket creation failed...\n");
                exit(0);
            }
            else
                log("NetChecker: Socket successfully created..\n");

            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr(p->address);
            servaddr.sin_port = htons(p->port);

            if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
                log("NetChecker: connection with the server failed... Remove from server list\n");
                removeServer(p->address, &servers);
                close(sockfd);
            }
            else{
                log("NetChecker: connected to the server %s:%d\n", p->address, p->port);
                write(sockfd, "ping", sizeof("ping"));
                close(sockfd);
            }
            p = p->next;
            sleep(5);
        }
    }
}

void *broadcastThread(void* arg) {
    int sock;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    int addr_len;
    int ret;
    fd_set readfd;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("sock error\n");
        return 0;
    }

    addr_len = sizeof(struct sockaddr_in);

    memset((void*)&server_addr, 0, addr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons(8000);

    ret = bind(sock, (struct sockaddr*)&server_addr, addr_len);
    if (ret < 0) {
        perror("bind error\n");
        return 0;
    }
    while(1) {
        FD_ZERO(&readfd);
        FD_SET(sock, &readfd);

        ret = select(sock + 1, &readfd, NULL, NULL, 0);
        if (ret > 0) {
            if (FD_ISSET(sock, &readfd)) {
                recvfrom(sock, buffer, 1024, 0, (struct sockaddr *) &client_addr, &addr_len);
                if (strstr(buffer, IP_FOUND)) {
                    log("Client connection information:\n\t IP: %s, Port: %d\n",
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    char msg[1024] = {0};
                    sprintf(msg, "%s%s", IP_FOUND_ACK, getenv("PORT"));
                    sendto(sock, msg, strlen(msg), 0, (struct sockaddr *) &client_addr, addr_len);
                }
                if (strstr(buffer, IP_REGISTER)) {
                    char port[6];
                    strcpy(port, buffer+strlen(IP_REGISTER));
                    log("New server in network found:\n\t IP: %s, Port: %s\n", inet_ntoa(client_addr.sin_addr), port);
                    char msg[1024] = {0};
                    sprintf(msg, "%s%s", IP_FOUND_ACK, getenv("PORT"));
                    sendto(sock, msg, strlen(msg), 0, (struct sockaddr *) &client_addr, addr_len);
                    addServer(inet_ntoa(client_addr.sin_addr),atoi(port), &servers);
                }
                if (strstr(buffer, MSG)) {
                    log("Message from:\n\t IP: %s, Port: %d\n",
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    sendto(sock, ACK, strlen(ACK), 0, (struct sockaddr *) &client_addr, addr_len);
                    log("Text is: %s", buffer+strlen(MSG));
                }
            }
        }
    }
}

int main(){    
    log("Starting server\n");

    int sockfd;
    struct sockaddr_in servaddr;

    //Load credentials
    FILE * fp;
    char * line = NULL;
    size_t lineLen = 0;

    fp = fopen("creds.data", "r");
    getline(&line, &lineLen, fp);
    strncpy(realLogin, line, lineLen);
    realLogin[strlen(realLogin)-1] = '\0';
    getline(&line, &lineLen, fp);
    strncpy(realPassword, line, lineLen-3);
    fclose(fp);

    log("Credentials loaded: %s %s \n", realLogin, realPassword);

    getServers();
    //creating socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        log("socket creation failed...\n");
        exit(0);
    }
    else
        log("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(getenv("PORT")));

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        log("socket bind failed...\n");
        exit(0);
    }
    else
        log("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        log("Listen failed...\n");
        exit(0);
    }
    else
        log("Server listening..\n");

    if (pipe(p) < 0)
        exit(1);


    pthread_create(&tid[0], NULL, controlThread, (void *) &tid[0]);
    pthread_create(&tid[1], NULL, connectionThread, (void *) sockfd);
    pthread_create(&tid[2], NULL, broadcastThread, (void *) &tid[2]);
    pthread_create(&tid[3], NULL, netCheckerThread, (void *) &tid[3]);

    pthread_detach(tid[0]);
    pthread_detach(tid[2]);
    pthread_detach(tid[3]);
    pthread_join(tid[1], NULL);
}