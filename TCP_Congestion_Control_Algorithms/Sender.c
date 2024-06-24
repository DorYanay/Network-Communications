#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define SIZE 1024 // Chunk size to sent each time
#define SERVER_PORT 9999
#define SERVER_IP_ADDRESS "127.0.0.1"
#define dor 9027 // 4 last numbers of the ID - Dor.
#define yev 0246 // 4 last numbers of the ID - Yevgeny.

int main()
{
    // TCP Client code
    int repeat = 1;
    int sock; // server socket
    int connectR;
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    FILE *fp;
    char *filename = "send.txt";

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("[-]Error in socket\n");
        exit(1);
    }
    printf("[+]Server socket created successfully.\n");
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    int rval = inet_pton(AF_INET, (const char *)SERVER_IP_ADDRESS, &serverAddress.sin_addr);

    if (rval <= 0)
    {
        printf("inet_pton() failed\n");
        return -1;
    }
    connectR = connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (connectR == -1)
    {
        perror("[-]Error in socket\n");
        exit(1);
    }
    printf("[+]Connected to Server.\n");

    // Main code to send the file.
    while (repeat == 1)
    {
        // Setting Cubic to be the CC algorithem at the start of the program
        char *CCCubic = "cubic";
        if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, CCCubic, strlen(CCCubic)) < 0)
        {
            printf("setsockopt() Failed\n");
            close(sock);
            return -1;
        }
        fp = fopen(filename, "r");
        if (fp == NULL)
        {
            perror("[-]Error in reading file.\n");
            exit(1);
        }
        // Checking the size of the given file.
        fseek(fp, 0L, SEEK_END);
        long size = ftell(fp);
        long halfsize = size / 2;
        fseek(fp, 0L, SEEK_SET);
        if (send(sock, &size, sizeof(size), 0) == -1)
        {
            perror("Error in sending file size");
            close(sock);
        }
        // char data[SIZE] = {0};
        // Send the first half of the file
        char data[halfsize];
        fread(data, 1, halfsize, fp);
        if (send(sock, data, sizeof(data), 0) == -1)
        {
            perror("Error in sending the first part of the file");
            close(sock);
            return -1;
        }
        bzero(data, halfsize);
        printf("First Half sent.\n");
        // Authentication
        int AuthCheck = dor ^ yev;
        int Auth;
        int AuthRec = recv(sock, &Auth, sizeof(Auth), 0);
        if (AuthRec == -1)
        {
            printf("recv() failed withe error code : %d", errno);
        }
        else if (AuthRec == 0)
        {
            printf("peer has closed the TCP connection prior to recv() \n");
        }
        else
        {
            printf("received %d bytes from server : %d\n", AuthRec, Auth);
        }
        if (Auth != AuthCheck)
        {
            printf("Authentication Failed.\n");
            close(sock);
            return -1;
        }
        else
        {
            printf("Authentication Succedded.\n");
        }
        // Change CC Algorithm to Reno
        char *CCReno = "reno";
        if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, CCReno, strlen(CCReno)) < 0)
        {
            printf("setsockopt() Failed\n");
            close(sock);
            return -1;
        }
        else
        {
            printf("CC algorithm changed to reno\n");
        }
        //
        // Send Second half
        memset(&serverAddress, 0, sizeof(serverAddress));
        // int total2 = 0;
        fread(data, 1, halfsize, fp);
        if (send(sock, data, sizeof(data), 0) == -1)
        {
            perror("Error in sending the second part of the file");
            close(sock);
            return -1;
        }
        bzero(data, halfsize);
        printf("Second Part Sent\n");
        printf("Enter '1' to repeat the send or any 'other number' to quit.\n");
        scanf("%d", &repeat);
        fclose(fp);
    }
    printf("Bye Receiver, Im out.\n");
    long int fullsize = -500;
    long int exitmsg = send(sock, &fullsize, sizeof(fullsize), 0);
    if (exitmsg == -1)
    {
        perror("Failed to send the exitmsg");
    }
    close(sock);
    return 0;
}
