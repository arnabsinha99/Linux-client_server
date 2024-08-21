/*
  
  Author: Arnab Sinha
  TImestamp: 14-04-2024 23:50:00 EST
*/

#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <libgen.h>

#define SERVER_IP "127.0.0.1"
#define MAX_MSG_LENGTH 4096
#define SERVER_PORT 4501
#define MAX_BUFFER_LENGTH 1000000
#define MAX_DATE_LENGTH 100
#define MAX_PATH_LENGTH 4096
#define MAX_EXTENSIONS 3
#define MAX_EXTENSION_LENGTH 100

char file_paths[1][MAX_PATH_LENGTH]; // Array to store file paths
int num_files = 0; // Counter for the number of files
int client_count_server = 0; // counter for number of clients
char * user_file_name;

/*
 * compareFileName: Compares the filename extracted from a file path with the user-input filename
 * 
 * Parameters:
 * - file_path: Path of the file whose name is to be compared
 * 
 * Return Value:
 * - int: 1 if the filenames match, 0 otherwise
 * 
 * Explanation:
 * This function extracts the filename from the given file path using the basename() function.
 * It then compares the extracted filename with the user-input filename stored in the global variable user_file_name.
 * If the filenames match, it returns 1; otherwise, it returns 0.
 * The function also includes an assertion to ensure that the user has entered a non-zero length filename.
 */

int compareFileName(const char * file_path) {

  assert(sizeof(user_file_name) > 0); //checks if the user entered a non-zero length file name.

  char * filename = basename((char * ) file_path); // basename() returns the filename out of the entire absolute/relative path
  
  // if match returns 1
  if (!strcmp(filename, user_file_name)) {
    return 1;
  }

  // if not a match returns 0
  return 0;
}

/*
 * traverse_and_extract: Callback function used for file traversal
 * 
 * Parameters:
 * - file_path: Path of the current file being traversed
 * - sb: Pointer to a struct containing information about the current file
 * - typeflag: Type of the current file (FTW_F for regular file, FTW_D for directory, etc.)
 * - ftwbuf: Pointer to a struct containing information about the file tree walk
 * 
 * Return Value:
 * - int: 0 to continue traversal, -2 to terminate traversal if a matching file is found
 * 
 * Explanation:
 * This function is used as a callback for the nftw function to traverse the directory structure.
 * It receives information about each file encountered during traversal and performs actions based on the file type and name.
 * If the encountered item is a regular file (typeflag == FTW_F), it compares the file path with the user-input filename.
 * If the filename matches, it stores the file path in an array and increments the count of matched files.
 * The traversal terminates if a matching file is found to optimize performance (returning -2).
 * If the encountered item is not a file, the traversal continues (returning 0).
 */
int traverse_and_extract(const char * file_path,  const struct stat * sb, int typeflag, struct FTW * ftwbuf) {

  // if file encountered    
  if (typeflag == FTW_F)
  {
    // if matches user input file name
    if (compareFileName(file_path)) {
      snprintf(file_paths[num_files], MAX_PATH_LENGTH, "%s", file_path);
      // printf("Matched file path: %s\n", file_path);// print file path
      printf("Matched file path: %s\n", file_paths[num_files]);// print file path
      num_files+=1;
      return -2; // terminate traversal
    }

  }

  return 0; // Continue traversal
}


void crequest(int client_fd);

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char message[MAX_MSG_LENGTH];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR option to allow reuse of the port
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1)
    {
        perror("Error setting socket option");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); // add htonl()
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) 
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 100) == -1) // backlog(queue) = 100
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", SERVER_PORT);

    while (1) {
        socklen_t client_len = sizeof(client_addr);

        // Accept incoming connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
            perror("Accept failed");
            continue;
        }

        printf("Connection accepted on mirror1 from %s\n", inet_ntoa(client_addr.sin_addr));

        int fork_pid = fork();

        if(fork_pid==0) // child process
        {
            crequest(client_fd);
            exit(EXIT_SUCCESS);
        }
        else if(fork_pid<0){
            fprintf(stderr, "Fork failed");
            exit(EXIT_FAILURE);
        }
        else{
            // Close client socket
            close(client_fd);
        }
    }

    // Close server socket
    close(server_fd);

    return 0;
}

