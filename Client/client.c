#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include <process.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define DATA_BUFFER_SIZE 1024
#define HUB_IP "127.0.0.1"
#define HUB_PORT 9999
#define MAX_LOG_ENTRIES 1000
#define ENTRIES_PER_PAGE 20
#define USERNAME_SIZE 50

// Communication log structure
typedef struct {
    char timestamp[32];
    char comm_type[16];  // BROADCAST, WHISPER, NOTIFICATION
    char from_user[USERNAME_SIZE];
    char to_user[USERNAME_SIZE];  // For private communications
    char message_content[DATA_BUFFER_SIZE];
} CommLog;

SOCKET hub_connection;
int is_connected = 0;
char username[USERNAME_SIZE];
CommLog communication_log[MAX_LOG_ENTRIES];
int log_entry_count = 0;
int current_log_page = 0;

// Function declarations
int initialize_client();
void connect_to_hub();
void send_data(const char* data);
unsigned __stdcall communication_thread(void* param);
void show_help_menu();
void cleanup_connection();
void save_comm_log(const char* type, const char* from_user, const char* to_user, const char* content);
void display_comm_history(int page);
void export_comm_log();
void parse_and_log_message(const char* buffer);

int main() {
    printf("=== Communication Client ===\n");
    printf("Connecting to hub %s:%d\n\n", HUB_IP, HUB_PORT);
    
    if (initialize_client() != 0) {
        printf("Failed to initialize communication client\n");
        printf("Press any key to exit...");
        _getch();
        return 1;
    }
    
    connect_to_hub();
    
    if (!is_connected) {
        printf("Failed to connect to communication hub\n");
        printf("Press any key to exit...");
        _getch();
        cleanup_connection();
        return 1;
    }
    
    // Get username from user
    printf("Enter your username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0; // Remove newline
    
    // Wait for hub authentication prompt (username will be sent automatically)
    
    // Start communication thread
    HANDLE comm_handle = (HANDLE)_beginthreadex(NULL, 0, communication_thread, NULL, 0, NULL);
    
    printf("\n=== Connected to Communication Hub ===\n");
    printf("Available Commands:\n");
    printf("  /help - Show command help\n");
    printf("  /users - List connected clients\n");
    printf("  /whisper <username> <message> - Send private message\n");
    printf("  /log [page] - View communication log\n");
    printf("  /export - Export communication log to file\n");
    printf("  /exit - Disconnect from hub\n");
    printf("  Just type to broadcast public message\n");
    printf("======================================\n\n");
    
    char user_input[DATA_BUFFER_SIZE];
    while (is_connected) {
        printf("> ");
        if (fgets(user_input, sizeof(user_input), stdin) != NULL) {
            user_input[strcspn(user_input, "\n")] = 0; // Remove newline
            
            if (strlen(user_input) == 0) {
                continue;
            }
            
            if (strcmp(user_input, "/exit") == 0) {
                break;
            } else if (strcmp(user_input, "/help") == 0) {
                show_help_menu();
            } else if (strcmp(user_input, "/users") == 0) {
                send_data("USERS");
            } else if (strcmp(user_input, "/log") == 0) {
                display_comm_history(0);
                current_log_page = 0;
            } else if (strncmp(user_input, "/log ", 5) == 0) {
                int page = atoi(user_input + 5) - 1;
                if (page < 0) page = 0;
                display_comm_history(page);
                current_log_page = page;
            } else if (strcmp(user_input, "/export") == 0) {
                export_comm_log();
            } else if (strcmp(user_input, "/next") == 0) {
                display_comm_history(++current_log_page);
            } else if (strcmp(user_input, "/prev") == 0) {
                 if (current_log_page > 0) {
                     display_comm_history(--current_log_page);
                 } else {
                     printf("Already at first page\n");
                 }
            } else if (strncmp(user_input, "/whisper ", 9) == 0) {
                // Parse private message: /whisper username message
                char* space_pos = strchr(user_input + 9, ' ');
                if (space_pos != NULL) {
                    *space_pos = '\0';
                    char whisper_msg[DATA_BUFFER_SIZE];
                    sprintf_s(whisper_msg, DATA_BUFFER_SIZE, "PRIVATE:%s:%s", user_input + 9, space_pos + 1);
                    send_data(whisper_msg);
                    // Save sent private message to log
                    save_comm_log("WHISPER", username, user_input + 9, space_pos + 1);
                } else {
                    printf("Usage: /whisper <username> <message>\n");
                }
            } else {
                // Regular broadcast message
                char broadcast_msg[DATA_BUFFER_SIZE];
                sprintf_s(broadcast_msg, DATA_BUFFER_SIZE, "CHAT:%s", user_input);
                send_data(broadcast_msg);
                // Save sent public message to log
                save_comm_log("BROADCAST", username, NULL, user_input);
            }
        }
    }
    
    printf("\nDisconnecting from hub...\n");
    is_connected = 0;
    
    if (comm_handle) {
        WaitForSingleObject(comm_handle, 1000);
        CloseHandle(comm_handle);
    }
    
    cleanup_connection();
    printf("Press any key to exit...");
    _getch();
    return 0;
}

int initialize_client() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Network initialization failed\n");
        return 1;
    }
    
    hub_connection = socket(AF_INET, SOCK_STREAM, 0);
    if (hub_connection == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }
    
    return 0;
}

