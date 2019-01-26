#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 3030
#define CMD_SIZE 80
#define BUF_SIZE 2048
#define LAST_DIGIT 4

//Encrypts the clients message
void encrypt_cmd(char buffer[BUF_SIZE]){
    int i = 0;
    while(buffer[i] != '\0'){
        buffer[i] += LAST_DIGIT;
        i++;
    }
}

//Gets the stopping point of the next command
int get_cmd_stop(char buffer[BUF_SIZE], int start){
	int i = start;
	while(buffer[i] != ';' && i < CMD_SIZE){
		i++;
	}
	return i;
}

//Gets a subset of the buffer that has a single command between start and stop
void get_split_cmd(char buffer[BUF_SIZE], char temp[BUF_SIZE], int start, int stop){
	int i = start; int j = 0;
	while(i < stop){
		temp[j] = buffer[i];
		j++; i++;
	}
	temp[j] = '\0';
}

//Indicates the number of commands present in the buffer
int num_commands(char buffer[BUF_SIZE]){
    int i = 0; int cmd_count = 1;
    while(i < CMD_SIZE){
        if(buffer[i] == ';'){
            cmd_count++;
        }
        i++;
    }
    return cmd_count;
}

/* Used once a connection has been establish, runs until "quit" is entered*/
void socket_function(int sock,const char* host,const char* cmd){
    char buffer[BUF_SIZE] = {0};
    //Send initial cmd
    strcpy(buffer,cmd);
	encrypt_cmd(buffer);
    send(sock, buffer, sizeof(buffer),0);
    recv(sock, buffer, sizeof(buffer),0);
    printf("%s$\n\n%s\n", host, buffer);

    //Continue any additional commands until quit is sent.
	while(1){
    printf("%s$ ",host);
		fgets(buffer,CMD_SIZE,stdin);
        int cmd_count = num_commands(buffer);

        if(cmd_count == 1){
            if(strncmp(buffer,"quit",4)==0){
                encrypt_cmd(buffer);
                send(sock,buffer,sizeof(buffer),0);
                printf("client exiting\n");
                break;
            } else {
                encrypt_cmd(buffer);
                send(sock,buffer,sizeof(buffer),0);
                memset(buffer,0,sizeof(buffer));
                recv(sock, buffer, sizeof(buffer), 0);
                printf("\n%s\n",buffer);
            }
        } else {
            int start = 0; char saved_buffer[CMD_SIZE];
            strcpy(saved_buffer, buffer);
            while(cmd_count > 0){
                char temp_buf[BUF_SIZE] = {0};
                int stop = get_cmd_stop(saved_buffer,start);
                get_split_cmd(saved_buffer, temp_buf, start, stop);
                start = stop + 1;
                if(strncmp(temp_buf, "quit", 4) == 0){
                    encrypt_cmd(temp_buf);
                    send(sock,temp_buf, sizeof(temp_buf),0);
                    printf("client exiting\n");
                    exit(0);
                } else {
                    encrypt_cmd(temp_buf);
                    send(sock,temp_buf, sizeof(temp_buf),0);
                    memset(buffer,0,sizeof(buffer));
                    recv(sock, buffer, sizeof(buffer), 0);
                    printf("\n%s\n",buffer);
                    cmd_count--;
                }
            }
        }
	}
	exit(0);
}

int main(int argc, char const *argv[]){
  struct sockaddr_in address;
  int sock = 0;
  struct sockaddr_in serv_addr;

  //ensured command line arguments are present
  if(argc < 3){
	  printf("Error: program <host> <command>\n");
	  return 0;
  }

  const char * host = argv[1];
  const char * command = argv[2];

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
      printf("\n Socket creation error \n");
      return -1;
  }

  memset(&serv_addr, '\0', sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
	  perror("connection: ");
      return -1;
  } else {
	  printf("connected to server\n");
	  socket_function(sock, host, command);

  }
  return 0;
}
