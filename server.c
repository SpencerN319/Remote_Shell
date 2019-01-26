#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define PORT 3030
#define CMD_SIZE 80
#define BUF_SIZE 2048
#define LAST_DIGIT 4
#define MAX 10
#define READY 0
#define TAKEN 1

struct Memory {
	int status;
	int data[MAX];
};

//Removes a specified child PID from shared data
void remove_childPID(int child_pid, struct Memory *ShmPTR){
	while(ShmPTR->status == TAKEN);
	ShmPTR->status = TAKEN;
	int i = 0;
	while(i < MAX){
		if(ShmPTR->data[i] == child_pid){
			ShmPTR->data[i] = 0;
			printf("Removed PID: %d\n", child_pid);
			break;
		}
		i++;
	}
	ShmPTR->status = READY;
}

//Adds a specified child PID to shared data
void add_childPID(int child_pid, struct Memory *ShmPTR){
	while(ShmPTR->status == TAKEN);
	ShmPTR->status = TAKEN;
	int i = 0;
	while(i < MAX){
		if(ShmPTR->data[i] == 0){
			ShmPTR->data[i] = child_pid;
			printf("Added PID: %d\n", child_pid);
			break;
		}
		i++;
	}
	ShmPTR->status = READY;
}

//Decrpyts the recieved message from client
void decrypt_cmd(char buffer[BUF_SIZE]){
	int i = 0;
	while(buffer[i] != '\0'){
		buffer[i] -= LAST_DIGIT;
		i++;
	}
}

/* parses the given buffer into tokens */
char** parse(char* line){
	char **cmds = malloc(sizeof(char*));
	if(cmds){
		size_t n = 0;
		char* tok = strtok(line," ");
		while(tok){
			cmds[n] = tok;
			tok = strtok(NULL," ");
			n++;
		}
		int m = 0;
		while(cmds[n-1][m] != '\n'){
			m++;
		}
		cmds[n-1][m] = '\0';
	}

	return cmds;
}

//Adds a given command to history
void add_history(char history[MAX][CMD_SIZE], char buffer[BUF_SIZE], int count){
	int i = 0;
	while(i < CMD_SIZE){
		history[count][i] = 0;
		i++;
	}
	strcpy(history[count], buffer);
}

//Retrieves the entire history from client
void get_history(char history[MAX][CMD_SIZE], char buffer[BUF_SIZE], int count, int total){
	int i = 0;
	while(i < BUF_SIZE){
		buffer[i] = 0;
		i++;
	}
	int buf_count = 0;
	// start -> 0
	for(int i = count; i >= 0; i--){
		for(int j = 0; j < CMD_SIZE; j++){
			if(history[i][j] != '\0'){
				buffer[buf_count] = history[i][j];
				buf_count++;
			} else {
				break;
			}
		}
	}
	if(total > 9){
		//size -> start
		for(int i = MAX-1; i > count; i--){
			for(int j = 0; j < CMD_SIZE; j++){
				if(history[i][j] != '\0'){
					buffer[buf_count] = history[i][j];
					buf_count++;
				} else {
					break;
				}
			}
		}
	}
}

//Gets the specified command from !N if N exists
int get_n_command(char history[MAX][CMD_SIZE],char buffer[BUF_SIZE],int total,int n){
	int i = 0;
	while(i < BUF_SIZE){
		buffer[i] = 0;
		i++;
	}
	if(n == -1 || n-1 > total){
		char* n_error = "No such command in history";
		strcpy(buffer, n_error);
		return -1;
	} else {
		int cmd = n-1;
		strcpy(buffer,history[cmd]);
		return 0;
	}
}

//Retrieves the last command entered from client
void get_last_command(char history[MAX][CMD_SIZE],char buffer[BUF_SIZE], int count, int total){
	int j = 0;
	while(j < BUF_SIZE){
		buffer[j] = 0;
		j++;
	}
	int cmd;
	if(count == 0 && total > 9){
		cmd = 9;
	} else {
		cmd = count - 1;
	}
	strcpy(buffer,history[cmd]);
}

