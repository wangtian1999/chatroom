#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include <process.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888
#define MAX_RECORDS 1000
#define RECORDS_PER_PAGE 20
#define NICKNAME_SIZE 50

// Chat record structure
typedef struct {
    char timestamp[32];
    char message_type[16];  // CHAT, PRIVATE, SYSTEM
    char sender[NICKNAME_SIZE];
    char receiver[NICKNAME_SIZE];  // For private messages
    char content[BUFFER_SIZE];
} ChatRecord;

SOCKET client_socket;
int connected = 0;
char nickname[NICKNAME_SIZE];
ChatRecord chat_history[MAX_RECORDS];
int record_count = 0;
int current_page = 0;

// Function declarations
int init_client();
void connect_to_server();
void send_message(const char* message);
unsigned __stdcall receive_thread(void* param);
void display_help();
void cleanup_client();
void save_chat_record(const char* type, const char* sender, const char* receiver, const char* content);
void display_chat_history(int page);
void export_chat_history();
void parse_and_save_message(const char* buffer);

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
    printf("  /history [page] - View chat history\n");
    printf("  /export - Export chat history to file\n");
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
                send_message("USERS");
            } else if (strcmp(input, "/history") == 0) {
                display_chat_history(0);
                current_page = 0;
            } else if (strncmp(input, "/history ", 9) == 0) {
                int page = atoi(input + 9) - 1;
                if (page < 0) page = 0;
                display_chat_history(page);
                current_page = page;
            } else if (strcmp(input, "/export") == 0) {
                export_chat_history();
            } else if (strcmp(input, "/next") == 0) {
                display_chat_history(++current_page);
            } else if (strcmp(input, "/prev") == 0) {
                 if (current_page > 0) {
                     display_chat_history(--current_page);
                 } else {
                     printf("Already at first page\n");
                 }
            } else if (strncmp(input, "/private ", 9) == 0) {
                // Parse private message: /private nickname message
                char* space_pos = strchr(input + 9, ' ');
                if (space_pos != NULL) {
                    *space_pos = '\0';
                    char private_msg[BUFFER_SIZE];
                    sprintf_s(private_msg, BUFFER_SIZE, "PRIVATE:%s:%s", input + 9, space_pos + 1);
                    send_message(private_msg);
                    // Save sent private message to history
                    save_chat_record("PRIVATE", nickname, input + 9, space_pos + 1);
                } else {
                    printf("Usage: /private <nickname> <message>\n");
                }
            } else {
                // Regular chat message
                char chat_msg[BUFFER_SIZE];
                sprintf_s(chat_msg, BUFFER_SIZE, "CHAT:%s", input);
                send_message(chat_msg);
                // Save sent public message to history
                save_chat_record("CHAT", nickname, NULL, input);
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
                save_chat_record("SYSTEM", "Server", NULL, buffer + 7);
            } else if (strncmp(buffer, "CHAT:", 5) == 0) {
                printf("\n%s\n> ", buffer + 5);
                parse_and_save_message(buffer);
            } else if (strncmp(buffer, "PRIVATE:", 8) == 0) {
                printf("\n%s\n> ", buffer + 8);
                parse_and_save_message(buffer);
            } else if (strncmp(buffer, "USERS:", 6) == 0) {
                printf("\n%s\n> ", buffer + 6);
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
    printf("/history [page]                 - View chat history (optional page number)\n");
    printf("/next                           - Next page of chat history\n");
    printf("/prev                           - Previous page of chat history\n");
    printf("/export                         - Export chat history to file\n");
    printf("/quit                           - Exit the chat application\n");
    printf("\nGeneral Usage:\n");
    printf("- Type any message and press Enter to send to all users\n");
    printf("- Commands must start with '/' character\n");
    printf("- Nicknames are case-sensitive\n");
    printf("\nExamples:\n");
    printf("  Hello everyone!                - Public message\n");
    printf("  /private John Hi there!        - Private message to John\n");
    printf("  /history 2                      - View page 2 of chat history\n");
    printf("  /export                         - Save chat history to file\n");
    printf("========================\n\n");
}

void cleanup_client() {
    if (client_socket != INVALID_SOCKET) {
        closesocket(client_socket);
    }
    WSACleanup();
}

void save_chat_record(const char* type, const char* sender, const char* receiver, const char* content) {
    if (record_count >= MAX_RECORDS) {
        // Remove oldest record to make space
        for (int i = 0; i < MAX_RECORDS - 1; i++) {
            chat_history[i] = chat_history[i + 1];
        }
        record_count = MAX_RECORDS - 1;
    }
    
    ChatRecord* record = &chat_history[record_count];
    
    // Get current time
    time_t now = time(NULL);
    struct tm local_time;
    localtime_s(&local_time, &now);
    strftime(record->timestamp, sizeof(record->timestamp), "%Y-%m-%d %H:%M:%S", &local_time);
    
    // Copy data
    strncpy_s(record->message_type, sizeof(record->message_type), type, _TRUNCATE);
    strncpy_s(record->sender, sizeof(record->sender), sender ? sender : "", _TRUNCATE);
    strncpy_s(record->receiver, sizeof(record->receiver), receiver ? receiver : "", _TRUNCATE);
    strncpy_s(record->content, sizeof(record->content), content, _TRUNCATE);
    
    record_count++;
}