void connect_to_hub() {
    struct sockaddr_in hub_addr;
    hub_addr.sin_family = AF_INET;
    hub_addr.sin_port = htons(HUB_PORT);
    
    if (inet_pton(AF_INET, HUB_IP, &hub_addr.sin_addr) <= 0) {
        printf("Invalid hub address\n");
        return;
    }
    
    if (connect(hub_connection, (struct sockaddr*)&hub_addr, sizeof(hub_addr)) == SOCKET_ERROR) {
        printf("Connection failed. Error: %d\n", WSAGetLastError());
        return;
    }
    
    is_connected = 1;
    printf("Connected to communication hub successfully!\n");
}

void send_data(const char* data) {
    if (is_connected && strlen(data) > 0) {
        int result = send(hub_connection, data, strlen(data), 0);
        if (result == SOCKET_ERROR) {
            printf("Data transmission failed. Error: %d\n", WSAGetLastError());
            is_connected = 0;
        }
    }
}

unsigned __stdcall communication_thread(void* param) {
    char data_buffer[DATA_BUFFER_SIZE];
    int bytes_received;
    
    while (is_connected) {
        bytes_received = recv(hub_connection, data_buffer, DATA_BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            data_buffer[bytes_received] = '\0';
            
            // Parse different communication types
            if (strncmp(data_buffer, "REGISTER:", 9) == 0) {
                // Hub is asking for authentication, send username
                printf("\n%s\n", data_buffer + 9);
                send_data(username);
            } else if (strncmp(data_buffer, "SYSTEM:", 7) == 0) {
                printf("\n%s\n> ", data_buffer + 7);
                save_comm_log("NOTIFICATION", "Hub", NULL, data_buffer + 7);
            } else if (strncmp(data_buffer, "CHAT:", 5) == 0) {
                printf("\n%s\n> ", data_buffer + 5);
                parse_and_log_message(data_buffer);
            } else if (strncmp(data_buffer, "PRIVATE:", 8) == 0) {
                printf("\n%s\n> ", data_buffer + 8);
                parse_and_log_message(data_buffer);
            } else if (strncmp(data_buffer, "USERS:", 6) == 0) {
                printf("\n%s\n> ", data_buffer + 6);
            } else {
                printf("\n%s\n> ", data_buffer);
            }
            fflush(stdout);
        } else if (bytes_received == 0) {
            printf("\nHub disconnected\n");
            is_connected = 0;
            break;
        } else {
            printf("\nCommunication error: %d\n", WSAGetLastError());
            is_connected = 0;
            break;
        }
    }
    
    return 0;
}

void show_help_menu() {
    printf("\n=== Communication Commands ===\n");
    printf("/help - Show this help menu\n");
    printf("/exit - Leave the communication hub\n");
    printf("/users - Show online members\n");
    printf("/whisper <username> <message> - Send private message\n");
    printf("/log [page] - View communication history (default: current page)\n");
    printf("/export - Export communication log to file\n");
    printf("Type any message to broadcast to all members\n\n");
}

void cleanup_connection() {
    if (hub_connection != INVALID_SOCKET) {
        closesocket(hub_connection);
    }
    WSACleanup();
}

void save_comm_log(const char* type, const char* from_user, const char* to_user, const char* content) {
    if (log_entry_count >= MAX_LOG_ENTRIES) {
        // Remove oldest entry to make space
        for (int i = 0; i < MAX_LOG_ENTRIES - 1; i++) {
            communication_log[i] = communication_log[i + 1];
        }
        log_entry_count = MAX_LOG_ENTRIES - 1;
    }
    
    CommLog* entry = &communication_log[log_entry_count];
    
    // Get current time
    time_t now = time(NULL);
    struct tm local_time;
    localtime_s(&local_time, &now);
    strftime(entry->timestamp, sizeof(entry->timestamp), "%Y-%m-%d %H:%M:%S", &local_time);
    
    // Copy data
    strncpy_s(entry->comm_type, sizeof(entry->comm_type), type, _TRUNCATE);
    strncpy_s(entry->from_user, sizeof(entry->from_user), from_user ? from_user : "", _TRUNCATE);
    strncpy_s(entry->to_user, sizeof(entry->to_user), to_user ? to_user : "", _TRUNCATE);
    strncpy_s(entry->message_content, sizeof(entry->message_content), content, _TRUNCATE);
    
    log_entry_count++;
}