/*
 * get_creation_date: Retrieves the creation date of a file
 * 
 * Parameters:
 * - file_path: Path of the file whose creation date is to be retrieved
 * 
 * Return Value:
 * - char *: A string representing the creation date of the file
 * 
 * Explanation:
 * This function constructs a command using the given file path to retrieve the creation date of the file.
 * The command is executed using popen(), and the output is read into the ctime_str array.
 * The newline character at the end of the output string is removed, if present.
 * The function then returns the string containing the creation date of the file.
 * Note: The returned string is stored in a static array, so it should be used or copied immediately after the function call to avoid overwriting.
 */


char *get_creation_date(char *file_path) {
    FILE *fp;
    char command[MAX_PATH_LENGTH];
    static char ctime_str[MAX_PATH_LENGTH];

    // Construct and execute the command
    snprintf(command, sizeof(command), "stat --format=%%w \"%s\"", file_path);
    fp = popen(command, "r");

    // Read the output and close the pipe
    if (fp == NULL || fgets(ctime_str, sizeof(ctime_str), fp) == NULL) {
        perror("Error executing or reading from pipe");
        exit(EXIT_FAILURE);
    }
    pclose(fp);

    // Remove the newline character at the end of ctime_str, if present
    size_t len = strlen(ctime_str);
    if (len > 0 && ctime_str[len - 1] == '\n') {
        ctime_str[len - 1] = '\0';
    }

    return ctime_str;
}

/*
 * This function processes messages received from a client connected to the server.
 * It continuously listens for messages from the client and performs appropriate actions based on the received message.
 * If the received message is "quitc", it decrements the client count, sends a shutdown message to the client, and breaks the loop to exit.
 * If the received message is "dirlist -a", it executes a command to list directories under the home directory in alphabetical order and sends the result back to the client.
 * If the received message is "dirlist -t", it executes a command to list directories under the home directory in the order of creation and sends the result back to the client.
 * If the received message starts with "w24fn ", it extracts the filename from the message and retrieves file information such as size, creation date, and permissions, then sends the information back to the client.
 * If the received message starts with "w24fdb " or "w24fda ", it extracts the date filter from the message and searches for files created before or after the specified date, respectively. It then sends the list of files meeting the criteria back to the client.
 * If the received message starts with "w24fz ", it extracts the size constraints from the message and searches for files within the specified size range. It sends the list of files meeting the criteria back to the client.
 * If the received message starts with "w24ft ", it extracts a list of file extensions from the message and searches for files with those extensions. It sends the list of files back to the client as a compressed archive (tar.gz).
 * If none of the above conditions are met, it echoes the received message back to the client.
 */


