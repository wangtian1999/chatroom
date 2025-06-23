#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define NICKNAME_SIZE 32

// Message types
#define MSG_REGISTER 1
#define MSG_CHAT 2
#define MSG_PRIVATE 3
#define MSG_SYSTEM 4
#define MSG_USER_LIST 5

// User information structure
typedef struct {
    SOCKET socket;
    char nickname[NICKNAME_SIZE];
    char ip_address[INET_ADDRSTRLEN];
    int port;
    time_t join_time;
    int is_active;
} UserInfo;

// Message structure
typedef struct {
    int type;
    char sender[NICKNAME_SIZE];
    char receiver[NICKNAME_SIZE];
    char content[BUFFER_SIZE];
    time_t timestamp;
} Message;

SOCKET server_socket;
UserInfo users[MAX_CLIENTS];
int user_count = 0;

// Function declarations
int init_server();
void start_listening();
void handle_client_message(int user_index);
void handle_user_registration(int user_index, const char* nickname);
void broadcast_user_join(int user_index);
void broadcast_user_leave(int user_index);
void send_users_list(int user_index);
void send_message_to_user(int sender_index, const char* receiver_nickname, const char* content);
void broadcast_message(int sender_index, const char* content);
void check_keyboard_input();
void disconnect_user(SOCKET client_socket);
void display_status();
void display_help();
void cleanup_server();
int find_user_by_socket(SOCKET socket);
int find_user_by_nickname(const char* nickname);

int main() {
    printf("=== TCP Chat Server v2.0 ===\n");
    printf("Starting server with user management...\n\n");
    
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed!\n");
        return 1;
    }

    // Initialize users array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        users[i].socket = INVALID_SOCKET;
        users[i].is_active = 0;
    }

    if (init_server() == 0) {
        printf("Server started successfully on port %d\n\n", PORT);
        printf("=== Server Commands ===\n");
        printf("Press 'q' or 'Q' - Quit server\n");
        printf("Press 's' or 'S' - Show server status\n");
        printf("Press 'u' or 'U' - Show online users\n");
        printf("Press 'h' or 'H' - Show help\n");
        printf("========================\n\n");
        printf("Server is listening for connections...\n");
        printf("Waiting for users to join the chat...\n\n");
        start_listening();
    }
    
    // Cleanup
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

int init_server() {
    WSADATA wsaData;
    struct sockaddr_in server_addr;
    
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed!\n");
        return -1;
    }
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed!\n");
        WSACleanup();
        return -1;
    }
    
    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed! Error: %d\n", WSAGetLastError());
        printf("Press any key to continue...");
        _getch();
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }
    
    // Start listening
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Listen failed!\n");
        printf("Press any key to continue...");
        _getch();
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }
    
    // Initialize user array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        users[i].socket = INVALID_SOCKET;
        users[i].nickname[0] = '\0';
        users[i].ip_address[0] = '\0';
        users[i].port = 0;
        users[i].join_time = 0;
        users[i].is_active = 0;
    }
    
    return 0;
}

