#define SHOULD_CREATE_THREAD 1




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
void delete_file_request(char* file, int socket);
int getFileSize(FILE *ptrfile);
void upload_file(char *file, int socket);
void client_interface();