#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define SERVER_PORT 21
#define BUFFER_SIZE 1024

// Function Prototypes
void parseUrl(const char *url, char *protocol, char *user, char *pass, char *server, char *path);
int connectToServer(const char *address, int port);
int sendCommand(int sockfd, const char *command, char *response, size_t responseSize);
int parsePassiveMode(const char *response, int *port);

// Helper function for error checking
void handleError(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Parse FTP URL into its components
void parseUrl(const char *url, char *protocol, char *user, char *pass, char *server, char *path) {
    char tempUrl[BUFFER_SIZE];
    strncpy(tempUrl, url, BUFFER_SIZE);

    if (strstr(tempUrl, "@")) {
        sscanf(tempUrl, "%[^:]://%[^:]:%[^@]@%[^/]%s", protocol, user, pass, server, path);
    } else {
        sscanf(tempUrl, "%[^:]://%[^/]%s", protocol, server, path);
        strncpy(user, "anonymous", BUFFER_SIZE);
        strncpy(pass, "anonymous", BUFFER_SIZE);
    }
}

// Establish a connection to the server
int connectToServer(const char *address, int port) {
    int sockfd;
    struct sockaddr_in serverAddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) handleError("socket");

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(address);

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        handleError("connect");
    }

    return sockfd;
}

// Send a command to the server and read the response
int sendCommand(int sockfd, const char *command, char *response, size_t responseSize) {
    if (command) {
        if (write(sockfd, command, strlen(command)) < 0) {
            handleError("write");
        }
        printf("SENT: %s", command);
    }

    ssize_t bytesRead = read(sockfd, response, responseSize - 1);
    if (bytesRead < 0) {
        handleError("read");
    }
    response[bytesRead] = '\0'; // Null-terminate the response
    printf("RECEIVED: %s", response);

    return 0;
}

// Parse passive mode response to extract the data port
int parsePassiveMode(const char *response, int *port) {
    const char *start = strchr(response, '(');
    if (!start) {
        fprintf(stderr, "Failed to locate '(' in passive mode response\n");
        return -1;
    }
    start++; // Move past '('

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(start, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        fprintf(stderr, "Failed to parse passive mode data from response\n");
        return -1;
    }

    *port = p1 * 256 + p2;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ftp-url>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char protocol[BUFFER_SIZE], user[BUFFER_SIZE], pass[BUFFER_SIZE];
    char server[BUFFER_SIZE], path[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    parseUrl(argv[1], protocol, user, pass, server, path);

    struct hostent *host = gethostbyname(server);
    if (!host) {
        handleError("gethostbyname");
    }
    const char *address = inet_ntoa(*(struct in_addr *)host->h_addr);

    int controlSock = connectToServer(address, SERVER_PORT);

    // Read the initial server response
    sendCommand(controlSock, NULL, response, sizeof(response));

    // Send USER command
    char command[BUFFER_SIZE];
    // Send USER command
    snprintf(command, sizeof(command), "USER %s\r\n", user);
    sendCommand(controlSock, command, response, sizeof(response));

// Check for a response indicating password is needed
    if (strstr(response, "331") || strstr(response, "230")) {
        if (strstr(response, "331")) {
            // Send PASS command only if prompted
            snprintf(command, sizeof(command), "PASS %s\r\n", pass);
            sendCommand(controlSock, command, response, sizeof(response));
        }
    } else {
        fprintf(stderr, "Unexpected response during authentication: %s\n", response);
        close(controlSock);
        return EXIT_FAILURE;
    }

// Ensure authentication succeeded
    if (!strstr(response, "230")) {
        fprintf(stderr, "Authentication failed: %s\n", response);
        close(controlSock);
        return EXIT_FAILURE;
    }


    // Send PASV command
    sendCommand(controlSock, "PASV\r\n", response, sizeof(response));

    int dataPort;
    if (parsePassiveMode(response, &dataPort) < 0) {
        close(controlSock);
        return EXIT_FAILURE;
    }

    int dataSock = connectToServer(address, dataPort);

    // Send RETR command
    snprintf(command, sizeof(command), "RETR %s\r\n", path);
    sendCommand(controlSock, command, response, sizeof(response));

    // Download file
    FILE *file = fopen("ftp_file", "wb");
    if (!file) {
        handleError("fopen");
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(dataSock, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }

    fclose(file);
    close(dataSock);
    close(controlSock);

    printf("File downloaded successfully.\n");
    return EXIT_SUCCESS;
}

