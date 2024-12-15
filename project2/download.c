#include "dowload.h"

int get_ip(char *hostname, char *ip)
{
    struct hostent *h;

    if ((h = gethostbyname(hostname)) == NULL)
    {
        herror("gethostbyname()");
        return -1;
    }

    strcpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)));
    return 0;
}

int read_url(char *path, struct URL *url)
{
    memset(url, 0, sizeof(struct URL));

    if (strncmp(path, "ftp://", 6) != 0)
    {
        fprintf(stderr, "Invalid URL. Expected format: ftp://[<user>:<password>@]<host>/<url-path>\n");
        return -1;
    }

    path += 6;

    char *at_sign = strchr(path, '@');
    if (at_sign)
    {
        char *colon = strchr(path, ':');
        if (colon && colon < at_sign)
        {
            strncpy(url->user, path, colon - path);
            strncpy(url->password, colon + 1, at_sign - colon - 1);
            
        }
        else
        {
            fprintf(stderr, "Invalid user:password format in URL.\n");
            return -1;
        }
        path = at_sign + 1;
    }
    else
    {
        strncpy(url->user, USER, MAX_LENGTH);
        strncpy(url->password, PASS, MAX_LENGTH);
    }

    char *slash = strchr(path, '/');
    if (slash)
    {
        strncpy(url->host, path, slash - path);
        strncpy(url->file, slash + 1, MAX_LENGTH);
    }
    else
    {
        fprintf(stderr, "Invalid URL. Expected format: ftp://[<user>:<password>@]<host>/<url-path>\n");
        return -1;
    }

    // Resolve the IP address from the hostname
    if (get_ip(url->host, url->ip) != 0)
    {
        fprintf(stderr, "Failed to resolve IP address for host: %s\n", url->host);
        return -1;
    }

    return 0;
}

