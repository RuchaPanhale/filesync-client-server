#include "common.h"

#define CLIENT_FOLDER "client_files/"

int connect_to_server(char *server_ip, char *port) {
    struct addrinfo hints;
    struct addrinfo *server_info;
    struct addrinfo *p;
    int sockfd;
    int rc;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(server_ip, port, &hints, &server_info);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(1);
    }

    for (p = server_info; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (sockfd == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(1);
    }

    freeaddrinfo(server_info);
    return sockfd;
}

int main(int argc, char *argv[]) {
    if (argc < 5 || argc > 6) {
        printf("Usage:\n");
        printf("  ./client <server_ip> <port> <client_id> push <filename>\n");
        printf("  ./client <server_ip> <port> <client_id> pull <filename>\n");
        printf("  ./client <server_ip> <port> <client_id> status\n");
        printf("  ./client <server_ip> <port> <client_id> done\n");
        return 1;
    }

    char *server_ip = argv[1];
    char *port = argv[2];
    char *client_id = argv[3];
    char *command = argv[4];
    char *filename = NULL;

    if (strcmp(command, "push") == 0 || strcmp(command, "pull") == 0) {
        if (argc != 6) {
            printf("Error: %s requires a filename.\n", command);
            printf("Usage: ./client <server_ip> <port> <client_id> %s <filename>\n", command);
            return 1;
        }

        filename = argv[5];
    }
    else if (strcmp(command, "status") == 0 || strcmp(command, "done") == 0) {
        if (argc != 5) {
            printf("Error: %s does not use a filename.\n", command);
            printf("Usage: ./client <server_ip> <port> <client_id> %s\n", command);
            return 1;
        }
    }
    else {
        printf("Invalid command. Use push, pull, status, or done.\n");
        return 1;
    }

    int sockfd = connect_to_server(server_ip, port);

    char request[BUFSIZE];
    char response[BUFSIZE];

    if (strcmp(command, "status") == 0) {
        snprintf(request, sizeof(request), "STATUS %s\n", client_id);

        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("Failed to send status request");
            close(sockfd);
            return 1;
        }

        printf("Request sent to server: %s\n", request);

        if (receive_response(sockfd, response) < 0) {
            close(sockfd);
            return 1;
        }

        printf("\nFile Status Information:\n");
        printf("%s\n", response);
    }

    else if (strcmp(command, "done") == 0) {
        snprintf(request, sizeof(request), "DONE %s\n", client_id);

        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("Failed to send done request");
            close(sockfd);
            return 1;
        }

        printf("Request sent to server: %s\n", request);

        if (receive_response(sockfd, response) < 0) {
            close(sockfd);
            return 1;
        }

        printf("Server response: %s\n", response);
    }

    else if (strcmp(command, "pull") == 0) {
        snprintf(request, sizeof(request), "PULL %s %s\n", client_id, filename);

        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("Failed to send pull request");
            close(sockfd);
            return 1;
        }

        if (recv_line(sockfd, response, BUFSIZE) < 0) {
            close(sockfd);
            return 1;
        }

        if (strncmp(response, "ERROR", 5) == 0) {
            printf("%s\n", response);
            close(sockfd);
            return 1;
        }

        long filesize;

        if (sscanf(response, "OK %ld", &filesize) != 1) {
            printf("Invalid server response: %s\n", response);
            close(sockfd);
            return 1;
        }

        char filepath[256];

        snprintf(filepath, sizeof filepath, "%s%s", CLIENT_FOLDER, filename);

        printf("Downloading %s (%ld bytes)\n", filename, filesize);

        if (receive_file(sockfd, filepath, filesize) < 0) {
            printf("Download failed: connection closed before all %ld bytes arrived.\n", filesize);
            close(sockfd);
            return 1;
        }

        printf("Download complete.\n");
    }

    else if (strcmp(command, "push") == 0) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s%s", CLIENT_FOLDER, filename);

        long filesize = get_file_size(filepath);

        if (filesize < 0) {
            printf("Error: File not found in client_files folder: %s\n", filename);
            close(sockfd);
            return 1;
        }

        snprintf(request, sizeof(request), "PUSH %s %s %ld\n", client_id, filename, filesize);

        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("Failed to send push request");
            close(sockfd);
            return 1;
        }

        printf("Request sent to server: %s\n", request);
        printf("Uploading file of size %ld bytes...\n", filesize);

        send_file(sockfd, filepath);

        if (receive_response(sockfd, response) < 0) {
            close(sockfd);
            return 1;
        }

        printf("Server response: %s\n", response);

        if (strncmp(response, "ERROR", 5) == 0) {
            printf("Push failed: %s\n", response);
            close(sockfd);
            return 1;
        }
    }

    else {
        printf("Invalid command. Use push, pull, status, or done.\n");
        close(sockfd);
        return 1;
    }

    close(sockfd);
    return 0;
}
