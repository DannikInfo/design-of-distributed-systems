#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX 80
#define PORT 8001
#define IP_FOUND "IP_FOUND"
#define IP_FOUND_ACK "IP_FOUND_ACK"
#define SA struct sockaddr

static char realLogin[16] = {0};
static char realPassword[16] = {0};
static int p[2];
static int forkCount = -1;
static pid_t pid[30] = {0};
static int gConnfd;
static int workEnd = 1;

void *controlThread(void *forkId){
    char pBuff[1];
    while(1){
        read(p[0], pBuff, 1);
        if(pBuff[0] == '1') {
            if (forkId != 0)
                for (; forkCount > 0; forkCount--) {
                    write(p[1], "1", 1);
                    waitpid(pid[forkCount], NULL, 0);
                }

            if (forkId == 0)
                write(gConnfd, "Bye", 3);

            printf("process stop %d\n", getpid());
            while(1){
                if(workEnd)
                    exit(0);
                sleep(1);
            }
        }
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
        if(!session){
            strncpy(login, buff, strlen(realLogin));
            strncpy(password, &buff[strlen(realLogin)+1], strlen(realPassword));
            printf("Client creds: %s %s \n", login, password);
            printf("%d %d \n", strcmp(realLogin, login), strcmp(realPassword, password));
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
                printf("Server stopped %d\n", getpid());
                write(p[1], "1", 1);
                exit(0);
            }
            if (strncmp("exit", buff, 4) == 0) {
                close(connfd);
                printf("Client disconnected\n");
                exit(0);
            }
            workEnd = 0;
            printf("From client: %s", buff);
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

            printf("Parsed: %d %c %d \n", num1, actor, num2);

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
            printf("server accept failed...\n");
            exit(0);
        } else
            printf("server accept the client...\n");

        pid[++forkCount] = fork();
        if (pid[forkCount] < 0) {
            printf("ERROR in new process creation");
        }
        if (pid[forkCount] == 0) {
            close(sockfd);
            worker(connfd, pid[forkCount]);
        } else {
            close(connfd);
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
    server_addr.sin_port = htons(PORT);

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
                    printf("\nClient connection information:\n\t IP: %s, Port: %d\n",
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    sendto(sock, IP_FOUND_ACK, strlen(IP_FOUND_ACK), 0, (struct sockaddr *) &client_addr, addr_len);
                }
            }
        }
    }
}

int main(){
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

    printf("Credentials loaded: %s %s \n", realLogin, realPassword);

    //creating socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");

    if (pipe(p) < 0)
        exit(1);

    pthread_t tid[3];
    pthread_create(&tid[0], NULL, controlThread, (void *) &tid[0]);
    pthread_create(&tid[1], NULL, connectionThread, (void *) sockfd);
    pthread_create(&tid[2], NULL, broadcastThread, (void *) &tid[2]);

    pthread_detach(tid[0]);
    pthread_detach(tid[2]);
    pthread_join(tid[1], NULL);
}