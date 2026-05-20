#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BACKLOG 10
#define BUFSIZE 1024

static inline void error(const char *msg)
{
    perror(msg);
    exit(1);
}

static inline void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &((struct sockaddr_in  *)sa)->sin_addr;
    return     &((struct sockaddr_in6 *)sa)->sin6_addr;
}


static inline long get_file_size(char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

static inline int receive_response(int sockfd, char *response) {
    int bytes_received = recv(sockfd, response, BUFSIZE - 1, 0);

    if (bytes_received < 0) {
        perror("Failed to receive response");
        return -1;
    }

    if (bytes_received == 0) {
        printf("Connection closed without sending a response.\n");
        return -1;
    }

    response[bytes_received] = '\0';
    return bytes_received;
}

static inline int recv_line(int sockfd, char *buf, size_t maxlen)
{
    size_t i = 0;

    while (i < maxlen - 1) {
        char c;

        int n = recv(sockfd, &c, 1, 0);
        if (n <= 0)
            return -1;
        if (c == '\n')
            break;

        buf[i++] = c;
    }

    buf[i] = '\0';

    return i;
}

static inline int send_file(int sockfd, char *filepath)
{
    char buf[BUFSIZE];

    FILE *fp = fopen(filepath, "rb");

    if (!fp) {
        perror("fopen");
        return -1;
    }

    int bytes_read;

    while ((bytes_read = fread(buf, 1, BUFSIZE, fp)) > 0) {

        int total_sent = 0;

        while (total_sent < bytes_read) {
            int n = send(sockfd, buf + total_sent, bytes_read - total_sent, 0);
            if (n <= 0) {
                fclose(fp);
                return -1;
            }

            total_sent += n;
        }
    }

    fclose(fp);

    return 0;
}

static inline int receive_file(int sockfd, char *filepath, long filesize)
{
    char buf[BUFSIZE];

    FILE *fp = fopen(filepath, "wb");

    if (!fp) {
        perror("fopen");
        return -1;
    }

    long total_received = 0;

    while (total_received < filesize) {

        int bytes_to_read;

        if (filesize - total_received < BUFSIZE)
            bytes_to_read = filesize - total_received;
        else
            bytes_to_read = BUFSIZE;

        int bytes_received =
            recv(sockfd,
                 buf,
                 bytes_to_read,
                 0);

        if (bytes_received <= 0) {
            fclose(fp);
            return -1;
        }

        fwrite(buf, 1, bytes_received, fp);

        total_received += bytes_received;
    }

    fclose(fp);

    return 0;
}

#endif // COMMON_H
