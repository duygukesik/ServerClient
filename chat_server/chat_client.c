#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SZ 1024
int sockfd;

void *recv_handler(void *arg) {
    char buffer[BUFFER_SZ];
    while (1) {
        int len = recv(sockfd, buffer, BUFFER_SZ - 1, 0);
        if (len <= 0) break;
        buffer[len] = '\0';
        printf("%s", buffer);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    char *server_ip = (argc >= 2) ? argv[1] : "127.0.0.1";
    int port = (argc >= 3) ? atoi(argv[2]) : 8888;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("Connection to server failed");
        return 1;
    }
    
        printf("Connected to chat server at 127.0.0.1: %d\n", PORT);
        printf("Commands:\n - Type '@nickname message' for private message\n - Type any other message for broadcast\n - Type 'quit' to exit\n------------------------------\n");


    char nickname[BUFFER_SZ];
    recv(sockfd, nickname, BUFFER_SZ, 0);
    printf("%s", nickname);
    fgets(nickname, BUFFER_SZ, stdin);
    send(sockfd, nickname, strlen(nickname), 0);

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, recv_handler, NULL);

    char buffer[BUFFER_SZ];
    while (fgets(buffer, BUFFER_SZ, stdin)) {
        send(sockfd, buffer, strlen(buffer), 0);
    }

    close(sockfd);
    return 0;
}

