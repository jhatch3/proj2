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

// Allows process to run for 1 sections
unsigned int timer_time = 1;
sig_atomic_t current_index = 0;
int total_processes = 0;


/* - - - - - - - - - - - HELPER FUNCTION FOR ALARM - - - - - - - - - - - */
void alarm_handler(int sig)
{
    // Stop current process
    if (kill(pid_array[current_index], SIGSTOP) == 0) {
        //printf("Stopping PID %d\n", pid_array[current_index]);
    }

    // Advance 
    current_index = (current_index + 1) % total_processes;

    // Resume 
    if (kill(pid_array[current_index], SIGCONT) == 0) {
        //printf("Resuming PID %d\n", pid_array[current_index]);
    }

    // Set next alarm
    alarm(timer_time);
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
            // printf("Child PID %d: waiting for SIGUSR1...\n", getpid());
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

    // Set total number of processes
    total_processes = line_number;

    // Send SIGUSR1 to all children to trigger exec
    // printf("- - - SENDING SIGUSR1 - - -\n");
    for (int i = 0; i < line_number; i++)
    {
        kill(pid_array[i], SIGUSR1);
    }

    // Send SIGSTOP to pause all children
    // printf("- - - SENDING SIGSTOP - - -\n");
    for (int i = 0; i < line_number; i++)
    {
        kill(pid_array[i], SIGSTOP);
    }

    // for alarm function
    struct sigaction sa;
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    // Resume the first child and start the timer
    kill(pid_array[0], SIGCONT);
    alarm(timer_time);

    // Wait for all children to finish
    for (int i = 0; i < line_number; i++)
    {
        waitpid(pid_array[i], NULL, 0);
    }

	// Reset
    alarm(0);

    exit(0);
}