void display_chat_history(int page) {
    if (record_count == 0) {
        printf("\nNo chat records\n\n");
        return;
    }
    
    int start_index = page * RECORDS_PER_PAGE;
    int end_index = start_index + RECORDS_PER_PAGE;
    
    if (start_index >= record_count) {
        printf("\nNo more records\n\n");
        return;
    }
    
    if (end_index > record_count) {
        end_index = record_count;
    }
    
    printf("\n=== Chat History (Page %d) ===\n", page + 1);
    printf("Showing records %d-%d, total %d\n", start_index + 1, end_index, record_count);
    printf("---------------------------\n");
    
    for (int i = start_index; i < end_index; i++) {
        ChatRecord* record = &chat_history[i];
        printf("[%s] ", record->timestamp);
        
        if (strcmp(record->message_type, "CHAT") == 0) {
            printf("<%s> %s\n", record->sender, record->content);
        } else if (strcmp(record->message_type, "PRIVATE") == 0) {
            if (strlen(record->receiver) > 0) {
                printf("[Private] %s -> %s: %s\n", record->sender, record->receiver, record->content);
            } else {
                printf("[Private] %s: %s\n", record->sender, record->content);
            }
        } else if (strcmp(record->message_type, "SYSTEM") == 0) {
            printf("[System] %s\n", record->content);
        }
    }
    
    int total_pages = (record_count + RECORDS_PER_PAGE - 1) / RECORDS_PER_PAGE;
    printf("---------------------------\n");
    printf("Page %d/%d | Use /next and /prev to navigate\n\n", page + 1, total_pages);
}

void export_chat_history() {
    if (record_count == 0) {
        printf("\nNo chat records to export\n\n");
        return;
    }
    
    char filename[256];
    time_t now = time(NULL);
    struct tm local_time;
    localtime_s(&local_time, &now);
    strftime(filename, sizeof(filename), "chat_history_%Y%m%d_%H%M%S.txt", &local_time);
    
    FILE* file;
    if (fopen_s(&file, filename, "w") != 0) {
        printf("\nFailed to create export file\n\n");
        return;
    }
    
    fprintf(file, "Chat History Export\n");
    fprintf(file, "Export Time: ");
    char export_time[64];
    strftime(export_time, sizeof(export_time), "%Y-%m-%d %H:%M:%S", &local_time);
    fprintf(file, "%s\n", export_time);
    fprintf(file, "Total Records: %d\n", record_count);
    fprintf(file, "================================\n\n");
    
    for (int i = 0; i < record_count; i++) {
        ChatRecord* record = &chat_history[i];
        fprintf(file, "[%s] ", record->timestamp);
        
        if (strcmp(record->message_type, "CHAT") == 0) {
            fprintf(file, "<%s> %s\n", record->sender, record->content);
        } else if (strcmp(record->message_type, "PRIVATE") == 0) {
            if (strlen(record->receiver) > 0) {
                fprintf(file, "[Private] %s -> %s: %s\n", record->sender, record->receiver, record->content);
            } else {
                fprintf(file, "[Private] %s: %s\n", record->sender, record->content);
            }
        } else if (strcmp(record->message_type, "SYSTEM") == 0) {
            fprintf(file, "[System] %s\n", record->content);
        }
    }
    
    fclose(file);
    printf("\nChat history exported to: %s\n\n", filename);
}

void parse_and_save_message(const char* buffer) {
    if (strncmp(buffer, "CHAT:", 5) == 0) {
        // Parse: "CHAT:[nickname] message"
        const char* content = buffer + 5;
        if (content[0] == '[') {
            const char* end_bracket = strchr(content, ']');
            if (end_bracket != NULL) {
                char sender[NICKNAME_SIZE];
                int sender_len = end_bracket - content - 1;
                if (sender_len > 0 && sender_len < NICKNAME_SIZE) {
                    strncpy_s(sender, sizeof(sender), content + 1, sender_len);
                    sender[sender_len] = '\0';
                    const char* message = end_bracket + 2; // Skip "] "
                    save_chat_record("CHAT", sender, NULL, message);
                }
            }
        }
    } else if (strncmp(buffer, "PRIVATE:", 8) == 0) {
        // Parse: "PRIVATE:[from_nickname] message"
        const char* content = buffer + 8;
        if (content[0] == '[') {
            const char* end_bracket = strchr(content, ']');
            if (end_bracket != NULL) {
                char sender[NICKNAME_SIZE];
                int sender_len = end_bracket - content - 1;
                if (sender_len > 0 && sender_len < NICKNAME_SIZE) {
                    strncpy_s(sender, sizeof(sender), content + 1, sender_len);
                    sender[sender_len] = '\0';
                    const char* message = end_bracket + 2; // Skip "] "
                    save_chat_record("PRIVATE", sender, nickname, message);
                }
            }
        }
    }
}