void start_listening() {
    fd_set read_fds;
    struct timeval timeout;
    SOCKET new_socket;
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    
    while (1) {
        // Check for keyboard input
        check_keyboard_input();
        
        // Setup fd_set for select
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        
        // Add user sockets to fd_set (including non-active users for registration)
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (users[i].socket != INVALID_SOCKET) {
                FD_SET(users[i].socket, &read_fds);
            }
        }
        
        // Set timeout for select
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        // Use select to check for activity
        int activity = select(0, &read_fds, NULL, NULL, &timeout);
        
        if (activity == SOCKET_ERROR) {
            printf("Select error!\n");
            break;
        }
        
        // Check for new connection
        if (FD_ISSET(server_socket, &read_fds)) {
            new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
            if (new_socket != INVALID_SOCKET) {
                // Find empty slot for new user
                int added = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (users[i].socket == INVALID_SOCKET) {
                        users[i].socket = new_socket;
                        inet_ntop(AF_INET, &client_addr.sin_addr, users[i].ip_address, INET_ADDRSTRLEN);
                        users[i].port = ntohs(client_addr.sin_port);
                        users[i].join_time = time(NULL);
                        users[i].is_active = 0; // Will be activated after registration
                        
                        printf("[%02d:%02d:%02d] New connection from %s:%d (Slot %d) - Waiting for registration...\n", 
                               (int)(time(NULL) % 86400) / 3600, 
                               (int)(time(NULL) % 3600) / 60, 
                               (int)(time(NULL) % 60),
                               users[i].ip_address, users[i].port, i + 1);
                        
                        // Send registration prompt
                        const char* prompt = "REGISTER:Please enter your nickname:";
                        send(new_socket, prompt, strlen(prompt), 0);
                        
                        added = 1;
                        break;
                    }
                }
                if (!added) {
                    printf("Maximum users reached. Connection rejected.\n");
                    const char* reject_msg = "SYSTEM:Server is full. Please try again later.";
                    send(new_socket, reject_msg, strlen(reject_msg), 0);
                    closesocket(new_socket);
                }
            }
        }
        
        // Check for user messages
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (users[i].socket != INVALID_SOCKET && FD_ISSET(users[i].socket, &read_fds)) {
                handle_client_message(i);
            }
        }
    }
}

void handle_client_message(int user_index) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(users[user_index].socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        
        // Check if user is not registered yet
        if (!users[user_index].is_active) {
            // Handle user registration
            handle_user_registration(user_index, buffer);
        } else {
            // Parse message format: TYPE:RECEIVER:CONTENT or TYPE:CONTENT
            if (strncmp(buffer, "PRIVATE:", 8) == 0) {
                // Private message format: PRIVATE:receiver:content
                char* receiver_start = buffer + 8;
                char* content_start = strchr(receiver_start, ':');
                if (content_start) {
                    *content_start = '\0';
                    content_start++;
                    send_message_to_user(user_index, receiver_start, content_start);
                }
            } else if (strncmp(buffer, "CHAT:", 5) == 0) {
                // Public chat message format: CHAT:content
                char* content = buffer + 5;
                broadcast_message(user_index, content);
            } else if (strncmp(buffer, "USERS", 5) == 0) {
                // Send user list
                send_users_list(user_index);
            } else {
                // Default to public chat
                broadcast_message(user_index, buffer);
            }
        }
    } else {
        // User disconnected
        if (users[user_index].is_active) {
            time_t now = time(NULL);
        printf("[%02d:%02d:%02d] User '%s' disconnected from %s:%d (Slot %d)\n", 
               (int)(now % 86400) / 3600, 
               (int)(now % 3600) / 60, 
               (int)(now % 60),
               users[user_index].nickname, users[user_index].ip_address, users[user_index].port, user_index + 1);
            broadcast_user_leave(user_index);
        } else {
            printf("Unregistered user from %s:%d disconnected\n", 
                   users[user_index].ip_address, users[user_index].port);
        }
        disconnect_user(users[user_index].socket);
    }
}

void handle_user_registration(int user_index, const char* nickname) {
    // Remove newline characters
    char clean_nickname[NICKNAME_SIZE];
    strncpy_s(clean_nickname, NICKNAME_SIZE, nickname, NICKNAME_SIZE - 1);
    clean_nickname[NICKNAME_SIZE - 1] = '\0';
    
    // Remove trailing newline/carriage return
    int len = strlen(clean_nickname);
    while (len > 0 && (clean_nickname[len - 1] == '\n' || clean_nickname[len - 1] == '\r')) {
        clean_nickname[--len] = '\0';
    }
    
    // Check if nickname is valid
    if (len == 0 || len >= NICKNAME_SIZE) {
        const char* error_msg = "SYSTEM:Invalid nickname. Please try again:";
        send(users[user_index].socket, error_msg, strlen(error_msg), 0);
        return;
    }
    
    // Check if nickname already exists
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != user_index && users[i].is_active && 
            strcmp(users[i].nickname, clean_nickname) == 0) {
            const char* error_msg = "SYSTEM:Nickname already taken. Please choose another:";
            send(users[user_index].socket, error_msg, strlen(error_msg), 0);
            return;
        }
    }
    
    // Register the user
    strncpy_s(users[user_index].nickname, NICKNAME_SIZE, clean_nickname, NICKNAME_SIZE - 1);
    users[user_index].is_active = 1;
    user_count++;
    
    time_t now = time(NULL);
    printf("[%02d:%02d:%02d] User '%s' registered successfully from %s:%d (Slot %d)\n",
           (int)(now % 86400) / 3600, 
           (int)(now % 3600) / 60, 
           (int)(now % 60),
           users[user_index].nickname, users[user_index].ip_address, users[user_index].port, user_index + 1);
    
    // Send welcome message
    char welcome_msg[BUFFER_SIZE];
    sprintf_s(welcome_msg, BUFFER_SIZE, 
              "SYSTEM:Welcome to the chat server, %s! Use /users to see online users.", 
              users[user_index].nickname);
    send(users[user_index].socket, welcome_msg, strlen(welcome_msg), 0);
    
    // Broadcast user join to all other users
    broadcast_user_join(user_index);
    

}

