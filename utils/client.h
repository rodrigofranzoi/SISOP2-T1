#define SHOULD_CREATE_THREAD 1

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )
#define LOG_DEBUG 1
#define MAGICTRICK 1000

#include <sys/file.h>
#include <libgen.h>
#define   LOCK_SH   1    /* shared lock */
#define   LOCK_EX   2    /* exclusive lock */
#define   LOCK_NB   4    /* don't block when locking */
#define   LOCK_UN   8    /* unlock */

char commands[7][13] = {"upload", "download", "list_server", "get_sync_dir", "exit", "list_client", "delete"};

int initHost(char *argv[], int argc);
int connect_server (char *host, int port);
char *itoa_simple(char *dest, int i);
void sync_client_first();
void *sync_thread();
void initializeNotifyDescription();
void get_all_files();
int create_sync_sock();
void getFilename(char *pathname, char *filename);
int exists(const char *fname);
void delete_file_request(char* file, int socket);
int getFileSize(FILE *ptrfile);
void upload_file(char *file, int socket);
void client_interface();
int commandRequest(char *request, char *file);
void show_files();
void close_connection();
void get_file(char *file, int shouldSaveOnMainDir);
void createMainDir();
void handleGetSyncDirClient();
void list_client();
void delete_file(char* file);
void refreshSocket();
void *sync_thread_server();
int create_sync_sock_server();
void signal2download(struct packet responseThread);
void signal2delete(struct packet responseThread);
void shutdownInotify();
void addWatchInotify();