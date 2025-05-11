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

// Structure to store process information
typedef struct {
    pid_t pid;
    char name[64];
    int vmrss_kb;
    long cpu_time;
    int exited;
    int status;
} ProcessInfo;

ProcessInfo process_table[MAXPROCESS];
int total_processes = 0;
sig_atomic_t current_index = 0;

// Allows process to run for 1 second per round
unsigned int timer_time = 1;

/* - - - - - - - - - - - HELPER FUNCTION FOR LOGGING - - - - - - - - - - - */

void update_process_info(int index) {
    char path[256], buffer[1024];
    FILE *fp;
    pid_t pid = process_table[index].pid;
    
    // Skip if process has already exited
    if (process_table[index].exited) {
        return;
    }
    
    // Check if process is still running
    if (kill(pid, 0) != 0) {
        process_table[index].exited = 1;
        return;
    }

    // Read command name if not already captured
    if (process_table[index].name[0] == '\0') {
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        fp = fopen(path, "r");
        if (fp) {
            if (fgets(process_table[index].name, sizeof(process_table[index].name), fp)) {
                process_table[index].name[strcspn(process_table[index].name, "\n")] = 0; // remove newline
            }
            fclose(fp);
        }
    }

    // Read memory usage (VmRSS)
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "VmRSS:", 6) == 0) {
                sscanf(buffer + 6, "%d", &process_table[index].vmrss_kb);
                break;
            }
        }
        fclose(fp);
    }

    // Read CPU time - first try the stat file
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) 
		{
			// Find first ')' to skip the (comm) field
			char *start = strchr(buffer, ')');
			if (start) 
			{
				start++; // move past ')'
				int field = 0;
				long utime = 0, stime = 0;
				char *token = strtok(start, " ");

				while (token != NULL) 
				{
					field++;

					// utime is field 14, stime is field 15
					if (field == 14) utime = atol(token);
					if (field == 15) 
					{
						stime = atol(token);
						break;
					}

					token = strtok(NULL, " ");
				}

				process_table[index].cpu_time = utime + stime;
			}
		}
        fclose(fp);
    }
    
    // If CPU time is still 0, try /proc/[pid]/times as a fallback
    if (process_table[index].cpu_time == 0) {
        snprintf(path, sizeof(path), "/proc/%d/times", pid);
        fp = fopen(path, "r");
        if (fp) {
            long user_time = 0, system_time = 0;
            if (fscanf(fp, "%ld %ld", &user_time, &system_time) == 2) {
                process_table[index].cpu_time = user_time + system_time;
            }
            fclose(fp);
        }
    }
}

void print_process_info(int index) {
    if (process_table[index].exited) {
        printf("PID: %d | Status: Exited with code %d\n", 
               process_table[index].pid, 
               process_table[index].status);
    } else {
        printf("PID: %d | Name: %-10s | RSS: %5d KB | CPU time: %ld ticks\n",
               process_table[index].pid,
               process_table[index].name[0] ? process_table[index].name : "Unknown",
               process_table[index].vmrss_kb,
               process_table[index].cpu_time);
    }
}

/* - - - - - - - - - - - HELPER FUNCTION FOR ALARM - - - - - - - - - - - */
void alarm_handler(int sig)
{
    // Update and stop current process if it's still running
    if (!process_table[current_index].exited) {
        update_process_info(current_index);
        
        printf("===== Process %d status before stopping =====\n", process_table[current_index].pid);
        print_process_info(current_index);
        printf("============================================\n\n");
        
        kill(process_table[current_index].pid, SIGSTOP);
    }

    // Advance to the next alive process
    int start = current_index;
    do {
        current_index = (current_index + 1) % total_processes;
    } while (process_table[current_index].exited && current_index != start);

    // Resume and print info if process is still alive
    if (!process_table[current_index].exited) {
        printf("===== Resuming process %d =====\n", process_table[current_index].pid);
        
        // Resume the process
        kill(process_table[current_index].pid, SIGCONT);

        // Give the process a moment to update
        usleep(10000);  // 10ms
        
        // Update and print info after resume
        update_process_info(current_index);
        print_process_info(current_index);
        
        alarm(timer_time);
    }
}

/* - - - - - - - - - - - MAIN - - - - - - - - - - - */
int main(int argc, char* argv[]) 
{
    // Validate Args
    if (argc != 2) 
    {
        write(STDERR_FILENO, "Usage: ./part4 <input.txt>\n", 28);
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

    // Initialize process table
    for (int i = 0; i < MAXPROCESS; i++) {
        process_table[i].pid = 0;
        process_table[i].name[0] = '\0';
        process_table[i].vmrss_kb = 0;
        process_table[i].cpu_time = 0;
        process_table[i].exited = 0;
        process_table[i].status = 0;
    }

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
        process_table[i].pid = fork();
        
        // Error check 
        if (process_table[i].pid < 0) 
        {
            write(STDERR_FILENO, "Fork failed\n", 12);
            exit(-1);
        }

        // Child process
        if (process_table[i].pid == 0) 
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

        // Save command name in parent process
        if (j > 0) {
            strncpy(process_table[i].name, argbuff[0], sizeof(process_table[i].name) - 1);
            process_table[i].name[sizeof(process_table[i].name) - 1] = '\0'; // ensure null termination
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
        kill(process_table[i].pid, SIGUSR1);
    }

    // Send SIGSTOP to pause 
    for (int i = 0; i < line_number; i++)
    {
        kill(process_table[i].pid, SIGSTOP);
    }

    // Setup alarm handler
    struct sigaction sa;
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    // Initial information collection for all processes
    for (int i = 0; i < total_processes; i++) {
        update_process_info(i);
    }

    // Resume the first child 
    printf("===== Starting execution with process %d =====\n", process_table[0].pid);
    kill(process_table[0].pid, SIGCONT);
    alarm(timer_time);

    // Wait for all children to finish 
    int processes_remaining = line_number;

    while (processes_remaining > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);  // Non-blocking wait
        
        if (pid > 0) {
            // Find which process exited
            for (int i = 0; i < total_processes; i++) {
                if (process_table[i].pid == pid && !process_table[i].exited) {
                    process_table[i].exited = 1;
                    processes_remaining--;
                    
                    if (WIFEXITED(status)) {
                        process_table[i].status = WEXITSTATUS(status);
                        printf("Process %d exited with status %d\n", pid, WEXITSTATUS(status));
                    } else {
                        printf("Process %d terminated abnormally\n", pid);
                    }
                    break;
                }
            }
        }
        
        // Brief sleep to avoid high CPU usage
        usleep(10000);  // 10ms
    }

    // Stop timer after all children exit
    alarm(0);
    printf("All processes completed\n");
    exit(0);
}