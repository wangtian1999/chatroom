#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define LISTEN_PORT 9999
#define MAX_CONNECTIONS 10
#define MSG_BUFFER_SIZE 1024
#define USERNAME_LENGTH 32

// Communication types
#define COMM_REGISTER 1
#define COMM_BROADCAST 2
#define COMM_WHISPER 3
#define COMM_NOTIFICATION 4
#define COMM_MEMBER_LIST 5

// Client connection structure
typedef struct {
    SOCKET connection;
    char username[USERNAME_LENGTH];
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
    time_t connect_time;
    int online_status;
} ClientInfo;

// Communication packet structure
typedef struct {
    int packet_type;
    char from_user[USERNAME_LENGTH];
    char to_user[USERNAME_LENGTH];
    char message_data[MSG_BUFFER_SIZE];
    time_t send_time;
} CommPacket;

SOCKET main_socket;
ClientInfo connections[MAX_CONNECTIONS];
int active_connections = 0;

// Function declarations
int initialize_network();
void start_server_loop();
void process_client_data(int client_index);
void register_new_client(int client_index, const char* username);
void notify_client_joined(int client_index);
void notify_client_left(int client_index);
void send_member_list(int client_index);
void forward_private_message(int sender_index, const char* target_username, const char* message);
void broadcast_public_message(int sender_index, const char* message);
void process_server_commands();
void disconnect_client(SOCKET client_connection);
void show_server_info();
void show_command_help();
void shutdown_server();
int find_client_by_socket(SOCKET connection);
int find_client_by_username(const char* username);

int main() {
    printf("=== Network Communication Hub v3.1 ===\n");
    printf("Initializing multi-client communication system...\n\n");
    
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Network initialization failed!\n");
        return 1;
    }

    // Initialize connections array
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connections[i].connection = INVALID_SOCKET;
        connections[i].online_status = 0;
    }

    if (initialize_network() == 0) {
        printf("Communication hub started successfully on port %d\n\n", LISTEN_PORT);
        printf("=== Hub Control Commands ===\n");
        printf("Press 'q' or 'Q' - Shutdown hub\n");
        printf("Press 's' or 'S' - Display hub status\n");
        printf("Press 'u' or 'U' - List connected clients\n");
        printf("Press 'h' or 'H' - Show command help\n");
        printf("============================\n\n");
        printf("Hub is ready for client connections...\n");
        printf("Waiting for clients to connect...\n\n");
        start_server_loop();
    }
    
    // Cleanup
    closesocket(main_socket);
    WSACleanup();
    return 0;
}

int initialize_network() {
    WSADATA wsaData;
    struct sockaddr_in hub_addr;
    
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Network startup failed!\n");
        return -1;
    }
    
    // Create main socket
    main_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket == INVALID_SOCKET) {
        printf("Socket creation failed!\n");
        WSACleanup();
        return -1;
    }
    
    // Setup hub address
    hub_addr.sin_family = AF_INET;
    hub_addr.sin_addr.s_addr = INADDR_ANY;
    hub_addr.sin_port = htons(LISTEN_PORT);
    
    // Bind socket
    if (bind(main_socket, (struct sockaddr*)&hub_addr, sizeof(hub_addr)) == SOCKET_ERROR) {
        printf("Port binding failed! Error: %d\n", WSAGetLastError());
        printf("Press any key to continue...");
        _getch();
        closesocket(main_socket);
        WSACleanup();
        return -1;
    }
    
    // Start listening
    if (listen(main_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Listen operation failed!\n");
        printf("Press any key to continue...");
        _getch();
        closesocket(main_socket);
        WSACleanup();
        return -1;
    }
    
    // Initialize connections array
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connections[i].connection = INVALID_SOCKET;
        connections[i].username[0] = '\0';
        connections[i].client_ip[0] = '\0';
        connections[i].client_port = 0;
        connections[i].connect_time = 0;
        connections[i].online_status = 0;
    }
    
    return 0;
}

