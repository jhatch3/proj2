#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>

#define MAXPROCESS 500
#define MAXARGS 100

pid_t pid_array[MAXPROCESS];         
int exited[MAXPROCESS] = {0};        // Track if process has exited

// Allows process to run for 1 second per round
unsigned int timer_time = 1;
sig_atomic_t current_index = 0;
int total_processes = 0;


/* - - - - - - - - - - - HELPER FUNCTION FOR ALARM - - - - - - - - - - - */
void alarm_handler(int sig)
{
    // Stop current process if still running
    if (!exited[current_index]) {
        kill(pid_array[current_index], SIGSTOP);
    }

    // Advance to next process that hasn't exited
    int start = current_index;
    do {
        current_index = (current_index + 1) % total_processes;
    } while (exited[current_index] && current_index != start);

    // Resume next process
    if (!exited[current_index]) {
        kill(pid_array[current_index], SIGCONT);
        alarm(timer_time); // Schedule next alarm
    }
}



/* - - - - - - - - - - - MAIN - - - - - - - - - - - */
int main(int argc, char* argv[]) 
{
    // Validate Args
    if (argc != 2) 
    {
        write(STDERR_FILENO, "Usage: ./part1 <input.txt>\n", 28);
        exit(1);
    }

    sigset_t set;

    // Block SIGUSR1 
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) 
    {
        perror("sigprocmask");
        exit(1);
    }

    char* lines[MAXPROCESS];
    size_t len = 0;
    ssize_t read;
    int line_number = 0;
    char* line = NULL;

    // Open input file
    FILE* in_fp = fopen(argv[1], "r");
    if (!in_fp) 
    {
        write(STDERR_FILENO, "Cannot open input file\n", 24);
        exit(-1);
    }

    // Read all lines and store copies
    while ((read = getline(&line, &len, in_fp)) != -1 && line_number < MAXPROCESS) 
    {
        lines[line_number++] = strdup(line);
    }
    free(line);
    fclose(in_fp);

    // Fork child processes from each line of input
    for (int i = 0; i < line_number; i++) 
    {
        // Tokenize line into command and args
        char* argbuff[MAXARGS];
        int j = 0;

        char* token = strtok(lines[i], " \n");
        while (token != NULL && j < MAXARGS - 1) 
        {
            argbuff[j++] = strdup(token);
            token = strtok(NULL, " \n");
        }
        argbuff[j] = NULL;

        // Fork process
        pid_array[i] = fork();
        
        // Error check 
        if (pid_array[i] < 0) 
        {
            write(STDERR_FILENO, "Fork failed\n", 12);
            exit(-1);
        }

        // Child process
        if (pid_array[i] == 0) 
        {   
            int sig;

            // Wait for SIGUSR1 signal
            sigwait(&set, &sig);

            // Execute command    
            if (execvp(argbuff[0], argbuff) == -1) 
            {
                write(STDERR_FILENO,"Execvp failed\n", 15);
                exit(-1);
            }
        }

        // Free tokenized args in parent
        for (int k = 0; k < j; k++) 
        {
            free(argbuff[k]);
        }

        free(lines[i]);
    }

    total_processes = line_number;

    // Send SIGUSR1 to exec
    for (int i = 0; i < line_number; i++)
    {
        kill(pid_array[i], SIGUSR1);
    }

    // Send SIGSTOP to pause 
    for (int i = 0; i < line_number; i++)
    {
        kill(pid_array[i], SIGSTOP);
    }

    // Setup alarm handler
    struct sigaction sa;
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    // Resume the first child 
    kill(pid_array[0], SIGCONT);
    alarm(timer_time);

    // Wait for all children to finish 
    int processes_remaining = line_number;

    while (processes_remaining > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid > 0) {
            for (int i = 0; i < total_processes; i++) {
                if (pid_array[i] == pid) {
                    exited[i] = 1;
                    processes_remaining--;
                    break;
                }
            }

            // Check if exited normally
            if (WIFEXITED(status)) {
                //int code = WEXITSTATUS(status);
                //printf("Process %d exited with status %d\n", pid, code);
            } else {
                //printf("Process %d terminated abnormally\n", pid);
            }
        }
    }

    // Stop timer after all children exit
    alarm(0);
    exit(0);
}
