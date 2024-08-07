#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdint.h>
#include <fcntl.h>


#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
char * expand(char const *word);

int backFlag = 0;
int childStat = 0;
char *foregPid = "0";
char *backgPid = "";
char *inputFile = NULL;
char *outputFile = NULL;
int lastbackgPid = 0;



void sigint_handler(int sig) {}

void sigstp_handler(int sig) {}

int main(int argc, char *argv[])
{
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "too many arguments");
  }

  char *line = NULL;
  size_t n = 0;
  
  //signal(SIGINT, sigint_handler);
  //signal(SIGTSTP, sigstp_handler);

  struct sigaction sa_shell;
  sigfillset(&sa_shell.sa_mask);
  sa_shell.sa_flags = 0;

  //ignore sigint
  sa_shell.sa_handler = SIG_IGN;
  sigaction(SIGINT, &sa_shell, NULL);

  //ignore sigtstp
  sa_shell.sa_handler = SIG_IGN;
  sigaction(SIGTSTP,&sa_shell,NULL);

  for (;;) {
prompt:;
    
    backFlag = 0;
    inputFile = NULL;
    outputFile = NULL;
      
    /* TODO: Manage background processes */
    
   
    int status;
    //use 0 was -1
    pid_t pid = waitpid(-1,&status, WNOHANG|WUNTRACED);
    //get rid of waitpid childPid
      if (pid > 0){
        if (WIFEXITED(status)) {
          fprintf(stderr, "Child process %jd done. Exit status %d\n", (intmax_t)pid, WEXITSTATUS(status));
        }else if (WIFSIGNALED(status)){
          fprintf(stderr, "Child process %jd done. Signaled %d\n", (intmax_t)pid, WTERMSIG(status));
        }else if (WIFSTOPPED(status)){
          //sending sigcont to stopped process
          kill(pid, SIGCONT);
          fprintf(stderr, "Child process %jd stopped. Continuing\n", (intmax_t)pid);
        }
      }

    if (pid == 0) continue;
    if (pid == -1){
      if (errno == ECHILD) {
        errno = 0;       
      }else if (errno == EINTR){
        errno = 0;        
      }else {
        perror("waitpid");
      }
    }


    /* TODO: prompt */
    if (input == stdin) {

      char *PS1 = getenv("PS1");
      if (PS1 == NULL){
        PS1 = "$ ";
      }else{
        char *expandedPS1 = expand(PS1);
        if (expandedPS1 != NULL){
          fprintf(stderr,"%s", expandedPS1);
          free(expandedPS1);
        }else{
          fprintf(stderr,"%s",PS1);
        }
        fflush(stdout);
        
      }     
     }
    //Where im getting error
    ssize_t line_len = getline(&line, &n, input);
    if (line_len == -1){
      if (feof(input)){
        clearerr(input);

        if (input == stdin){
          printf("\n");
          continue;

        }else{
          break;
        }
       }else if (errno == EINTR){

         clearerr(input);
         continue;

       }else{

         perror("Getline failed");
         clearerr(input);
         continue;
       }
    }

    size_t nwords = wordsplit(line);
    
    for (size_t i = 0; i < nwords; ++i) {
      char *exp_word = expand(words[i]);         
      words[i] = exp_word;
    }
    
     
          //Checking if last word is the background
    if (nwords > 0 && strcmp(words[nwords - 1], "&") == 0) {
       backFlag = 1;
       words[nwords - 1] = NULL;
       //nwords--;
     }
    
    
    int isAppendMode = 0;

      
    //Skipping empty commands
    //if (nwords == 0) continue;
 
    //Continue to next part of loop
      // built in commands
      
    if (strcmp(words[0], "exit") == 0){
       int exitStatus = 0;
       if(nwords > 1){
         exitStatus = atoi(words[1]);
       }else{
         exitStatus = childStat;
       }
       exit(exitStatus);
       //continue;

      }else if (strcmp(words[0], "cd") == 0){
         char *dir = nwords > 1 ? words[1] :getenv("HOME");
         if(chdir(dir) != 0) perror("chdir");
         continue;
         }
    
       
       
      if(nwords < MAX_WORDS){
        words[nwords] = NULL;

      }else{
        words[MAX_WORDS - 1] = NULL;
      }

      //Non Built in commands
      pid = fork();      
      if ( pid == 0){

        struct sigaction sa_child;
        sigfillset(&sa_child.sa_mask);
        sa_child.sa_flags = 0;

        sa_child.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sa_child, NULL);

        /*struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        sa.sa_handler = backFlag ? SIG_IGN : SIG_DFL;
        sigaction(SIGINT,&sa,NULL);
          
        //resetting signals
        sa.sa_handler = SIG_DFL;
        sigaction(SIGTSTP,&sa, NULL);*/

        for (size_t i = 0; i < nwords; i++){
          //For redirection
          //add some errors for this part put under else if pid > 0 for child process
          if (strcmp(words[i], ">") == 0 || strcmp(words[i], ">>") == 0 || strcmp(words[i],"<") == 0){
            char *filename = strdup(words[i+1]);                          
                if(!filename){
                  perror("Error duplicating filename");
                }         
                        
                if(strcmp(words[i], ">") == 0){
                  outputFile = filename;
                  isAppendMode = 0;
                }else if(strcmp(words[i], ">>") == 0){
                  outputFile = filename;
                  isAppendMode = 1;

                }else if(strcmp(words[i], "<") == 0){
                  inputFile = filename;
                }
                memmove(&words[i], &words[i + 2], (nwords - i - 2) * sizeof(char*));
                nwords -= 2;
                i--;
                words[nwords] = NULL;
              }
          

        if (inputFile != NULL) {

          int inputFd = open(inputFile, O_RDONLY);
          if (inputFd < 0) {
            perror("failed to open input file");
          }
          if (dup2(inputFd, STDIN_FILENO) == -1){
            perror("failed to redirect stdin");
          }
          close(inputFd);
          }
        if (outputFile != NULL) {
          //Checking for append
          int flags = O_WRONLY | O_CREAT | (isAppendMode ? O_APPEND : O_TRUNC);
          int outputFd = open(outputFile, flags, 0777);
                  
          if(outputFd == -1){
            perror("Failed to open output file");
          }
          if (dup2(outputFd, STDOUT_FILENO) == -1){
            perror("Failed to redirect stdout");
          }
          close(outputFd);
       }
        }
     
          //Execute cmd
          /*for(size_t i = 0; words[i] != NULL; ++i){
            printf("arg[%zu]: %s\n",i,words[i]);
          }*/
          execvp(words[0], words);
          perror("Execvp failed");
     
          
      }else if (pid > 0){
          //Using waitpid to wait for child to finish foreground
          if (backFlag) {

            lastbackgPid = pid;
                                         
          }else{

            pid_t wpid = waitpid(pid, &status, WUNTRACED); 
            if (wpid == -1) {
              perror("Wait pid");            
            }else{
              if(WIFEXITED(status)){
                childStat = WEXITSTATUS(status);
              }else if (WIFSIGNALED(status)) {
                childStat =WTERMSIG(status) + 128;
              }
            }            
          }
        }
        
        
  }


}

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}

/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char const *start, *end;
  char c = param_scan(pos, &start, &end);
  char *s = NULL;
  build_str(NULL, NULL);
  build_str(pos, start);
  while (c) {
    
    if (c == '!') {
      asprintf(&s, "%jd", (intmax_t)lastbackgPid);
      build_str(s, s + strlen(s)); 

    }else if (c == '$'){
      //replace with shell id
      asprintf(&s, "%jd", (intmax_t)getpid());
      build_str(s, s + strlen(s));

    }else if (c == '?'){
      //Replacing $? with exit stat of last child
      asprintf(&s,"%jd", (intmax_t)childStat);
      build_str(s, s + strlen(s));

    }else if (c == '{') {
      char varName[MAX_WORDS];
      strncpy(varName, start + 2, (size_t)(end - start - 3));
      varName[end - start - 3] = '\0';//ensuring null term
      char* paramValue = getenv(varName);
      if(paramValue){
        build_str(paramValue, paramValue + strlen(paramValue));  
      }

          
    }
 
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);

  }

  return build_str(start, NULL);
}