void start_server_loop() {
    fd_set read_fds;
    struct timeval timeout;
    SOCKET new_connection;
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    
    while (1) {
        // Check for server commands
        process_server_commands();
        
        // Setup fd_set for select
        FD_ZERO(&read_fds);
        FD_SET(main_socket, &read_fds);
        
        // Add client connections to fd_set (including inactive clients for registration)
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (connections[i].connection != INVALID_SOCKET) {
                FD_SET(connections[i].connection, &read_fds);
            }
        }
        
        // Set timeout for select
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        // Use select to check for activity
        int activity = select(0, &read_fds, NULL, NULL, &timeout);
        
        if (activity == SOCKET_ERROR) {
            printf("Network activity check error!\n");
            break;
        }
        
        // Check for new connection
        if (FD_ISSET(main_socket, &read_fds)) {
            new_connection = accept(main_socket, (struct sockaddr*)&client_addr, &addr_len);
            if (new_connection != INVALID_SOCKET) {
                // Find empty slot for new client
                int slot_found = 0;
                for (int i = 0; i < MAX_CONNECTIONS; i++) {
                    if (connections[i].connection == INVALID_SOCKET) {
                        connections[i].connection = new_connection;
                        inet_ntop(AF_INET, &client_addr.sin_addr, connections[i].client_ip, INET_ADDRSTRLEN);
                        connections[i].client_port = ntohs(client_addr.sin_port);
                        connections[i].connect_time = time(NULL);
                        connections[i].online_status = 0; // Will be activated after registration
                        
                        printf("[%02d:%02d:%02d] New client connected from %s:%d (Slot %d) - Awaiting authentication...\n", 
                               (int)(time(NULL) % 86400) / 3600, 
                               (int)(time(NULL) % 3600) / 60, 
                               (int)(time(NULL) % 60),
                               connections[i].client_ip, connections[i].client_port, i + 1);
                        
                        // Send authentication prompt
                        const char* prompt = "REGISTER:Please enter your username:";
                        send(new_connection, prompt, strlen(prompt), 0);
                        
                        slot_found = 1;
                        break;
                    }
                }
                if (!slot_found) {
                    printf("Maximum client capacity reached. Connection rejected.\n");
                    const char* reject_msg = "SYSTEM:Communication hub is full. Please try again later.";
                    send(new_connection, reject_msg, strlen(reject_msg), 0);
                    closesocket(new_connection);
                }
            }
        }
        
        // Check for client data
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (connections[i].connection != INVALID_SOCKET && FD_ISSET(connections[i].connection, &read_fds)) {
                process_client_data(i);
            }
        }
    }
}

void process_client_data(int client_index) {
    char data_buffer[MSG_BUFFER_SIZE];
    int bytes_received = recv(connections[client_index].connection, data_buffer, MSG_BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        data_buffer[bytes_received] = '\0';
        
        // Check if client is not authenticated yet
        if (!connections[client_index].online_status) {
            // Handle client authentication
            register_new_client(client_index, data_buffer);
        } else {
            // Parse communication format: TYPE:TARGET:CONTENT or TYPE:CONTENT
            if (strncmp(data_buffer, "PRIVATE:", 8) == 0) {
                // Private message format: PRIVATE:target:content
                char* target_start = data_buffer + 8;
                char* content_start = strchr(target_start, ':');
                if (content_start) {
                    *content_start = '\0';
                    content_start++;
                    forward_private_message(client_index, target_start, content_start);
                }
            } else if (strncmp(data_buffer, "CHAT:", 5) == 0) {
                // Public broadcast format: CHAT:content
                char* content = data_buffer + 5;
                broadcast_public_message(client_index, content);
            } else if (strncmp(data_buffer, "USERS", 5) == 0) {
                // Send member list
                send_member_list(client_index);
            } else {
                // Default to public broadcast
                broadcast_public_message(client_index, data_buffer);
            }
        }
    } else {
        // Client disconnected
        if (connections[client_index].online_status) {
            time_t now = time(NULL);
        printf("[%02d:%02d:%02d] Client '%s' disconnected from %s:%d (Slot %d)\n", 
               (int)(now % 86400) / 3600, 
               (int)(now % 3600) / 60, 
               (int)(now % 60),
               connections[client_index].username, connections[client_index].client_ip, connections[client_index].client_port, client_index + 1);
            notify_client_left(client_index);
        } else {
            printf("Unauthenticated client from %s:%d disconnected\n", 
                   connections[client_index].client_ip, connections[client_index].client_port);
        }
        disconnect_client(connections[client_index].connection);
    }
}

