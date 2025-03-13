#include <stdio.h>
#include <unistd.h>    // fork()
#include <stdlib.h>    // exit()
#include <inttypes.h>  // intmax_t
#include <sys/wait.h>  // wait()
#include <string.h>    // strcspn(), strtok
#include <errno.h>     // ECHILD
#include <fcntl.h>     // O_RDONLY, open
#include <stdbool.h>   // bool type, true/false

#define BUFSIZE 1024
#define P_RDEND 0
#define P_WREND 1

// Helper string functions to parse input
char** strSplit(char* str, const char* delim);

void strArrFree(char** arr);

int strArrLen(char** arr);

void executeCommand(char* prog, char** args); // args must be NULL terminated.

int main() {
    // Code copied from shell notes.
    // Handle input
    char buffer[BUFSIZE];
    while (1) {
        printf("$ ");
        // if you don't flush the buffer, it gets printed out in child as well apparently!

        // read in command with maximum size
        if (!fgets(buffer, BUFSIZE, stdin)) {
        // use ctrl-d to end the input
            fprintf(stderr, "no more input\n");
            exit(EXIT_SUCCESS);
        }

        // remove newline
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0) {
            continue;
        }

        // Get arguments
        char** command = strSplit(buffer, " ");
        if (command == NULL) continue;

        char* program = command[0];
        // Prints the args if we need to.
        /*
        for (int i = 0; args[i] != NULL; i++) {
            fprintf(stderr, "%s ", args[i]);
        }
        */
        
        executeCommand(program, command);
         
        strArrFree(command);
    }
}

void executeCommand(char* prog, char** args) {
    // Fork the program.
    int pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    // Parent, return as the shell needs to keep running
    else if (pid > 0) {
        // Wait til the child preocess ends
        wait(NULL);
        return;
    }

    // Child
    // Check for pipes or redirection.
    for (int i = 1; args[i] != NULL; i++) {
        if (args[i][0] == '>' || args[i][0] == '<') {
            // Redirection
            int std; 
            if (args[i][0] == '>') 
                std = STDOUT_FILENO;
            else if (args[i][0] == '<')
                std = STDIN_FILENO;

            // Open file to redirect to, pick an open mode based on what we're redirecting
            char* targetPath = args[i] + 1;
            int fd;
            if (std == STDOUT_FILENO) fd = open(targetPath, O_RDWR | O_TRUNC | O_CREAT, 0644);
            else fd = open(targetPath, O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }

            // Redirect
            if (dup2(fd, std) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }

            // This is not an argument so we set it to NULL and break the loop
            args[i] = NULL;
        }

        else if (args[i][0] == '|') {
            // Create a pipe.
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            // Fork for Writer
            int pidWrite = fork();
            if (pidWrite == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            // Writer (Child 1)
            if (pidWrite == 0) {
                //fprintf(stderr, "[in writer] ");
                // Reader reads, close read end
                if (close(pipefd[P_RDEND]) == -1) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Redirect stdout to pipe(write end)
                if (dup2(pipefd[P_WREND], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                if (close(pipefd[P_WREND]) == -1) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Set propper prog and args. We only want to execute the program before the pipe.
                // Do this by setting args[i] to NULL.
                args[i] = NULL;
                executeCommand(prog, args);
                exit(EXIT_SUCCESS);
            }

            // Fork Reader
            int pidRead = fork();
            if (pidRead == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }            

            // Reader
            if (pidRead == 0) {
                //fprintf(stderr, "[in reader] ");
                // Writer writes, close write end.
                if (close(pipefd[P_WREND]) == -1) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Redirect stdin to pipe(read end)
                if (dup2(pipefd[P_RDEND], STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd[P_RDEND]) == -1) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Execute the next command recursively to support multiple pipes.
                executeCommand(args[i+1], &args[i+1]);
                exit(EXIT_SUCCESS);
            }

            // In parent;
            if (pidRead > 0) {
                // Close both pipe ends
                if (close(pipefd[P_RDEND]) == -1) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
                if (close(pipefd[P_WREND]) == -1) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Wait for both children
                wait(NULL);
                wait(NULL);
                return;
            }
        }
    }

    // Execute the command if we didn't pipe.
    execvp(prog, args);
    perror("execvp");
    exit(EXIT_FAILURE);

}

// Return NULL if 2 delimeters exist in a row.
// NULL = failed
char** strSplit(char* str, const char* delim) {
    // Return if given empty string
    if (!strcmp(str, "")) return NULL;
    
    // Count tokens in the string
    // token = 1 + amount of times delimeter is in the string
    int tokenCount = 1;
    bool lastWasDelim = false;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == delim[0]) {
            if (lastWasDelim) {
                return NULL;
            }
            else {
                tokenCount++;
                lastWasDelim = true;
            }
        }
        else {
            lastWasDelim = false;
        }
    }

    // Add space for the NULL terminator.
    int arrLen = tokenCount + 1;

    // Allocate memory
    char** result = (char**)malloc(sizeof(char*) * arrLen);

    // Split the string into tokens and put them into the array
    char* nextToken;
    nextToken = strtok(str, delim);
    result[0] = (char*)malloc(sizeof(char) * (strlen(nextToken)+1));
    strcpy(result[0], nextToken);
    for (int i = 1; i < tokenCount; i++) {
        nextToken = strtok(NULL, delim);
        if (nextToken == NULL) continue;
        result[i] = (char*)malloc(sizeof(char) * (strlen(nextToken)+1));
        strcpy(result[i], nextToken);
    }
    
    // Add NULL terminator
    result[arrLen-1] = NULL;
    return result;
}

void strArrFree(char** arr) {
    if (arr == NULL) return;
    for (int i = 0; arr[i] != NULL; i++) {
        if (arr[i] == NULL) exit(EXIT_FAILURE);
        free(arr[i]);
    }
    if (arr == NULL) exit(EXIT_FAILURE);
    free(arr);
}

int strArrLen(char** arr) {
    if (arr == NULL) return 0;
    int count = 0;
    int i = 0;
    while (arr[i] != NULL) {
        count++;
        i++;
    }
    return count;
}