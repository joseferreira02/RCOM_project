#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_LENGTH 500
#define PORT 21

// credentials
#define USER "anonymous"
#define PASS "anonymous@"


//server response codes

// Connection Responses
#define SV_REGISTER 220   // Service ready for a new user
#define SV_BUSY 421       // Service not available (e.g., server is busy)

// Login Responses
#define SV_USER_OK 331    // Username okay, need password
#define SV_LOGGED_IN 230  // User logged in successfully
#define SV_NOT_LOGGED_IN 530 // Not logged in (invalid username/password)

// Passive Mode (PASV) Responses
#define SV_ENTERING_PASV 227 // Entering Passive Mode (h1,h2,h3,h4,p1,p2)

// File Transfer Responses
#define SV_FILE_STATUS_OK 150  // File status okay; about to open data connection
#define SV_ALREADY_OPEN 125 // Data connection already open; transfer starting
#define SV_TRANSFER_COMPLETE 226 // Closing data connection; file transfer successful
#define SV_ACTION_NOT_TAKEN 550 // Requested action not taken (e.g., file not found)
#define SV_DATA_SOCKET_UNCLOSED 426 // Connection closed; transfer aborted

// Quit and Session Responses
#define SV_CLOSING 221 // Service closing control connection


#define RESPONSE_LENGTH 1024

// Structure to store URL components
struct URL
{
    char host[MAX_LENGTH];
    char resource[MAX_LENGTH];
    char file[MAX_LENGTH];
    char user[MAX_LENGTH];
    char password[MAX_LENGTH];
    char ip[MAX_LENGTH];
};

#define h_addr h_addr_list[0] //The first address in h_addr_list.

int read_url(char *path, struct URL *url);

int create_socket(char *ip, int port);

int get_ip(char *hostname, char *ip);

int auth_connection(const int socket, const char *user, const char *pass);

int enter_passive_mode(const int socket, char *ip, int *port);

int close_connection(const int socket, const int socketB);

int read_response(const int socket, char *response);

int request_download(const int socket, const char *filePath);

int download_file(const int socketMain,const int socketData, const char *filePath);