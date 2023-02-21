#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024
#define TOKEN_DELIMITERS " \t\r\n\a"

char error_message[30] = "An error has occurred\n";

void execute_command(char **tokens, int num_tokens) {
    int input_fd = 0, output_fd = 0;
    int pipefd[2], fd_in = 0, fd_out = 0;
    int is_piped = 0;

    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tokens[i], ">") == 0) {
            output_fd = open(tokens[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd < 0) {
                perror("Error opening output file");
                return;
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
            tokens[i] = NULL;
            tokens[i+1] = NULL;
        } else if (strcmp(tokens[i], "|") == 0) {
            tokens[i] = NULL;
            is_piped = 1;
            if (pipe(pipefd) < 0) {
                perror("Error creating pipe");
                return;
            }
        }
    }

    int pid = fork();
    if (pid < 0) {
        perror("Error forking process");
        return;
    } else if (pid == 0) {
        if (is_piped) {
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("Error dup2 pipefd[1]");
                return;
            }
            close(pipefd[0]);
            close(pipefd[1]);
        }
        execvp(tokens[0], tokens);
        perror("Error executing command");
        exit(EXIT_FAILURE);
    } else {
        waitpid(pid, NULL, 0);
        if (is_piped) {
            pid = fork();
            if (pid < 0) {
                perror("Error forking process");
                return;
            } else if (pid == 0) {
                if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                    perror("Error dup2 pipefd[0]");
                    return;
                }
                close(pipefd[0]);
                close(pipefd[1]);
                execvp(tokens[i+1], tokens+i+1);
                perror("Error executing command");
                exit(EXIT_FAILURE);
            } else {
                waitpid(pid, NULL, 0);
                close(pipefd[0]);
                close(pipefd[1]);
            }
        }
    }
}


int lexer(char *line, char ***args, int *num_args) {
    *num_args = 0;
    // count number of args
    char *l = strdup(line);
    if (l == NULL) {
        return -1;
    }
    char *token = strtok(l, " \t\n");
    while (token != NULL) {
        (*num_args)++;
        token = strtok(NULL, " \t\n");
    }
    free(l);
    // split line into args
    *args = malloc(sizeof(char **) * *num_args + 1);
    *num_args = 0;
    token = strtok(line, " \t\n");
    while (token != NULL) {
        char *token_copy = strdup(token);
        if (token_copy == NULL) {
            return -1;
        }
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " \t\n");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(0);
    }

    char **args;
    int num;
    int status;
    int loop = 1;

    while (1) {
        if (argc == 1) {
            printf("smash> ");
            fflush(stdout);//clear and print immediately
        }

        char *line = NULL;
        size_t buffer = 0;
        //read line
        if (getline(&line, &buffer, stdin) == -1) {
            if (feof(stdin)) {
                exit(EXIT_SUCCESS);  // received an EOF
            }
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(0);
        }
        //skip empty
        if (strcmp(line, "\n") == 0) {
            continue;
        }

        int valid = lexer(line, &args, &num);

        if (valid == -1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        args[num] = NULL; //mark the end

        if (strcmp(args[0], "exit") == 0) {
            if (num != 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            exit(0);
        } else if (strcmp(args[0], "cd") == 0) {
            if (num != 2) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            int resultCd = chdir(args[1]);
            if (resultCd != 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            if (num != 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            char *dir = getcwd(NULL, 0);
            if (dir == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            printf("%s\n", dir);
            free(dir);
        } else if (strcmp(args[0], "loop") == 0) {
            loop = atoi(args[1]);
        }else {
            for (int i = 0; i < loop; i++){
                execute_command(args, num);
                
            }
        }
    }
}