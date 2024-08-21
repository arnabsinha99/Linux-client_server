/*
  
  Author: Arnab Sinha
  TImestamp: 14-04-2024 23:50:00 EST
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>
#include <ftw.h>
#include <regex.h>

#define SERVER_IP "127.0.0.1"
#define MIRROR_IP "127.0.0.1"
#define SERVER_PORT 4500
#define MIRROR_IP_PORT1 4501
#define MIRROR_IP_PORT2 4502
#define MAX_MSG_LENGTH 4096

int clientCount; // to store the count of client and take respective action

/*
 * countWords: Counts the number of words in a given string
 * 
 * Parameters:
 * - message: The string in which words are to be counted
 * 
 * Return Value:
 * - int: The number of words in the given string
 * 
 * Explanation:
 * This function iterates through each character in the given string.
 * It maintains a flag 'in_word' to indicate whether the current character is inside a word or not.
 * If the current character is a space and we were inside a word, it increments the word count.
 * If the current character is not a space, it sets the 'in_word' flag to 1, indicating that we are inside a word.
 * After iterating through the entire string, if the last character was part of a word, it increments the count.
 * Finally, it returns the count of words in the string.
 */

int countWords(char *message) 
{
    int count = 0;
    int in_word = 0; // Flag to indicate whether we are currently inside a word

    // Iterate through each character in the string
    for (int i = 0; message[i] != '\0'; i++) {
        // Check if the current character is a space
        if (message[i] == ' ') {
            // If we were inside a word, increment the word count
            if (in_word) {
                count++;
                in_word = 0; // Reset the flag
            }
        } else {
            // If the current character is not a space, we are inside a word
            in_word = 1;
        }
    }

    // Increment count if the string ends with a word
    if (in_word) {
        count++;
    }

    return count;
}

/*
 * isWordLimitExceeded: Checks if the word count in a message exceeds a specified limit
 * 
 * Parameters:
 * - message: The message to be checked
 * - limit: The maximum number of words allowed
 * 
 * Return Value:
 * - int: 1 if the word count exceeds the limit, 0 otherwise
 * 
 * Explanation:
 * This function calculates the number of words in the given message using the countWords() function.
 * It then compares the word count with the specified limit.
 * If the word count is greater than the limit, it returns 1, indicating that the word limit is exceeded.
 * Otherwise, it returns 0, indicating that the word limit is not exceeded.
 */

int isWordLimitExceeded(char *message, int limit) {
    int word_count = countWords(message);
    return word_count > limit ? 1 : 0;
}

/*
  Copy a file from a source path to a destination path.
 
  Parameters:
   - source_path: Path of the source file to copy.
   - destination_path: Path of the destination directory where the file will be copied.
   - filename: Name of the file to be copied.
 
  Returns:
   - 1 if the file is copied successfully.
   - 0 if an error occurs during the copy operation.
 
  This function constructs the source and destination file paths using the provided
  source_path, destination_path, and filename. It then constructs and executes a command
  to copy the file using the 'cp' command in Unix-like systems. The function opens a
  pipe to execute the command and checks the exit status to ensure the success of the
  copy operation. If successful, it returns 1; otherwise, it returns 0.

 */

int copy_file_to_home(char *source_path, char *destination_path, char * filename) {
    // Construct the source file path
    char source_file_path[MAX_MSG_LENGTH];
    snprintf(source_file_path, sizeof(source_file_path), "%s", source_path);

    // Construct the destination file path
    char destination_file_path[MAX_MSG_LENGTH];
    snprintf(destination_file_path, sizeof(destination_file_path), "%s/w24project/%s", destination_path, filename);

    // Construct and execute the command to copy the file
    char command[MAX_MSG_LENGTH];
    snprintf(command, sizeof(command), "cp %s %s", source_file_path, destination_file_path);
    //printf("Command is: %s", command);
    
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Error executing command");
        return 0;
    }

    // Check the exit status of the command
    int status = pclose(fp);
    if (status == -1) {
        perror("Error closing pipe");
        return 0;
    } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Command failed with exit status %d\n", WEXITSTATUS(status));
        return 0;
    }

    printf("File copied successfully to home\n");
    return 1;
}


