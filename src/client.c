#include <netdb.h> 
#include <pwd.h>
#include <sys/inotify.h>
#include "../utils/config.h"
#include "../utils/client.h"
#include "../utils/utils.h"


char username[USER_MAX_NAME];
char *host;
int port;
char directory[FILENAME_MAX_SIZE + 50];
int sockfd = -1, sync_socket = -1, sync_server_client_socket = -1; 
pthread_mutex_t lock; 
int notifyfd;
int watchfd;

int main(int argc, char *argv[]) {

    struct sockaddr_in serv_addr;
    struct hostent *server;
	pthread_mutex_init(&lock, NULL);
	
    char buffer[256];
    
    //Verifica os parametros e configura o host
    if(initHost(argv, argc) == -1) exit(0);

     if ((connect_server(host, port)) > 0) {
		// sincroniza diretório do servidor com o do cliente
		  sync_client_first();
		  

		// espera por um comando de usuário
		  client_interface();
     }


    return 0;
}

int initHost(char *argv[], int argc) {
    int resp = 1;
    
    if (argc < 3) {
		printf("[LOG] - Insufficient arguments: user, hostname or port \n");
		return -1;
    }

    // primeiro argumento nome do usuário
	if (strlen(argv[1]) <= USER_MAX_NAME) {
        strcpy(username, argv[1]);
    } else {
        printf("[LOG] - User Name Is Too long \n");
        return -1;
    };

    //segundo arumento hostname do server	
	host = malloc(sizeof(argv[2]));
	strcpy(host, argv[2]);

	// terceiro argumento porta
    port = atoi(argv[3]);

    return resp;
}

int connect_server (char *host, int port) {

	int socketByteSize;
	struct sockaddr_in server_addr;
	struct hostent *server;
	char buffer[256];

    packet connected;

    // Inicializa Server
	server = gethostbyname(host);
	if (server == NULL) {
        printf("[LOG] - ERROR, no such host\n");
        return -1;
    }

	// Inicializa Socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		printf("[LOG] - ERROR opening socket\n");
		return -1;
	}

	// Inicializa Addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(server_addr.sin_zero), 8);

    // Testa Socket
	if (connect(sockfd,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0) {
  		printf("[LOG] - ERROR connecting\n");
		return -1;
	}

    packet threadSetter;
    threadSetter.type = RESP;
    threadSetter.payloadCommand = SHOULD_CREATE_THREAD;
	write(sockfd, &threadSetter, sizeof(struct packet));

    packet userPacket;
    userPacket.type = RESP;
    strcpy(userPacket._payload, username);

	//envia username para o servidor
    socketByteSize = write(sockfd, &userPacket, sizeof(struct packet));
    if (socketByteSize < 0) {
 	    printf("[LOG] - ERROR sending username to server\n");
        return -1;
    }

	socketByteSize = read(sockfd, &connected, sizeof(struct packet));

    if(connected.type ==  RESP){
        if (socketByteSize < 0){
            printf("[LOG] - ERROR receiving connected message\n");
            return -1;
        } else if (connected.payloadCommand) {
            printf("[LOG] - Connected\n");
            return 1;
        } else {
            printf("[LOG] - You already have two devices connected\n");
            return -1;
        }
    } else return -1;
}

void sync_client_first() {
	char *homedir;
	char fileName[FILE_MAX + 10] = "sync_dir_";
	pthread_t syn_th, sync_server_client;

	if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
	// nome do arquivo
	strcat(fileName, username);

	// forma o path do arquivo
	strcpy(directory, homedir);
	strcat(directory, "/");
	strcat(directory, fileName);

	if (mkdir(directory, 0777) < 0) {
		// erro
		if (errno != EEXIST)
			printf("ERROR creating directory\n");
	}
	// diretório não existe
	else {
		printf("[LOG] - Creating %s directory in your home\n", fileName);
	}

	initializeNotifyDescription();

	//cria thread para sincronização Client Server
	if(pthread_create(&syn_th, NULL, sync_thread, NULL)) {
		printf("ERROR creating thread\n");
	}

	//cria thread para sincronização Server Client
	if(pthread_create(&sync_server_client, NULL, sync_thread_server, NULL)) {
		printf("ERROR creating thread\n");
	}
}