void register_new_client(int client_index, const char* username) {
    // Remove newline characters
    char clean_username[USERNAME_LENGTH];
    strncpy_s(clean_username, USERNAME_LENGTH, username, USERNAME_LENGTH - 1);
    clean_username[USERNAME_LENGTH - 1] = '\0';
    
    // Remove trailing newline/carriage return
    int len = strlen(clean_username);
    while (len > 0 && (clean_username[len - 1] == '\n' || clean_username[len - 1] == '\r')) {
        clean_username[--len] = '\0';
    }
    
    // Check if username is valid
    if (len == 0 || len >= USERNAME_LENGTH) {
        const char* error_msg = "SYSTEM:Invalid username. Please try again:";
        send(connections[client_index].connection, error_msg, strlen(error_msg), 0);
        return;
    }
    
    // Check if username already exists
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (i != client_index && connections[i].online_status && 
            strcmp(connections[i].username, clean_username) == 0) {
            const char* error_msg = "SYSTEM:Username already taken. Please choose another:";
            send(connections[client_index].connection, error_msg, strlen(error_msg), 0);
            return;
        }
    }
    
    // Register the client
    strncpy_s(connections[client_index].username, USERNAME_LENGTH, clean_username, USERNAME_LENGTH - 1);
    connections[client_index].online_status = 1;
    active_connections++;
    
    time_t now = time(NULL);
    printf("[%02d:%02d:%02d] Client '%s' registered successfully from %s:%d (Slot %d)\n",
           (int)(now % 86400) / 3600, 
           (int)(now % 3600) / 60, 
           (int)(now % 60),
           connections[client_index].username, connections[client_index].client_ip, connections[client_index].client_port, client_index + 1);
    
    // Send welcome message
    char welcome_msg[MSG_BUFFER_SIZE];
    sprintf_s(welcome_msg, MSG_BUFFER_SIZE, 
              "SYSTEM:Welcome to the communication hub, %s! Use USERS to see online clients.", 
              connections[client_index].username);
    send(connections[client_index].connection, welcome_msg, strlen(welcome_msg), 0);
    
    // Broadcast client join to all other clients
    notify_client_joined(client_index);
}

void notify_client_joined(int client_index) {
    char join_msg[MSG_BUFFER_SIZE];
    sprintf_s(join_msg, MSG_BUFFER_SIZE, 
              "SYSTEM:*** %s has joined the communication hub! ***", 
              connections[client_index].username);
    
    // Send to all other active clients
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (i != client_index && connections[i].online_status) {
            send(connections[i].connection, join_msg, strlen(join_msg), 0);
        }
    }
    
    printf("Broadcasted: %s joined the hub\n", connections[client_index].username);
}

void notify_client_left(int client_index) {
    char leave_msg[MSG_BUFFER_SIZE];
    sprintf_s(leave_msg, MSG_BUFFER_SIZE, 
              "SYSTEM:*** %s has left the communication hub! ***", 
              connections[client_index].username);
    
    // Send to all other active clients
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (i != client_index && connections[i].online_status) {
            send(connections[i].connection, leave_msg, strlen(leave_msg), 0);
        }
    }
    
    printf("Broadcasted: %s left the hub\n", connections[client_index].username);
}

void send_member_list(int client_index) {
    char member_list[MSG_BUFFER_SIZE];
    strcpy_s(member_list, MSG_BUFFER_SIZE, "USERS:Online members: ");
    
    int count = 0;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].online_status) {
            if (count > 0) {
                strcat_s(member_list, MSG_BUFFER_SIZE, ", ");
            }
            strcat_s(member_list, MSG_BUFFER_SIZE, connections[i].username);
            count++;
        }
    }
    
    if (count == 0) {
        strcpy_s(member_list, MSG_BUFFER_SIZE, "USERS:No members online");
    }
    
    send(connections[client_index].connection, member_list, strlen(member_list), 0);
}

void forward_private_message(int sender_index, const char* target_username, const char* message) {
    int receiver_index = find_client_by_username(target_username);
    
    if (receiver_index == -1) {
        char error_msg[MSG_BUFFER_SIZE];
        sprintf_s(error_msg, MSG_BUFFER_SIZE, 
                  "SYSTEM:User '%s' not found or offline", target_username);
        send(connections[sender_index].connection, error_msg, strlen(error_msg), 0);
        return;
    }
    
    // Send private message to receiver
    char private_msg[MSG_BUFFER_SIZE];
    sprintf_s(private_msg, MSG_BUFFER_SIZE, 
              "PRIVATE:[%s -> You]: %s", 
              connections[sender_index].username, message);
    send(connections[receiver_index].connection, private_msg, strlen(private_msg), 0);
    
    // Send confirmation to sender
    char confirm_msg[MSG_BUFFER_SIZE];
    sprintf_s(confirm_msg, MSG_BUFFER_SIZE, 
              "PRIVATE:[You -> %s]: %s", 
              target_username, message);
    send(connections[sender_index].connection, confirm_msg, strlen(confirm_msg), 0);
    
    printf("Private message: %s -> %s: %s\n", 
           connections[sender_index].username, target_username, message);
}

