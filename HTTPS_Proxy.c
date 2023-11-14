#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PROXY_PORT 4450
#define MAX_URL_LENGTH 100

void handle_https_client(int client_socket) {
    char request[4096];
    ssize_t bytes_received = recv(client_socket, request, sizeof(request) - 1, 0);

    if (bytes_received < 0) {
        perror("Error receiving request from client");
        return;
    }

    request[bytes_received] = '\0'; // Null-terminate the request

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
    host[host_length] = '\0'; // Null-terminate the host

    printf("Connecting to host: %s\n", host);

    // Create a socket to connect to the host
    int host_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (host_socket < 0) {
        perror("Error creating socket for the host");
        free(host);
        return;
    }

    // Get the IP address of the host
    host[strlen(host) - 4] = '\0'; // Null-terminate the host
    struct hostent* host_entry = gethostbyname(host);
    if (!host_entry) {
        perror("Error resolving host");
        free(host);
        close(host_socket);
        return;
    }

    struct sockaddr_in host_addr;
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(443); // Assuming HTTPS, change if needed
    memcpy(&host_addr.sin_addr.s_addr, host_entry->h_addr, host_entry->h_length);

    // Connect to the host
    if (connect(host_socket, (struct sockaddr*)&host_addr, sizeof(host_addr)) < 0) {
        perror("Error connecting to the host");
        free(host);
        close(host_socket);
        return;
    }

    // Respond to the client that the connection is established
    const char* response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client_socket, response, strlen(response), 0);

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
            break;
        }

        // Check if there is data to read from the client
        if (FD_ISSET(client_socket, &read_fds)) {
            bytes_received = recv(client_socket, request, sizeof(request), 0);
            if (bytes_received <= 0) {
                break;
            }
            send(host_socket, request, bytes_received, 0);
        }

        // Check if there is data to read from the host
        if (FD_ISSET(host_socket, &read_fds)) {
            bytes_received = recv(host_socket, request, sizeof(request), 0);
            if (bytes_received <= 0) {
                break;
            }
            send(client_socket, request, bytes_received, 0);
        }
    }

    // Close the sockets
    close(client_socket);
    close(host_socket);
    free(host);
}

int main() {
    int proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in proxy_addr;

    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(PROXY_PORT);
    proxy_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(proxy_socket, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("Error binding to the proxy port");
        return 1;
    }

    if (listen(proxy_socket, 10) < 0) {
        perror("Error listening on the proxy socket");
        return 1;
    }

    printf("Proxy server listening on port %d...\n", PROXY_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(proxy_socket, (struct sockaddr*)&client_addr, &client_addr_len);

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        if (client_socket < 0) {
            perror("Error accepting client connection");
        } else {
            printf("Handling client request...\n");
            handle_https_client(client_socket);
            printf("Done handling client request\n\n\n");
        }
    }

    close(proxy_socket);
    return 0;
}