void crequest(int client_fd){
    
    char message[MAX_MSG_LENGTH];

    // printf("before loop..\n");

    while(1)
    {
        memset(message, '\0', sizeof(message));
        // printf("In child again..\n");
        // Receive message from client
        if (recv(client_fd, message, MAX_MSG_LENGTH, 0) == -1) {
            perror("Receive failed");
            close(client_fd);
            continue;
        }

        // Remove the newline character from the end of the input message
        if (message[strlen(message) - 1] == '\n') {
            message[strlen(message) - 1] = '\0';
        }

        printf("Received message: %s\n", message);
        // sleep(2);

        if(strlen(message)==0){
            printf("No message received from client.. continuing to look for client\n");
            continue;
        }

        if(strcmp(message,"quitc")==0)
        {
            // printf("quitc received\n");
            client_count_server-=1; //decrement client count
            char *close_client_msg = "shut yourself";
            if (send(client_fd, close_client_msg, strlen(close_client_msg), 0) == -1) {
                perror("Send failed");
                close(client_fd);
            }
            break;
        }
        else if(strcmp(message,"dirlist -a")==0) // FILES IN ALPHABETICAL ORDER - working
        {
            // Capture directory list
            FILE *fp = popen("find $HOME -type d -not -wholename '*/[.]*' | sort", "r");
            if (fp == NULL) {
                perror("Error executing command");
                exit(EXIT_FAILURE);
            }

            char buffer[MAX_BUFFER_LENGTH];
            size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
            if (bytes_read == 0) {
                perror("Error reading from command output");
                exit(EXIT_FAILURE);
            }

            // Close pipe
            pclose(fp);

            if (send(client_fd, buffer, strlen(buffer), 0) == -1) {
                perror("Send failed");
                close(client_fd);
                continue;
            }
        }
        else if(strcmp(message,"dirlist -t")==0) // FILES IN time of creation ORDER - working
        {

            FILE *fp, *check_existence;
            char output_for_client[MAX_BUFFER_LENGTH], message_to_client[MAX_BUFFER_LENGTH];

            // path to search
            char * root = "$HOME";
            
            // ignoring hidden files and using birth date as filter        
            snprintf(output_for_client, sizeof(output_for_client), "find %s -type d -not -wholename '*/[.]*' -exec stat --format=%%w\' \'%%n {} + | sort -r | cut -d' ' -f4-", root);
            
            // Open a pipe to execute the command
            check_existence = popen(output_for_client, "r");
            if (check_existence == NULL) {
                perror("popen failed to run.");
                exit(EXIT_FAILURE);
            }

            memset(output_for_client, 0, sizeof(output_for_client)); //empty it

            // Read the output of the command
            if (fgets(output_for_client, MAX_BUFFER_LENGTH, check_existence) == NULL) {
                
                snprintf(message_to_client, sizeof(message_to_client), "No file found");

                if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }
            }
            else
            {

                memset(output_for_client, 0, sizeof(output_for_client)); //empty it

                // checking for non-zero output
                
                // change f4 accordingly in debian
                snprintf(output_for_client, sizeof(output_for_client), "find %s -type d -not -wholename '*/[.]*' -exec stat --format=%%w\' \'%%n {} + | sort -r | cut -d' ' -f4-", root);
                // printf("Command to fopen is: %s\n", output_for_client);

                // Open a pipe to the command
                if ((fp = popen(output_for_client, "r")) == NULL)
                {
                    perror("popen failed to run");
                    exit(EXIT_FAILURE);
                }

                char buffer[MAX_BUFFER_LENGTH];
                size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
                if (bytes_read == 0) {
                    perror("Error reading from command output");
                    exit(EXIT_FAILURE);
                }

                // Close pipe
                pclose(fp);
            
                if (send(client_fd, buffer, strlen(buffer), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }
            }
        }
        else if(strstr(message, "w24fn ") == message) // FILE INFORMATION - WORKING
        {

            num_files=0;
            char * message_copy = strdup(message);
            user_file_name = strtok(message_copy + 6, "\0");

            char * root = getenv("HOME");
            
            int flags = FTW_PHYS; // For not following symbolic links. Ensures that traversal process stays within the boundaries of the directory structure being traversed.
            
            // using nftw to find the first match out of possibly many
            int ret = nftw(root, traverse_and_extract, 1, flags);
            char *message_to_client = (char *)malloc(MAX_BUFFER_LENGTH * sizeof(char));

            if (ret == -1) // if nftw fails
            {
                perror("nftw");
                snprintf(message_to_client, MAX_BUFFER_LENGTH, "nftw failed");
            }
            else
            {
                //printf("The number of files found is : %d\n", num_files);

                if(num_files==0){
                    snprintf(message_to_client, MAX_BUFFER_LENGTH, "No file found");
                    // printf("File not found.\n");
                }
                else
                {
                    struct stat sb;
                    
                    // Call stat() to retrieve file information
                    if (stat(file_paths[0], &sb) == -1) {
                        snprintf(message_to_client, MAX_BUFFER_LENGTH, "Error in retrieving file stat.");
                    }
                    else{
                        // Print file size
                        snprintf(message_to_client, MAX_BUFFER_LENGTH, "File Size: %ld bytes\n", sb.st_size);

                        // Print file creation date (using st_ctime)
                        char *ctime_str = get_creation_date(file_paths[0]);
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "Creation Date: %s\n", ctime_str);

                        // Print file permissions
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "File Permissions: ");
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (S_ISDIR(sb.st_mode)) ? 'd' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IRUSR) ? 'r' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IWUSR) ? 'w' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IXUSR) ? 'x' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IRGRP) ? 'r' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IWGRP) ? 'w' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IXGRP) ? 'x' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IROTH) ? 'r' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IWOTH) ? 'w' : '-');
                        snprintf(message_to_client + strlen(message_to_client), MAX_BUFFER_LENGTH - strlen(message_to_client), "%c", (sb.st_mode & S_IXOTH) ? 'x' : '-');
                    }
                }

            }   

            if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                perror("Send failed");
                close(client_fd);
                continue;
            }
        }
        else if((strstr(message, "w24fdb ") == message) || (strstr(message, "w24fda ") == message)) // DATE FILTER - WORKING ON DEBIAN
        {
            char *date = &message[7]; // Find the prefix in the message
            
            char *date_sign = (strstr(message, "w24fdb ") == message) ? "<=" : ">=";

            // Use the find command to search for files with creation date <= date
            FILE *fp, *check_existence;
            char output_for_client[MAX_BUFFER_LENGTH], message_to_client[MAX_BUFFER_LENGTH];

            // path to search
            char * root = "$HOME";

            // change command as per sign. Ignoring hidden files
            if(strcmp(date_sign,">=")==0)
                snprintf(output_for_client, sizeof(output_for_client), "find %s -type f -not -wholename '*/[.]*' -exec stat --format=%%w\\ %%n {} + | awk -v date='%s' '$1 >= date' | cut -d' ' -f4-", root, date);
            else
                snprintf(output_for_client, sizeof(output_for_client), "find %s -type f -not -wholename '*/[.]*' -exec stat --format=%%w\\ %%n {} + | awk -v date='%s' '$1 <= date' | cut -d' ' -f4-", root, date);

            // Open a pipe to execute the command
            check_existence = popen(output_for_client, "r");
            if (check_existence == NULL) {
                perror("popen failed to run.");
                exit(EXIT_FAILURE);
            }

            memset(output_for_client, 0, sizeof(output_for_client)); //empty it

            // Read the output of the command
            if (fgets(output_for_client, MAX_BUFFER_LENGTH, check_existence) == NULL) {
                
                snprintf(message_to_client, sizeof(message_to_client), "No file found");

                if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }
            }
            else // if output is non=zero
            {
                memset(output_for_client, 0, sizeof(output_for_client)); //empty it

                // checking for non-zero output
                // change command as per sign. Ignoring hidden files
                if(strcmp(date_sign,">=")==0)
                    snprintf(output_for_client, sizeof(output_for_client), "find %s -type f -not -wholename '*/[.]*' -exec stat --format=%%w\\ %%n {} + | awk -v date='%s' '$1 >= date' | cut -d' ' -f4- | tar -czvf temp.tar.gz -T -", root, date);
                else
                    snprintf(output_for_client, sizeof(output_for_client), "find %s -type f -not -wholename '*/[.]*' -exec stat --format=%%w\\ %%n {} + | awk -v date='%s' '$1 <= date' | cut -d' ' -f4- | tar -czvf temp.tar.gz -T -", root, date);

                    //printf("Command to fopen is: %s\n", output_for_client);

                // Open a pipe to the command
                if ((fp = popen(output_for_client, "r")) == NULL)
                {
                    perror("popen failed to run");
                    exit(EXIT_FAILURE);
                }

                // implement send message
                snprintf(message_to_client, sizeof(message_to_client), "temp.tar.gz");

                if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }

                pclose(fp);
            }

            pclose(check_existence);

        }
        else if(strstr(message, "w24fz ") == message) // SIZE CONSTRAINT - WORKING
        {

            long size1, size2;
            // store the sizes
            sscanf(message, "w24fz %ld %ld", &size1, &size2);

            // Use the find command to search for files with creation date <= date
            FILE *fp, *check_existence;
            char output_for_client[MAX_BUFFER_LENGTH], message_to_client[MAX_BUFFER_LENGTH];

            // path to search
            char * root = "$HOME";

            // checking for non-zero output
            snprintf(output_for_client, sizeof(output_for_client), "find %s -type f -not -wholename '*/[.]*' -size +%ldc -size -%ldc", root, size1, size2);

            // Open a pipe to execute the command
            check_existence = popen(output_for_client, "r");
            
            if (check_existence == NULL) {
                perror("popen failed to run.");
                exit(EXIT_FAILURE);
            }

            memset(output_for_client, 0, sizeof(output_for_client)); //empty it

            // Read the output of the command
            if (fgets(output_for_client, MAX_BUFFER_LENGTH, check_existence) == NULL) {
                
                snprintf(message_to_client, sizeof(message_to_client), "No file found");

                if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }
            }
            else // if output is non=zero
            {
                memset(output_for_client, 0, sizeof(output_for_client)); //empty it
                snprintf(output_for_client, sizeof(output_for_client), "find %s -type f -not -wholename '*/[.]*' -size +%ldc -size -%ldc -exec tar -cf temp.tar.gz {} +", root, size1, size2);
                //printf("Command to fopen is: %s\n", output_for_client);

                // Open a pipe to the command
                if ((fp = popen(output_for_client, "r")) == NULL)
                {
                    perror("popen failed to run");
                    exit(EXIT_FAILURE);
                }

                // implement send message
                snprintf(message_to_client, sizeof(message_to_client), "temp.tar.gz");

                if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }
                pclose(fp);
            }

            pclose(check_existence);
        }
        else if(strstr(message, "w24ft ") == message) // 3 EXTENSIONS - WORKING
        {

            char extensions[MAX_EXTENSIONS][MAX_EXTENSION_LENGTH + 1];
            FILE *fp, *check_existence;
            char output_for_client[MAX_BUFFER_LENGTH], output_for_client_final[MAX_BUFFER_LENGTH], message_to_client[MAX_BUFFER_LENGTH];
            int numExtensions = 0;

            // Find the start of extension list
            char *extensionList = strstr(message, " ");
            char * root = "$HOME";
            extensionList++; // Move pointer past the space character

            // Extract extensions
            char extensionList2[MAX_BUFFER_LENGTH]; // Assuming MAX_BUFFER_LENGTH is large enough
            strcpy(extensionList2, message);
            char *token = strtok(extensionList2, " ");
            int i=0;
            while (token != NULL) {
                //printf("token is: %s\n", token);    
                token = strtok(NULL, " ");
                if(token==NULL)
                    break;
                if(i==0)
                    snprintf(output_for_client, sizeof(output_for_client), "find %s -type f -not -wholename '*/[.]*' \\( -name '*.%s'", root, token);
                else // add extensions to command
                    snprintf(output_for_client + strlen(output_for_client), sizeof(output_for_client) - strlen(output_for_client), " -o -name '*.%s'", token);
                i+=1;
            }

            char *ending1 = " \\)";
            snprintf(output_for_client + strlen(output_for_client), sizeof(output_for_client_final) - strlen(ending1), ending1);

            // adding TAR component to command
            char *ending2 = " -exec tar -cf temp.tar.gz {} +";
            snprintf(output_for_client_final, sizeof(output_for_client_final), output_for_client);
            snprintf(output_for_client_final + strlen(output_for_client_final), sizeof(output_for_client_final) - strlen(ending2), ending2);

            // Open a pipe to execute the command
            check_existence = popen(output_for_client, "r");

            if (check_existence == NULL) {
                perror("popen failed to run.");
                exit(EXIT_FAILURE);
            }

            memset(output_for_client, 0, sizeof(output_for_client)); //empty it

            // if output is NULL
            if (fgets(output_for_client, MAX_BUFFER_LENGTH, check_existence) == NULL) {
                snprintf(message_to_client, sizeof(message_to_client), "No file found");
                if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }
            }
            else{
                // Open a pipe to the command
                if ((fp = popen(output_for_client_final, "r")) == NULL)
                {
                    perror("popen failed to run");
                    exit(EXIT_FAILURE);
                }

                // implement send message
                snprintf(message_to_client, sizeof(message_to_client), "temp.tar.gz");

                if (send(client_fd, message_to_client, strlen(message_to_client), 0) == -1) {
                    perror("Send failed");
                    close(client_fd);
                    continue;
                }
                pclose(fp);
            }

            pclose(check_existence);
        }
        else{
            // Echo message back to client
            if (send(client_fd, message, strlen(message), 0) == -1) {
                perror("Send failed");
                close(client_fd);
                continue;
            }
        }
    }

    // Close client socket in the child process
    close(client_fd);

}
