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
  return ((x >= 'A' && x <= 'Z') || x == ' ' || x == '\n');
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

     
  //validation for characters and newline
  for (int i = 0; i < *length; i++){
    if (!validChar(buffer[i])){
      fprintf(stderr, "enc_client error: input contains bad characters\n");
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
      return -1;

     }
    totalSent += byte_last;

  }
  return totalSent;

}

int main(int argc, char *argv[]) {
  int socketFD, portNumber, charsRead; 
  struct sockaddr_in serverAddress;
  char buffer[1001];
  int ciphertext_len, key_len;
  char *cipher_txt = readFile(argv[1], &ciphertext_len);
  char *key = readFile(argv[2], &key_len);
  // Check usage & args
  if (argc < 2) { 
    fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); 
    exit(1); 
  }

  if(key_len < ciphertext_len){

    fprintf(stderr, " Error: key '%s' is too short\n", argv[4]);
    free(cipher_txt);
    free(key);
    exit(1);

  }
  
  if(key_len > ciphertext_len){

    key[key_len] = '\0';

  }
  setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");
  

  // Create a socket
  socketFD = socket(AF_INET, SOCK_STREAM, 0); 
  if (socketFD < 0){
    error("CLIENT: ERROR opening socket");
  }
   
  // Connect to server
  if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
    error("CLIENT: ERROR connecting");
  }

 const char *identifier = "DEC_CLIENT\n";
  if (send_all(socketFD, identifier, strlen(identifier)) < strlen(identifier)) error("CLIENT: ERROR sending identifier");

  if (send_all(socketFD, cipher_txt, ciphertext_len) < ciphertext_len) error("CLIENT: ERROR sending plaintext");

  if (send_all(socketFD, key, key_len) < key_len) error("CLIENT: ERROR sending key");

  memset(buffer, '\0', sizeof(buffer));

  while((charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0)) > 0) {
    printf("%s", buffer);
    
  }

  if (charsRead < 0) error("CLIENT: ERROR reading from socket");

  close(socketFD); 
  free(cipher_txt);
  free(key);
  return 0;
}
