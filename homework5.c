#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <dirent.h>
#define BACKLOG (10)
#define PATH_ARG 1
#define DIRECTORY_LISTING_MAX_CHAR 1013

void serve_request(int);

char * request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n";

char * request_errorFL = "<b> HTTP/1.0 error: 404 File not found\r\n</b>"
	"Content-type: text/html; charset=UTF-8\r\n\r\n";

char * request_pdfFL = "HTTP/1.0 200 OK\r\n"
	"Content-type: application/pdf; charset=UTF-8\r\n\r\n";


char * request_icoFL = "HTTP/1.0 200 OK\r\n"
	"Content-type: image/ico; charset=UTF-8\r\n\r\n";

char * request_gifFL = "HTTP/1.0 200 OK\r\n"
	"Content-type: image/gif; charset=UTF-8\r\n\r\n";


char * request_jpgFL = "HTTP/1.0 200 OK\r\n"
	"Content-type: image/jpg; charset=UTF-8\r\n\r\n";

char * request_pngFL = "HTTP/1.0 200 OK\r\n"
	"Content-type: image/png; charset=UTF-8\r\n\r\n";

char * error = "<b>HTTP/1.0 404 Not Found\r\n</b>"
	"Content-type: text/html; charset=UTF-8\r\n\r\n"
	"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
	"<title>Directory listing for %s</title>"
	"<body>"
	"<Error 404: File not Found>"
	"</body>"
	"<html>";

char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
        "<title>Directory listing for %s</title>"
"<body>"
"<h2>Directory listing for %s</h2><hr><ul>";

// snprintf(output_buffer,4096,index_hdr,filename,filename);


char * index_body = "<li><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr></body></html>";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char* parseRequest(char* request) {
  //assume file paths are no more than 256 bytes + 1 for null. 
  char *buffer = malloc(sizeof(char)*257);
  memset(buffer, 0, 257);
  
  if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0; 

  sscanf(request, "GET %s HTTP/1.", buffer);
  return buffer; 
}

int DirectoryDoesExist(const char *path){
 struct stat statbuf;
 if (stat(path, &statbuf) != 0)
	return 0;
 return S_ISDIR(statbuf.st_mode);
}

int FileDoesExist(const char *path){
 struct stat statbuf;
 if (stat(path, &statbuf) != 0) 
	return 0;
 return S_ISREG(statbuf.st_mode);
} 

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
     strcpy(filetype, "text/html");
  else if  (strstr(filename, ".gif"))
     strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
     strcpy(filetype, "image/jpeg");
  else
     strcpy(filetype, "text/plain");
}

int file_exist(char *filename)
{
  FILE *fp = fopen(filename, "r");
  if (fp) {
	return 1;
  } 
  else { 
	return 0;
  } 
}


char* get_directory_contents(char* directory_path,int client_fd)
{
  char* directory_listing = NULL;
  char newPath[2048];
 
  // open directory path up 
  DIR* path = opendir(directory_path);

  // check to see if opening up directory was successful
  if(path != NULL)
  {
      directory_listing = (char*) malloc(sizeof(char)*DIRECTORY_LISTING_MAX_CHAR);
      directory_listing[0] = '\0';

      // stores underlying info of files and sub_directories of directory_path
      struct dirent* underlying_file = NULL;

      // iterate through all of the  underlying files of directory_path
      while((underlying_file = readdir(path)) != NULL)
      {
//        if(strcmp(underlying_file->d_name,"index.html")==0) { return 0; } 
       //   strcat(directory_listing, underlying_file->d_name);
          snprintf(newPath, 2048, "%s/%s", directory_path, underlying_file->d_name);         
          snprintf(directory_listing, 1013, "<li><a href=\"%s\">%s</a>",newPath, underlying_file->d_name);
       //   strcat(directory_listing, "\n");

      send(client_fd, directory_listing, strlen(directory_listing),0);
	}
      
      closedir(path);
  }

  return directory_listing;
}

void* thread(void* arg)
{
  int sock = (*((int*) arg));
  free(arg);
  pthread_detach(pthread_self());
  
  serve_request(sock);
  
  return NULL;
} 
 
