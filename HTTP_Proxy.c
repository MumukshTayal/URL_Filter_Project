#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>

#define PROXY_PORT 8080
#define MAX_URL_LENGTH 100
#define MAX_BLACKLIST_SIZE 10
#define BLOCKLIST_FILE "Blocklist.txt"
#define MAX_CACHE_SIZE 10

// Cache entry structure
struct cache_entry {
    char url[MAX_URL_LENGTH];
    char response[4096];
    time_t last_accessed;
};

struct cache_entry cache[MAX_CACHE_SIZE];
int cache_size = 0;

void add_to_cache(char* url, char* response) {
    // If the cache is full, find the least recently used entry
    if (cache_size == MAX_CACHE_SIZE) {
        int lru_index = 0;
        for (int i = 1; i < MAX_CACHE_SIZE; i++) {
            if (cache[i].last_accessed < cache[lru_index].last_accessed) {
                lru_index = i;
            }
        }

        // Replace the least recently used entry
        strncpy(cache[lru_index].url, url, MAX_URL_LENGTH - 1);
        strncpy(cache[lru_index].response, response, sizeof(cache[lru_index].response) - 1);
        cache[lru_index].last_accessed = time(NULL);
    } else {
        // Add a new entry to the cache
        strncpy(cache[cache_size].url, url, MAX_URL_LENGTH - 1);
        strncpy(cache[cache_size].response, response, sizeof(cache[cache_size].response) - 1);
        cache[cache_size].last_accessed = time(NULL);
        cache_size++;
    }
}

char* get_from_cache(char* url) {
    for (int i = 0; i < cache_size; i++) {
        if (strcmp(cache[i].url, url) == 0) {
            cache[i].last_accessed = time(NULL);
            return cache[i].response;
        }
    }
    return NULL;
}

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
    url[urlLength] = '\0';

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

    printf("Received request from client: %s\n", request);

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

    // Check if the URL is in the cache
    char* cached_response = get_from_cache(url);
    if (cached_response != NULL) {
        printf("URL found in the cache\n");
        send(client_socket, cached_response, strlen(cached_response), 0);
        close(client_socket);
        return;
    } else{
        printf("URL not found in the cache\n");
    }

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

    // Set up the timeout for the select call
    struct timeval timeout;
    timeout.tv_sec = 10;  // Timeout after 5 seconds
    timeout.tv_usec = 0;

    // Set up the fd_set for the select call
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(target_socket, &read_fds);

    // Wait for a response from the target server or for the timeout to expire
    int rett = select(target_socket + 1, &read_fds, NULL, NULL, &timeout);
    if (rett == 0) {
        // The select call timed out
        char* timeout_response = "HTTP/1.1 408 Request Timeout\r\n\r\n";
        send(client_socket, timeout_response, strlen(timeout_response), 0);
        close(client_socket);
        return;
    }

    // Receive the response from the target server and forward to the client
    char response_buffer[4096];
    ssize_t bytes_sent;
    while ((bytes_received = recv(target_socket, response_buffer, sizeof(response_buffer), 0)) > 0) {
        bytes_sent = send(client_socket, response_buffer, bytes_received, 0);
        add_to_cache(url, response_buffer);
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

void process_user_input(int proxy_socket) {
    printf("Enter a command (a - add, r - remove, l - list, q - quit): ");
    char command;
    char url[MAX_URL_LENGTH];

    scanf(" %c", &command);

    switch (command) {
        case 'a':
            printf("Enter the URL to add: ");
            scanf("%s", url);
            add_to_blacklist(url);
            break;
        case 'r':
            printf("Enter the URL to remove: ");
            scanf("%s", url);
            remove_from_blacklist(url);
            break;
        case 'l':
            print_blacklist();
            break;
        case 'q':
            printf("Quitting...\n");
            close(proxy_socket);
            exit(0);
        default:
            printf("Invalid command\n");
    }
}

int main(int argc, char *argv[]) {
    // Load blacklist from file at the beginning
    load_blacklist_from_file();

    int proxy_port;
    if (argc > 2) {
        if (strcmp(argv[1], "-p") != 0) {
            printf("Usage: %s [-p proxy_port]\n", argv[0]);
            return 1;
        }
        proxy_port = atoi(argv[2]);
    } else {
        printf("Please enter the port number: ");
        scanf("%d", &proxy_port);
    }

    // Set stdin to non-blocking mode
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);

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
        int client_socket = accept(proxy_socket, (struct sockaddr*)&client_addr, &client_addr_len);

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        if (client_socket < 0) {
            perror("Error accepting client connection");
        } else {
            printf("Handling client request...\n");
            handle_client(client_socket);
            printf("Done handling client request\n\n\n");
        }

        fd_set set;
        struct timeval timeout;

        FD_ZERO(&set);
        FD_SET(0, &set);

        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms

        int ready = select(1, &set, NULL, NULL, &timeout);

        if (ready > 0) {
            process_user_input(proxy_socket);
        }
    }

    close(proxy_socket);
    return 0;
}
