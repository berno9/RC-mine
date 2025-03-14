#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 21
#define BUFFER_SIZE 1024

// Global variables to hold parsed URL components
char *ftp_server;
char *username;
char *password;
char *file_path;
char server_ip[INET_ADDRSTRLEN];

// Function to parse the FTP URL
int parse_url(const char *url) {
    char protocol[10];
    char user[256] = "anonymous";
    char pass[256] = "anonymous";
    char server[256];
    char path[512];

    if (strchr(url, '@') != NULL) {
        // URL with username and password
        if (sscanf(url, "%[^:]://%[^:]:%[^@]@%[^/]%s", protocol, user, pass, server, path) != 5) {
            fprintf(stderr, "Error: Invalid URL format\n");
            return -1;
        }
    } else {
        // URL without username and password
        if (sscanf(url, "%[^:]://%[^/]%s", protocol, server, path) != 3) {
            fprintf(stderr, "Error: Invalid URL format\n");
            return -1;
        }
    }

    // Assign parsed values to global variables
    ftp_server = strdup(server);
    username = strdup(user);
    password = strdup(pass);
    file_path = strdup(path);

    return 0;
}

// Function to resolve the hostname to an IP address
int resolve_hostname(const char *hostname) {
    struct hostent *he = gethostbyname(hostname);
    if (he == NULL) {
        herror("gethostbyname");
        return -1;
    }

    struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
    if (addr_list[0] != NULL) {
        strcpy(server_ip, inet_ntoa(*addr_list[0]));
        return 0;
    }

    return -1;
}

// Function to send a command and read the server's response
int send_command(int sockfd, const char *cmd, char *response) {
    if (write(sockfd, cmd, strlen(cmd)) < 0) {
        perror("write");
        return -1;
    }

    if (read(sockfd, response, BUFFER_SIZE) < 0) {
        perror("read");
        return -1;
    }

    printf("Server Response: %s\n", response);
    return 0;
}

// Function to establish a data connection in passive mode
int setup_data_connection(int control_sock, int *data_sock) {
    char response[BUFFER_SIZE];
    char pasv_cmd[] = "PASV\n";

    // Send PASV command
    if (send_command(control_sock, pasv_cmd, response) < 0) {
        return -1;
    }

    // Parse PASV response to extract IP and port
    int ip1, ip2, ip3, ip4, p1, p2;
    if (sscanf(response, "%*[^(](%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &p1, &p2) != 6) {
        fprintf(stderr, "Error: Failed to parse PASV response\n");
        return -1;
    }

    int port = p1 * 256 + p2;
    char data_ip[INET_ADDRSTRLEN];
    snprintf(data_ip, sizeof(data_ip), "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    // Set up the data socket
    struct sockaddr_in data_addr;
    *data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*data_sock < 0) {
        perror("socket");
        return -1;
    }

    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(port);
    inet_pton(AF_INET, data_ip, &data_addr.sin_addr);

    if (connect(*data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
        perror("connect");
        return -1;
    }

    return 0;
}

// Main function
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ftp_url>\n", argv[0]);
        return 1;
    }

    // Parse the FTP URL
    if (parse_url(argv[1]) < 0) {
        return 1;
    }

    // Resolve the hostname to an IP address
    if (resolve_hostname(ftp_server) < 0) {
        return 1;
    }

    printf("Resolved IP: %s\n", server_ip);

    // Connect to the FTP server
    int control_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (control_sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(control_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    // Read server's welcome message
    char response[BUFFER_SIZE];
    read(control_sock, response, BUFFER_SIZE);
    printf("Server: %s\n", response);

    // Send username
    char user_cmd[BUFFER_SIZE];
    snprintf(user_cmd, sizeof(user_cmd), "USER %s\n", username);
    if (send_command(control_sock, user_cmd, response) < 0) {
        return 1;
    }

    // Send password
    char pass_cmd[BUFFER_SIZE];
    snprintf(pass_cmd, sizeof(pass_cmd), "PASS %s\n", password);
    if (send_command(control_sock, pass_cmd, response) < 0) {
        return 1;
    }

    // Set up data connection
    int data_sock;
    if (setup_data_connection(control_sock, &data_sock) < 0) {
        return 1;
    }

    // Send RETR command
    char retr_cmd[BUFFER_SIZE];
    snprintf(retr_cmd, sizeof(retr_cmd), "RETR %s\n", file_path);
    if (send_command(control_sock, retr_cmd, response) < 0) {
        return 1;
    }

    // Receive file data
    FILE *file = fopen("downloaded_file", "wb");
    if (!file) {
        perror("fopen");
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(data_sock, buffer, BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, bytes_read, file);
    }

    fclose(file);
    close(data_sock);
    close(control_sock);

    printf("File downloaded successfully.\n");

    // Free allocated memory
    free(ftp_server);
    free(username);
    free(password);
    free(file_path);

    return 0;
}
