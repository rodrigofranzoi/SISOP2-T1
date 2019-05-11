#define SHOULD_CREATE_THREAD 1

#define KBYTE 1024
#define DELETE 6
#define DOWNLOADALL 5
#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0


#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

int initHost(char *argv[], int argc);
int connect_server (char *host, int port);
char *itoa_simple(char *dest, int i);
void sync_client_first();
void *sync_thread();
void initializeNotifyDescription();
void get_all_files();
int create_sync_sock();
void getFilename(char *pathname, char *filename);
time_t getFileModifiedTime(char *path);
int exists(const char *fname);