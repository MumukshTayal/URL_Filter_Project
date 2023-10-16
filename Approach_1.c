#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <libnet.h>
#include <stdbool.h>

// Define your list of blocked IP addresses here
const char* blockedIPs[] = {"10.0.0.1", "192.168.1.1", "72.1.241.188"};


// Define your list of blocked domains here
const char* blockedDomains[] = {"example.com", "blockedsite.com"};

// Function to check if a domain is in the blacklist
bool isBlockedDomain(const char* domain) {
    for (int i = 0; i < sizeof(blockedDomains) / sizeof(blockedDomains[0]); i++) {
        if (strcmp(domain, blockedDomains[i]) == 0) {
            return true; // The domain is blocked
        }
    }
    return false; // The domain is not blocked
}


void packet_handler(unsigned char *user_data, const struct pcap_pkthdr *pkthdr, const unsigned char *packet) {
    struct ip *ip_header = (struct ip *)(packet + 14); // Assuming Ethernet header is 14 bytes
    struct tcphdr *tcp_header = (struct tcphdr *)(packet + 14 + (ip_header->ip_hl << 2));

    if (ip_header->ip_p == IPPROTO_TCP) {
        char *payload = (char *)(packet + 14 + (ip_header->ip_hl << 2) + (tcp_header->doff << 2));

        
        // Check if the payload contains an HTTP request
        if (strstr(payload, "GET ") || strstr(payload, "POST ")) {
            // Extract the host from the HTTP request
            char* host = strstr(payload, "Host: ");
            if (host != NULL) {
                host += 6; // Skip "Host: "
                char* end = strchr(host, '\r');
                if (end != NULL) {
                    *end = '\0'; // Null-terminate the host
		    
                    // Resolve the host to an IP address
                    struct in_addr addr;
                    if (inet_pton(AF_INET, host, &addr) == 1) {
                        // Check if the resolved IP address is in the blocklist
                        for (int i = 0; i < sizeof(blockedIPs) / sizeof(blockedIPs[0]); i++) {
                            if (strcmp(host, blockedIPs[i]) == 0) {
                                printf("Blocked request to IP: %s\n", blockedIPs[i]);
                                
                                // Terminate the TCP connection by sending a RST packet
				libnet_t *ln = libnet_init(LIBNET_RAW4, NULL, NULL);
				if (ln == NULL) {
				    fprintf(stderr, "libnet_init error: %s\n", libnet_geterror(ln));
				    exit(1);
				}

				libnet_ptag_t tcp_tag = libnet_build_tcp(
				    ntohs(tcp_header->th_dport), // Source port
				    ntohs(tcp_header->th_sport), // Destination port
				    htonl(tcp_header->th_ack),     // Acknowledgment number
				    0,                             // Sequence number
				    TH_RST,                        // Control flags (RST flag set)
				    0,                             // Window size
				    0,                             // Checksum (0 for libnet to autofill)
				    0,                             // Urgent pointer
				    LIBNET_TCP_H,                  // Header length
				    NULL,                          // Payload
				    0,                             // Payload length
				    ln,                            // libnet context
				    0                             // New packet tag
				);

				if (tcp_tag == -1) {
				    fprintf(stderr, "libnet_build_tcp error: %s\n", libnet_geterror(ln));
				    libnet_destroy(ln);
				    exit(1);
				}

				// Send the RST packet
				if (libnet_write(ln) == -1) {
				    fprintf(stderr, "libnet_write error: %s\n", libnet_geterror(ln));
				}

				libnet_destroy(ln);

                            }
                        }
                    }
                }
            }
        }
    }
}

int main() {
    pcap_t *handle;
    char *dev;
    char errbuf[PCAP_ERRBUF_SIZE];

    // Find a suitable network device (you may need to specify the interface)
    //dev = pcap_lookupdev(errbuf);
    dev = "wlp0s20f3";
    if (dev == NULL) {
        fprintf(stderr, "Device not found: %s\n", errbuf);
        return 1;
    }

    // Open the network device for capturing
    handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Error opening device: %s\n", errbuf);
        return 1;
    }

    // Set a packet filter to capture only TCP traffic (you can customize this further)
    struct bpf_program fp;
    char filter_exp[] = "tcp";
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "Error compiling filter: %s\n", pcap_geterr(handle));
        return 1;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Error setting filter: %s\n", pcap_geterr(handle));
        return 1;
    }

    // Start capturing packets and call packet_handler for each packet
    pcap_loop(handle, 0, packet_handler, NULL);

    pcap_close(handle);
    return 0;
}