void serve_request(int client_fd){
  int read_fd;
  int bytes_read;
  int file_offset = 0;
  char client_buf[4096];
  char send_buf[4096];
  char* temp[4096];
  char filename[4096];
  char * requested_file;
  char indexString[4096];
  DIR* directory;
  struct dirent* dir_entry;
  struct stat statbuf;
  memset(client_buf,0,4096);
  memset(filename,0,4096);
 
  while(1){
    file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
    if(strstr(client_buf,"\r\n\r\n"))
      break;
  }
  requested_file = parseRequest(client_buf); 
 // send(client_fd,request_str,strlen(request_str),0);
  // take requested_file, add a . to beginning, open that file
  filename[0] = '.';
  strncpy(&filename[1],requested_file,4095);
  int isFile = stat(filename,&statbuf);
  
  if(isFile == -1){
      send(client_fd,request_errorFL,strlen(request_errorFL),0);
      send(client_fd,error,strlen(error),0);
    } 
 
  else if(FileDoesExist(filename)){
    if (strstr(filename, ".pdf")){
      send(client_fd,request_pdfFL,strlen(request_pdfFL),0);
    }
      
       else if (strstr(filename, ".gif")){
      send(client_fd,request_gifFL,strlen(request_gifFL),0);
    }
      
       else if (strstr(filename, ".jpg")){
      send(client_fd,request_jpgFL,strlen(request_jpgFL),0);
    }
    else if(strstr(filename, ".png")){
      send(client_fd,request_pngFL,strlen(request_pngFL),0);
    }
    else if (strstr(filename, ".ico")){
      send(client_fd,request_icoFL,strlen(request_icoFL),0);
    }
    else
    //html file
   {
      send(client_fd,request_str,strlen(request_str),0);
    }	

//ready to open file
  read_fd = open(filename,0,0);
  while(1){
    bytes_read = read(read_fd,send_buf,4096);
    if(bytes_read == 0)
      break;

   send(client_fd,send_buf,bytes_read,0);
   }
}
// temp[0] = '.';
// strncpy(&temp[1],requested_file,4095);
  stat(filename,&statbuf);
  //check to see if it is an existing directory 
  if (S_ISDIR(statbuf.st_mode))
 {  
  //check to see if there is an index.html file
 // strcat(temp,"/"); 
  char htmlPath[4096];
 // snprintf(htmlPath, 4096, "%sindex.html", filename);

  directory = opendir(filename);
  if(directory == NULL){
	return;
  } 
 else{
	while(( dir_entry = readdir(directory))){ 
	if(strcmp(dir_entry->d_name, "index.html") == 0){
		send(client_fd,request_str,strlen(request_str),0);
		snprintf(htmlPath, 4096, "%s/index.html", filename);
		read_fd = open(htmlPath,0,0);
	//	strcat(filename, "index.html");
	//	read_fd = open(filename,0,0);
                while(1){
		  bytes_read = read(read_fd,send_buf,4096);
		  if(bytes_read == 0)
		    break;
		  send(client_fd,send_buf,bytes_read,0);
		 }
        	} 
         } 

    directory = opendir(filename); 
    if(directory==NULL){
	 return;
    }
 
    send(client_fd, request_str, strlen(request_str),0);
    snprintf(indexString, 4096, index_hdr, requested_file, requested_file);
    send(client_fd,indexString, strlen(indexString),0);
  
    get_directory_contents(filename, client_fd); 
    
    send(client_fd, index_ftr, strlen(index_ftr), 0);
    }
	
 } 

/*
  while(1){
    bytes_read = read(read_fd,send_buf,4096);
    if(bytes_read == 0)
      break;

    send(client_fd,send_buf,bytes_read,0);
*/

  close(read_fd);
  close(client_fd);
  return;
}

/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
    /* For checking return values. */
    int retval;

    /* Read the port number from the first command line argument. */
    int port = atoi(argv[1]);

    //make directory specified in second argument current directory
    chdir(argv[2]);
    
    /* Create a socket to which clients will connect. */
    int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    
    int reuse_true = 1;
    
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));
    
    if (retval < 0) {
        perror("Setting socket option failed.");
        exit(1);
    }

   
    
   
    struct sockaddr_in6 addr;   // internet socket address data structure
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces

    
    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    
    if(retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    
    if(retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }
    
    pthread_t* threadInfo;
    while(1) {
        
        int sock; 
        char buffer[256];
        
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr);
        
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        
        serve_request(sock);
        
        close(sock); 
       

	

    }

    close(server_sock);
}
