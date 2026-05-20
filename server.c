#include "common.h"
#include <dirent.h>

#define SERVER_FOLDER "server_files/"
#define MAX_FILES 100
#define MAX_FILENAME 128
#define MAX_CLIENT_ID 64

/*
 * FileMetadata stores the server's current state for each tracked file.
 * filename: Name of the file inside server_files/
 * locked:
 *   0 = available
 *   1 = currently checked out / locked by a client
 * locked_by: client_id of the client that currently owns the lock
 * size: Latest known file size in bytes
 */
typedef struct {
    char filename[MAX_FILENAME];
    int locked;
    char locked_by[MAX_CLIENT_ID];
    long size;
} FileMetadata;

// in-memory metadata table.
FileMetadata metadata_table[MAX_FILES];
int metadata_count = 0;


// Build a full path inside the server_files directory.
void build_server_filepath(char *filepath, size_t filepath_size, char *filename)
{
    snprintf(filepath, filepath_size, "%s%s", SERVER_FOLDER, filename);
}

/*
 * Search for a file in the metadata table.
 * Returns:
 *   index of the file if found
 *   -1 if not found
 */
int find_metadata_index(char *filename)
{
    for (int i = 0; i < metadata_count; i++) {
        if (strcmp(metadata_table[i].filename, filename) == 0) {
            return i;
        }
    }

    return -1;
}

/*
 * Add a new file to the metadata table.
 * The file starts as unlocked.
 * Returns:
 *   index of the new metadata entry
 *   -1 if the metadata table is full
 */
int add_metadata_entry(char *filename)
{
    if (metadata_count >= MAX_FILES) {
        return -1;
    }

    int index = metadata_count;

    strncpy(metadata_table[index].filename, filename, MAX_FILENAME - 1);
    metadata_table[index].filename[MAX_FILENAME - 1] = '\0';

    metadata_table[index].locked = 0;
    strncpy(metadata_table[index].locked_by, "none", MAX_CLIENT_ID - 1);
    metadata_table[index].locked_by[MAX_CLIENT_ID - 1] = '\0';

    char filepath[256];
    build_server_filepath(filepath, sizeof(filepath), filename);

    metadata_table[index].size = get_file_size(filepath);

    metadata_count++;

    return index;
}

// Get an existing metadata entry, or create one if it does not exist yet.
int get_or_create_metadata_index(char *filename)
{
    int index = find_metadata_index(filename);

    if (index != -1) {
        return index;
    }

    return add_metadata_entry(filename);
}

/*
 * Lock a file for a specific client.
 * Returns:
 *   0 on success
 *   -1 if the file is already locked by another client
 */
int lock_file_for_client(char *filename, char *client_id)
{
    int index = get_or_create_metadata_index(filename);

    if (index == -1) {
        return -1;
    }

    if (metadata_table[index].locked &&
        strcmp(metadata_table[index].locked_by, client_id) != 0) {
        return -1;
    }

    metadata_table[index].locked = 1;

    strncpy(metadata_table[index].locked_by, client_id, MAX_CLIENT_ID - 1);
    metadata_table[index].locked_by[MAX_CLIENT_ID - 1] = '\0';

    return 0;
}

/*
 * Check whether a client owns the lock for a file.
 * Returns:
 *   1 if the client owns the lock
 *   0 otherwise
 */
int client_owns_file_lock(char *filename, char *client_id)
{
    int index = find_metadata_index(filename);

    if (index == -1) {
        return 0;
    }

    return metadata_table[index].locked &&
           strcmp(metadata_table[index].locked_by, client_id) == 0;
}

// Unlock a file after the owning client pushes it back or after the client says it is done.
void unlock_file(char *filename)
{
    int index = find_metadata_index(filename);

    if (index == -1) {
        return;
    }

    metadata_table[index].locked = 0;

    strncpy(metadata_table[index].locked_by, "none", MAX_CLIENT_ID - 1);
    metadata_table[index].locked_by[MAX_CLIENT_ID - 1] = '\0';
}