/*
 * This function handles writing messages to the server and receiving responses.
 *
 * Parameters:
 *  - arg: Pointer to the client socket file descriptor.
 *
 * This function prompts the user to enter a command and sends it to the server using the client socket.
 * It then waits for the server's response and handles different types of responses accordingly.
 * Supported commands include 'dirlist -a', 'dirlist -t', 'w24fn', 'w24fdb', 'w24fda', 'w24fz', and 'w24ft'.
 * Responses are printed to the console, and in case of receiving a TAR file response, it copies the file to the project folder.
 */

void * write_to_server(void * arg)
{
    int clientSocket = *((int *)arg);
    char message[MAX_MSG_LENGTH];
    char *message_copy, *message_copy2;

    // start client loop
    int i=1;
    int success_command_count = 0;
    
    while(i)
    {
    	// Send a message to the server
        printf("Kindly enter your command: ");
        fgets(message, sizeof(message), stdin);
        
        printf("-----------------------------------------\n");
        // Remove the newline character from the end of the input message
    	if (message[strlen(message) - 1] == '\n') {
        	message[strlen(message) - 1] = '\0';
    	}

	    
        message_copy = strdup(message);
        message_copy2 = strdup(message); // while printing file information

        //printf("Entered command: %s\n", message_copy);

        if(strcmp("dirlist -a", message_copy)==0){
        	// do nothing. Skip to printing output of command
        }
        else if(strcmp("dirlist -t", message_copy)==0){
        	// do nothing. Skip to printing output of command
        }
        else if (strstr(message_copy, "w24fn ") == message_copy) {
	        
	        // extract filename after "w24fn "
	        char * filename = strtok(message_copy + 6, "\n");
	        //printf("Filename is: %s\n", filename);

            if (filename == NULL)
            {
                printf("Invalid Command\n");    
                continue;
            }
	    } 
	    else if (strstr(message_copy, "w24fz ") == message_copy) {

	    	char *token;
	    	long size1, size2;

	        // Tokenize the input message to extract size1 and size2
		    token = strtok(message_copy, " ");
		    token = strtok(NULL, " "); // Move to the next token (size1)

		    if (token != NULL) {
		        size1 = strtol(token, NULL, 10); // Convert size1 to long integer
		        token = strtok(NULL, " "); // Move to the next token (size2)
		        if (token != NULL) {
		            size2 = strtol(token, NULL, 10); // Convert size2 to long integer
		            token = strtok(NULL, " "); // Move to the next token
		            if (token == NULL) {
		                if(size1<0 || size2<0)
		                {
		                	fprintf(stderr, "Both sizes should be positive. Please try again.\n");

		                	continue;
		                }
		                else if(size1>size2){
		                	fprintf(stderr, "size1 should be <= size2. Please try again.\n");
		                	continue;
		                }
		                else{
		                	printf("size1: %ld, size2: %ld\n", size1, size2); // success
		                }
		            } else {
		                fprintf(stderr,"Error: Only two size parameters are allowed.\n");
		                continue;
		            }
		        } 
		        else // exceeded number of sizes 
		        {
		            fprintf(stderr,"Error: Both size1 and size2 must be provided.\n");
		            continue;
		        }
		    } // less number of sizes
		    else {
		        fprintf(stderr,"Error: Both size1 and size2 must be provided.\n");
		        continue;
		    }
	    } 
	    else if (strstr(message_copy, "w24ft ") == message_copy) {

	    	char *token;
		    char *file_types[3]; // Array to store file types
		    int count = 0; // count to store number of file types
		    int exceeded = 0; // to check for limit of extensions

	    	    // Tokenize the input message to extract file types
		    token = strtok(message_copy, " ");
		    token = strtok(NULL, " "); // Move to the next token (first file type)

		    // Store file types in the array
		    while (token != NULL) {
		    	if(count==3){
		    		exceeded = 1;
		    		fprintf(stderr, "Maximum 3 file types allowed. Please try again.\n" );
		    		break;
		    	}
		    	else{
		    		file_types[count] = strdup(token); // Store the file type
			        count++;
			        token = strtok(NULL, " "); // Move to the next token
		    	}
		    }

		    if(exceeded==1)
		    	continue;

		    // Check if at least one file type is provided and at most three
		    if (count >= 1 && count <= 3) {
		    	// do nothing
		    } else {
		        printf("Error: Enter at least one and at most three file types.\n");
		        continue;
		    }
	    } 
	    else if ((strstr(message_copy, "w24fdb ") == message_copy) || (strstr(message_copy, "w24fda ") == message_copy)) {
	        
	        // check if excess arguments have been passed
	        if(isWordLimitExceeded(message_copy,2)==1)
		    {
		    	printf("Too many arguments. Please try again\n\n");
		    	continue;
		    }

	        char *token,*date;
		    regex_t regex;
		    int ret;

		    token = strtok(message_copy, " "); // Tokenize the input message to extract the date
		    token = strtok(NULL, " ");

		    // Check if the date is not NULL and matches the expected format
		    if (token != NULL) {
		        
		        date = strdup(token); // Store the date

		        // Compile regular expression to match the date format (YYYY-MM-DD)
		        ret = regcomp(&regex, "^[0-9]{4}-[0-9]{2}-[0-9]{2}$", REG_EXTENDED);

		        if (ret == 0) {

		            // Match the date against the regular expression
		            ret = regexec(&regex, date, 0, NULL, 0);

		            if (ret == 0) {
		                // do nothing if okay
		            } else {
		                printf("Error: Invalid date format. Please enter date in YYYY-MM-DD format.\n");
		                continue;
		            }

		            regfree(&regex);
		        } else {
		            printf("Error: Failed to compile regular expression.\n");
		            continue;
		        }

		        free(date); // Free memory allocated by strdup
		    } else {
		        printf("Error: Date not provided.\n");
		        continue;
		    }
	    } 
	    else if (strcmp(message_copy, "quitc")==0) {
	        printf("Command: quitc\n");
	    }
	    else {
	        printf("Invalid command. Please try again.\n");
	        continue;
	    }

	
		// now sending message to server
        send(clientSocket, message, strlen(message), 0);

        // Wait for the server's response
        printf("Waiting for response...\n");

        memset(message, '\0', sizeof(message)); // emptying message

        ssize_t bytesRead = recv(clientSocket, message, sizeof(message), 0);

        if (bytesRead <= 0)
        {
            // Server disconnected or error
            printf("Server disconnected.\n");
            break;
        }
        else if(strcmp(message,"shut yourself")==0)
        {
        	printf("Client shutting down..\n");
        	break;
        }
	
		success_command_count+=1; //increment counter for sucess command. Used to name TAR file
	
        // HANDLE RESPONSES
	
        if(strcmp("dirlist -a", message_copy)==0){
        	// Print server response
        	printf("Directories under the home directory are (in alphabetical order): \n%s", message);
        }
        else if(strcmp("dirlist -t", message_copy)==0){
        	printf("Directories under the home directory are (in order or creation): \n%s", message);
        }
        else if (strstr(message_copy2, "w24fn ") == message_copy2) {

        	if(strcmp(message,"No file found")==0 || strcmp(message,"nftw failed.")==0 || strcmp(message,"Error in retrieving file stat.")==0){
        		printf("%s\n",message);
        		continue;
        	}
        	else{
        		printf("File information: \n\n");

        		printf("File name: %s\n", strtok(message_copy2 + 6, "\n"));
        		printf("%s\n\n",message);
        	}
        }
        else if((strstr(message_copy2, "w24fdb ") == message_copy2) || (strstr(message_copy2, "w24fda ") == message_copy2))
        {
        	if(strcmp(message,"No file found")==0){
        		printf("No file found.\n");
        	}
        	else if(strcmp(message,"temp.tar.gz")==0){
        		printf("TAR file received for dates. Saving to project folder $HOME/w24project/\n");
        	}
        }
        else if(strstr(message_copy2, "w24fz ") == message_copy2){

        	if(strcmp(message,"No file found")==0){
        		printf("No file found.\n");
        	}
        	else if(strcmp(message,"temp.tar.gz")==0){
        		printf("TAR file received for size constraints. Saving to project folder $HOME/w24project/\n");
        	}
        }
        else if(strstr(message_copy2, "w24ft ") == message_copy2){

        	if(strcmp(message,"No file found")==0){
        		printf("No file found.\n");
        	}
        	else if(strcmp(message,"temp.tar.gz")==0){
        		printf("No file found.\n");
        		printf("TAR file received for extension list. Saving to project folder $HOME/w24project/\n");
        	}
        }		
        else
        {
        	printf("Message from server: %s \n", message);
        }
        
        sleep(1); // wait for TAR file to be saved in client side

        // COPY tar file to project folder
        if(strcmp(message,"temp.tar.gz")==0)
        {
            char *source_path = "temp.tar.gz"; // Current directory
		    char *destination_path = getenv("HOME");
		    char filename[MAX_MSG_LENGTH];
		    int num1 = clientCount;
		    int num2 = success_command_count;
		    
		    // Format the filename string with the integer variables
		    snprintf(filename, sizeof(filename), "temp_client%d_cmd%d.tar.gz", num1, num2);
		    
		    if (!copy_file_to_home(source_path, destination_path, filename)) 
		    {
			fprintf(stderr, "Failed to copy file to home directory\n");
			continue;
		    }
        }
    }
}