void broadcast_user_join(int user_index) {
    char join_msg[BUFFER_SIZE];
    sprintf_s(join_msg, BUFFER_SIZE, 
              "SYSTEM:*** %s has joined the chat! ***", 
              users[user_index].nickname);
    
    // Send to all other active users
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != user_index && users[i].is_active) {
            send(users[i].socket, join_msg, strlen(join_msg), 0);
        }
    }
    
    printf("Broadcasted: %s joined the chat\n", users[user_index].nickname);
}

void broadcast_user_leave(int user_index) {
    char leave_msg[BUFFER_SIZE];
    sprintf_s(leave_msg, BUFFER_SIZE, 
              "SYSTEM:*** %s has left the chat! ***", 
              users[user_index].nickname);
    
    // Send to all other active users
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != user_index && users[i].is_active) {
            send(users[i].socket, leave_msg, strlen(leave_msg), 0);
        }
    }
    
    printf("Broadcasted: %s left the chat\n", users[user_index].nickname);
}

void send_users_list(int user_index) {
    char user_list[BUFFER_SIZE];
    strcpy_s(user_list, BUFFER_SIZE, "USERS:Online users: ");
    
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].is_active) {
            if (count > 0) {
                strcat_s(user_list, BUFFER_SIZE, ", ");
            }
            strcat_s(user_list, BUFFER_SIZE, users[i].nickname);
            count++;
        }
    }
    
    if (count == 0) {
        strcpy_s(user_list, BUFFER_SIZE, "USERS:No users online");
    }
    
    send(users[user_index].socket, user_list, strlen(user_list), 0);
}

void send_message_to_user(int sender_index, const char* receiver_nickname, const char* content) {
    int receiver_index = find_user_by_nickname(receiver_nickname);
    
    if (receiver_index == -1) {
        char error_msg[BUFFER_SIZE];
        sprintf_s(error_msg, BUFFER_SIZE, 
                  "SYSTEM:User '%s' not found or offline", receiver_nickname);
        send(users[sender_index].socket, error_msg, strlen(error_msg), 0);
        return;
    }
    
    // Send private message to receiver
    char private_msg[BUFFER_SIZE];
    sprintf_s(private_msg, BUFFER_SIZE, 
              "PRIVATE:[%s -> You]: %s", 
              users[sender_index].nickname, content);
    send(users[receiver_index].socket, private_msg, strlen(private_msg), 0);
    
    // Send confirmation to sender
    char confirm_msg[BUFFER_SIZE];
    sprintf_s(confirm_msg, BUFFER_SIZE, 
              "PRIVATE:[You -> %s]: %s", 
              receiver_nickname, content);
    send(users[sender_index].socket, confirm_msg, strlen(confirm_msg), 0);
    
    printf("Private message: %s -> %s: %s\n", 
           users[sender_index].nickname, receiver_nickname, content);
}

