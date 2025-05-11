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
    if (process_table[index].exited) return;

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
                process_table[index].name[strcspn(process_table[index].name, "\n")] = 0;
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

    // Read CPU time (utime + stime)
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            char *start = strchr(buffer, ')');
            if (start) {
                start += 2; // Skip ") "
                char *token = strtok(start, " ");
                int field_count = 3;
                long utime = 0, stime = 0;

                while (token != NULL) {
                    if (field_count == 14) utime = atol(token);
                    if (field_count == 15) {
                        stime = atol(token);
                        break;
                    }
                    token = strtok(NULL, " ");
                    field_count++;
                }
                process_table[index].cpu_time = utime + stime;
            }
        }
        fclose(fp);
    }
}

void print_process_info(int index) {
    if (process_table[index].exited) {
        printf("PID: %d | Status: Exited with code %d\n",
               process_table[index].pid, process_table[index].status);
    } else {
        long ticks_per_sec = sysconf(_SC_CLK_TCK);
        float cpu_secs = process_table[index].cpu_time / (float)ticks_per_sec;
        printf("PID: %d | Name: %-10s | RSS: %5d KB | CPU time: %.2f sec\n",
               process_table[index].pid,
               process_table[index].name[0] ? process_table[index].name : "Unknown",
               process_table[index].vmrss_kb,
               cpu_secs);
    }
}

/* - - - - - - - - - - - ALARM HANDLER - - - - - - - - - - - */
void alarm_handler(int sig) {
    if (!process_table[current_index].exited) {
        update_process_info(current_index);
        printf("===== Process %d status before stopping =====\n", process_table[current_index].pid);
        print_process_info(current_index);
        printf("============================================\n\n");
        kill(process_table[current_index].pid, SIGSTOP);
    }

    // Move to next live process
    int found = 0;
    int start = current_index;
    do {
        current_index = (current_index + 1) % total_processes;
        if (!process_table[current_index].exited) {
            found = 1;
            break;
        }
    } while (current_index != start);

    if (!found) return; // All processes exited

    printf("===== Resuming process %d (%s) =====\n",
           process_table[current_index].pid, process_table[current_index].name);

    kill(process_table[current_index].pid, SIGCONT);
    usleep(50000); // Give time to run
    update_process_info(current_index);
    print_process_info(current_index);
    alarm(timer_time);
}

/* - - - - - - - - - - - MAIN FUNCTION - - - - - - - - - - - */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        write(STDERR_FILENO, "Usage: ./part4 <input.txt>\n", 28);
        exit(1);
    }

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    FILE* in_fp = fopen(argv[1], "r");
    if (!in_fp) {
        write(STDERR_FILENO, "Cannot open input file\n", 24);
        exit(-1);
    }

    char* lines[MAXPROCESS];
    size_t len = 0;
    ssize_t read;
    int line_number = 0;
    char* line = NULL;

    while ((read = getline(&line, &len, in_fp)) != -1 && line_number < MAXPROCESS) {
        lines[line_number++] = strdup(line);
    }
    free(line);
    fclose(in_fp);

    for (int i = 0; i < line_number; i++) {
        char* argbuff[MAXARGS];
        int j = 0;

        char* token = strtok(lines[i], " \n");
        while (token && j < MAXARGS - 1) {
            argbuff[j++] = strdup(token);
            token = strtok(NULL, " \n");
        }
        argbuff[j] = NULL;

        process_table[i].pid = fork();
        if (process_table[i].pid < 0) {
            write(STDERR_FILENO, "Fork failed\n", 12);
            exit(-1);
        }

        if (process_table[i].pid == 0) {
            int sig;
            sigset_t child_set;
            sigemptyset(&child_set);
            sigaddset(&child_set, SIGUSR1);
            sigprocmask(SIG_UNBLOCK, &child_set, NULL);
            sigwait(&child_set, &sig);
            execvp(argbuff[0], argbuff);
            write(STDERR_FILENO,"Execvp failed\n", 15);
            exit(-1);
        }

        if (j > 0) {
            strncpy(process_table[i].name, argbuff[0], sizeof(process_table[i].name) - 1);
            process_table[i].name[sizeof(process_table[i].name) - 1] = '\0';
        }

        for (int k = 0; k < j; k++) free(argbuff[k]);
        free(lines[i]);
    }

    total_processes = line_number;

    for (int i = 0; i < total_processes; i++) kill(process_table[i].pid, SIGUSR1);
    for (int i = 0; i < total_processes; i++) kill(process_table[i].pid, SIGSTOP);

    struct sigaction sa;
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    for (int i = 0; i < total_processes; i++) update_process_info(i);

    printf("===== Starting execution with process %d =====\n", process_table[0].pid);
    kill(process_table[0].pid, SIGCONT);
    alarm(timer_time);

    int remaining = total_processes;
    while (remaining > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            for (int i = 0; i < total_processes; i++) {
                if (process_table[i].pid == pid && !process_table[i].exited) {
                    process_table[i].exited = 1;
                    remaining--;
                    if (WIFEXITED(status)) {
                        process_table[i].status = WEXITSTATUS(status);
                        printf("Process %d exited with status %d\n", pid, WEXITSTATUS(status));
                    } else if (WIFSIGNALED(status)) {
                        printf("Process %d terminated by signal %d\n", pid, WTERMSIG(status));
                    } else {
                        printf("Process %d terminated abnormally\n", pid);
                    }
                    break;
                }
            }
        }
        usleep(10000);
    }

    alarm(0);
    printf("All processes completed\n");
    return 0;
}
