#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MAXPROCESS 500

int main(int argc, char* argv[]) {
    char* error;
    char* in_file;
    FILE* in_fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read_int;

    pid_t pid_array[MAXPROCESS];
    int process_count = 0;

    // Validate argument
    if (argc != 2) {
        error = "Usage: ./part1 <input.txt>\n";
        write(STDERR_FILENO, error, strlen(error));
        return 1;
    }

	// Open input file as read only and get file descriptor
    in_file = argv[1];
    int in_fd = open(in_file, O_RDONLY);
    if (in_fd == -1) {
        error = "Cannot open input file\n";
        write(STDERR_FILENO, error, strlen(error));
        return 1;
    }

	// Open input file from file descriptor and get FILE*
    in_fp = fdopen(in_fd, "r");
    if (!in_fp) {
        error = "Cannot fdopen input file\n";
        write(STDERR_FILENO, error, strlen(error));
        close(in_fd);
        return 1;
    }

	// changed for (int i = 0; i < line_number, i++ )
	// While there are still lines, read line and write to line (char*)
    while ((read_int = getline(&line, &len, in_fp)) != -1) {

        // Parse tokens
        char* buff[100]; 
        int j = 0;
        char* token = strtok(line, " \n");
        while (token != NULL) {
            buff[j++] = strdup(token);
            token = strtok(NULL, " \n");
        }
		// And NULL to the end of the line
        buff[j] = NULL; 

        // Fork process
        pid_t pid = fork();
        
		// Check for error
		if (pid < 0) 
		{
            error = "Fork failed\n";
            write(STDERR_FILENO, error, strlen(error));
            continue;
        }

		// Child process
        if (pid == 0) 
		{	
			// If child process, run the command at buff[0]
			// -1 -> Erorr
			if (execvp(buff[0], buff) == -1) 
			{	
				// Error check
                error = "Error running execvp\n";
                write(STDERR_FILENO, error, strlen(error));
                exit(EXIT_FAILURE);
            }
        } 
        
		// Parent	
		else 
		{	
			// If parent process, add to pid_array
            pid_array[process_count++] = pid;
			
        }

        // Free memory used by strdup
        for (int k = 0; k < j; k++) 
		{
            free(buff[k]);
        }
    }

    // Parent waits for all child processes to be done
	// Ensures that this main program wont end before all child process
	// are done
    for (int i = 0; i < process_count; i++) {
        waitpid(pid_array[i], NULL, 0);
    }

	// Close, free then exit using system cmd after
	// all child processses are done
    fclose(in_fp); if (line) free(line); exit(0);

}