void broadcast_message(int sender_index, const char* content) {
    char broadcast_msg[BUFFER_SIZE];
    sprintf_s(broadcast_msg, BUFFER_SIZE, 
              "CHAT:[%s]: %s", 
              users[sender_index].nickname, content);
    
    // Send to all other active users
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != sender_index && users[i].is_active) {
            send(users[i].socket, broadcast_msg, strlen(broadcast_msg), 0);
        }
    }
    
    printf("Public chat: %s: %s\n", users[sender_index].nickname, content);
}

int find_user_by_socket(SOCKET socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].is_active && users[i].socket == socket) {
            return i;
        }
    }
    return -1;
}

int find_user_by_nickname(const char* nickname) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].is_active && strcmp(users[i].nickname, nickname) == 0) {
            return i;
        }
    }
    return -1;
}

void display_help() {
    printf("\n=== Server Commands ===\n");
    printf("q - Quit server\n");
    printf("s - Show server status\n");
    printf("u - Show online users\n");
    printf("h - Show this help\n");
    printf("=====================\n\n");
}

void check_keyboard_input() {
    if (_kbhit()) {
        char key = _getch();
        if (key == 'q' || key == 'Q') {
            printf("Shutting down server...\n");
            exit(0);
        } else if (key == 's' || key == 'S') {
            display_status();
        } else if (key == 'u' || key == 'U') {
            printf("\n=== Online Users ===\n");
            if (user_count == 0) {
                printf("No users online\n");
            } else {
                printf("Total online: %d/%d\n", user_count, MAX_CLIENTS);
                printf("--------------------\n");
                int count = 1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (users[i].is_active) {
                        char time_str[64];
                        struct tm timeinfo;
                        localtime_s(&timeinfo, &users[i].join_time);
                        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
                        printf("%d. %s\n", count++, users[i].nickname);
                        printf("   IP: %s:%d\n", users[i].ip_address, users[i].port);
                        printf("   Joined: %s\n", time_str);
                        if (count <= user_count) printf("\n");
                    }
                }
            }
            printf("==================\n\n");
        } else if (key == 'h' || key == 'H') {
            display_help();
        }
    }
}

void disconnect_user(SOCKET client_socket) {
    int user_index = find_user_by_socket(client_socket);
    
    if (user_index != -1) {
        // Broadcast user leave message if user was registered
        if (strlen(users[user_index].nickname) > 0) {
            broadcast_user_leave(user_index);
        }
        
        // Close socket and clean up user data
        closesocket(users[user_index].socket);
        users[user_index].socket = INVALID_SOCKET;
        users[user_index].is_active = 0;
        memset(users[user_index].nickname, 0, NICKNAME_SIZE);
        memset(users[user_index].ip_address, 0, INET_ADDRSTRLEN);
        users[user_index].port = 0;
        users[user_index].join_time = 0;
        
        user_count--;
        printf("User disconnected. Active connections: %d\n", user_count);
    }
}

void display_status() {
    printf("\n=== Server Status ===\n");
    printf("Server Version: TCP Chat Server v2.0\n");
    printf("Listening Port: %d\n", PORT);
    printf("Max Capacity: %d users\n", MAX_CLIENTS);
    printf("Current Load: %d/%d users (%.1f%%)\n", 
           user_count, MAX_CLIENTS, 
           (float)user_count / MAX_CLIENTS * 100);
    printf("Server Status: %s\n", user_count > 0 ? "Active" : "Waiting for connections");
    
    if (user_count > 0) {
        printf("\nConnected Users:\n");
        printf("----------------\n");
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (users[i].is_active) {
                char time_str[64];
                struct tm timeinfo;
                localtime_s(&timeinfo, &users[i].join_time);
                strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
                printf("Slot %d: %s (%s:%d) - Online since %s\n", 
                       i + 1, users[i].nickname, users[i].ip_address, users[i].port, time_str);
            }
        }
    }
    printf("====================\n\n");
}

void cleanup_server() {
    // Close all user connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].socket != INVALID_SOCKET) {
            closesocket(users[i].socket);
        }
    }
    
    // Close server socket
    if (server_socket != INVALID_SOCKET) {
        closesocket(server_socket);
    }
    
    // Cleanup Winsock
    WSACleanup();
}