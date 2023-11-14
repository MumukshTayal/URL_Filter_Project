#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PROXY_PORT 8080
#define MAX_URL_LENGTH 100
#define MAX_BLACKLIST_SIZE 10
#define BLOCKLIST_FILE "Blocklist.txt"
// #define BLACKLISTED_URL "example.com"

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

void handle_client(int client_socket) {
    char request[4096];
    ssize_t bytes_received = 0;

    printf("Inside handle_client\n");

    // Receive the client's request
    // bytes_received = recv(client_socket, request, sizeof(request), 0);

    int target_socket;
    struct sockaddr_in target_addr;

    bytes_received = recv(client_socket,request,sizeof(request)-1,0);
    printf("Inside while loop\n");

    // Find the start of the "Host:" field
    char* hostStart = strstr(request, "Host:");
    if (hostStart == NULL) {
        printf("No Host field found in the request\n");
        return;
    }

    // Skip past "Host: " to the start of the URL
    hostStart += strlen("Host: ");

    // Find the end of the URL
    char* hostEnd = strchr(hostStart, '\n');
    if (hostEnd == NULL) {
        printf("No end of line found after the Host field\n");
        return;
    }

    // Copy the URL into a new string
    size_t urlLength = hostEnd - hostStart;
    char* url = malloc(urlLength + 1);
    if (url == NULL) {
        perror("Error allocating memory for URL");
        return;
    }
    strncpy(url, hostStart, urlLength);
    url[urlLength] = '\0'; // Null-terminate the string

    // Remove trailing newline or carriage return characters
    url[strcspn(url, "\r\n")] = '\0';
    
    printf("Extracted URL: %s\n", url);


    struct addrinfo hints = {0};
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addr = NULL;
    struct sockaddr_in target;

    int ret = getaddrinfo(url, NULL, &hints, &addr);
    if (ret == EAI_NONAME) // not an IP, retry as a hostname
    {
        hints.ai_flags = 0;
        ret = getaddrinfo(url, NULL, &hints, &addr);
    }
    if (ret == 0)
    {
        target = *(struct sockaddr_in*)(addr->ai_addr);
        freeaddrinfo(addr);
    }

    printf("IP address: %s & %s \n", inet_ntoa(target.sin_addr), url);

    // Create a connection to the target server
    target_socket = socket(AF_INET, SOCK_STREAM, 0);
    // struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(80);
    target_addr.sin_addr.s_addr = target.sin_addr.s_addr;

    printf("Forwarding request to the target server: \n");

    
    // if (strstr(url, BLACKLISTED_URL) == NULL) {
    //     printf("Sending request to target server\n");
    // }

    printf("Received request from client: %s\n", request);

    // Check if the request contains a blacklisted URL
    // if (strstr(url, BLACKLISTED_URL) != NULL) {
    //     printf("URL blocked: %s\n", BLACKLISTED_URL);
    //     const char* response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 19\r\n\r\nAccess Denied: URL blocked\r\n";
    //     send(client_socket, response, strlen(response), 0);
    //     close(client_socket);
    //     return;
    // }

    int isBlocked = 0;
    for (int i = 0; i < blacklist_size; i++) {
        if (strstr(request, blacklist[i]) != NULL) {
            isBlocked = 1;
            printf("URL blocked: %s\n", blacklist[i]);
            const char* response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 19\r\n\r\nAccess Denied: URL blocked\r\n";
            send(client_socket, response, strlen(response), 0);
            break;
        }
    }

    if (!isBlocked) {
        printf("Forwarding request to the target server...\n");
    } else {
        printf("Blocked request from the client\n");
        close(client_socket);
        return;
    }

    // printf("Forwarding request to the target server...\n");

    int ret_;
    if ((ret_ = connect(target_socket, (struct sockaddr*)&target_addr, sizeof(target_addr))) < 0) {
        perror("Error connecting to the target server");
        close(client_socket);
        return;
    }

    printf("Connected to the target server\n");

    // Forward the request to the target server
    send(target_socket, request, bytes_received, 0);

    printf("Request forwarded to the target server\n");

    // Forward the response from the target server to the client
    char response_buffer[4096];
    ssize_t bytes_sent;
    while ((bytes_received = recv(target_socket, response_buffer, sizeof(response_buffer), 0)) > 0) {
        bytes_sent = send(client_socket, response_buffer, bytes_received, 0);
        if (bytes_sent < 0) {
            perror("Error sending response to the client");
            break;
        }
    }

    printf("Response forwarded to the client\n");


    // Close the sockets
    close(client_socket);
    close(target_socket);
}

int main(int argc, char *argv[]) {
    // Load blacklist from file at the beginning
    load_blacklist_from_file();

    if (argc > 1) {
        if (strcmp(argv[1], "-a") == 0 && argc > 2) {
            add_to_blacklist(argv[2]);
        } else if (strcmp(argv[1], "-r") == 0 && argc > 2) {
            remove_from_blacklist(argv[2]);
        } else if (strcmp(argv[1], "-l") == 0) {
            print_blacklist();
        } else {
            printf("Invalid command-line arguments\n");
            return 1;
        }
    }


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
            handle_client(client_socket);
            printf("Done handling client request\n\n\n");
        }
    }

    close(proxy_socket);
    return 0;
}
