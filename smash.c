#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

struct command {
    char **argv;
};

char error_message[30] = "An error has occurred\n";

void execute_command(char **tokens, int num_tokens) {
    int input_fd = 0, output_fd = 0;
    int pipefd[2], fd_in = 0, fd_out = 0;
    int is_piped = 0;
    int is_red = 0;
    int pipe_pos = 0;
    int red_pos = -1;
    int semi_pos = -1;


    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tokens[i], ">") == 0) {
            if (i == num_tokens - 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }
            red_pos = i;
            is_red = 1;
        } else if (strcmp(tokens[i], "|") == 0) {
            tokens[i] = NULL;
            is_piped = 1;
            pipe_pos = i;
            if (pipe(pipefd) < 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }
        }
    }

    int pid = fork();
    if (pid < 0) {
        perror("Error forking process");
        return;
    } else if (pid == 0) {
        //child process
        if (is_red){
            output_fd = open(tokens[red_pos+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd < 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }
            //printf("file opened");
            dup2(output_fd, STDOUT_FILENO);
            //dup output to file descriptor
            close(output_fd);
            //fd will never be used
            tokens[red_pos] = NULL;
            execvp(tokens[0], tokens);
            //execute [0], [1], output to file
        }
        if (is_piped) {
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                //first child, replace stdout file descriptor to fd1
                perror("Error dup2 pipefd[1]");
                return;
            }
            close(pipefd[0]);//close input
            close(pipefd[1]);//close output
        }
        if (access(tokens[0], X_OK) == 0){
            execvp(tokens[0], tokens);
        }else {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        //execvp(tokens[0], tokens);
        perror("Error executing command");
        exit(EXIT_FAILURE);
    } else {
        int pid2 = -1;
        //main stream
        if (is_piped) {
            pid2 = fork();
            if (pid2 < 0) {
                perror("Error forking process");
                return;
            } else if (pid2 == 0) {
                //2nd child
                //execute second command
                if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                    perror("Error dup2 pipefd[0]");
                    return;
                }
                close(pipefd[0]);//close input
                close(pipefd[1]);//close output
                char *pipe_com[3];
                pipe_com[0] = strdup(tokens[pipe_pos + 1]);
                pipe_com[1] = strdup(tokens[pipe_pos + 2]);
                pipe_com[2] = NULL;
                execvp(pipe_com[0], pipe_com);
                //execvp(tokens[semi_pos+1], tokens+semi_pos+1);
                perror("Error executing command");
                exit(EXIT_FAILURE);
            } else {
                close(pipefd[0]);
                close(pipefd[1]);
                waitpid(pid, NULL, 0);
                waitpid(pid2, NULL, 0);
            }
        }
        waitpid(pid, NULL, 0);
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
    *args = malloc(sizeof(char *) * (*num_args + 1));
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



int main(int argc, char *argv[]) {

    if (argc != 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(0);
    }

    char **args;
    int num;
    int status;

    while (1) {
        int loop = 1;
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

        if (strcmp(args[0], "loop") == 0) {
            loop = atoi(args[1]);
        }

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
            for (int f = 0; f < loop; f++) {
                int resultCd = chdir(args[1]);
                if (resultCd != 0) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            if (num != 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            for (int f = 0; f < loop; f++) {
                char *dir = getcwd(NULL, 0);
                if (dir == NULL) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }
                printf("%s\n", dir);
                free(dir);
            }
        }  else {

            //check for >, ; and |

            int command_num = 1;
            int red_num[10];
            int pipe_num[10] = {0};
            int semi_pos[10] = {0};
            int red_pos[10] = {0};
            int pipe_pos[10] = {0};
            int pipefd[2];
            int semi_num = 0;
            int pipfalg;
            int redflag;

            for (int i = 0; i < num; i++) {
                if (strcmp(args[i], ">") == 0) {
                    red_num[command_num]++;
                    red_pos[command_num] = i;
                }
                if (strcmp(args[i], "|") == 0) {
                    pipe_pos[command_num] = i;
                    pipe_num[command_num]++;
                }
                if (strcmp(args[i], ";") == 0) {
                    semi_pos[semi_num] = i;
                    semi_num++;
                    command_num++;
                }
            }

            //if (strcmp(args[0], "loop")) {
            //                if (access(args[0], X_OK) != 0) {
            //                    write(STDERR_FILENO, error_message, strlen(error_message));
            //                    continue;
            //                }
            //                if (semi_num >= 1) {
            //                    for (int i = 0; i < semi_num; i++) {
            //                        if (access(args[semi_pos[i] + 1], X_OK) != 0) {
            //                            write(STDERR_FILENO, error_message, strlen(error_message));
            //                            continue;
            //                        }
            //                    }
            //                }
            //            }



            //locate path

            char **semi_command;
            int semi_index;
            int count = 0;
            int counter = 0;
            if (semi_num >= 1) {
                //semi
                for (int i = 0; i < semi_num; i++) {
                    semi_index = semi_pos[i];
                    counter = (semi_index + 1) - count;
                    semi_command = malloc(sizeof(char *) * (counter));
                    //if malloc failed
                    if (semi_command == NULL) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        continue;
                    }
                    int temp = count;
                    if (loop == 1) {
                        for (int j = temp; j < semi_index; j++) {
                            semi_command[j - temp] = strdup(args[j]);
                        }
                        //make ';' to NULL
                    }else {
                        for (int j = (temp + 2); j < semi_index; j++) {
                            semi_command[j - 2 - temp] = strdup(args[j]);
                        }
                    }
                    semi_command[semi_index] = NULL;
                    count = semi_pos[i] + 1;
                    if (loop > 1) {
                        for (int x = 0; x < loop; x++) {
                            execute_command(semi_command, counter - 1);
                        }
                    } else {
                        execute_command(semi_command, counter - 1);
                    }
                    free(semi_command);
                }

                counter = num - count + 1;
                semi_command = malloc(sizeof(char *) * (counter));
                if (loop == 1) {
                    for (int j = count; j < num; j++) {
                        semi_command[j - count] = strdup(args[j]);
                    }
                    //make ';' to NULL
                }else {
                    for (int j = (count + 2); j < semi_index; j++) {
                        semi_command[j - 2 - count] = strdup(args[j]);
                    }
                }
                if (loop > 1) {
                    for (int x = 0; x < loop; x++) {
                        execute_command(semi_command, counter - 1);
                    }
                } else {
                    execute_command(semi_command, counter - 1);
                }
                free(semi_command);
            } else {
                // only loop
                semi_command = malloc(sizeof(char *) * (num + 1));
                if (loop > 1) {
                    for (int j = 2; j < num; j++) {
                        semi_command[j - 2] = strdup(args[j]);
                    }
                    int temp_num = num - 2;
                    for (int i = 0; i < loop; i++){
                        execute_command(semi_command, temp_num);
                    }
                    free(semi_command);
                }else {
                    // no loop, no semi
                    execute_command(args,num);
                }
            }

        }

    }



    //execute_command(args, num);

    return 0;
}
