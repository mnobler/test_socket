#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>

#define PORT1 65432
#define PORT2 65433
#define BUFFER_SIZE 1024

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
} Peer;

int running = 1;

void* handle_connection(void* arg);
void* periodic_request(void* arg);
void get_current_time(char* buffer, size_t len);
void stop(int sig);

int main() {
    signal(SIGINT, stop);

    // Create two threads for two peers
    pthread_t peer1_thread, peer2_thread;

    Peer peer1, peer2;

    // Setup peer1
    peer1.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    peer1.addr.sin_family = AF_INET;
    peer1.addr.sin_addr.s_addr = INADDR_ANY;
    peer1.addr.sin_port = htons(PORT1);

    // Setup peer2
    peer2.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    peer2.addr.sin_family = AF_INET;
    peer2.addr.sin_addr.s_addr = INADDR_ANY;
    peer2.addr.sin_port = htons(PORT2);

    // Bind and listen for peer1
    bind(peer1.sockfd, (struct sockaddr*)&peer1.addr, sizeof(peer1.addr));
    listen(peer1.sockfd, 5);

    // Bind and listen for peer2
    bind(peer2.sockfd, (struct sockaddr*)&peer2.addr, sizeof(peer2.addr));
    listen(peer2.sockfd, 5);

    // Create threads to handle connections
    pthread_create(&peer1_thread, NULL, handle_connection, (void*)&peer1);
    pthread_create(&peer2_thread, NULL, handle_connection, (void*)&peer2);

    // Create threads to send periodic requests
    pthread_t req1_thread, req2_thread;
    pthread_create(&req1_thread, NULL, periodic_request, (void*)&peer2);
    pthread_create(&req2_thread, NULL, periodic_request, (void*)&peer1);

    // Wait for threads to finish
    pthread_join(peer1_thread, NULL);
    pthread_join(peer2_thread, NULL);

    close(peer1.sockfd);
    close(peer2.sockfd);

    return 0;
}

void* handle_connection(void* arg) {
    Peer* peer = (Peer*)arg;
    fd_set readfds;
    int max_sd, activity, new_socket;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    while (running) {
        FD_ZERO(&readfds);
        FD_SET(peer->sockfd, &readfds);
        max_sd = peer->sockfd;

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && running) {
            perror("select error");
        }

        if (FD_ISSET(peer->sockfd, &readfds)) {
            new_socket = accept(peer->sockfd, (struct sockaddr*)&client_addr, &addr_len);
            if (new_socket < 0) {
                perror("accept error");
                continue;
            }

            memset(buffer, 0, BUFFER_SIZE);
            read(new_socket, buffer, BUFFER_SIZE);
            if (strcmp(buffer, "GET_TIME") == 0) {
                get_current_time(buffer, BUFFER_SIZE);
                send(new_socket, buffer, strlen(buffer), 0);
            }

            close(new_socket);
        }
    }

    return NULL;
}

void* periodic_request(void* arg) {
    Peer* peer = (Peer*)arg;
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    while (running) {
        sleep(rand() % 5 + 5); // Sleep for 5 to 10 seconds

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation error");
            continue;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = peer->addr.sin_port;
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            close(sock);
            continue;
        }

        send(sock, "GET_TIME", strlen("GET_TIME"), 0);
        memset(buffer, 0, BUFFER_SIZE);
        read(sock, buffer, BUFFER_SIZE);
        printf("Peer %d received time: %s\n", ntohs(peer->addr.sin_port), buffer);

        close(sock);
    }

    return NULL;
}

void get_current_time(char* buffer, size_t len) {
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    strftime(buffer, len, "%Y-%m-%d %H:%M:%S", tm_info);
}

void stop(int sig) {
    running = 0;
    printf("Stopping peers...\n");
}
