#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define PROXY_PORT 4450
#define MAX_URL_LENGTH 100
#define MAX_BLACKLIST_SIZE 10
#define BLOCKLIST_FILE "Blocklisthttps.txt"

// Global variables for the blacklist and its size
char blacklist[MAX_BLACKLIST_SIZE][MAX_URL_LENGTH];
int blacklist_size = 0;

void print_blacklist() {
    printf("Current blacklist:\n");
    for (int i = 0; i < blacklist_size; i++) {
        printf("%s\n", blacklist[i]);
    }
}

void load_blacklist_from_file() {
    FILE* file = fopen(BLOCKLIST_FILE, "r");
    if (file == NULL) {
        perror("Error opening Blocklist.txt");
        return;
    }

    while (fscanf(file, "%s", blacklist[blacklist_size]) == 1 && blacklist_size < MAX_BLACKLIST_SIZE) {
        blacklist_size++;
    }

    fclose(file);
}

void save_blacklist_to_file() {
    FILE* file = fopen(BLOCKLIST_FILE, "w");
    if (file == NULL) {
        perror("Error opening Blocklist.txt");
        return;
    }

    for (int i = 0; i < blacklist_size; i++) {
        fprintf(file, "%s\n", blacklist[i]);
    }

    fclose(file);
}

void add_to_blacklist(char* url) {
    if (blacklist_size < MAX_BLACKLIST_SIZE) {
        strncpy(blacklist[blacklist_size], url, MAX_URL_LENGTH - 1);
        blacklist[blacklist_size][MAX_URL_LENGTH - 1] = '\0';  // Null-terminate the string
        blacklist_size++;
        printf("Added %s to the blacklist\n", url);
        print_blacklist();
        save_blacklist_to_file();
    } else {
        printf("Blacklist is full. Cannot add more URLs.\n");
    }
}

void remove_from_blacklist(char* url) {
    for (int i = 0; i < blacklist_size; i++) {
        if (strcmp(blacklist[i], url) == 0) {
            // Remove the URL by shifting remaining elements
            for (int j = i; j < blacklist_size - 1; j++) {
                strncpy(blacklist[j], blacklist[j + 1], MAX_URL_LENGTH);
            }
            blacklist_size--;
            printf("Removed %s from the blacklist\n", url);
            print_blacklist();
            save_blacklist_to_file();
            return;
        }
    }
    printf("%s not found in the blacklist\n", url);
}


void handle_https_client(int client_socket) {
    char request[4096];
    ssize_t bytes_received = recv(client_socket, request, sizeof(request) - 1, 0);

    if (bytes_received < 0) {
        perror("Error receiving request from client");
        return;
    }

    request[bytes_received] = '\0';

    // Extract the host from the CONNECT request
    char* host_start = strstr(request, "CONNECT") + strlen("CONNECT") + 1;
    char* host_end = strchr(host_start, ' ');

    if (!host_start || !host_end) {
        perror("Invalid CONNECT request");
        return;
    }

    size_t host_length = host_end - host_start;
    char* host = malloc(host_length + 1);
    strncpy(host, host_start, host_length);
    host[host_length] = '\0';

    printf("Connecting to host: %s\n", host);

    // Create a socket to connect to the host
    int host_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (host_socket < 0) {
        perror("Error creating socket for the host");
        free(host);
        return;
    }

    // Get the IP address of the host
    host[strlen(host) - 4] = '\0';

    // Check if the host is in the blacklist
    for (int i = 0; i < blacklist_size; i++) {
        if (strcmp(blacklist[i], host) == 0) {
            const char* response = "HTTP/1.1 403 Forbidden\r\n\r\n";
            send(client_socket, response, strlen(response), 0);
            printf("URL blocked: %s\n", host);
            free(host);
            return;
        }
    }


    printf("Resolving host: %s\n", host);
    struct hostent* host_entry = gethostbyname(host);
    if (!host_entry) {
        perror("Error resolving host");
        free(host);
        close(host_socket);
        return;
    }

    printf("Resolved host: %s\n", inet_ntoa(*(struct in_addr*)host_entry->h_addr_list[0]));

    struct sockaddr_in host_addr;
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(443);
    memcpy(&host_addr.sin_addr.s_addr, host_entry->h_addr, host_entry->h_length);

    // Connect to the host
    if (connect(host_socket, (struct sockaddr*)&host_addr, sizeof(host_addr)) < 0) {
        perror("Error connecting to the host");
        free(host);
        close(host_socket);
        return;
    }

    printf("Connected to host\n");

    // Respond to the client that the connection is established
    const char* response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client_socket, response, strlen(response), 0);

    printf("Creating the tunnel\n");

    // Set up FD_SET for select
    fd_set read_fds;
    FD_ZERO(&read_fds);

    while (1) {
        FD_SET(client_socket, &read_fds);
        FD_SET(host_socket, &read_fds);

        // Use select to monitor both sockets
        int max_fd = (client_socket > host_socket) ? client_socket : host_socket;
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Error in select");
            return;
        }

        // Check if there is data to read from the client
        if (FD_ISSET(client_socket, &read_fds)) {
            bytes_received = recv(client_socket, request, sizeof(request), 0);
            if (bytes_received <= 0) {
                return;
            }
            send(host_socket, request, bytes_received, 0);
        }

        // Check if there is data to read from the host
        if (FD_ISSET(host_socket, &read_fds)) {
            bytes_received = recv(host_socket, request, sizeof(request), 0);
            if (bytes_received <= 0) {
                return;
            }
            send(client_socket, request, bytes_received, 0);
        }
    }
    printf("Successfully served the client request\n");
    // Close the sockets
    close(client_socket);
    close(host_socket);
    free(host);
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    handle_https_client(client_socket);
    close(client_socket);
    free(arg);
    return NULL;
}

int main(int argc, char* argv[]) {
    load_blacklist_from_file();
    int proxy_port = PROXY_PORT;
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            add_to_blacklist(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            remove_from_blacklist(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            proxy_port = atoi(argv[i + 1]);
            i++;
        }
    }

    int proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in proxy_addr;

    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(proxy_port);
    proxy_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(proxy_socket, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("Error binding to the proxy port");
        return 1;
    }

    if (listen(proxy_socket, 10) < 0) {
        perror("Error listening on the proxy socket");
        return 1;
    }

    printf("Proxy server listening on port %d...\n", proxy_port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int* client_socket = malloc(sizeof(int));
        *client_socket = accept(proxy_socket, (struct sockaddr*)&client_addr, &client_addr_len);

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        if (client_socket < 0) {
            perror("Error accepting client connection");
        } else {
            printf("Handling client request...\n");
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, handle_client, client_socket);
            pthread_detach(thread_id);
            printf("Done handling client request\n\n\n");
        }
    }
    save_blacklist_to_file();

    close(proxy_socket);
    return 0;
}
