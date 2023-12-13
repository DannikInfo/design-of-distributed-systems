#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX 1024
#define PORT 8000
#define IP_FOUND "IP_FOUND"
#define IP_FOUND_ACK "IP_FOUND_ACK"
#define SA struct sockaddr

void func(int sockfd)
{
    char buff[MAX];
    int n;

    bzero(buff, sizeof(buff));
    read(sockfd, buff, sizeof(buff));
    printf("%s", buff);

    for (;;) {
        bzero(buff, sizeof(buff));
        n = 0;
        while ((buff[n++] = getchar()) != '\n');
        write(sockfd, buff, sizeof(buff));

        if ((strncmp(buff, "exit", 4)) == 0 || (strncmp(buff, "stop", 4)) == 0) {
            close(sockfd);
            printf("Client Exit...\n");
            break;
        }

        bzero(buff, sizeof(buff));
        read(sockfd, buff, sizeof(buff));
        if ((strncmp(buff, "startList", 9)) == 0) {
            sprintf(buff, "Servers list: \n");
            while(strncmp(buff, "endList", 7) != 0){
                write(sockfd, "ack", 4);
                printf("%s \n", buff);
                read(sockfd, buff, sizeof(buff));
            }
            continue;
        }
        printf("%s ", buff);

        if ((strncmp(buff, "Bye", 3)) == 0) {
            printf("Server stopped...\n");
            break;
        }
    }
}

struct sockaddr_in getServer(){
    int sock;
    struct sockaddr_in broadcast_addr;
    struct sockaddr_in server_addr;
    int yes = 1;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 2000;
    int addr_len;
    int ret, ret2;
    fd_set readfd;
    char buffer[1024];
    int found = 0;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("sock error");
        return server_addr;
    }
    ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes));
    ret2 = setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (ret == -1 || ret2 == -1) {
        perror("setsockopt error");
        return server_addr;
    }

    addr_len = sizeof(struct sockaddr_in);

    memset((void*)&broadcast_addr, 0, addr_len);
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(PORT);

    while(!found){
        sendto(sock, IP_FOUND, strlen(IP_FOUND), 0, (struct sockaddr*) &broadcast_addr, addr_len);

        FD_ZERO(&readfd);
        FD_SET(sock, &readfd);

        ret = select(sock + 1, &readfd, NULL, NULL, NULL);

        if (ret > 0) {
            if (FD_ISSET(sock, &readfd)) {
                recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&server_addr, &addr_len);
                if (strstr(buffer, IP_FOUND_ACK)) {
                    found = 1;
                    char port[6];
                    strcpy(port, buffer+strlen(IP_FOUND_ACK));
                    server_addr.sin_port = htons(atoi(port));
                    printf("\tFound server IP is %s, Port is %d\n", inet_ntoa(server_addr.sin_addr),htons(server_addr.sin_port));
                }
            }
        }
    }
    return server_addr;
}

int main()
{
    int sockfd;
    struct sockaddr_in servaddr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    servaddr = getServer();

    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    func(sockfd);
    close(sockfd);
}