void broadcast_public_message(int sender_index, const char* message) {
    char broadcast_msg[MSG_BUFFER_SIZE];
    sprintf_s(broadcast_msg, MSG_BUFFER_SIZE, 
              "CHAT:[%s]: %s", 
              connections[sender_index].username, message);
    
    // Send to all other active clients
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (i != sender_index && connections[i].online_status) {
            send(connections[i].connection, broadcast_msg, strlen(broadcast_msg), 0);
        }
    }
    
    printf("Public chat: %s: %s\n", connections[sender_index].username, message);
}

int find_client_by_socket(SOCKET connection) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].online_status && connections[i].connection == connection) {
            return i;
        }
    }
    return -1;
}

int find_client_by_username(const char* username) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].online_status && strcmp(connections[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

void show_command_help() {
    printf("\n=== Hub Control Commands ===\n");
    printf("q - Shutdown hub\n");
    printf("s - Display hub status\n");
    printf("u - List connected clients\n");
    printf("h - Show command help\n");
    printf("============================\n\n");
}

void process_server_commands() {
    if (_kbhit()) {
        char key = _getch();
        if (key == 'q' || key == 'Q') {
            printf("Shutting down hub...\n");
            shutdown_server();
        } else if (key == 's' || key == 'S') {
            show_server_info();
        } else if (key == 'u' || key == 'U') {
            printf("\n=== Connected Clients ===\n");
            if (active_connections == 0) {
                printf("No clients connected\n");
            } else {
                printf("Total connected: %d/%d\n", active_connections, MAX_CONNECTIONS);
                printf("-------------------------\n");
                int count = 1;
                for (int i = 0; i < MAX_CONNECTIONS; i++) {
                    if (connections[i].online_status) {
                        char time_str[64];
                        struct tm timeinfo;
                        localtime_s(&timeinfo, &connections[i].connect_time);
                        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
                        printf("%d. %s\n", count++, connections[i].username);
                        printf("   IP: %s:%d\n", connections[i].client_ip, connections[i].client_port);
                        printf("   Connected: %s\n", time_str);
                        if (count <= active_connections) printf("\n");
                    }
                }
            }
            printf("========================\n\n");
        } else if (key == 'h' || key == 'H') {
            show_command_help();
        }
    }
}

void disconnect_client(SOCKET client_connection) {
    int client_index = find_client_by_socket(client_connection);
    
    if (client_index != -1) {
        // Broadcast client leave message if client was registered
        if (strlen(connections[client_index].username) > 0) {
            notify_client_left(client_index);
        }
        
        // Close socket and clean up client data
        closesocket(connections[client_index].connection);
        connections[client_index].connection = INVALID_SOCKET;
        connections[client_index].online_status = 0;
        memset(connections[client_index].username, 0, USERNAME_LENGTH);
        memset(connections[client_index].client_ip, 0, INET_ADDRSTRLEN);
        connections[client_index].client_port = 0;
        connections[client_index].connect_time = 0;
        
        active_connections--;
        printf("Client disconnected. Active connections: %d\n", active_connections);
    }
}

void show_server_info() {
    printf("\n=== Hub Status ===\n");
    printf("Hub Version: Network Communication Hub v3.1\n");
    printf("Listening Port: %d\n", LISTEN_PORT);
    printf("Max Capacity: %d clients\n", MAX_CONNECTIONS);
    printf("Current Load: %d/%d clients (%.1f%%)\n", 
           active_connections, MAX_CONNECTIONS, 
           (float)active_connections / MAX_CONNECTIONS * 100);
    printf("Hub Status: %s\n", active_connections > 0 ? "Active" : "Waiting for connections");
    
    if (active_connections > 0) {
        printf("\nConnected Clients:\n");
        printf("------------------\n");
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (connections[i].online_status) {
                char time_str[64];
                struct tm timeinfo;
                localtime_s(&timeinfo, &connections[i].connect_time);
                strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
                printf("Slot %d: %s (%s:%d) - Online since %s\n", 
                       i + 1, connections[i].username, connections[i].client_ip, connections[i].client_port, time_str);
            }
        }
    }
    printf("==================\n\n");
}

void shutdown_server() {
    // Close all client connections
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].connection != INVALID_SOCKET) {
            closesocket(connections[i].connection);
        }
    }
    
    // Close main socket
    if (main_socket != INVALID_SOCKET) {
        closesocket(main_socket);
    }
    
    // Cleanup Winsock
    WSACleanup();
    exit(0);
}