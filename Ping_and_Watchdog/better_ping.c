

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h> /* Added for the nonblocking socket */

#define SERVER_PORT 3000              // watchdog port
#define SERVER_IP_ADDRESS "127.0.0.1" // watchdog ip
// IPv4 header len without options
#define IP4_HDRLEN 20
#define BUFFER_SIZE 1024
// ICMP header len for echo req
#define ICMP_HDRLEN 8

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

int main(int argc, char *argv[])
{
    int cnt = 0;
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }
    char *args[2];
    args[0] = "./watchdog";
    args[1] = argv[1];
    //    Watchdog forking
    pid_t watchdogpid;
    watchdogpid = fork();
    if (watchdogpid == 0)
    {
        char *watchdog_path = "./watchdog"; // assumes .watchdog.out is compiled
        // runs the watchdog.out file
        execvp(watchdog_path, args);
    }
    // sleep for one second allowing watchdog to turn on listeningSocket
    sleep(1);

    // connecting ping to watchdog
    struct sockaddr_in watchDogAddress;
    int watchdog_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (watchdog_socket < 0)
    {
        printf("watchdog socket error.\n");
        exit(0);
    }
    printf("watchdog socket successful.\n");
    watchDogAddress.sin_family = AF_INET;
    watchDogAddress.sin_port = htons(SERVER_PORT);
    int rval = inet_pton(AF_INET, (const char *)SERVER_IP_ADDRESS, &watchDogAddress.sin_addr);
    if (rval <= 0)
    {
        printf("inet_pton() failed\n");
        return -1;
    }
    int connectR = connect(watchdog_socket, (struct sockaddr *)&watchDogAddress, sizeof(watchDogAddress));
    if (connectR == -1)
    {
        perror("[-]Error in socket\n");
        exit(1);
    }
    printf("connected to watchdog.\n");
    fcntl(watchdog_socket, F_SETFL, O_NONBLOCK); // Change the socket into non-blocking state
    while (1)
    {
        struct icmp icmphdr; // ICMP-header
        char data[IP_MAXPACKET] = "This is the ping.\n";

        int datalen = strlen(data) + 1;

        //===================
        // ICMP header
        //===================

        // Message Type (8 bits): ICMP_ECHO_REQUEST
        icmphdr.icmp_type = ICMP_ECHO;

        // Message Code (8 bits): echo request
        icmphdr.icmp_code = 0;

        // Identifier (16 bits): some number to trace the response.
        // It will be copied to the response packet and used to map response to the request sent earlier.
        // Thus, it serves as a Transaction-ID when we need to make "ping"
        icmphdr.icmp_id = 18;

        // Sequence Number (16 bits): starts at 0
        icmphdr.icmp_seq = cnt;

        // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
        icmphdr.icmp_cksum = 0;

        // Combine the packet
        char packet[IP_MAXPACKET];

        // Next, ICMP header
        memcpy((packet), &icmphdr, ICMP_HDRLEN);

        // After ICMP header, add the ICMP data.
        memcpy(packet + ICMP_HDRLEN, data, datalen);

        // Calculate the ICMP header checksum
        icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
        memcpy((packet), &icmphdr, ICMP_HDRLEN);

        struct sockaddr_in dest_in;
        memset(&dest_in, 0, sizeof(struct sockaddr_in));
        dest_in.sin_family = AF_INET;

        // The port is irrelant for Networking and therefore was zeroed.
        // dest_in.sin_addr.s_addr = iphdr.ip_dst.s_addr;
        dest_in.sin_addr.s_addr = inet_addr(argv[1]);
        // inet_pton(AF_INET, DESTINATION_IP, &(dest_in.sin_addr.s_addr));

        // Create raw socket for IP-RAW (make IP-header by yourself)

        struct timeval start, end;
        gettimeofday(&start, 0);

        // Send the packet using sendto() for sending datagrams.
        int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));

        if (bytes_sent == -1)
        {
            fprintf(stderr, "sendto() failed with error: %d", errno);
            return -1;
        }

        // notify watchdog that the initial packet has been sent
        char buffer[BUFFER_SIZE];
        int bytesRecv, bytesSent;
        bytesSent = send(watchdog_socket, "PACKET-SENT", sizeof("PACKET-SENT"), 0);
        // sleep(11);
        memset(&buffer, 0, BUFFER_SIZE);
        bytesRecv = recv(watchdog_socket, buffer, 1024, 0);
        // check if watchdog notifies of TIMEOUT
        if (bytesRecv != -1)
        {
            if (strcmp(buffer, "TIMEOUT"))
            {
                printf("\n");
                printf("server <%s> cannot be reached.\n", argv[1]);
                close(watchdog_socket);
                close(sock);
                break;
            }
            else
            {
                continue;
            }
        }
        // Get the ping response
        bzero(packet, IP_MAXPACKET);
        socklen_t len = sizeof(dest_in);
        ssize_t bytes_received = -1;
        bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len);

        if (bytes_received > 0)
        {
            // Check the IP header
            struct iphdr *iphdr = (struct iphdr *)packet;
            struct icmphdr *icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4));
            char srcIPADDR[32] = {'\0'};
            inet_ntop(AF_INET, &iphdr->saddr, srcIPADDR, sizeof(srcIPADDR));
            printf("response from %s: icmp_seq=%d ", srcIPADDR, icmphdr->un.echo.sequence);
            // notify watchdog that and ICMP response has been received, in order for watchdog to reset the timer
            bytesSent = send(watchdog_socket, "ICMP-RESPONSE-RECEIVED", sizeof("ICMP-RESPONSE-RECEIVED"), 0);
        }

        gettimeofday(&end, 0);

        char reply[IP_MAXPACKET];
        memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, datalen);
        // printf("ICMP reply: %s \n", reply);

        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
        printf("RTT=%f ms\n", milliseconds);
        cnt++;
        sleep(1);
    }

    close(sock);
    exit(1);
    return 0;
}

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}

// char *pkill_path = "/usr/bin/kill";
// char *arg1 = "-9";
// int process_kill = getpid;
// kill newping
// execl(pkill_path, arg1, process_kill, NULL);
// Close the raw socket descriptor.