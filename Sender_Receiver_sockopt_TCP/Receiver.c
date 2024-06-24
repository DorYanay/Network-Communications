#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define SIZE 1024 // Chunk size of the recv.
#define SERVER_PORT 9999
#define SERVER_IP_ADDRESS "127.0.0.1"
#define dor 9027 // 4 last numbers of the ID - Dor.
#define yev 0246 // 4 last numbers of the ID - Yevgeny.

int main()
{
    long int timearray1[1000];             // Initializing array that will hold the times of the send of the first half of the file.
    long int timearray2[1000];             // Initializing array that will hold the times of the send of the second half of the file.
    bzero(timearray1, sizeof(timearray1)); // Initializing the "First half" array with preset of "0" in all the cells.
    bzero(timearray2, sizeof(timearray2)); // Initializing the "Second half" array with preset of "0" in all the cells.
    int timeindex1 = 0;                    // index that will count the times the First send happend.
    int timeindex2 = 0;                    // index that will count the times the Second send happend.
    int indexcount = 0;                    // index that will count the times the file was successfully sent fully.
                                           // Start of the TCP server CODE
    int listeningSocket;
    struct sockaddr_in serverAddress, clientAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    socklen_t clientAddressLen;

    listeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listeningSocket < 0)
    {
        perror("[-]Error in socket\n");
        exit(1);
    }
    printf("[+]Server socket created successfully.\n");
    int enableReuse = 1;
    int ret = setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(int));
    if (ret < 0)
    {
        printf("setsockopt() failed with error code : %d\n", errno);
        return 1;
    }
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    int bindResult = bind(listeningSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bindResult < 0)
    {
        perror("[-]Error in bind\n");
        close(listeningSocket);
        return -1;
    }
    printf("[+]Binding successfull.\n");

    if (listen(listeningSocket, 3) == -1)
    {
        printf("listen() failed with error code : %d\n", errno);
        // close the socket
        close(listeningSocket);
        return -1;
    }
    printf("Waiting for incoming TCP-connections...\n");

    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddressLen = sizeof(clientAddress);
    int clientSocket = accept(listeningSocket, (struct sockaddr *)&clientAddress, &clientAddressLen);
    if (clientSocket == -1)
    {
        printf("listen failed with error code : %d\n", errno);
        // close the sockets
        close(listeningSocket);
        return -1;
    }

    printf("A new client connection accepted\n");
    // Start of the main code for receiving the file and Measuring the time.
    int done = 1;
    while (done == 1)
    {
        long int fullsize;                                                          // Initializing integer variable that will receive the size of the file from the client.
        long int filesizerecv = recv(clientSocket, &fullsize, sizeof(fullsize), 0); // Receiving from the client the size of the size the client sends.
        if (fullsize == -1)
        {
            printf("recv() failed withe error code : %d", errno);
        }
        else if (filesizerecv == 0)
        {
            printf("peer has closed the TCP connection prior to recv() \n");
        }
        // checking if the size of the file is Negative
        // the Negative size will be the exit msg from the client.
        if (fullsize == -500)
        {
            printf("Got the exit msg. lets finish things up\n");
            break;
        }
        printf("File size: %ld\n", fullsize);
        long int halftotal = fullsize / 2; // size of the first half.
        char buffer[SIZE];
        memset(buffer, 0, SIZE); // preseting the buffer to be "0" at the start.
        char *CCCubic = "cubic";
        // setting the program to run at cubic CC algorithm at the start.
        if (setsockopt(clientSocket, IPPROTO_TCP, TCP_CONGESTION, CCCubic, strlen(CCCubic)) < 0)
        {
            printf("setsockopt() Failed\n");
            close(clientSocket);
            return -1;
        }
        struct timespec start, end;             // Initializing the struct we will use to measure the time.
        clock_gettime(CLOCK_MONOTONIC, &start); // start to measure time.
        long int startns = (start.tv_sec * 1000000000) + start.tv_nsec;
        long int total = 0;
        int byte;

        // receive the first half of the file.

        while (total < halftotal)
        {
            bzero(buffer, SIZE);

            if ((byte = recv(clientSocket, buffer, SIZE, 0)) < 0)
            {
                printf("recv failed with error code : %d\n", errno);
                // close socket
                close(clientSocket);
                return -1;
            }
            else
            {
                total += byte;
                // printf("Bytes First: %d\n", total);
                if (total >= halftotal)
                {
                    clock_gettime(CLOCK_MONOTONIC, &end); // End time measure
                    long int endns = (end.tv_sec * 1000000000) + end.tv_nsec;
                    timearray1[timeindex1++] = endns - startns;
                    break;
                }
            }
        }
        printf("First Half received\n");

        printf("Received %ld first\n", total);
        // Authentication
        int Auth = dor ^ yev;
        int AuthSend = send(clientSocket, &Auth, sizeof(Auth), 0);
        if (AuthSend == -1)
        {
            perror("Failed to send the Authentication");
            close(clientSocket);
            close(listeningSocket);
        }
        // end of Authentication
        // Settings of the second half.
        long int total2 = 0;
        int byte2;
        int halftotal2 = fullsize - total; //
        // Changing the CC algorithm to reno like the Assignment asked.
        char *CCReno = "reno";

        if (setsockopt(clientSocket, IPPROTO_TCP, TCP_CONGESTION, CCReno, strlen(CCReno)) < 0)
        {
            printf("setsockopt() Failed\n");
            close(clientSocket);
            return -1;
        }
        else
        {
            printf("CC algorithm changed to reno\n");
        }
        //
        clock_gettime(CLOCK_MONOTONIC, &start); // start measuring the second half.
        startns = (start.tv_sec * 1000000000) + start.tv_nsec;
        // Receive the Second half of the file.
        while (total2 < halftotal2)
        {
            bzero(buffer, SIZE);

            if ((byte2 = recv(clientSocket, buffer, SIZE, 0)) < 0)
            {
                printf("recv failed with error code : %d\n", errno);
                // close socket
                close(clientSocket);
                return -1;
            }
            else
            {
                total2 += byte2;
                // printf("Bytes Second: %d\n", total2);
                if (total2 >= halftotal2)
                {
                    clock_gettime(CLOCK_MONOTONIC, &end); // Stop measuring the time of the second half.
                    long int endns = (end.tv_sec * 1000000000) + end.tv_nsec;
                    timearray2[timeindex2++] = endns - startns;
                    break;
                }
            }
        }
        printf("Second Half DONE\n");
        printf("Received %ld\n", total2);
        printf("whole package = %ld\n", total = total + total2);
        indexcount++;
    }
    // printing the times
    long int preavg1 = 0;
    long int preavg2 = 0;
    for (int i = 0; i < indexcount; i++)
    {
        printf("Run number %d : First send took %ld nanoseconds with Cubic CC Alg\n Second send took %ld nanoseconds with Reno CC Alg\n", i + 1, timearray1[i], timearray2[i]);
        preavg1 += timearray1[i];
        preavg2 += timearray2[i];
    }
    double avg1 = preavg1 / (double)timeindex1;
    double avg2 = preavg2 / (double)timeindex2;
    printf("Cubic : First send average time: %lf\n", avg1);
    printf("Reno : Second send average time: %lf\n", avg2);
    // Closing the connections (sockets);
    printf("Client finished - Connection closed.\n");
    close(clientSocket);
    close(listeningSocket);
    return 0;
}