// Unlock every file currently locked by a specific client.
int unlock_all_files_for_client(char *client_id)
{
    int unlocked_count = 0;

    for (int i = 0; i < metadata_count; i++) {
        if (metadata_table[i].locked &&
            strcmp(metadata_table[i].locked_by, client_id) == 0) {

            metadata_table[i].locked = 0;

            strncpy(metadata_table[i].locked_by, "none", MAX_CLIENT_ID - 1);
            metadata_table[i].locked_by[MAX_CLIENT_ID - 1] = '\0';

            unlocked_count++;
        }
    }

    return unlocked_count;
}

// Update the stored file size after a successful upload.
void update_file_size(char *filename, long filesize)
{
    int index = get_or_create_metadata_index(filename);

    if (index == -1) {
        return;
    }

    metadata_table[index].size = filesize;
}

// to update the server metadata table with any regular files currently present in server_files/.
void sync_metadata_with_server_folder()
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(SERVER_FOLDER);

    if (dir == NULL) {
        perror("opendir server_files");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char filepath[256];

        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        build_server_filepath(filepath, sizeof(filepath), entry->d_name);

        struct stat st;

        if (stat(filepath, &st) == -1) {
            continue;
        }

        // Only track regular files, not folders.
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        int index = get_or_create_metadata_index(entry->d_name);

        if (index == -1) {
            printf("Metadata table full. Could not track %s\n", entry->d_name);
            continue;
        }

        metadata_table[index].size = st.st_size;
    }

    closedir(dir);
}


void handle_status(int client_fd)
{
    sync_metadata_with_server_folder();
    
    char status_msg[BUFSIZE];
    int offset = 0;

    if (metadata_count == 0) {
        snprintf(status_msg, sizeof(status_msg),
                 "No files are currently tracked by metadata.\n"
                 "Try pulling or pushing a file first.\n");

        send(client_fd, status_msg, strlen(status_msg), 0);
        return;
    }

    offset += snprintf(status_msg + offset,
                       sizeof(status_msg) - offset,
                       "Server File Status:\n");

    for (int i = 0; i < metadata_count; i++) {
        if (metadata_table[i].locked) {
            offset += snprintf(status_msg + offset,
                               sizeof(status_msg) - offset,
                               "%s -> locked by %s, size=%ld bytes\n",
                               metadata_table[i].filename,
                               metadata_table[i].locked_by,
                               metadata_table[i].size);
        } else {
            offset += snprintf(status_msg + offset,
                               sizeof(status_msg) - offset,
                               "%s -> available, size=%ld bytes\n",
                               metadata_table[i].filename,
                               metadata_table[i].size);
        }

        if (offset >= BUFSIZE - 1) {
            break;
        }
    }

    send(client_fd, status_msg, strlen(status_msg), 0);
}

void handle_pull(int client_fd, char *client_id, char *filename)
{
    printf("Client %s requested pull: %s\n",
           client_id, filename);

    char filepath[256];
    build_server_filepath(filepath, sizeof(filepath), filename);

    long filesize = get_file_size(filepath);

    if (filesize < 0) {
        char *msg = "ERROR file not found";
        send(client_fd, msg, strlen(msg), 0);
        return;
    }

    int index = get_or_create_metadata_index(filename);

    if (index == -1) {
        char *msg = "ERROR metadata table full";
        send(client_fd, msg, strlen(msg), 0);
        return;
    }

    metadata_table[index].size = filesize;

    // If another client already has this file locked, reject the pull.
    if (metadata_table[index].locked &&
        strcmp(metadata_table[index].locked_by, client_id) != 0) {

        char response[BUFSIZE];

        snprintf(response, sizeof(response),
                 "ERROR file is locked by %s",
                 metadata_table[index].locked_by);

        send(client_fd, response, strlen(response), 0);
        return;
    }

    // Pulling a file counts as checking it out.
    // The client now owns the lock until it either pushes the file back
    // or sends DONE.
    if (lock_file_for_client(filename, client_id) < 0) {
        char *msg = "ERROR could not lock file";
        send(client_fd, msg, strlen(msg), 0);
        return;
    }

    char response[64];
    snprintf(response, sizeof(response), "OK %ld", filesize);

    send(client_fd, response, strlen(response), 0);

    if (send_file(client_fd, filepath) < 0) {
        printf("ERROR sending file %s\n", filename);
        return;
    }

    printf("Sent file %s and locked it for client %s\n",
           filename, client_id);
}


