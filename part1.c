#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h> 


#define MAXPROCESS 500

int c;
char* error;
char* in_file;
FILE* in_fp;
int line_number = 0;

pid_t pid_array[MAXPROCESS];

// File for part 1 of Project 2
int main(int argc, char* argv[]){

	printf("Running Part 1\n");

	if (argc != 2){
		error = "Usage: ./part1 <input.txt>\n";
		write(STDERR_FILENO, error, strlen(error));
		return 1;
	}

	else
	{	
		in_file = argv[1];
		printf("file is %s\n", in_file);
		int in_fd = open(in_file, O_RDONLY);
        if (in_fd == -1) {
			error = "Can not open(in_file)\n";
            write(STDERR_FILENO, error, strlen(error));
            return 1;
        }

        in_fp = fdopen(in_fd, "r");
        if (!in_fp) {
			error = "Can not fopen input file\n";
            write(STDERR_FILENO, error, strlen(error));           
			close(in_fd);
            return 1;
        }

		// Get number of lines
		for (c = getc(in_fp); c != EOF; c = getc(in_fp))
		{
        	if (c == '\n') 
            {
			line_number++;
			}
		}printf("line count %d\n", line_number);
	}

	for (int i = 0; i < line_number; i++)
	{
		
		pid_array[i] = fork();
		
		if (pid_array[i] < 0)
		{
			printf("%d: %d\n",i, pid_array[i]);
		}
		
		
		if (pid_array[i] == 0)
		{
			printf("%d: %d\n",i, pid_array[i]);
		}



	}

	fclose(in_fp);
}