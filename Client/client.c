#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

SOCKET client_socket;
int connected = 0;
char nickname[50];

// Function declarations
int init_client();
void connect_to_server();
void send_message(const char* message);
unsigned __stdcall receive_thread(void* param);
void display_help();
void cleanup_client();

int main() {
    printf("=== Chat Client ===\n");
    printf("Connecting to server %s:%d\n\n", SERVER_IP, SERVER_PORT);
    
    if (init_client() != 0) {
        printf("Failed to initialize client\n");
        printf("Press any key to exit...");
        _getch();
        return 1;
    }
    
    connect_to_server();
    
    if (!connected) {
        printf("Failed to connect to server\n");
        printf("Press any key to exit...");
        _getch();
        cleanup_client();
        return 1;
    }
    
    printf("Connected to server successfully!\n");
    // Get nickname from user
    printf("Enter your nickname: ");
    fgets(nickname, sizeof(nickname), stdin);
    nickname[strcspn(nickname, "\n")] = 0; // Remove newline
    
    // Wait for server registration prompt (nickname will be sent automatically)
    
    // Start receive thread
    HANDLE receive_handle = (HANDLE)_beginthreadex(NULL, 0, receive_thread, NULL, 0, NULL);
    
    printf("\n=== Connected to Chat Server ===\n");
    printf("Commands:\n");
    printf("  /help - Show help\n");
    printf("  /users - Show online users\n");
    printf("  /private <nickname> <message> - Send private message\n");
    printf("  /quit - Quit chat\n");
    printf("  Just type to send public message\n");
    printf("================================\n\n");
    
    char input[BUFFER_SIZE];
    while (connected) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) != NULL) {
            input[strcspn(input, "\n")] = 0; // Remove newline
            
            if (strlen(input) == 0) {
                continue;
            }
            
            if (strcmp(input, "/quit") == 0) {
                break;
            } else if (strcmp(input, "/help") == 0) {
                display_help();
            } else if (strcmp(input, "/users") == 0) {
                send_message("USERLIST");
            } else if (strncmp(input, "/private ", 9) == 0) {
                // Parse private message: /private nickname message
                char* space_pos = strchr(input + 9, ' ');
                if (space_pos != NULL) {
                    *space_pos = '\0';
                    char private_msg[BUFFER_SIZE];
                    sprintf_s(private_msg, BUFFER_SIZE, "PRIVATE:%s:%s", input + 9, space_pos + 1);
                    send_message(private_msg);
                } else {
                    printf("Usage: /private <nickname> <message>\n");
                }
            } else {
                // Regular chat message
                char chat_msg[BUFFER_SIZE];
                sprintf_s(chat_msg, BUFFER_SIZE, "CHAT:%s", input);
                send_message(chat_msg);
            }
        }
    }
    
    printf("\nDisconnecting...\n");
    connected = 0;
    
    if (receive_handle) {
        WaitForSingleObject(receive_handle, 1000);
        CloseHandle(receive_handle);
    }
    
    cleanup_client();
    printf("Press any key to exit...");
    _getch();
    return 0;
}

int init_client() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }
    
    return 0;
}

void connect_to_server() {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        printf("Invalid server address\n");
        return;
    }
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed. Error: %d\n", WSAGetLastError());
        return;
    }
    
    connected = 1;
    printf("Connected to server successfully!\n");
}

void send_message(const char* message) {
    if (connected && strlen(message) > 0) {
        int result = send(client_socket, message, strlen(message), 0);
        if (result == SOCKET_ERROR) {
            printf("Send failed. Error: %d\n", WSAGetLastError());
            connected = 0;
        }
    }
}

unsigned __stdcall receive_thread(void* param) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    while (connected) {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            
            // Parse different message types
            if (strncmp(buffer, "REGISTER:", 9) == 0) {
                // Server is asking for registration, send nickname
                printf("\n%s\n", buffer + 9);
                send_message(nickname);
            } else if (strncmp(buffer, "SYSTEM:", 7) == 0) {
                printf("\n%s\n> ", buffer + 7);
            } else if (strncmp(buffer, "CHAT:", 5) == 0) {
                printf("\n%s\n> ", buffer + 5);
            } else if (strncmp(buffer, "PRIVATE:", 8) == 0) {
                printf("\n%s\n> ", buffer + 8);
            } else if (strncmp(buffer, "USERLIST:", 9) == 0) {
                printf("\n%s\n> ", buffer + 9);
            } else {
                printf("\n%s\n> ", buffer);
            }
            fflush(stdout);
        } else if (bytes_received == 0) {
            printf("\nServer disconnected\n");
            connected = 0;
            break;
        } else {
            printf("\nReceive error: %d\n", WSAGetLastError());
            connected = 0;
            break;
        }
    }
    
    return 0;
}

void display_help() {
    printf("\n=== Chat Commands Help ===\n");
    printf("Available Commands:\n");
    printf("------------------\n");
    printf("/help                           - Show this help menu\n");
    printf("/users                          - Display all online users\n");
    printf("/private <nickname> <message>   - Send private message to user\n");
    printf("/quit                           - Exit the chat application\n");
    printf("\nGeneral Usage:\n");
    printf("- Type any message and press Enter to send to all users\n");
    printf("- Commands must start with '/' character\n");
    printf("- Nicknames are case-sensitive\n");
    printf("\nExamples:\n");
    printf("  Hello everyone!                - Public message\n");
    printf("  /private John Hi there!        - Private message to John\n");
    printf("========================\n\n");
}

void cleanup_client() {
    if (client_socket != INVALID_SOCKET) {
        closesocket(client_socket);
    }
    WSACleanup();
}