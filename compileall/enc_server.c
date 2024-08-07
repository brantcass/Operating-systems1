#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>


#define BUFFER_SIZE 100000
// Error function used for reporting issues
void error(const char *msg)
{
  perror(msg);
  exit(1);
}

void cleanUpChildProccesses(int signal)
{

  while (waitpid(-1, NULL, WNOHANG) > 0);
    
}

void encryptText(char *plaintext, char *key, char *ciphertext, int textLength, int keyLength) {
    for (int i = 0; i < textLength; i++) {
        int ptVal = (plaintext[i] == ' ') ? 26 : plaintext[i] - 'A';
        int keyVal = (key[i] == ' ') ? 26 : key[i] - 'A';
        int ctVal = (ptVal + keyVal) % 27;
        ciphertext[i] = (ctVal == 26) ? ' ' : 'A' + ctVal;
    }
    ciphertext[textLength] = '\0';  // Null-terminate the ciphertext
}

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in *address,
                        int portNumber)
{

  // Clear out the address struct
  memset((char *)address, '\0', sizeof(*address));

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);
  // Allow a client at any address to connect to this server
  address->sin_addr.s_addr = INADDR_ANY;
}

int main(int argc, char *argv[])
{
  int connectionSocket;
  //char plaintextChar, keyChar, ciphertextChar;
  ssize_t charsRead, charsWritten;
  socklen_t sizeofClientinfo;
  struct sockaddr_in serverAddress, clientAddress;

  // Check usage & args
  if (argc < 2)
  {
    fprintf(stderr, "USAGE: %s port\n", argv[0]);
    exit(1);
  }

  // Create the socket that will listen for connections
  int listenSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (listenSocket < 0)
  {
    error("ERROR opening socket");
    exit(1);
  }

  setupAddressStruct(&serverAddress, atoi(argv[1]));

  if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    fprintf(stderr, "ERROR on binding");
    exit(1);
  }

  listen(listenSocket, 5);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = cleanUpChildProccesses;
  sigaction(SIGCHLD, &sa, NULL);

  while (1)
  {

    // accept the connection request which creates a connection socket
    //  need to always be accepting

    sizeofClientinfo = sizeof(clientAddress);

    connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeofClientinfo);

    if (connectionSocket < 0)
    {
      error("ERROR on accept");
    }

    // fork a new proces for the client connection
    pid_t pid = fork();
    if (pid < 0)
    {
      error("ERROR on fork");
    }

    // child process
    if (pid == 0)
    {
      //close(listenSocket); // Child doesn't need access to listenSocket

      // To make sure that it is connecting to ENC
      char identifierBuffer[12];
      memset(identifierBuffer, '\0', sizeof(identifierBuffer));
      charsRead = recv(connectionSocket, identifierBuffer, sizeof(identifierBuffer) - 1, 0);
      if (charsRead < 0)
        error("Error reading from socket");

      if (strcmp(identifierBuffer, "ENC_CLIENT\n") != 0)
      {
        close(connectionSocket);
        exit(1);
      }
     
      char plaintext[BUFFER_SIZE];
      memset(plaintext, '\0', sizeof(plaintext));
      //receive plaintext
      charsRead = recv(connectionSocket, plaintext, sizeof(plaintext) - 1, 0);
      if (charsRead < 0) error("ERROR reading from socket");

      char key[BUFFER_SIZE];
      memset(key, '\0', sizeof(key));
      //receive key
      charsRead = recv(connectionSocket, key, sizeof(key) - 1, 0);
      if (charsRead < 0) error("ERROR reading from socket");

      char ciphertext[BUFFER_SIZE];
      memset(ciphertext, '\0', sizeof(ciphertext));
      //encrypt plaintext
      encryptText(plaintext, key, ciphertext, strlen(plaintext), strlen(plaintext));
      //Send ciphertext
      charsWritten = send(connectionSocket, ciphertext, strlen(ciphertext), 0);
      if (charsWritten < 0) error("ERROR writing to socket");
          close(connectionSocket); 
      }
  }

  close(listenSocket);
  return 0;
}