void createMainDir(){
	char *homedir;
	char fileName[FILE_MAX + 10] = "sync_dir_";

	if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
	// nome do arquivo
	strcat(fileName, username);

	// forma o path do arquivo
	strcpy(directory, homedir);
	strcat(directory, "/");
	strcat(directory, fileName);

	if (mkdir(directory, 0777) < 0) {
		// erro
		if (errno != EEXIST)
			printf("ERROR creating directory\n");
	}
	// diretório não existe
	else {
		printf("[LOG] - Creating %s directory in your home\n", fileName);
	}
}

void handleGetSyncDirClient(){
	createMainDir();
	get_all_files();
}

void *sync_thread_server() {
	int byteCount;
	create_sync_sock_server();


    packet respUserPacket;
	//Get Ok do server
	if(LOG_DEBUG) printf("[SYNC_SERVER] - Waiting to get response OK from Server\n");
    byteCount = read(sync_server_client_socket, &respUserPacket, sizeof(struct packet));
    if (byteCount < 0) {
 	    printf("[SYNC_SERVER] - ERROR sending username to server\n");
    }
	if(respUserPacket.payloadCommand == 1) {
		if(LOG_DEBUG) printf("[SYNC_SERVER] - SYNC SERVER CLIENT OK %d\n\n", respUserPacket.payloadCommand);
	}


	do {
		packet respServerPacket;
		byteCount = read(sync_server_client_socket, &respServerPacket, sizeof(struct packet));

		if(LOG_DEBUG) printf("[SYNC_SERVER] - Server CMD %d \n", respServerPacket.payloadCommand);
		
		switch (respServerPacket.payloadCommand) {
			case S_DOWNLOAD: signal2download(respServerPacket); break;
			case S_DELETE: signal2delete(respServerPacket);  break;
			default: break;
		}
	} while (1);


}

void signal2download(struct packet responseThread){
	char filePath[200]; 
	time_t localFileMdTime;
	struct stat attr;
	double seconds;

	if(LOG_DEBUG) printf("[SIGNAL_DOWNLOAD] - New Signal Download\n");
	if(LOG_DEBUG) printf("[SIGNAL_DOWNLOAD] - Signal File Name %s \n", responseThread._payload);
	if(LOG_DEBUG) printf("[SIGNAL_DOWNLOAD] - Signal File Last Modified %s", ctime(&responseThread.lst_modified));


	strcpy(filePath, directory);
	strcat(filePath, "/");
	strcat(filePath, responseThread._payload);

	if(stat(filePath, &attr) == 0){
		localFileMdTime = attr.st_ctime;
	} else{
		localFileMdTime = 0;
	}

	if(LOG_DEBUG) printf("[SIGNAL_DOWNLOAD] - Signal Local File Last Modified %s", ctime(&localFileMdTime));
	seconds = difftime(responseThread.lst_modified, localFileMdTime);
	if( seconds > 0 ) {
		if(LOG_DEBUG) printf("[SIGNAL_DOWNLOAD] - Getting a New Copy of %s \n", responseThread._payload);
		// sleep(1.5);
		usleep(MAGICTRICK);
		get_file(responseThread._payload, 1);
	} else {
		if(LOG_DEBUG) printf("[SIGNAL_DOWNLOAD] - Signal File Name %s Is Already Up to Date \n", responseThread._payload);
	}

	if(LOG_DEBUG) printf("[SIGNAL_DOWNLOAD] - END \n\n");
}

void signal2delete(struct packet responseThread){
	char filePath[200];

	if(LOG_DEBUG) printf("[SIGNAL_DELETE] - New Signal Delet\n");
	if(LOG_DEBUG) printf("[SIGNAL_DELETE] - Signal File Name %s \n", responseThread._payload);

	delete_file(responseThread._payload);

	if(LOG_DEBUG) printf("[SIGNAL_DELETE] - END \n\n");
}

