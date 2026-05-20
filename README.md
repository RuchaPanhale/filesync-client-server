# FileSync - Client-Server File Synchronization System

## Overview

FileSync is a client-server based file synchronization system implemented in C. It allows clients to push and pull files from a centralized server while preventing conflicting edits through a file-locking mechanism.

When a client pulls a file, the server temporarily locks that file so that another client cannot pull or edit it at the same time. The lock is released when the client pushes the updated file back or uses the `done` command.

This project demonstrates core systems programming concepts such as socket programming, file I/O, TCP-based client-server communication, command parsing, metadata management, and file synchronization.

## Features

- Client-server architecture using TCP sockets
- Push files from client to server
- Pull files from server to client
- File-locking mechanism to prevent conflicting edits
- Status command to view file availability and lock state
- Done command to release locks
- Chunk-based file transfer
- Error handling for invalid commands, missing files, and locked files
- Makefile-based build system

## Project Structure

```text
file_sync/
├── client.c
├── server.c
├── common.h
├── Makefile
├── README.md
├── file_sync_report.txt
├── client_files/
└── server_files/
```

## Build Instructions

Compile the project using:

```bash
make
```

Remove compiled files using:

```bash
make clean
```

## Running the Project

Start the server:

```bash
./server <port>
```

Example:

```bash
./server 8080
```

Run the client using one of the following commands:

```bash
./client <server_ip> <port> <client_id> push <filename>
./client <server_ip> <port> <client_id> pull <filename>
./client <server_ip> <port> <client_id> status
./client <server_ip> <port> <client_id> done
```

Example commands:

```bash
./client 127.0.0.1 8080 client1 status
```

```bash
./client 127.0.0.1 8080 client1 pull notes.txt
```

```bash
./client 127.0.0.1 8080 client1 push notes.txt
```

```bash
./client 127.0.0.1 8080 client1 done
```

## Commands

### Pull

The `pull` command downloads a file from the server into the client's local folder.

```bash
./client 127.0.0.1 8080 client1 pull notes.txt
```

The server checks whether the file exists and whether it is currently locked. If the file is available, the server locks it for that client and sends the file.

### Push

The `push` command uploads a file from the client to the server.

```bash
./client 127.0.0.1 8080 client1 push notes.txt
```

If the file is new, the server accepts it. If the file already exists, the server accepts the update only if the same client previously pulled and locked that file.

### Status

The `status` command shows the current status of files on the server.

```bash
./client 127.0.0.1 8080 client1 status
```

The server lists available files, locked files, lock owners, and tracked file sizes.

### Done

The `done` command releases locks held by a client.

```bash
./client 127.0.0.1 8080 client1 done
```

This allows other clients to access the files again.

## Example Test Sequence

Start the server:

```bash
./server 8080
```

Check file status:

```bash
./client 127.0.0.1 8080 client1 status
```

Pull and lock a file:

```bash
./client 127.0.0.1 8080 client1 pull notes.txt
```

Check status from another client:

```bash
./client 127.0.0.1 8080 client2 status
```

Try pulling the same locked file:

```bash
./client 127.0.0.1 8080 client2 pull notes.txt
```

Release the lock:

```bash
./client 127.0.0.1 8080 client1 done
```

Check that the file is available again:

```bash
./client 127.0.0.1 8080 client2 status
```

## Concepts Demonstrated

- Systems programming in C
- TCP socket programming
- Client-server communication
- File I/O
- Directory-based file management
- Metadata tracking
- File-locking logic
- Command-line interface design
- Error handling and debugging
- Makefile usage

## Notes

- The server stores files inside `server_files/`.
- The client stores downloaded files inside `client_files/`.
- File transfers are performed using chunked transmission.
- The server maintains metadata for lock status, lock ownership, and file size.
- Existing files can only be updated by the client that currently holds the lock.
- New files can be pushed directly from the client to the server.