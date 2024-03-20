#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <Server IP> <Server Port> <File Path>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    const char *file_path = argv[3];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        exit(1);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("connect failed");
        close(sock);
        exit(1);
    }

    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("fopen failed");
        close(sock);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size > 65535) {
        fprintf(stderr, "File size exceeds 16-bit limit\n");
        fclose(file);
        close(sock);
        exit(1);
    }

    uint16_t net_file_size = htons((uint16_t)file_size);
    if (send(sock, &net_file_size, sizeof(net_file_size), 0) != sizeof(net_file_size)) {
        perror("send failed");
        fclose(file);
        close(sock);
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    while (file_size > 0) {
        int bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytes_read < 1) {
            perror("fread failed");
            fclose(file);
            close(sock);
            exit(1);
        }

        if (send(sock, buffer, bytes_read, 0) < 0) {
            perror("send failed");
            fclose(file);
            close(sock);
            exit(1);
        }
        file_size -= bytes_read;
    }

    uint16_t printable_count;
    if (recv(sock, &printable_count, sizeof(printable_count), 0) != sizeof(printable_count)) {
        perror("recv failed");
        fclose(file);
        close(sock);
        exit(1);
    }

    printf("# of printable characters: %hu\n", ntohs(printable_count));

    fclose(file);
    close(sock);
    return 0;
}
