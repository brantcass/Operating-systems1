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

void decryptText(char *ciphertext, char *key, char *plaintext, int length) {
    for (int i = 0; i < length; i++) {
        int ctVal = (ciphertext[i] == ' ') ? 26 : ciphertext[i] - 'A';  //convert ciphertext char to int
        int keyVal = (key[i] == ' ') ? 26 : key[i] - 'A';  //Convert key char to int

        //decryption
        int ptVal = ctVal - keyVal;
        if (ptVal < 0) {
            ptVal += 27;  //ensure the value is positive before modulo operation
        }
        ptVal = ptVal % 27;  // Modulo 27

        //convert back to character
        plaintext[i] = (ptVal == 26) ? ' ' : 'A' + ptVal;
    }

    plaintext[length] = '\n';
    plaintext[length + 1] = '\0';
    
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

      if (strcmp(identifierBuffer, "DEC_CLIENT\n") != 0)
      {
        close(connectionSocket);
        exit(1);
      }

      //Read plaintext
      char ciphertext[100001];
      memset(ciphertext, '\0', sizeof(ciphertext));
      charsRead = recv(connectionSocket, ciphertext, sizeof(ciphertext) - 1, 0);
      if (charsRead < 0)
        error("ERROR reading from socket");

      //read key
      char key[100001]; //adjust size as needed, should be at least as long as plaintext
      memset(key, '\0', sizeof(key));
      charsRead = recv(connectionSocket, key, sizeof(key) - 1, 0);
      if (charsRead < 0)
        error("ERROR reading from socket");

      //decrypt the plaintext using the key
      char plaintext[100001];
      memset(plaintext,'\0', sizeof(plaintext));
      
      decryptText(ciphertext, key, plaintext, strlen(ciphertext)); //Use strlen(plaintext) as length

      //Send plaintext back to the client
      int charsWritten = send(connectionSocket, plaintext, strlen(plaintext), 0);
      if (charsWritten < 0)
        error("ERROR writing to socket");

      close(connectionSocket); 
    }
  }

  close(listenSocket);
  return 0;
}
