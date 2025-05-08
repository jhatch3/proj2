#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MAXPROCESS 500
#define MAXARGS 100

int main(int argc, char* argv[]) 
{
    // Validate Args
    if (argc != 2) 
    {
        write(STDERR_FILENO, "Usage: ./part1 <input.txt>\n", 28);
         exit(1);
    }

    // Open file 
    FILE* in_fp = fopen(argv[1], "r");
    if (!in_fp) 
    {
        write(STDERR_FILENO, "Cannot open input file\n", 24);
        exit(-1);
    }

    char* lines[MAXPROCESS];
    size_t len = 0;
    ssize_t read;
    int line_number = 0;
    char* line = NULL;

    // Read all lines and store copies
    while ((read = getline(&line, &len, in_fp)) != -1 && line_number < MAXPROCESS) 
    {
        lines[line_number++] = strdup(line);
    }
    free(line);
    fclose(in_fp);

    pid_t pid_array[MAXPROCESS];

    for (int i = 0; i < line_number; i++) 
    {
        // Tokenize line into command and args
        char* argbuff[MAXARGS];
        int j = 0;

	    // Get tokens of each cmd line 
        char* token = strtok(line, " \n");
        while (token != NULL) 
	    {
            argbuff[j++] = strdup(token);
            token = strtok(NULL, " \n");
        } 
	    
	    argbuff[j] = NULL;

        // Fork process
        pid_array[i] = fork();
	    
        // Check for errors 
        if (pid_array[i] < 0) 
	    {
            write(STDERR_FILENO, "Fork failed\n", 12);
            exit(-1);
        }
        
        // If child processes run cmd 
        if (pid_array[i] == 0) 
	    {
            if (execvp(argbuff[0], argbuff) == -1) 
	        {
                write(STDERR_FILENO, "Execvp failed\n", 14);
                exit(-1);
            }
        }

        // Free tokens
        for (int k = 0; k < j; k++) 
	    {
            free(argbuff[k]);
        }
        free(lines[i]);
    }

    // Wait for all child processes
    for (int i = 0; i < line_number; i++) 
    {
        waitpid(pid_array[i], NULL, 0);
    } 

  exit(0);
}
