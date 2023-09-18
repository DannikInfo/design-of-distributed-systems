#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()
#include <errno.h>

#define MAX 80
#define PORT 8001
#define SA struct sockaddr

static char realLogin[16] = {0};
static char realPassword[16] = {0};

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

// Function designed for chat between client and server.
void worker(int connfd)
{
    char buff[MAX];
    int session = 0;
    char login[16] = {0};
    char password[16] = {0};


    bzero(buff, MAX);
    strcpy(buff, "Please log in: \n");
    write(connfd, buff, sizeof(buff));


    for (;;) {
        bzero(buff, MAX);
        read(connfd, buff, sizeof(buff));
        if(!session){
            strncpy(login, buff, strlen(realLogin));
            strncpy(password, &buff[strlen(realLogin)+1], strlen(realPassword));
            printf("Client creds: %s %s \n", login, password);
            if(strcmp(realLogin, login) == 0 && strcmp(realPassword, password) == 0) {
                session = 1;

                bzero(buff, MAX);
                strcpy(buff, "Success log in! \n Send operation: \n");
                write(connfd, buff, sizeof(buff));
            }else{
                bzero(buff, MAX);
                strcpy(buff, "Log in failed! Try again: \n");
                write(connfd, buff, sizeof(buff));
            }
        }else{
            printf("From client: %s", buff);
            int num1 = 0, num2 = 0;
            int cursor = 0;
            char actor = 0;
            char current[32] = {0};
            int j = 0;
            errno = 0;
            for(int i = 0; i < sizeof(buff); i++){
                if(buff[i] != ' ' && i+1 != sizeof(buff))
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
                continue;
            }


            bzero(buff, MAX);
            if(actor != '!')
                sprintf(buff, "Answer is: %f \n", calc(actor, num1, num2));
            else
                sprintf(buff, "Answer is: %llu \n", calcFact(num1));

            write(connfd, buff, sizeof(buff));

            if (strncmp("exit", buff, 4) == 0) {
                printf("Server Exit...\n");
                break;
            }
        }
    }
}

// Driver function
int main(){
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    //Load credentials
    FILE * fp;
    char * line = NULL;
    size_t lineLen = 0;

    fp = fopen("creds.data", "r");
    getline(&line, &lineLen, fp);
    strncpy(realLogin, line, lineLen-4);
    getline(&line, &lineLen, fp);
    strncpy(realPassword, line, lineLen-3);
    fclose(fp);

    printf("Credentials loaded: %s %s \n", realLogin, realPassword);

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
    len = sizeof(cli);

    // Accept the data packet from client and verification
    connfd = accept(sockfd, (SA*)&cli, &len);
    if (connfd < 0) {
        printf("server accept failed...\n");
        exit(0);
    }
    else
        printf("server accept the client...\n");

    // Function for chatting between client and server
    worker(connfd);

    // After chatting close the socket
    close(sockfd);
}
