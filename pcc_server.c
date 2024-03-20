#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define LISTEN_QUEUE_SIZE 10
#define BUFFER_SIZE 1024
#define PRINTABLE_MIN 32
#define PRINTABLE_MAX 126

volatile sig_atomic_t sigint_received = 0;
uint16_t pcc_total[95] = {0}; // Use uint16_t for 16-bit unsigned integers.

void handle_sigint(int sig) {
    sigint_received = 1;
}

void print_statistics_and_exit() {
    for (int i = 0; i < 95; i++) {
        if (pcc_total[i] > 0) { // Only print characters that were counted.
            printf("char '%c' : %hu times\n", i + PRINTABLE_MIN, pcc_total[i]);
        }
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <Server Port>\n", argv[0]);
        exit(1);
    }

    int server_port = atoi(argv[1]);
    int server_fd, client_fd;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);

    signal(SIGINT, handle_sigint);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(1);
    }

    // Use SO_REUSEADDR to allow quick reuse of the port.
    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_port);

    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, LISTEN_QUEUE_SIZE) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(1);
    }

    while (!sigint_received) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
        if (client_fd < 0) {
            if (errno == EINTR && sigint_received) {
                print_statistics_and_exit();
            }
            perror("accept failed");
            continue;
        }

        uint16_t net_file_size;
        if (recv(client_fd, &net_file_size, sizeof(net_file_size), 0) != sizeof(net_file_size)) {
            perror("recv failed");
            close(client_fd);
            continue;
        }

        uint16_t file_size = ntohs(net_file_size);
        uint16_t printable_count = 0;
        char buffer[BUFFER_SIZE];
        int bytes_received = 0;
        uint16_t temp_pcc_total[95] = {0}; // Temporary counter for this connection

        while (file_size > 0 && (bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            file_size -= bytes_received;
            for (int i = 0; i < bytes_received; i++) {
                if (buffer[i] >= PRINTABLE_MIN && buffer[i] <= PRINTABLE_MAX) {
                    printable_count++;
                    temp_pcc_total[buffer[i] - PRINTABLE_MIN]++;
                }
            }
        }

        // Only update global counts if the entire message was received
        if (file_size == 0) {
            for (int i = 0; i < 95; i++) {
                pcc_total[i] += temp_pcc_total[i];
            }
        } else {
            // Here, you can log or handle the incomplete transmission as needed
        }

        if (bytes_received < 0 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE)) {
            perror("recv failed with TCP error");
            close(client_fd);
            continue;
        } else if (bytes_received < 0) {
            perror("recv failed");
            close(client_fd);
            continue;
        }

        uint16_t net_printable_count = htons(printable_count);
        if (send(client_fd, &net_printable_count, sizeof(net_printable_count), 0) != sizeof(net_printable_count)) {
            perror("send failed");
        }

        close(client_fd);
    }

    print_statistics_and_exit();
    return 0;
}