int create_socket(char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;

    // Create a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        return -1;
    }

    // Server address handling
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); // 32-bit Internet address network byte ordered
    server_addr.sin_port = htons(port);          // Server TCP port must be network byte ordered

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int read_response(const int socket, char *response)
{
    char buffer[RESPONSE_LENGTH];
    int bytes_read;
    int total_bytes_read = 0;

    // Clear the response buffer
    memset(response, 0, RESPONSE_LENGTH);

    // Read the response from the server
    while ((bytes_read = read(socket, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        strcat(response, buffer);  // Append the buffer to the response
        total_bytes_read += bytes_read;

        // Check if the response is complete (ends with a newline)
        if (buffer[bytes_read - 1] == '\n')
        {
            break;
        }
    }

    if (bytes_read < 0)
    {
        perror("read()");
        return -1;
    }

    int response_code = atoi(response);
    return response_code;
}

int auth_connection(const int socket, const char *user, const char *pass)
{
    char command[RESPONSE_LENGTH];
    char response[RESPONSE_LENGTH];

    snprintf(command, sizeof(command), "USER %s\r\n", user);
    if (write(socket, command, strlen(command)) < 0)
    {
        perror("write()");
        return -1;
    }

    if (read_response(socket, response) != SV_USER_OK)
    {
        fprintf(stderr, "Failed to authenticate user. Response: %s\n", response);
        return -1;
    }

    snprintf(command, sizeof(command), "PASS %s\r\n", pass);
    if (write(socket, command, strlen(command)) < 0)
    {
        perror("write()");
        return -1;
    }

    if (read_response(socket, response) != SV_LOGGED_IN)
    {
        fprintf(stderr, "Failed to authenticate password. Response: %s\n", response);
        return -1;
    }

    return 0;
}

int enter_passive_mode(const int socket, char *ip, int *port)
{
    char command[RESPONSE_LENGTH];
    char response[RESPONSE_LENGTH];

    snprintf(command, sizeof(command), "PASV\r\n");
    if (write(socket, command, strlen(command)) < 0)
    {
        perror("write()");
        return -1;
    }

    if (read_response(socket, response) != SV_ENTERING_PASV)
    {
        fprintf(stderr, "Failed to enter passive mode. Response: %s\n", response);
        return -1;
    }

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
    {
        fprintf(stderr, "Invalid PASV response format: %s\n", response);
        return -1;
    }

    snprintf(ip, MAX_LENGTH, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = (p1 * 256) + p2;

    return 0;
}

int request_download(const int socket, const char *filePath)
{
    char command[RESPONSE_LENGTH];
    char response[RESPONSE_LENGTH];

    snprintf(command, sizeof(command), "RETR %s\r\n", filePath);
    if (write(socket, command, strlen(command)) < 0)
    {
        perror("write()");
        return -1;
    }

    int resp=read_response(socket, response);
    if ( resp != SV_FILE_STATUS_OK)
    {
        if(resp == SV_ALREADY_OPEN){
            return 0;
        }
        fprintf(stderr, "Failed to initiate file download. Response: %s\n", response);
        return -1;
    }
    return 0;
}

int download_file(const int socketMain, const int socketData, const char *filePath)
{
    char data[RESPONSE_LENGTH];
    char response[RESPONSE_LENGTH];
    int bytes_read;
    FILE *file;

    if (request_download(socketMain, filePath) != 0)
    {
        fprintf(stderr, "Failed to request file download.\n");
        return -1;
    }

    const char *fileName = strrchr(filePath, '/');
    if (fileName)
    {
        fileName++;
    }
    else
    {
        fileName = filePath;
    }

    char modifiedFileName[MAX_LENGTH];
    strncpy(modifiedFileName, fileName, strlen(fileName));
    modifiedFileName[strlen(fileName)] = '\0';

    file = fopen(modifiedFileName, "wb");
    if (!file)
    {
        perror("fopen()");
        return -1;
    }

    while ((bytes_read = read(socketData, data, sizeof(data))) > 0)
    {
        fwrite(data, 1, bytes_read, file);
    }

    if (bytes_read < 0)
    {
        perror("read()");
        fclose(file);
        return -1;
    }

    fclose(file);

    int resp=read_response(socketMain, response);
    if (resp!= SV_TRANSFER_COMPLETE)
    {
        if(resp == SV_DATA_SOCKET_UNCLOSED){
            return 0;
        }
        fprintf(stderr, "File transfer was not completed successfully. Response: %s\n", response);
        return -1;
    }

    return 0;
}

int close_connection(const int socketMain, const int socketData)
{
    char command[RESPONSE_LENGTH];
    char response[RESPONSE_LENGTH];

    snprintf(command, sizeof(command), "QUIT\r\n");
    if (write(socketMain, command, strlen(command)) < 0)
    {
        perror("write()");
        return -1;
    }

    if (read_response(socketMain, response) != SV_CLOSING)
    {
        fprintf(stderr, "Failed to close connection. Response: %s\n", response);
        return -1;
    }

    /*
    if (close(socketData) < 0)
    {
        perror("close()");
        return -1;
    }

    if (close(socketMain) < 0)
    {
        perror("close()");
        return -1;
    }
    */

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <ftp_url>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct URL url;
    if (read_url(argv[1], &url) == 0)
    {
        printf("Parsed URL:\n");
        printf("User: %s\n", url.user);
        printf("Password: %s\n", url.password);
        printf("Host: %s\n", url.host);
        printf("File: %s\n", url.file);
        printf("Ip: %s\n", url.ip);
    }
    else
    {
        fprintf(stderr, "Failed to parse URL.\n");
        return EXIT_FAILURE;
    }


    char response[RESPONSE_LENGTH];
    int socketMain = create_socket(url.ip, PORT);
    if (socketMain < 0 || read_response(socketMain, response) != SV_REGISTER)
    {
        close(socketMain);
        fprintf(stderr, "Failed to create socket.\n");
        return EXIT_FAILURE;
    }

    if (auth_connection(socketMain, url.user, url.password) != 0)
    {
        fprintf(stderr, "Failed to authenticate connection.\n");
        close(socketMain);
        return EXIT_FAILURE;
    }

    char pasv_ip[MAX_LENGTH];
    int pasv_port;

    if (enter_passive_mode(socketMain, pasv_ip, &pasv_port) != 0)
    {
        fprintf(stderr, "Failed to enter passive mode.\n");
        close(socketMain);
        return EXIT_FAILURE;
    }
    else
    {
        printf("PASV IP: %s\n", pasv_ip);
        printf("PASV Port: %d\n", pasv_port);
    }

    int socketData = create_socket(pasv_ip, pasv_port);
    if (socketData < 0)
    {
        fprintf(stderr, "Failed to create socket.\n");
        return EXIT_FAILURE;
    }

    if (download_file(socketMain, socketData, url.file) != 0)
    {
        fprintf(stderr, "Failed to download file.\n");
        close(socketMain);
        close(socketData);
        return EXIT_FAILURE;
    }

    if (close_connection(socketMain, socketData) != 0)
    {
        fprintf(stderr, "Failed to close connection.\n");
        close(socketMain);
        close(socketData);
        return EXIT_FAILURE;
    }

    printf("File downloaded successfully.\n");
    close(socketMain);
    close(socketData);
    

    return EXIT_SUCCESS;
}