//Gets the value of N and returns it as an integer
int get_n(char buffer[BUF_SIZE]){
	if(buffer[2] == '0'){
		if(buffer[1] == '1'){
			return 10;
		} else {
			return -1;
		}
	} else {
		return buffer[1] - '0';
	}
}

//Gets all process IDs from live processes in shared memory
void get_jobs(int child_pid, struct Memory *ShmPTR, char buffer[BUF_SIZE]){
	int i = 0;
	while(i < BUF_SIZE){
		buffer[i] = 0;
		i++;
	}
	int pid_count = 0;
	while(ShmPTR->status == TAKEN);
	ShmPTR->status = TAKEN;
	i = 0;
	while(pid_count < MAX){
		if(ShmPTR->data[pid_count] > 0){
			char str[15] = {0};
			sprintf(str,"%s%d%s%d","PID ", pid_count+1, ": ", ShmPTR->data[pid_count]);
			printf("%s\n",str);
			int j = 0;
			while(str[j] != '\0'){
				buffer[i] = str[j];
				i++; j++;
			}
			//Indicating what jobs is your PID
			if(ShmPTR->data[pid_count] == child_pid){
				buffer[i] = ' '; i++; buffer[i] = '<'; i++; buffer[i] = '-'; i++;
			}
			buffer[i] = '\n';
			i++;
		}
		pid_count++;
	}
	ShmPTR->status = READY;
}

/* Once a new socket connection is established it runs until quit is entered from client */
void socket_function(int new_socket){
	char buffer[BUF_SIZE] = {0};
	char history[MAX][CMD_SIZE] = {0};    int hist_count = 0;
	int has_history = 0; int total = 0;

	//Shared Memory initialization
	key_t ShmKEY; int ShmID; struct Memory *ShmPTR;

	ShmKEY = ftok(".",'x');
	ShmID = shmget(ShmKEY, sizeof(struct Memory), 0666);
	if(ShmID < 0){
		printf("shmget error (client)\n");
		exit(1);
	}
	printf("Client has recieved 10 integer shared memory\n");

	ShmPTR = (struct Memory *) shmat(ShmID, NULL, 0);
    if ((int) ShmPTR == -1) {
        printf("*** shmat error (client) ***\n");
        exit(1);
    }
    printf("Client has attached the shared memory...\n");

	while(1){
		recv(new_socket, buffer, sizeof(buffer), 0);
		decrypt_cmd(buffer);

		if(strncmp(buffer,"quit",4)==0){
			break;
		} else if(strncmp(buffer, "jobs", 4)==0){
			add_history(history, buffer, hist_count); hist_count++; total++;
			if(hist_count >= MAX){
				hist_count = hist_count % MAX;
			}
			get_jobs(getpid(),ShmPTR,buffer);
			send(new_socket, buffer, sizeof(buffer),0);
		} else if(strncmp(buffer,"History", 7)==0 && has_history == 1){
			add_history(history, buffer, hist_count); hist_count++; total++;
			if(hist_count >= MAX){
				hist_count = hist_count % MAX;
			}
			get_history(history, buffer, hist_count, total);
			send(new_socket, buffer, sizeof(buffer),0);
		} else if(strncmp(buffer, "!!", 2)==0){
			if(has_history){
				get_last_command(history, buffer, hist_count, total);
				add_history(history, buffer, hist_count); hist_count++; total++;
				if(hist_count >= MAX){
					hist_count = hist_count % MAX;
				}
				char** cmds = parse(buffer);
				if(fork() == 0){
					dup2(new_socket, STDOUT_FILENO);
					dup2(new_socket, STDERR_FILENO);
					if(execvp(cmds[0], cmds) < 0){
						perror("Exec: ");
						exit(1);
					}
	        memset(buffer,0,sizeof(buffer));
					exit(0);
				} else{
					wait(NULL);
					fflush(stdout);
				}
				free(cmds);
			}
		} else if(buffer[0] == 33){
			//The Nth command number after ! should be executed
			int n = get_n(buffer);
			char temp[80] = {0};
			strcpy(temp,buffer);
			if((get_n_command(history, buffer, total, n)) == 0){
				if(has_history){
					add_history(history, buffer, hist_count); hist_count++; total++;
					if(hist_count >= MAX){
						hist_count = hist_count % MAX;
					}
					char** cmds = parse(buffer);
					if(fork() == 0){
						dup2(new_socket, STDOUT_FILENO);
						dup2(new_socket, STDERR_FILENO);
						if(execvp(cmds[0], cmds) < 0){
							perror("Exec: ");
							exit(1);
						}
		        memset(buffer,0,sizeof(buffer));
						exit(0);
					} else{
						wait(NULL);
						fflush(stdout);
					}
					free(cmds);
				}
			} else{
				add_history(history, temp, hist_count); hist_count++; total++;
				send(new_socket,buffer,sizeof(buffer),0);
			}
		} else {
			add_history(history, buffer, hist_count); hist_count++; total++;
			if(hist_count >= MAX){
				hist_count = hist_count % MAX;
			}
			has_history = 1;
			char** cmds = parse(buffer);
			if(fork() == 0){
				dup2(new_socket, STDOUT_FILENO);
				dup2(new_socket, STDERR_FILENO);
				if(execvp(cmds[0], cmds) < 0){
					perror("Exec: ");
					exit(1);
				}
        memset(buffer,0,sizeof(buffer));
				exit(0);
			} else{
				int ppid = getppid();
				wait(NULL);

				fflush(stdout);
			}
			free(cmds);
		}
		memset(buffer, 0, sizeof(buffer));
	}

	remove_childPID(getpid(), ShmPTR);
	shmdt((void*) ShmPTR);
	printf("%s%d%s\n","Client ",getpid()," has detatched shared memory");
	printf("Client %d exited\n", getpid());
	exit(0);
}