void *sync_thread() {
	int length, i = 0;
    char buffer[BUF_LEN];
	char path[200];
	char lasEvent[200];
	bzero(lasEvent, sizeof(lasEvent));

	create_sync_sock();
	get_all_files();

	while(1) {
	  length = read( notifyfd, buffer, BUF_LEN );

	  if ( length < 0 ) {
	    perror( "read" );
	  }

	  while ( i < length ) {
	    struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];		
	    if ( event->len ) {
				if ( event->mask & IN_CLOSE_WRITE || event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
					strcpy(path, directory);
					strcat(path, "/");
					strcat(path, event->name);
					if(exists(path) && (event->name[0] != '.')) {
						if(LOG_DEBUG) printf("\n\n\n[INOTIFY] - THIS EVENT %s LAST EVENT %d\n\n\n", event->name, event->mask);
						sleep(1);
						// usleep(MAGICTRICK);
						upload_file(path, sync_socket);
					}
				}
				else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
					strcpy(path, directory);
					strcat(path, "/");
					strcat(path, event->name);
					if(event->name[0] != '.') {
						if(LOG_DEBUG) printf("[INOTIFY] - Inotify Deleting File %s Under Event %d\n", event->name, event->mask);
						delete_file_request(path, sync_socket);
					}
				}		
	    }
	    i += EVENT_SIZE + event->len;	
  	  }
		i = 0;
		usleep(MAGICTRICK);
	}
}

