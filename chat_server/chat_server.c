#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_CLIENTS 50
#define BUFFER_SZ 1024
#define NICKNAME_LEN 32
#define PORT 8888

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    char nickname[NICKNAME_LEN];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *log_file;

// Log
void log_event(const char *message) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';

    pthread_mutex_lock(&clients_mutex);
    fprintf(log_file, "[%s] %s\n", time_str, message);
    fflush(log_file);
    printf("[LOG] %s\n", message);
    fflush(stdout);

    pthread_mutex_unlock(&clients_mutex);
}

// general message
void send_message(char *msg, int except_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->sockfd != except_fd) {
            send(clients[i]->sockfd, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// private message
void send_private_message(char *msg, char *target_nick, int sender_fd) {
    pthread_mutex_lock(&clients_mutex);
    int found = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && strcmp(clients[i]->nickname, target_nick) == 0) {
            send(clients[i]->sockfd, msg, strlen(msg), 0);
            found = 1; break;
        }
    }
    if (!found) {
        char err_msg[BUFFER_SZ];
        snprintf(err_msg, sizeof(err_msg), "User '%s' not found.\n", target_nick);
        send(sender_fd, err_msg, strlen(err_msg), 0);
    }
    pthread_mutex_unlock(&clients_mutex);
}

//thread for clients
void *handle_client(void *arg) {
    char buffer[BUFFER_SZ], msg[BUFFER_SZ + NICKNAME_LEN];
    client_t *cli = (client_t *)arg;

    // Nickname
    while (1) {
        send(cli->sockfd, "Enter your nickname: ", 22, 0);
        int len = recv(cli->sockfd, cli->nickname, NICKNAME_LEN, 0);
        if (len <= 0) return NULL;

        cli->nickname[strcspn(cli->nickname, "\n")] = 0;

        int unique = 1;
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] && strcmp(clients[i]->nickname, cli->nickname) == 0) {
                unique = 0;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        if (unique)
        {
                send(cli->sockfd, "\nNickname accepted! You can now chat.\n", 40, 0);
                break;

        }
        else
                send(cli->sockfd, "Nickname already taken or invalid. Please choose another: ", 25, 0);
    }

    char connect_msg[BUFFER_SZ];
    sprintf(connect_msg, "Client %s connected from %s:%d", cli->nickname, inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
    log_event(connect_msg);

    // add list
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cli;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex); snprintf(msg, sizeof(msg), "%s has joined the chat.\n", cli->nickname);
    send_message(msg, cli->sockfd);

    // message
    while (1) {
        int len = recv(cli->sockfd, buffer, BUFFER_SZ - 1, 0);
        if (len <= 0) break;

        buffer[len] = '\0';
        buffer[strcspn(buffer, "\n")] = 0;

        if (buffer[0] == '@') {
            char target[NICKNAME_LEN];
            sscanf(buffer, "@%s", target);
            char *private_msg = strchr(buffer, ' ');
            if (private_msg) {
                snprintf(msg, sizeof(msg), "[Private from %s]: %s\n", cli->nickname, private_msg + 1);
                send_private_message(msg, target, cli->sockfd);
                
		char log_msg[BUFFER_SZ + NICKNAME_LEN];
                snprintf(log_msg, sizeof(log_msg), "Message from %s: %s", cli->nickname, buffer);
		log_event(log_msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "[%s] %s\n", cli->nickname, buffer);
            send_message(msg, cli->sockfd);

	    char log_msg[BUFFER_SZ + NICKNAME_LEN];
	    snprintf(log_msg, sizeof(log_msg), "Message from %s: %s", cli->nickname, buffer);
            log_event(log_msg);
        }
    }

    // unconnecting
    snprintf(msg, sizeof(msg), "%s left the chat.\n", cli->nickname);
    log_event(msg);
    send_message(msg, cli->sockfd);

    close(cli->sockfd);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == cli) {
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    free(cli);
    return NULL;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT); server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_fd, 10);
    printf("Chat server started on port %d\n", PORT);

    log_file = fopen("chat_server.log", "a");
    if (!log_file) {
        perror("Log file error");
        return 1;
    }
    log_event("Chat server started.");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = client_addr;
        cli->sockfd = client_fd;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)cli);
        pthread_detach(tid);
    }

    fclose(log_file);
    return 0;
}
