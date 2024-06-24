/*
    TCP/IP-server
*/

#include <stdio.h>

// Linux and other UNIXes
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h> /* Added for the nonblocking socket */

#define SERVER_PORT 3000 // The port that the server listens
#define BUFFER_SIZE 1024
#define SERVER_IP_ADDRESS "127.0.0.1"
#define WAIT_TIME 10

int main(int argc, char *argv[])
{
    // signal(SIGPIPE, SIG_IGN);  // on linux to prevent crash on closing socket

    // Open the listening (server) socket
    int listeningSocket = -1;
    listeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 0 means default protocol for stream sockets (Equivalently, IPPROTO_TCP)
    if (listeningSocket == -1)
    {
        printf("Could not create listening socket : %d", errno);
        return 1;
    }

    // Reuse the address if the server socket on was closed
    // and remains for 45 seconds in TIME-WAIT state till the final removal.
    //
    int enableReuse = 1;
    int ret = setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(int));
    if (ret < 0)
    {
        printf("setsockopt() failed with error code : %d", errno);
        return 1;
    }

    // "sockaddr_in" is the "derived" from sockaddr structure
    // used for IPv4 communication. For IPv6, use sockaddr_in6
    //
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;  // any IP at this port (Address to accept any incoming messages)
    serverAddress.sin_port = htons(SERVER_PORT); // network order (makes byte order consistent)

    // Bind the socket to the port with any IP at this port
    int bindResult = bind(listeningSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bindResult == -1)
    {
        printf("Bind failed with error code : %d", errno);
        // close the socket
        close(listeningSocket);
        return -1;
    }

    printf("Bind() success\n");

    // Make the socket listening; actually mother of all client sockets.
    // 500 is a Maximum size of queue connection requests
    // number of concurrent connections
    int listenResult = listen(listeningSocket, 3);
    if (listenResult == -1)
    {
        printf("listen() failed with error code : %d", errno);
        // close the socket
        close(listeningSocket);
        return -1;
    }

    // Accept and incoming connection
    printf("Waiting for incoming TCP-connections...\n");
    struct sockaddr_in clientAddress; //
    socklen_t clientAddressLen = sizeof(clientAddress);

    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddressLen = sizeof(clientAddress);
    int clientSocket = accept(listeningSocket, (struct sockaddr *)&clientAddress, &clientAddressLen);
    if (clientSocket == -1)
    {
        printf("listen failed with error code : %d", errno);
        // close the sockets
        close(listeningSocket);
        return -1;
    }

    printf("A new client connection accepted\n");
    fcntl(clientSocket, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
    // give ping one second to start sending its packets
    sleep(1);
    // Receive a message from client
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived == -1)
    {
        printf("recv failed with error code : %d", errno);
        // close the sockets
        close(listeningSocket);
        close(clientSocket);
        return -1;
    }

    struct timeval start, current;
    long seconds;
    gettimeofday(&start, NULL);
    double deltaT = 0;
    int bytesSent;
    // Timer
    while (deltaT < WAIT_TIME)
    {

        gettimeofday(&current, NULL);
        deltaT = current.tv_sec - start.tv_sec;
        // because clientSocket is non-blocking, only if a response is received the if statement is triggered.
        int pingSignal = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (!strcmp(buffer, "ICMP-RESPONSE-RECEIVED"))
        {
            memset(&buffer, 0, BUFFER_SIZE);
            gettimeofday(&start, NULL);
        }
    }
    // Reply to client if time exceeds 10 Seconds
    printf("\n");
    printf("Timer exceeded 10 seconds. sending timeout\n");
    printf("server <%s> cannot be reached.\n", argv[1]);
    char *message = "TIMEOUT";
    int messageLen = strlen(message) + 1;

    bytesSent = send(clientSocket, message, messageLen, 0);
    if (bytesSent == -1)
    {
        printf("send() failed with error code : %d", errno);
        close(listeningSocket);
        close(clientSocket);
        return -1;
    }
    else if (bytesSent == 0)
    {
        printf("peer has closed the TCP connection prior to send().\n");
    }
    else if (bytesSent < messageLen)
    {
        printf("sent only %d bytes from the required %d.\n", messageLen, bytesSent);
    }
    kill(0, SIGINT);
    close(listeningSocket);
    close(clientSocket);
    exit(1);

    return 0;
}
