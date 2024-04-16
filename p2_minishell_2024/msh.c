//P2-SSOO-23/24

//  MSH main file

//#include "parser.h"
#include <stddef.h>			/* NULL */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMANDS 8


// files in case of redirection
char filev[3][64];

//to store the execvp second parameter
char *argv_execvp[8];


// Initialize or update the "Acc" environment variable
/**
 * Function that either intializes the "Acc" environment variable that is to be used by the
 * mycalc internal function after performing addition operations.
 * @param value the value to increment Acc by
 */
void updateAcc(int value) {
    char *accStr = getenv("Acc");
    int acc = 0;
    if (accStr != NULL) {
        acc = atoi(accStr);
    }
    acc += value;
    char newAcc[20];
    sprintf(newAcc, "%d", acc);
    setenv("Acc", newAcc, 1);
}


/**
 * Function that executes the mycalc internal function by checking for the correct number of
 * arguments, detects errors if there are any, and prints out the appropriate output.
 * @param argv The array of arguments inputted into the command line by the user.
 */
void execute_mycalc(char **argv) {
    // Check for the correct number of arguments
    // Assuming argv is ["mycalc", "operand1", "operator", "operand2", NULL]
    if (argv[1] == NULL || argv[2] == NULL || argv[3] == NULL || argv[4] != NULL) {
        printf("[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return;
    }

    int op1 = atoi(argv[1]);
    int op2 = atoi(argv[3]);
    int result = 0;
    int remainder = 0;
    if (strcmp(argv[2], "add") == 0) {
        result = op1 + op2;
        updateAcc(result);
        char *accStr = getenv("Acc");
        /** Per the lab instructions, we are supposed to write the result to the standard ERROR
         * output not sure why because we are writing when no error occurs: note to grader,
         * we followed the exact instructions in the lab, where it says to write to the
         * standard ERROR output (instead of just the standard output). Not sure if this
         * was a mistake in the lab description, but we followed those instructions instead of
         * using printf() to print the result.
        **/
        // buffer that constructs the string that we want print to the standard error output
        char std_err_output_msg[256];
        snprintf(std_err_output_msg, 256, "[OK] %d + %d = %d; Acc %s\n", op1, op2, result, accStr);
        write(STDERR_FILENO,  std_err_output_msg, strlen( std_err_output_msg));
    } else if (strcmp(argv[2], "mul") == 0) {
        result = op1 * op2;
        char std_err_output_msg[256];
        snprintf(std_err_output_msg, 256, "[OK] %d * %d = %d\n", op1, op2, result);
        write(STDERR_FILENO,  std_err_output_msg, strlen( std_err_output_msg));
    } else if (strcmp(argv[2], "div") == 0) {
        if (op2 == 0) {
            printf("[ERROR] Division by zero\n");
            return;
        }

        result = op1 / op2;
        remainder = op1 % op2;
        char std_err_output_msg[256];
        snprintf(std_err_output_msg, 256, "[OK] %d / %d = %d; Remainder %d\n", op1, op2,
                 result, remainder);
        write(STDERR_FILENO,  std_err_output_msg, strlen( std_err_output_msg));
    } else {
        printf("[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
    }
}

struct command
{
    // Store the number of commands in argvv
    int num_commands;
    // Store the number of arguments of each command
    int *args;
    // Store the commands
    char ***argvv;
    // Store the I/O redirection
    char filev[3][64];
    // Store if the command is executed in background or foreground
    int in_background;
};

/**
 * Variables relevant to storing and keeping track of the 20 most recent command for the
 * myhistory command
 */
int history_size = 20;
struct command * history;
int head = 0;
int tail = 0;
int n_elem = 0;


/**
 * Function that is called by the myhistory command in order to list the 20 most recent commands
 * inputted by the user (excluding any myhistory commands made). Again, the lab description says
 * to list the history by writing to the standard ERROR output instead of just the standard
 * output (printf()).
 */
void list_history() {
    for (int i = 0; i < n_elem; i++) {
        int index = (head - n_elem + i + history_size) % history_size; // Ensure positive index
        char std_err_output_msg[1024];
        sprintf(std_err_output_msg, "%d ", i);
        write(STDERR_FILENO,  std_err_output_msg, strlen(std_err_output_msg));

        // Write the redirection info to the standard error output
        if (strcmp(history[index].filev[0], "0") != 0) {
            sprintf(std_err_output_msg, "< %s ", history[index].filev[0]);
            write(STDERR_FILENO,  std_err_output_msg, strlen(std_err_output_msg));
        }
        for (int j = 0; j < history[index].num_commands; j++) {
            if (j != 0) {
                sprintf(std_err_output_msg, "| ");
                write(STDERR_FILENO,  std_err_output_msg, strlen(std_err_output_msg));
            }
            for (int p = 0; p < history[index].args[j]; p++) {
                sprintf(std_err_output_msg, "%s ", history[index].argvv[j][p]);
                write(STDERR_FILENO,  std_err_output_msg, strlen(std_err_output_msg));
            }

        }
        if (strcmp(history[index].filev[1], "0") != 0) {
            sprintf(std_err_output_msg, "> %s ", history[index].filev[1]); // Output redirection
            write(STDERR_FILENO,  std_err_output_msg, strlen(std_err_output_msg));
        }
        if (strcmp(history[index].filev[2], "0") != 0) {
            sprintf(std_err_output_msg, "2> %s ", history[index].filev[2]); // Error redirection
            write(STDERR_FILENO,  std_err_output_msg, strlen(std_err_output_msg));
        }
        sprintf(std_err_output_msg, "\n");
        write(STDERR_FILENO,  std_err_output_msg, strlen(std_err_output_msg));
    }
}


/**
 * General purpose function that is used to run a list of commands, taking into account the
 * number of commands inputted by the user, if it should be run in the background, and the
 * respective arguments for each
 * @param command_counter The number of commands inputted by the user
 * @param argvv The array of commands inputted by the user
 * @param in_background Dictates whether the command should be run in the background or not
 * @param filev Represents the input/output redirection, if any
 */
void run_command(int command_counter, char*** argvv, int in_background, char filev[3][64]){
    if (command_counter > MAX_COMMANDS) {
        printf("Error: Maximum number of commands is %d \n", MAX_COMMANDS);
    } else if (command_counter == 1 && strcmp(argvv[0][0], "mycalc") == 0) {
        execute_mycalc(argvv[0]);
    } else if (command_counter == 1) {
        pid_t pid = fork();
        if (pid == 0) {

            // Input Redirection
            if (strcmp(filev[0], "0") != 0) {
                printf("changing input to other file for 1 command\n");
                int fd_in = open(filev[0], O_RDONLY);
                if (fd_in == -1) { perror("open"); exit(EXIT_FAILURE); }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            // Output Redirection
            if (strcmp(filev[1], "0") != 0) {
                printf("changing output to other file for 1 command\n");
                int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out == -1) { perror("open"); exit(EXIT_FAILURE); }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            // Error Redirection
            if (strcmp(filev[2], "0") != 0) {
                int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_err == -1) { perror("open"); exit(EXIT_FAILURE); }
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
            }
            if (execvp(argvv[0][0], argvv[0]) < 0) {
                perror("Execvp failed");
                exit(-1);
            }
        } else if (pid > 0) {
            if (!in_background) {
                int status;
                waitpid(pid, &status, 0);
            } else {
                printf("[%d]/n", pid);
            }
        } else {
            perror("Fork Error");
            exit(-2);
        }
    } else if (command_counter > 1) {
        int pipefd[2 * (command_counter - 1)]; // Array to store pipe file descriptors

        // Setup pipes
        for (int i = 0; i < command_counter - 1; i++) {
            if (pipe(pipefd + i * 2) < 0) {
                perror("Piping Error");
                exit(EXIT_FAILURE);
            }
        }

        // Execute each command in the chain
        for (int i = 0; i < command_counter; i++) {
            pid_t pid = fork();

            if (pid == 0) { // Child process
                // Redirect output for all but the last command
                if (i < command_counter - 1) {
                    if (dup2(pipefd[i * 2 + 1], STDOUT_FILENO) < 0) {
                        perror("Duplication Error");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    // Last command: handle output redirection
                    if (strcmp(filev[1], "0") != 0) {
                        int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        dup2(fd_out, STDOUT_FILENO);
                        close(fd_out);
                    }
                    // Handle error redirection for last command
                    if (strcmp(filev[2], "0") != 0) {
                        int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        dup2(fd_err, STDERR_FILENO);
                        close(fd_err);
                    }
                }

                // Redirect input for all but the first command
                if (i > 0) {
                    if (dup2(pipefd[(i - 1) * 2], STDIN_FILENO) < 0) {
                        perror("Duplication Error");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    // First command: handle input redirection
                    if (strcmp(filev[0], "0") != 0) {
                        int fd_in = open(filev[0], O_RDONLY);
                        dup2(fd_in, STDIN_FILENO);
                        close(fd_in);
                    }
                }

                // Close all pipe file descriptors in the child
                for (int j = 0; j < 2 * (command_counter - 1); j++) {
                    close(pipefd[j]);
                }

                // Execute the command
                if (execvp(argvv[i][0], argvv[i]) < 0) {
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            } else if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
        }

        // Close all pipe file descriptors in the parent
        for (int i = 0; i < 2 * (command_counter - 1); i++) {
            close(pipefd[i]);
        }

        // Wait for all child processes to finish if not in background
        if (!in_background) {
            for (int i = 0; i < command_counter; i++) {
                wait(NULL);
            }
        } else {
            printf("Background command executed.\n");
        }
    }
}

/**
 * Helper function that can be called from the myhistory command if a user wants to run a specific
 * past command from a user's history of their 20 most recent commands.
 * @param hist_index The index of the last command that the user would like to run.
 */
void execute_from_history(int hist_index) {
    // Ensure positive index, % by history size to ensure that it wraps around
    int index = (head - n_elem + hist_index + history_size) % history_size;
    struct command *cmd = &history[index];


    // Prepare for execution
    char ***argvv = cmd->argvv;
    int in_background = cmd->in_background;
    int command_counter = cmd->num_commands; // Adding 1 as stored num_commands is one less

    // No need for getCompleteCommand as run_command will handle execution
    // Execute command
    run_command(command_counter, argvv, in_background, cmd->filev);
}

// Given default code
void free_command(struct command *cmd)
{
    if((*cmd).argvv != NULL)
    {
        char **argv;
        for (; (*cmd).argvv && *(*cmd).argvv; (*cmd).argvv++)
        {
            for (argv = *(*cmd).argvv; argv && *argv; argv++)
            {
                if(*argv){
                    free(*argv);
                    *argv = NULL;
                }
            }
        }
    }
    free((*cmd).args);
}

void siginthandler(int param)
{
    // Make sure to free the stored commands to free resources after done running MSH
    for (int i = 0; i < n_elem; i++) {
        free_command(&history[i]);
    }
    printf("****  Exiting MSH **** \n");
    //signal(SIGINT, siginthandler);
    exit(0);
}

// Given default code
void store_command(char ***argvv, char filev[3][64], int in_background, struct command* cmd)
{
    int num_commands = 0;
    while(argvv[num_commands] != NULL){
        num_commands++;
    }

    for(int f=0;f < 3; f++)
    {
        if(strcmp(filev[f], "0") != 0)
        {
            strcpy((*cmd).filev[f], filev[f]);
        }
        else{
            strcpy((*cmd).filev[f], "0");
        }
    }

    (*cmd).in_background = in_background;
    (*cmd).num_commands = num_commands-1;
    (*cmd).argvv = (char ***) calloc((num_commands) ,sizeof(char **));
    (*cmd).args = (int*) calloc(num_commands , sizeof(int));

    for( int i = 0; i < num_commands; i++)
    {
        int args= 0;
        while( argvv[i][args] != NULL ){
            args++;
        }
        (*cmd).args[i] = args;
        (*cmd).argvv[i] = (char **) calloc((args+1) ,sizeof(char *));
        int j;
        for (j=0; j<args; j++)
        {
            (*cmd).argvv[i][j] = (char *)calloc(strlen(argvv[i][j]),sizeof(char));
            strcpy((*cmd).argvv[i][j], argvv[i][j] );
        }
    }
}


/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv The array of commands inputted by the user
 * @param num_command The number of commands inputted by the user.
 */
void getCompleteCommand(char*** argvv, int num_command) {
    //reset first
    for(int j = 0; j < 8; j++)
        argv_execvp[j] = NULL;

    int i = 0;
    for ( i = 0; argvv[num_command][i] != NULL; i++)
        argv_execvp[i] = argvv[num_command][i];
}

/**
 * Main shell  Loop
 */
int main(int argc, char* argv[]) {
    /**** Do not delete this code.****/
    int end = 0;
    int executed_cmd_lines = -1;
    char *cmd_line = NULL;
    char *cmd_lines[10];

    if (!isatty(STDIN_FILENO)) {
        cmd_line = (char *) malloc(100);
        while (scanf(" %[^\n]", cmd_line) != EOF) {
            if (strlen(cmd_line) <= 0) return 0;
            cmd_lines[end] = (char *) malloc(strlen(cmd_line) + 1);
            strcpy(cmd_lines[end], cmd_line);
            end++;
            fflush(stdin);
            fflush(stdout);
        }
    }

    /*********************************/

    char ***argvv = NULL;
    int num_commands;

    history = (struct command *) malloc(history_size * sizeof(struct command));
    int run_history = 0;

    while (1) {
        int status = 0;
        int command_counter = 0;
        int in_background = 0;
        signal(SIGINT, siginthandler);

        if (run_history) {
            run_history = 0;
        } else {
            // Prompt
            write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));

            // Get command
            //********** DO NOT MODIFY THIS PART. IT DISTINGUISH BETWEEN NORMAL/CORRECTION MODE***************
            executed_cmd_lines++;
            if (end != 0 && executed_cmd_lines < end) {
                command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]);
            } else if (end != 0 && executed_cmd_lines == end)
                return 0;
            else
                command_counter = read_command(&argvv, filev, &in_background); //NORMAL MODE
        }
        //************************************************************************************************


        /************************ STUDENTS CODE ********************************/

        if (command_counter > 0 && strcmp(argvv[0][0], "myhistory") != 0) { // Avoid storing "myhistory" command itself
            store_command(argvv, filev, in_background, &history[head]);
            head = (head + 1) % history_size; // Move head to next position circularly
            if (n_elem < history_size) n_elem++; // Increment number of elements until history is full
        }

        if (strcmp(argvv[0][0], "myhistory") == 0) {
            if (argvv[0][1] == NULL) {
                list_history();
            } else {
                int hist_index = atoi(argvv[0][1]);
                if (hist_index >= 0 && hist_index < n_elem) {
                    char print_msg[20];
                    // Constructs the message to print to the standard error output
                    snprintf(print_msg, 20, "Running command %d\n", hist_index);
                    write(STDERR_FILENO, print_msg, strlen(print_msg));
                    execute_from_history(hist_index);
                } else {
                    printf("ERROR: Command not found\n");
                }
            }
        }
        else {
            run_command(command_counter, argvv, in_background, filev);
        }
    }
    return 0;
}