void initializeNotifyDescription() {
	printf("[INIT_NOTIFY] - Started Watch Inotify\n");
	notifyfd = inotify_init();
	watchfd = inotify_add_watch(notifyfd, directory, IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
	if(LOG_DEBUG) printf("[INIT_NOTIFY] - END \n\n");
}

void shutdownInotify(){
	int ret;
	ret = inotify_rm_watch(notifyfd, watchfd);
	if(ret < 0) {
		printf("[SHUTDOWN_NOTIFY] - Removed Watch Inotify Error\n");
	} else {
		printf("[SHUTDOWN_NOTIFY] - Removed Watch Inotify OK\n");
	}
	if(LOG_DEBUG) printf("[SHUTDOWN_NOTIFY] - END \n\n");
}

void addWatchInotify(){
	printf("[ADD_NOTIFY] - Added Watch Inotify\n");
	watchfd = inotify_add_watch(notifyfd, directory, IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
	if(LOG_DEBUG) printf("[ADD_NOTIFY] - END \n\n");
}

pthread_mutex_t file_mutex2;
pthread_mutex_t file_mutex3;
void get_all_files() {
	printf("[GET_ALL_FILES] - Getting All Files From Server\n");
	int byteCount, bytesLeft, fileSize, fileNum, i;
	FILE* ptrfile;
	char dataBuffer[KBYTE], file[FILENAME_MAX_SIZE], path[KBYTE];

	// copia nome do arquivo e comando para enviar para o servidor
	printf("[GET_ALL_FILES] - Making Download Request\n");
    packet threadRequest;
    threadRequest.type = CMD;
    threadRequest.payloadCommand = DOWNLOADALL;
	byteCount = write(sync_socket, &threadRequest, sizeof(struct packet));
	if (byteCount < 0){
		printf("[GET_ALL_FILES] - Error sending DOWNLOAD message to server\n");
	}
		
	packet connection;
	printf("[GET_ALL_FILES] - Waiting Server to Respond Number of Files\n");
	byteCount = read(sync_socket, &connection, sizeof(struct packet));
	if (byteCount < 0){
		printf("[GET_ALL_FILES] - Error FILENUM \n");
	}		
	fileNum = connection.payloadCommand;
	printf("[GET_ALL_FILES] - File Number on Server: %d\n", fileNum);
	for(i = 0; i < fileNum; i++) {
		// lê nome do arquivo do servidor
		printf("[GET_ALL_FILES] - Starting to Get File Num: %d\n", i);
		packet connectionFile;
		printf("[GET_ALL_FILES] - Waiting to Server Send File Name\n");
		byteCount = read(sync_socket, &connectionFile, sizeof(struct packet));
		if (byteCount < 0) {
			printf("Error receiving filename\n");
		}
		strcpy(file, connectionFile._payload);

		strcpy(path, directory);
		strcat(path, "/");
		strcat(path, file);

		printf("[GET_ALL_FILES] - Receive File Name: %s\n", file);
		// pthread_mutex_lock(&file_mutex2);
		get_file(file, 1);
		// pthread_mutex_unlock(&file_mutex2);
		bzero(file, sizeof(file));
		usleep(MAGICTRICK);
	}
	if(LOG_DEBUG) printf("[GET_ALL_FILES] - END \n\n");
}

int create_sync_sock_server() {
	int byteCount, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int client_thread = 0;
	char buffer[256];

	server = gethostbyname(host);
	if (server == NULL) {
  		return -1;
	}

	// tenta abrir o socket
	if ((sync_server_client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		return -1;
	}

	// inicializa server_addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);

	bzero(&(server_addr.sin_zero), 8);

	// tenta conectar ao socket
	if (connect(sync_server_client_socket,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0){
		printf("[SYNC_SERVER] - TENTANDO CONECTAR COM SYNC SOCKET");
		  return -1;
	}

	printf("[SYNC_SERVER] - Starting Server On Cmd 2\n");
    packet threadSetter;
    threadSetter.type = RESP;
    threadSetter.payloadCommand = 2;
	byteCount = write(sync_server_client_socket, &threadSetter, sizeof(struct packet));
	if (byteCount < 0) {
 	    printf("[SYNC_SERVER] - ERROR sending 0 to zerverr\n");
        return -1;
    }

	printf("[SYNC_SERVER] - Writing To Server Username\n");
    packet userPacket;
    userPacket.type = RESP;
    strcpy(userPacket._payload, username);
	//envia username para o servidor
    byteCount = write(sync_server_client_socket, &userPacket, sizeof(struct packet));
    if (byteCount < 0) {
 	    printf("[SYNC_SERVER] - ERROR Sending Username To Server\n");
        return -1;
    }
}

int create_sync_sock() {
	int byteCount, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int client_thread = 0;
	char buffer[256];

	server = gethostbyname(host);

	if (server == NULL)
	{
  	return -1;
  }

	// tenta abrir o socket
	if ((sync_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		return -1;
	}

	// inicializa server_addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);

	bzero(&(server_addr.sin_zero), 8);

	// tenta conectar ao socket
	if (connect(sync_socket,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0){
			printf("[SYNC] - TENTANDO CONECTAR COM SYNC SOCKET");
		  return -1;
	}

    packet threadSetter;
    threadSetter.type = RESP;
    threadSetter.payloadCommand = 0;
	strcpy(threadSetter._payload, username);
	byteCount = write(sync_socket, &threadSetter, sizeof(struct packet));
	if (byteCount < 0) {
 	    printf("ERROR sending 0 to zerverr\n");
        return -1;
    }
	
}

// função que extrai o nome do arquivo a partir de um pathname
void getFilename(char *pathname, char *filename) {
	char *filenameAux;

	filenameAux = strtok(pathname, "/");
	strcpy(filename, filenameAux);
	while(filenameAux != NULL) {
		strcpy(filename, filenameAux);
		filenameAux = strtok(NULL, "/");
	}
}

int exists(const char *fname) {
    FILE *file;
	shutdownInotify();
    if (file = fopen(fname, "rb")){
        fclose(file);
		addWatchInotify();
        return 1;
    }
	addWatchInotify();
    return 0;
}

void delete_file_request(char* file, int socket) {
	if(LOG_DEBUG) printf("[DELETE_FILE] - Starting Delete\n");
	int byteCount;
    packet threadRequest;
    threadRequest.type = CMD;
    threadRequest.payloadCommand = DELETE;
	getFilename(file, threadRequest._payload);

	if(LOG_DEBUG) printf("[DELETE_FILE] - Sent File Name: %s To Server\n", threadRequest._payload);
	byteCount = write(socket, &threadRequest, sizeof(struct packet));

	if (byteCount < 0) {
		printf("[DELETE_FILE] - ERROR sending delete file request\n");
	}

	if(LOG_DEBUG) printf("[DELETE_FILE] - END \n\n");	
}

void upload_file(char *file, int socket) {
	if(LOG_DEBUG) printf("[UPLOAD_FILE] - Getting Ready To Upload File\n");
	int byteCount, fileSize;
	FILE* ptrfile;
	char dataBuffer[KBYTE];
	struct stat attr;
	time_t changeTimeFile;

	pthread_mutex_lock(&lock);
	shutdownInotify();
    packet threadRequest;		
	    
    stat(file, &attr);
    changeTimeFile = attr.st_ctime;
   
	if (ptrfile = fopen(file, "rb")) {
			getFilename(file, threadRequest._payload);
            threadRequest.type = RESP;
            threadRequest.payloadCommand = UPLOAD;
            threadRequest.length = getFileSize(ptrfile);
			threadRequest.lst_modified = changeTimeFile;
			if(LOG_DEBUG) printf("[UPLOAD_FILE] - File Name: %s \n", threadRequest._payload);
			if(LOG_DEBUG) printf("[UPLOAD_FILE] - File Size: %d \n", threadRequest.length);
			if(LOG_DEBUG) printf("[UPLOAD_FILE] - File Last Modified: %s", ctime(&threadRequest.lst_modified));

			// if (threadRequest.length == 0) {
			// 	if(LOG_DEBUG) printf("[UPLOAD_FILE] - Closing File Due Size 0\n");
			// 	fclose(ptrfile);
			// 	pthread_mutex_unlock(&lock);
			// 	return;
			// }
            write(socket, &threadRequest, sizeof(struct packet));

		int cont =0;
			while(!feof(ptrfile)) {
				if(LOG_DEBUG) printf("[UPLOAD_FILE] - Sending Pack Number %d\n", ++cont);
					packet fileRequestBuff;
					fread(dataBuffer, sizeof(dataBuffer), 1, ptrfile);
					fileRequestBuff.type = DATA;
					memcpy(&fileRequestBuff._payload, &dataBuffer, sizeof(dataBuffer));
					fileRequestBuff.length = sizeof(dataBuffer);
					fileRequestBuff.payloadCommand = 9;
					byteCount = write(socket, &fileRequestBuff, sizeof(struct packet));
					if(byteCount < 0){
						printf("ERROR sending file\n");	
					}
					bzero(dataBuffer, sizeof(dataBuffer));		
			}
			fclose(ptrfile);

			if (socket != sync_socket)
				printf("[LOG] - The File %s Has Been Uploaded\n", file);
	}
	// arquivo não existe
	else {
		printf("[UPLOAD_FILE] - ERROR this file doesn't exist\n\n");
	}	
	addWatchInotify();
	pthread_mutex_unlock(&lock);
	if(LOG_DEBUG) printf("[UPLOAD_FILE] - END \n\n");
}

int getFileSize(FILE *ptrfile) {
	if(LOG_DEBUG) printf("[FILE_SIZE] - Starting \n");
	int size;

	fseek(ptrfile, 0L, SEEK_END);
	size = ftell(ptrfile);
	rewind(ptrfile);

	if(LOG_DEBUG) printf("[FILE_SIZE] - File Size Is %d \n", size);
	return size;
	if(LOG_DEBUG) printf("[FILE_SIZE] - END \n");
}

void client_interface() {
	int command = 0;
	char request[200], file[200];

	printf("\nCommands:\nupload <path/filename.ext>\ndownload <filename.ext>\nlist_server\nget_sync_dir\nexit\nlist_client\ndelete <filename.ext>\n");
	do {
		printf("\ntype your command: ");
		fgets(request, sizeof(request), stdin);
		command = commandRequest(request, file);

		// verifica requisição do cliente
		switch (command)
		{
			case LIST: show_files(); break;
			case EXIT: close_connection(); break;
			case SYNC: handleGetSyncDirClient(); break;
			case DOWNLOAD: get_file(file, 0); break;
		    case UPLOAD: upload_file(file, sockfd); break;
			case LIST_CLIENT: list_client(); break;
			case DELETE: delete_file(file); break;
			default: printf("ERROR invalid command\n");
		}
	}while(command != EXIT);
}

void list_client(){
	DIR *d, *userDir;
	struct dirent *dir, *userDirent;
	int i = 0;
	FILE *file_d;
	struct stat st;
	char folder[FILE_MAX], path[200], initial[200];

	strcpy(initial, directory);
    strcat(initial, "/");
	printf("diretorio: %s\n", initial);

  	d = opendir(initial);
  	if (d){
        while((userDirent = readdir(d)) != NULL) {
              //testa se e um arquivo regular e se esta no root dir
			if(userDirent->d_type == DT_REG && strcmp(userDirent->d_name,".")!=0 && strcmp(userDirent->d_name,"..")!=0) {
                	strcpy(path, initial);
                	strcat(path, userDirent->d_name);			
                 	stat(path, &st);
					printf("------------\n");
					printf("file name %s\n", userDirent->d_name) ;
					printf("file Size %ld\n", st.st_size);
					printf("file Last Modified: %s", ctime(&st.st_mtime));
					printf("file Access Time: %s", ctime(&st.st_atime));
					printf("file Creation: %s", ctime(&st.st_ctime));

					i++;
           	} 
        }
    }
		closedir(d);
}

void get_file(char *file, int shouldSaveOnMainDir) {
	int byteCount, bytesLeft, fileSize;
	struct client_request clientRequest;
	FILE* ptrfile;
	char dataBuffer[KBYTE];

	pthread_mutex_lock(&lock);
	shutdownInotify();
	// copia nome do arquivo e comando para enviar para o servidor
	packet clientCMDRequest;
	clientCMDRequest.type = CMD;
	clientCMDRequest.payloadCommand = DOWNLOAD;
	strcpy(clientCMDRequest._payload, file);
	if(LOG_DEBUG) printf("[GET_FILE] - File Name: %s To Server\n", clientCMDRequest._payload);

	byteCount = write(sockfd, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0){
		printf("[GET_FILE] - Error sending DOWNLOAD message to server\n");
	}
		
	packet clientCMDSize;
	if(LOG_DEBUG) printf("[GET_FILE] - Waiting to Receive File Structure\n");
	byteCount = read(sockfd, &clientCMDSize, sizeof(struct packet));
	if (byteCount < 0){
		printf("Error receiving filesize\n");
	}
	fileSize = clientCMDSize.length;
	if(LOG_DEBUG) printf("[GET_FILE] - File Size: %d\n", fileSize);

	if (fileSize < 0) {
		printf("[GET_FILE] - The file doesn't exist\n\n\n");
		return;
	}

	char fileDirector[200];
	bzero(fileDirector, sizeof(fileDirector));
	if(shouldSaveOnMainDir){
		strcpy(fileDirector, directory);
		strcat(fileDirector, "/");
		strcat(fileDirector, file);
	
	} else {
		strcpy(fileDirector, file);
	}

	if(LOG_DEBUG) printf("[GET_FILE] - Downloading File To: %s\n", fileDirector);

	// cria arquivo no diretório do cliente
	ptrfile = fopen(fileDirector, "wb");

	// número de bytes que faltam ser lidos
	bytesLeft = fileSize;
	int cont = 0;
	while(bytesLeft > 0) {
		// lê 1kbyte de dados do arquivo do servidor
		bzero(dataBuffer, KBYTE);
		cont++;
		packet clientCMDData;
		byteCount = read(sockfd, &clientCMDData, sizeof(struct packet));
		if(bytesLeft > KBYTE) {
			byteCount = fwrite(clientCMDData._payload, KBYTE, 1, ptrfile);
		}
		else {
			fwrite(clientCMDData._payload, bytesLeft, 1, ptrfile);
		}
		// decrementa os bytes lidos
		bytesLeft -= KBYTE;
		usleep(MAGICTRICK);
	}

	fclose(ptrfile);
	printf("[LOG] - File %s has been downloaded\n\n", file);
	addWatchInotify();
	pthread_mutex_unlock(&lock);
	if(LOG_DEBUG) printf("[GET_FILE] - END \n\n");
}

int commandRequest(char *request, char *file) {
	char *requestAux, *fileAux;
	int strLen;

	strLen = strlen(request);
	if ((strLen > 0) && (request[strLen-1] == '\n')) {
		  request[strLen-1] = '\0';
	}

	if (!strcmp(request, commands[LIST]))
		return LIST;
	else if (!strcmp(request, commands[EXIT]))
		return EXIT;
	else if (!strcmp(request, commands[SYNC]))
		return SYNC;
	else if (!strcmp(request, commands[LIST_CLIENT]))
		return LIST_CLIENT;	

	requestAux = strtok(request, " ");
	fileAux = strtok(NULL, "\n");
	if (fileAux != NULL)
		strcpy(file, fileAux);
	else
		return -1;

	if (file != NULL){
		if (!strcmp(requestAux, commands[DOWNLOAD]))
			return DOWNLOAD;
		else if (!strcmp(requestAux, commands[UPLOAD]))
			return UPLOAD;
		else if (!strcmp(requestAux, commands[DELETE]))
			return DELETE;	
	}
	else
		return -1;
}

void show_files() {
	int byteCount, fileNum, i;
	struct file_info file_info;
	struct stat st;

	// pthread_mutex_lock(&lock);
	packet clientCMDRequest;
	clientCMDRequest.type = CMD;
	clientCMDRequest.payloadCommand = LIST;
	byteCount = write(sockfd, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error sending LIST message to server\n");

	// lê número de arquivos existentes no diretório
	packet clientCMDFileSizes;
	byteCount = read(sockfd, &clientCMDFileSizes, sizeof(struct packet));
		if (byteCount < 0)
		printf("Error READING LIST message to server\n");
	usleep(MAGICTRICK);	
	fileNum = clientCMDFileSizes.payloadCommand;
	printf("FILE SIZE RECEIVED: %d", clientCMDFileSizes.payloadCommand);
	printf("Count Files: %d \n", fileNum);

	if (clientCMDFileSizes.payloadCommand == 0){
		printf("Empty directory\n\n\n");
		return;
	}

	for (i = 0; i < fileNum; i++) {
		byteCount = read(sockfd, &file_info, sizeof(file_info));
		printf("\nFile: %s \nLast modified: %sLast Access: %sLast Created: %ssize: %d\n", file_info.name, file_info.last_modified, ctime(&file_info.st.st_atime), ctime(&file_info.st.st_ctime), file_info.size);
	}
	// pthread_mutex_unlock(&lock);
}

void close_connection() {
	int byteCount;

	packet clientCMDRequest;
	clientCMDRequest.type = CMD;
	clientCMDRequest.payloadCommand = EXIT;

	//Disconect ASYNC
	byteCount = write(sockfd, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error sending LIST message to server\n");
	
	//Disconect SYNC
	byteCount = write(sync_socket, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error sending LIST message to server\n");

	close(sockfd);
	// close(sync_server_client_socket);
	close(sync_socket);
	printf("Connection with server has been closed\n");
}

void delete_file(char* file) {
	char *homedir;
	char folder[FILE_MAX + 50];

	// constroi caminho do home do usuário
	strcpy(folder, directory);
	strcat(folder, "/");
	strcat(folder, file);

    int status = remove(folder);
 
  	if (status == 0){
    	printf("%s file deleted successfully.\n", file);
	} else {
    	printf("Unable to delete the file\n");
  	}
}