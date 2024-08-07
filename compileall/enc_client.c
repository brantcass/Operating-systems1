#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()


// Error function used for reporting issues
void error(const char *msg) { 
  perror(msg); 
  exit(1); 
}

int validChar(char x){
  return ((x >= 'A' && x <= 'Z') || x == ' ');
}

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber, 
                        char* hostname){
 
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);

  // Get the DNS entry for this host name
  struct hostent* hostInfo = gethostbyname(hostname); 
  if (hostInfo == NULL) { 
    fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
    exit(0); 
  }
  // Copy the first IP address from the DNS entry to sin_addr.s_addr
  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

char* readFile(const char* file_name, int* length){

  FILE *file = fopen(file_name, "r+");
  
  if(!file){
    fprintf(stderr, "Error: could not open file %s\n", file_name);
    exit(1);

  }
  
  //Seeking to EOF for size
  fseek(file, 0, SEEK_END);
  *length = ftell(file);
  rewind(file);// goes back to start


  //allocating memory
  char* buffer = (char*) malloc (*length + 1);
  if(!buffer){
    fprintf(stderr, "Memory allocation error\n");
    exit(1);
  }
  

  fread(buffer,1, *length, file);
  buffer[*length] = '\0';

  if(buffer[*length - 1] == '\n'){
    buffer [--(*length)] = '\0';
  }



  for (int i = 0; i < *length; i++) {
    if (!validChar(buffer[i])) {
      fprintf(stderr, "enc_client error: input contains bad characters: '%c' (ASCII %d)\n", buffer[i], buffer[i]);
      exit(1);
    }
  }

      
  fclose(file); 
  return buffer;

}
ssize_t send_all(int socketFD, const void *buffer, size_t length){

  size_t totalSent = 0;
  ssize_t byte_last = 0;
  const char* bufPtr = buffer;

  while(totalSent < length){
    byte_last = send(socketFD, bufPtr + totalSent, length - totalSent, 0);
    if(byte_last == -1){
      error("CLIENT: error sending data in send_all function");
      return -1;
     }
    totalSent += byte_last;
  }
  return totalSent;

}

int main(int argc, char *argv[]) {
  int socketFD, charsRead; 
  struct sockaddr_in serverAddress;
  //socklen_t sizeOfClientInfo = sizeof(clientAddress);
  int plain_text_len, key_len;
  char *plain_txt = readFile(argv[1], &plain_text_len);
  char *key = readFile(argv[2], &key_len);


  printf("Key length before sending %d\n", key_len);
  printf("plain text length before sending %d\n",plain_text_len);
  //Check usage & args
  if (argc < 2) { 
    fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); 
    exit(1); 
  }

  if(key_len < plain_text_len){
    fprintf(stderr, " Error: key '%s' is too short\n", argv[2]);
    free(plain_txt);
    free(key);
    exit(1);
  }
  
  setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");
  
  socketFD = socket(AF_INET, SOCK_STREAM, 0); 
  if (socketFD < 0){
    error("CLIENT: ERROR opening socket");
  }
   
  if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
    error("CLIENT: ERROR connecting");
  }
  
  //trying to send it all in one buffer instead of multiple send_alls
  size_t buffer_info = strlen("ENC_CLIENT") + plain_text_len + key_len + strlen("@@") + 1;
  char *buffer = malloc(buffer_info);

  if(!buffer){
    error("Error with buffer allocation in client");
  }
  
  strcpy(buffer, "ENC_CLIENT");
  strncat(buffer, plain_txt, plain_text_len);
  strncat(buffer,key,key_len);
  strcat(buffer, "@@");

  if(send_all(socketFD, buffer, strlen(buffer)) < 0){
    error("CLIENT: ERROR sending the main msg");
  }
  char *responseBuffer = malloc(1);

  // Make sure it is initialized before
  if (responseBuffer == NULL) {
      error("CLIENT: error allocating memory for response buffer");
  }
  responseBuffer[0] = '\0';

  size_t responseBufferSize = 0;
  int terminatorFound = 0;

  //Read the server's response until @@ found
  while (!terminatorFound) {

    char tempBuffer[10000]; //temporary buffer to hold received data
    memset(tempBuffer, '\0', sizeof(tempBuffer)); // Clear the buffer

    charsRead = recv(socketFD, tempBuffer, sizeof(tempBuffer) - 1, 0);
    if (charsRead < 0) {
        error("CLIENT: ERROR reading from socket");
    }

    //check if terminator found
    if (strstr(tempBuffer, "@@") != NULL) {
        terminatorFound = 1;
        //calculate the length until the terminator
        size_t terminatorIndex = strstr(tempBuffer, "@@") - tempBuffer;
        responseBufferSize += terminatorIndex;
    } else {
        //terminator not found add the entire received buffer length
        responseBufferSize += charsRead;
    }

    //reallocate responseBuffer for new data
    responseBuffer = realloc(responseBuffer, responseBufferSize + 1);
    if (responseBuffer == NULL) {
        error("CLIENT: ERROR allocating memory for response buffer");
    }

    //append new data to responseBuffer
    strncat(responseBuffer, tempBuffer, charsRead);
  }

  //Remove the termination sequence "@@" 
  char *terminatorLocation = strstr(responseBuffer, "@@");
  if (terminatorLocation != NULL) {
    *terminatorLocation = '\0';
  }

  printf("%s", responseBuffer);

  // Free dynamically allocated memory
  free(buffer);
  free(responseBuffer);
  close(socketFD);
  free(plain_txt);
  free(key);

  return 0;
}
