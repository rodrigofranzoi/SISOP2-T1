#include "../utils/config.h"


#define DEVICE_FREE -1
#define MAIN_REPOSITORY "./../repository"
#define LOG_DEBUG 1

struct client
{
  int devices[2];
  int syncSocket[2];
  char username[USER_MAX_NAME];
  struct file_info file_info[FILE_MAX];
  int logged;
};

struct client_list
{
  struct client client;
  struct client_list *next;
};

struct socketPacket{
  struct packet packet;
  int socket;
};

void insertList(struct client_list **client_list, struct client client);
int initializeClientList();
void *client_thread (void *socket);
int initializeClient(int client_socket, char *username, struct client *client);
int findNode(char *username, struct client_list *client_list, struct client_list **client_node);
void *sync_thread_sv(void *socketPacket);
void listen_sync(int client_socket, char *username);
void send_all_files(int client_socket, char *username);
int getFileSize(FILE *ptrfile);
void receive_file(struct packet responseThread, int socket, char*username);
void updateFileInfo(char *username, struct file_info file_info);
void delete_file(char *file, int socket, char *username);
void listen_client(int client_socket, char *username);
void show_files();
void send_file_info(int socket, char *username);
void close_client_connection(int socket, char *username);
void send_file(char *file, int socket, char *username);
void listen_syncServerClient(int client_socket, char *username);
void sync_thread_ServerClient(void *socket);
void signal_download2client(char *username, char *fileName, time_t *last_modified);
void signal_delete2client(char *username, char *fileName);
struct file_info getFileInfo(char *username, char*filename);
