#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 65536
#define BLACKLISTED_URL "142.251.42.36"

int ip_isBlacklisted(char* dest_ip, int raw_socket, struct sockaddr saddr, int saddr_len) {
    // Check if the destination IP is in the blacklist and if in blacklist then respond the client with 403 forbidden
    if (strstr(dest_ip, BLACKLISTED_URL) != NULL) {
        printf("URL blocked: %s\n", BLACKLISTED_URL);
        const char* response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 19\r\n\r\nAccess Denied: URL blocked\r\n";
        sendto(raw_socket, response, strlen(response), 0, &saddr, saddr_len);
        close(raw_socket);
        return 1;
    } else {
        return 0;
    }
}

int main() {
    int raw_socket;
    struct sockaddr saddr;
    int saddr_len;

    unsigned char *buffer = (unsigned char *)malloc(BUFFER_SIZE);

    // Create a raw socket to read IP packets
    raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (raw_socket < 0) {
        perror("Socket creation error");
        return 1;
    }

    while (1) {
        saddr_len = sizeof(saddr);
        // Receive packets
        int data_size = recvfrom(raw_socket, buffer, BUFFER_SIZE, 0, &saddr, (socklen_t*)&saddr_len);
        if (data_size < 0) {
            perror("Packet receive error");
            return 1;
        }


        // Extracting IP header information
        struct iphdr *ip_header = (struct iphdr*)(buffer);

        // Construct destination IP address and store in dest_ip as character array
        char dest_ip[16];
        sprintf(dest_ip, "%d.%d.%d.%d",
                (unsigned int)(ip_header->daddr & 0xFF),
                (unsigned int)((ip_header->daddr >> 8) & 0xFF),
                (unsigned int)((ip_header->daddr >> 16) & 0xFF),
                (unsigned int)((ip_header->daddr >> 24) & 0xFF));

        // Construct sender IP address and store in src_ip as character array
        char src_ip[16];
        sprintf(src_ip, "%d.%d.%d.%d",
                (unsigned int)(ip_header->saddr & 0xFF),
                (unsigned int)((ip_header->saddr >> 8) & 0xFF),
                (unsigned int)((ip_header->saddr >> 16) & 0xFF),
                (unsigned int)((ip_header->saddr >> 24) & 0xFF));
        
        printf("Source IP: %s\n", src_ip);
        printf("Destination IP: %s\n", dest_ip);

        unsigned short dest_port = 0;
        unsigned short src_port = 0;
        memcpy(&src_port, buffer + sizeof(struct iphdr) + sizeof(struct tcphdr), sizeof(src_port));
        memcpy(&dest_port, buffer + sizeof(struct iphdr) + sizeof(struct tcphdr) + 2, sizeof(dest_port));

        printf("The source port is: %d \n", src_port);
        printf("The destination port is: %d \n", dest_port);

        // Assuming you have a variable 'src_port' that stores the source port of the incoming packet
        if (src_port != 8081) {
            // Process the packet
        }

        // When reading the response from the host server
        ssize_t numBytes = read(hostSocket, buffer, BUFFER_SIZE);
        if (numBytes == -1) {
            perror("Error reading from host server");
            return 1;
        } else if (numBytes > 0) {
            // Process the response
        }


        if (ip_isBlacklisted(dest_ip, raw_socket, saddr, saddr_len)){
            continue;
        }
    
        // The following code is to forward the packet to the destination IP and port
        int hostSocket;
        if ((hostSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Error opening host socket");
            return 1;
        }

        struct sockaddr_in hostAddr;
        hostAddr.sin_family = AF_INET;
        hostAddr.sin_addr.s_addr = inet_addr(dest_ip);
        hostAddr.sin_port = htons(dest_port);

        printf("Forwarding request to the target server: \n");
        for (int i = 0; i < data_size; i++) {
            printf("%02x ", buffer[i]);
        }
        printf("\n");

        // Connect to the host server
        if (connect(hostSocket, (struct sockaddr *)&hostAddr, sizeof(hostAddr)) == -1) {
            perror("Connection to host server failed");
            // return 1;
            close(hostSocket);
            continue;
        }
        printf("Connected to host server\n");

        // Extract the body of the application layer content of the data packet from the `buffer` variable to be forwarded as the request to the host server
        // struct iphdr *ip_header = (struct iphdr*)(buffer);
        struct tcphdr *tcp_header = (struct tcphdr*)(buffer + sizeof(struct iphdr));
        int tcp_header_size = tcp_header->doff * 4;
        int data_size_ = ntohs(ip_header->tot_len) - sizeof(struct iphdr) - tcp_header_size;
        char *data = buffer + sizeof(struct iphdr) + tcp_header_size;
        
        // Forward the body of the request to the host server
        if (send(hostSocket, data, data_size_, 0) == -1) {
            perror("Error sending data to host server");
            return 1;
        }
        
        printf("request forwarded");
        // // Forward the body of the request to the host server
        // if (write(hostSocket, buffer, data_size) == -1) {
        //     perror("Error writing to host server");
        //     return 1;
        // }

        // Receive the response packet from the response coming on the hostSocket using recv()
        char response[BUFFER_SIZE];
        int response_size = recv(hostSocket, response, BUFFER_SIZE, 0);
        if (response_size == -1) {
            perror("Error receiving response from host server");
            return 1;
        }

        close(hostSocket);

        // Forward the response to the client
        if (sendto(raw_socket, response, response_size, 0, &saddr, saddr_len) == -1) {
            perror("Error forwarding response to client");
            return 1;
        }

        // // Read the host server's response
        // if (read(hostSocket, buffer, BUFFER_SIZE) == -1) {
        //     perror("Error reading from host server");
        //     return 1;
        // }

        // printf("Received response from host server: %s\n", buffer);

        // // Forward the response to the client
        // if (write(raw_socket, buffer, strlen(buffer)) == -1) {
        //     perror("Error writing to client");
        //     return 1;
        // }
    
    }
    
    // Close the sockets
    close(raw_socket);
    free(buffer);
    return 0;
}
