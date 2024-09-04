#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <ctime>
#include <csignal>
#include <atomic>
#include <mutex>

#define PORT1 65432
#define PORT2 65433
#define BUFFER_SIZE 1024

std::atomic<bool> running(true);

class Peer {
public:
    Peer(int port) : port(port) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Errore nella creazione del socket");
            exit(EXIT_FAILURE);
        }
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
    }

    void start_server() {
        if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Errore nel binding del socket");
            exit(EXIT_FAILURE);
        }
        if (listen(sockfd, 5) < 0) {
            perror("Errore nel listen del socket");
            exit(EXIT_FAILURE);
        }
        std::thread(&Peer::handle_connections, this).detach();
    }

    void start_client(Peer* other_peer) {
        std::thread(&Peer::periodic_request, this, other_peer).detach();
    }

    void stop() {
        running = false;
        close(sockfd);
        std::cout << "Peer sulla porta " << port << " chiuso.\n";
    }

private:
    int sockfd;
    int port;
    struct sockaddr_in addr;

    void handle_connections() {
        fd_set readfds;
        char buffer[BUFFER_SIZE];
        while (running) {
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);
            int activity = select(sockfd + 1, &readfds, NULL, NULL, NULL);

            if (activity < 0 && running) {
                perror("Errore nella select");
                continue;
            }

            if (FD_ISSET(sockfd, &readfds)) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int new_socket = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
                if (new_socket < 0) {
                    perror("Errore nella accept");
                    continue;
                }
                memset(buffer, 0, BUFFER_SIZE);
                read(new_socket, buffer, BUFFER_SIZE);
                if (strcmp(buffer, "GET_TIME") == 0) {
                    send_time(new_socket);
                }
                close(new_socket);
            }
        }
    }

    void periodic_request(Peer* other_peer) {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(rand() % 5 + 5));

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("Errore nella creazione del socket");
                continue;
            }

            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(other_peer->port);
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Errore nella connessione");
                close(sock);
                continue;
            }

            send(sock, "GET_TIME", strlen("GET_TIME"), 0);
            char buffer[BUFFER_SIZE] = {0};
            read(sock, buffer, BUFFER_SIZE);
            std::cout << "Peer " << port << " ha ricevuto il tempo: " << buffer << std::endl;

            close(sock);
        }
    }

    void send_time(int socket) {
        char buffer[BUFFER_SIZE];
        std::time_t t = std::time(nullptr);
        std::strftime(buffer, BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        send(socket, buffer, strlen(buffer), 0);
        std::cout << "Inviato il tempo corrente a un peer.\n";
    }
};

void signal_handler(int signum) {
    running = false;
    std::cout << "Interruzione ricevuta, chiusura dei peer...\n";
}

int main() {
    srand(time(0));
    signal(SIGINT, signal_handler);

    Peer peer1(PORT1);
    Peer peer2(PORT2);

    peer1.start_server();
    peer2.start_server();

    peer1.start_client(&peer2);
    peer2.start_client(&peer1);

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    peer1.stop();
    peer2.stop();

    return 0;
}