int main(){
	
    int clientSocket;
    struct sockaddr_in serverAddr;
    char message[MAX_MSG_LENGTH];

    // Create socket
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP); // add htonl()
    serverAddr.sin_port = htons(SERVER_PORT);

    // Connect to server
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    //receive client count from server
    if (recv(clientSocket, message, MAX_MSG_LENGTH, 0) == -1) {
        perror("Receive failed");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    clientCount = atoi(message);
    printf("Received client count: %d\n", clientCount);

    int change_connection = 0;

    // Conditions for switching between server and mirrors

    if(clientCount>=1 && clientCount<=3){
    	// do nothing
    }
    else if(clientCount>=4 && clientCount<=6){
    	serverAddr.sin_port = htons(MIRROR_IP_PORT1);
    	change_connection=1;
    }
    else if(clientCount>=7 && clientCount<=9){
    	serverAddr.sin_port = htons(MIRROR_IP_PORT2);
    	change_connection=2;
    }
    else if(clientCount%3==0){
    	serverAddr.sin_port = htons(MIRROR_IP_PORT2);
    	change_connection=2;
    }
    else if(clientCount%3==2){
    	serverAddr.sin_port = htons(MIRROR_IP_PORT1);
    	change_connection=1;
    }
    else if(clientCount%3==1){
    	// do nothing
    }

    // Changing connections as per client count

    if(change_connection==1){
    	close(clientSocket); // close previous client socket

    	// Create socket
	    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	        perror("Socket creation failed");
	        exit(EXIT_FAILURE);
	    }

	    printf("Redirecting to the mirror1...\n");
    	// Connect to mirror
	    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
	        perror("Connection failed");
	        exit(EXIT_FAILURE);
	    }

	    printf("Redirected to mirror1\n");
    }
    else if(change_connection==2){
    	close(clientSocket); // close previous client socket

    	// Create socket
	    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	        perror("Socket creation failed");
	        exit(EXIT_FAILURE);
	    }

	    printf("Redirecting to the mirror2...\n");
    	// Connect to mirror
	    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
	        perror("Connection failed");
	        exit(EXIT_FAILURE);
	    }

	    printf("Redirected to mirror2\n");
    }

    // creating thread and waiting for client to finish
    
    pthread_t server_write_thread;

    if (pthread_create(&server_write_thread, NULL, write_to_server, (void *)&clientSocket) != 0)
    {
        perror("Error creating threads");
        exit(EXIT_FAILURE);
    }

    // Wait for both threads to finish
    pthread_join(server_write_thread, NULL);

    // Close socket
    close(clientSocket);

    return 0;
}