void handle_push(int client_fd, char *client_id, char *filename, long filesize)
{
    printf("Client %s pushing file %s (%ld bytes)\n",
           client_id, filename, filesize);

    // push is only valid if this same client previously pulled and locked the file
    // OR if the file does not exist on the server yet

    int index = find_metadata_index(filename);

    if (find_metadata_index(filename) >= 0 && !client_owns_file_lock(filename, client_id)) {
        char response[BUFSIZE];

        if (metadata_table[index].locked) {
            snprintf(response, sizeof(response),
                     "ERROR file is locked by %s",
                     metadata_table[index].locked_by);
        } else {
            snprintf(response, sizeof(response),
                     "ERROR file was not locked before push");
        }

        send(client_fd, response, strlen(response), 0);
        return;
    }

    char filepath[256];
    build_server_filepath(filepath, sizeof(filepath), filename);

    if (receive_file(client_fd, filepath, filesize) < 0) {
        char *msg = "ERROR upload failed";

        send(client_fd,
             msg,
             strlen(msg),
             0);

        return;
    }

    // After a successful push, update metadata and release the lock.
    update_file_size(filename, filesize);
    unlock_file(filename);

    char *msg = "OK uploaded and lock released";

    send(client_fd,
         msg,
         strlen(msg),
         0);

    printf("Stored file %s and released lock for client %s\n",
           filename, client_id);
}

void handle_done(int client_fd, char *client_id)
{
    int unlocked_count = unlock_all_files_for_client(client_id);

    char response[BUFSIZE];

    snprintf(response, sizeof(response),
             "OK released %d lock(s) for client %s",
             unlocked_count, client_id);

    send(client_fd, response, strlen(response), 0);

    printf("Client %s done, released %d lock(s)\n",
           client_id, unlocked_count);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    sync_metadata_with_server_folder();

    char *port = argv[1];

    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,   // IPv4 or IPv6
        .ai_socktype = SOCK_STREAM, // TCP
        .ai_flags    = AI_PASSIVE,  // fill in own IP automatically
    };

    struct addrinfo *self_info;
    int rc = getaddrinfo(NULL, port, &hints, &self_info);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(1);
    }

    int socket_client;
    struct addrinfo *p;

    for (p = self_info; p != NULL; p = p->ai_next) {
        socket_client = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_client == -1) {
            perror("server: socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(socket_client, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
            error("setsockopt");

        if (bind(socket_client, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_client);
            perror("server: bind");
            continue;
        }

        break; /* successfully bound */
    }

    if (p == NULL)
        error("server: failed to bind");

    freeaddrinfo(self_info);

    if (listen(socket_client, BACKLOG) == -1)
        error("listen");

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof client_addr;

    printf("Waiting for connections on port %s...\n", port);

    while (1) {
        int client_fd = accept(socket_client, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        char header[256];
        int n = recv_line(client_fd, header, sizeof header);

        if (n < 0) {
            printf("failed to receive request header\n");
            close(client_fd);
            continue;
        }

        header[n] = '\0';
        printf("received: %s\n", header);

        char action[16];
        char client_id[64];
        char filename[128];

        if (strncmp(header, "STATUS", 6) == 0) {
            sscanf(header, "%15s %63s",
                   action, client_id);
            handle_status(client_fd);
        }

        else if (strncmp(header, "DONE", 4) == 0) {
            sscanf(header, "%15s %63s",
                action, client_id);
            handle_done(client_fd, client_id);
        }

        else if (strncmp(header, "PUSH", 4) == 0) {
            long filesize;
            sscanf(header, "%15s %63s %127s %ld",
                   action, client_id, filename, &filesize);
            handle_push(client_fd, client_id, filename, filesize);
        }

        else if (strncmp(header, "PULL", 4) == 0) {
            sscanf(header, "%15s %63s %127s",
                   action, client_id, filename);
            handle_pull(client_fd, client_id, filename);
        }

        else {
            char *msg = "ERROR invalid command";
            printf("%s\n", msg);
            send(client_fd, msg, strlen(msg), 0);
        }

        close(client_fd);
    }
}