int main(int argc, char const *argv[]){
	key_t ShmKEY; int ShmID; struct Memory *ShmPTR;
	//CREATE AND INITIALIZE SHARED MEMORY FOR CHILD PIDS
	ShmKEY = ftok(".",'x');
	ShmID = shmget(ShmKEY, sizeof(struct Memory), IPC_CREAT | 0666);
	if(ShmID < 0){
		printf("shmget error (server)\n");
		exit(1);
	}
	printf("Server has created 10 integer shared memory\n");

	ShmPTR = (struct Memory *) shmat(ShmID, NULL, 0);
	if((int) ShmPTR == -1){
		printf("shmat error (server)\n");
		exit(1);
	}
	printf("Server has attached shared memory\n");
	ShmPTR->status = TAKEN;
	//clear the shared data if not cleared
	for(int i = 0; i < MAX; i++){
		if(ShmPTR->data[i] > 0){
			ShmPTR->data[i] = 0;
		}
	}
	ShmPTR->status = READY;

	int server_fd, new_socket;
	struct sockaddr_in serv_addr;
	int opt = 1;
	int addrlen = sizeof(serv_addr);

	  //created socket
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
	    perror("socket failed");
	    exit(EXIT_FAILURE);
	}
	printf("Server socket created.\n");
	memset(&serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons( PORT );

	  //bind socket
	if (bind(server_fd, (struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
	    perror("bind failed");
	    exit(EXIT_FAILURE);
	}

	  //listen for connections
	if (listen(server_fd, 10) == 0){
	    printf("listening....\n");
	}


	do {
		if ((new_socket = accept(server_fd, (struct sockaddr *)&serv_addr,(socklen_t*)&addrlen))<0){
			perror("accept: ");
			exit(EXIT_FAILURE);
		}
		else {
	 		int pid = fork();
			if(pid == 0){
				//child
				printf("client connected: %d\n", getpid());
				close(server_fd);
				socket_function(new_socket);
				exit(0);
				return 0;
			} else {
				add_childPID(pid, ShmPTR);
				//ZOMBIE PREVENTION
				signal(SIGCHLD,SIG_IGN);
			}
		}

		close(new_socket);
		listen(server_fd, 10);

	} while(1);
	printf("closing server\n");
	shmdt((void *) ShmPTR);
	printf("Server has detatched shared memory\n");
	shmctl(ShmID, IPC_RMID, NULL);
	printf("Server has removed its shared memory\n");
	close(server_fd);

	return 0;
}
