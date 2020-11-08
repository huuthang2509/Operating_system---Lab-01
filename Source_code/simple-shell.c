#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

//child process runs concurently: & using fork(), execvp(), wait() 
//history: !!
//redirection: >, < using dup2()
//pipe: | using pipe(), dup2()

#define MAX_LENGTH_COMMAND 80
#define MAX_LENGTH_ARGS 80


void printError(char* error)
{
	printf("osh: %s\n", error);
}


void init(char *a , int n){

	for (int i = 0; i < n; i++){
		
		a[i] = '\0';	
	}
}




int inputCommand(char* input){
	int len = 0;
	char c;
	
	while (c = getchar()){
	
		if ( c == '\n')
			break;
		
		input[len++] = c;
		
	}
	
	return len;
}

//cut parse in sentences by "character" and save number of arg after parse
int parseCommand(char* input, char* args[MAX_LENGTH_ARGS], char* character)
{
	int numOfToken = 0;
	char* copy = (char*)malloc(MAX_LENGTH_COMMAND*sizeof(char));
	
	strcpy(copy, input);

	args[numOfToken] = strtok(copy, character);
	while (args[numOfToken] != NULL)
		args[++numOfToken] = strtok(NULL, character);
	args[numOfToken++] = NULL; 
	

	return numOfToken;
}



//Solve redirection > and <
//caseRedir = 1 (redirect output) or = 2 (redirect input)
int redirectInAndOut(char* args[MAX_LENGTH_ARGS], int numArgs, int pos, int caseRedir){
	
	int fd;
	

	if (caseRedir == 1)
		fd = open(args[pos + 1], O_CREAT | O_WRONLY | O_APPEND, 0777);
	else if (caseRedir == 2){
		fd = open(args[pos + 1], O_RDONLY | O_APPEND);
		}	

	//error open file
	if (fd < 0){
		printError("Error opening file\n");
		return 1;
	}
	
	int caseStd =  (caseRedir == 1) ? STDOUT_FILENO: STDIN_FILENO;
	
	dup2(fd, caseStd);

	numArgs -= 2;
	args[numArgs - 1] = NULL;

	
	if (close(fd) == -1) {
            printError("Closing input failed");
            
        }
			

	return 0;
}



// |
int Pipe(char* args[MAX_LENGTH_ARGS], int numArgs, int pos){

	// separate commands 2 args[] from 1 args using format arg_1, |, arg_2
	char* args_1[MAX_LENGTH_ARGS];
	char* args_2[MAX_LENGTH_ARGS];
	for (int i = 0; i < numArgs - 2; i++)
		if ( i < pos){
			args_1[i] = args[i]; 
			
		}
		else{
			args_2[i - pos] = args[i + 1]; 
		}
	args_1[pos] = NULL;
	

	args_2[numArgs - pos - 1] = NULL;

	
	int fd[2];
	int status1 = 0, status2 = 0;
	if (pipe(fd) == -1){ //fd[0] is set up for reading, fd[1] is set up for writing
    	
       	 	printError("Failed to pipe cmd");
       		return 0;
    	}

	if (fork() == 0) {

	    	 // Redirect STDOUT to output part of pipe 
	      	dup2(fd[1], STDOUT_FILENO);       
	      	close(fd[0]);     
	      	close(fd[1]);      

	      	status1 = execvp(args_1[0], args_1);
	   
	      	if (status1 == -1)
				printError("Error executing the first command");
			return 0;
	}

	 // Create 2nd child
	 if (fork() == 0) {

	      	// Redirect STDIN to input part of pipe
	      	dup2(fd[0], STDIN_FILENO);            
	     	close(fd[1]);      
	     	close(fd[0]);       

	      	status2 = execvp(args_2[0], args_2);   

	      	if (status2 == -1)
				printError("Error executing the second command");
			return 0;
	}

	close(fd[0]);
	close(fd[1]);
	//wait for child 1
	wait(0);   
	//wait for child 2
	wait(0);
	return 0;
}



int main()
{
	char command[MAX_LENGTH_COMMAND];
	char commandCopy[MAX_LENGTH_COMMAND];

	char history[MAX_LENGTH_COMMAND];
	

	//init full \0 for array
	init(history, MAX_LENGTH_COMMAND);
	
	char* args[MAX_LENGTH_ARGS];
	
	//flag check history exists
	_Bool checkHistory = 0;

	while (1)
	{
		printf("osh> ");
		fflush(stdout);

		// reset command after every running
		init(command, MAX_LENGTH_COMMAND);
		
		
		//Input command 
		int length = inputCommand(command);
		
		//Check concurrent 
		int concurrent = 0;
		if (command[length - 1] == '&'){
			concurrent = 1;
			command[length - 1] = '\0';
			length--;
		}

		// number of arg after cut token 
		// command save this token
		int num_args = parseCommand(command, args, " ");
		

		if (length == 0) // no input
			continue;

		if (strcmp(command, "exit") == 0){ // exit
		
			break;
		}
		if (strcmp(command, "!!") == 0){ // history: !!
		
			if (checkHistory > 0){
				// print in history
				// exec history
				printf("%s\n", history);
				num_args = parseCommand(history, args, " ");
			}
			else{
				//error if no history
				printError("No commands in history\n");
				continue;
			}
		}
		else // if the first time command => save in history
		{
			if (!checkHistory){
				strcpy(history, command);
				checkHistory = 1;
			}
			else if (checkHistory){ // else reset history and copy new command

				init(history, MAX_LENGTH_COMMAND);
				strcpy(history, command);
			}
		}

		

		
		
		
		int task = 0; // not both redirection and pipe
		int pos = 0; //position of args
		for (pos; pos < num_args - 1; pos++)
		{
			//printf("%s", args[pos]);
			
			if (strcmp(args[pos], ">") == 0){
				task = 1; // redirect >
				break;
			}
			if (strcmp(args[pos], "<") == 0){
				task = 2; // redirect <
				
				break;
			}
			if (strcmp(args[pos], "|") == 0)
			{
				task = 3; // pipe |
				break;
			}
		}
		
		
		
	
		
		// executing command in a child progress and redirection
		int status = 0;
		pid_t pid = fork();
		if (pid < 0) // fail
		{
			printError("Error creating child process\n");
			return 0;
		}
		//printf("%d", pid);
		if (pid == 0) // in child process
		{
			
			int check = 0;
			
			if (task == 3) {
				int check = Pipe(args, num_args, pos); 
			}
			else {
				if (task == 1 || task == 2){	
										
					check = redirectInAndOut(args, num_args, pos, task);
							
				}
				status = execvp(args[0], args);
					
				if (status == -1)
					printError("Error executing commands\n");
					
			
				if (check == 1){
			
					continue;
				}
			}
				
			return 1;
		}
		else // child process id - in parent process
		{
			
			printf("Parent <%d> spawned a child <%d>.\n", getpid(), pid);
			switch (concurrent) {

				// Parent and child are running concurrently
				case 1: {
					printf("Child process <%d> run concurrently\n", pid);
					// waitpid(pid, &status, 0);
					break;
				}

				// Parent waits for child process
				default: {
					waitpid(pid, &status, WUNTRACED);
					printf("\n");
					break;
				 }
			}
	   
		}
		
		

    }
	return 0;
}