void display_comm_history(int page) {
    if (log_entry_count == 0) {
        printf("\nNo communication log entries\n\n");
        return;
    }
    
    int start_index = page * ENTRIES_PER_PAGE;
    int end_index = start_index + ENTRIES_PER_PAGE;
    
    if (start_index >= log_entry_count) {
        printf("\nNo more entries\n\n");
        return;
    }
    
    if (end_index > log_entry_count) {
        end_index = log_entry_count;
    }
    
    printf("\n=== Communication History (Page %d) ===\n", page + 1);
    printf("Showing entries %d-%d, total %d\n", start_index + 1, end_index, log_entry_count);
    printf("---------------------------\n");
    
    for (int i = start_index; i < end_index; i++) {
        CommLog* entry = &communication_log[i];
        printf("[%s] ", entry->timestamp);
        
        if (strcmp(entry->comm_type, "BROADCAST") == 0) {
            printf("<%s> %s\n", entry->from_user, entry->message_content);
        } else if (strcmp(entry->comm_type, "WHISPER") == 0) {
            if (strlen(entry->to_user) > 0) {
                printf("[Whisper] %s -> %s: %s\n", entry->from_user, entry->to_user, entry->message_content);
            } else {
                printf("[Whisper] %s: %s\n", entry->from_user, entry->message_content);
            }
        } else if (strcmp(entry->comm_type, "NOTIFICATION") == 0) {
            printf("[Notification] %s\n", entry->message_content);
        }
    }
    
    int total_pages = (log_entry_count + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
    printf("---------------------------\n");
    printf("Page %d/%d | Use /next and /prev to navigate\n\n", page + 1, total_pages);
}

void export_comm_log() {
    if (log_entry_count == 0) {
        printf("\nNo communication log entries to export\n\n");
        return;
    }
    
    char filename[256];
    time_t now = time(NULL);
    struct tm local_time;
    localtime_s(&local_time, &now);
    strftime(filename, sizeof(filename), "comm_log_%Y%m%d_%H%M%S.txt", &local_time);
    
    FILE* file;
    if (fopen_s(&file, filename, "w") != 0) {
        printf("\nFailed to create export file\n\n");
        return;
    }
    
    fprintf(file, "Communication Log Export\n");
    fprintf(file, "Export Time: ");
    char export_time[64];
    strftime(export_time, sizeof(export_time), "%Y-%m-%d %H:%M:%S", &local_time);
    fprintf(file, "%s\n", export_time);
    fprintf(file, "Total Entries: %d\n", log_entry_count);
    fprintf(file, "================================\n\n");
    
    for (int i = 0; i < log_entry_count; i++) {
        CommLog* entry = &communication_log[i];
        fprintf(file, "[%s] ", entry->timestamp);
        
        if (strcmp(entry->comm_type, "BROADCAST") == 0) {
            fprintf(file, "<%s> %s\n", entry->from_user, entry->message_content);
        } else if (strcmp(entry->comm_type, "WHISPER") == 0) {
            if (strlen(entry->to_user) > 0) {
                fprintf(file, "[Whisper] %s -> %s: %s\n", entry->from_user, entry->to_user, entry->message_content);
            } else {
                fprintf(file, "[Whisper] %s: %s\n", entry->from_user, entry->message_content);
            }
        } else if (strcmp(entry->comm_type, "NOTIFICATION") == 0) {
            fprintf(file, "[Notification] %s\n", entry->message_content);
        }
    }
    
    fclose(file);
    printf("\nCommunication log exported to: %s\n\n", filename);
}

void parse_and_log_message(const char* buffer) {
    if (strncmp(buffer, "CHAT:", 5) == 0) {
        // Parse: "CHAT:[username] message"
        const char* content = buffer + 5;
        if (content[0] == '[') {
            const char* end_bracket = strchr(content, ']');
            if (end_bracket != NULL) {
                char sender[USERNAME_SIZE];
                int sender_len = end_bracket - content - 1;
                if (sender_len > 0 && sender_len < USERNAME_SIZE) {
                    strncpy_s(sender, sizeof(sender), content + 1, sender_len);
                    sender[sender_len] = '\0';
                    const char* message = end_bracket + 2; // Skip "] "
                    save_comm_log("BROADCAST", sender, NULL, message);
                }
            }
        }
    } else if (strncmp(buffer, "PRIVATE:", 8) == 0) {
        // Parse: "PRIVATE:[from_username] message"
        const char* content = buffer + 8;
        if (content[0] == '[') {
            const char* end_bracket = strchr(content, ']');
            if (end_bracket != NULL) {
                char sender[USERNAME_SIZE];
                int sender_len = end_bracket - content - 1;
                if (sender_len > 0 && sender_len < USERNAME_SIZE) {
                    strncpy_s(sender, sizeof(sender), content + 1, sender_len);
                    sender[sender_len] = '\0';
                    const char* message = end_bracket + 2; // Skip "] "
                    save_comm_log("WHISPER", sender, username, message);
                }
            }
        }
    }
}