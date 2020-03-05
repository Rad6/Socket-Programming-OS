#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <errno.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h> 
#include <sys/time.h>


#define TRUE       1 
#define FALSE      0  
#define BUFF_SIZE  1024
#define NAME_LEN   100
#define SA struct sockaddr
#define bool int


// ----- HAND SHAKING MESSAGES -----
#define NAME_ENTERED   "name_entered"
#define WAITING_FOR_NAME "waiting_for_name"
#define SHOW_ME_THE_LIST "show_me_the_list"
#define IDLE "idle"
#define WANT_TO_CHAT_WITH "want_to_chat_with"
#define LET_C_CONNECT "let_c_connect"
#define MAKE_ME_FREE "make_me_free"
#define MAKE_A_GP "make_a_gp"
#define CAN_I_JOIN "can_I_join"
#define HB_FROM "hb_from"
#define HERE_IS_LIST "here_is_list"
#define SEC_CHAT_NOT_VALID "sec_chat_not_valid"
#define TO_C_START_SEC "to_c_start_sec"
#define TO_S_LISTEN "to_s_listen"
#define JOIN_GP_NOT_VALID "join_gp_not_valid"
#define START_GP_CHAT "start_gp_chat"


int main_server;
char user_name[NAME_LEN];
char curr_gp_name[NAME_LEN];


typedef enum
{
	waiting_for_name,
	idle,
	waiting_for_list,
	waiting_to_start_chat,
	waiting_to_join_gp
} c_state;

typedef struct{
	int port;
	char name[NAME_LEN];
	bool online;
	bool busy;
} User;


void print(char* buf) {
    write(1, buf, strlen(buf));
}


char * delimit(char buff[], int which) {
    char *result;
    result = (char *) malloc (sizeof(char));
	int cnt = 0;
	for (int i = 0; i < strlen(buff); i++) {
		if (buff[i] == '$')
		{
			cnt += 1;
			continue;
		}
		if(cnt == which) 
			strncat(result, &buff[i], 1);
	}
    return (char *)result;	
}


char* itoa(int value, int base){
    static  char rep[] = "0123456789abcdef";
    static  char buf[50];
    char    *ptr;
    int     num;

    ptr = &buf[49];
    *ptr = '\0';
    num = value;
    if (value < 0 && base == 10)
        value *= -1;
    if (value == 0)
        *--ptr = rep[value % base];
    while (value != 0)
    {
        *--ptr = rep[value % base];
        value /= base;
    }
    if (num < 0 && base == 10)
        *--ptr = '-';
    return (ptr);
}


void send_hb() {
	char buff[BUFF_SIZE];
	memset(buff, '\0', sizeof(buff));
	strcat(buff, HB_FROM);
	strcat(buff, "$");
	strcat(buff, curr_gp_name);
	strcat(buff, "$");
	send(main_server, buff, BUFF_SIZE, 0);
	alarm(5);
}


void join_to_gp(int port) {

	char buff[BUFF_SIZE];
	int gp_fd, readval;
	struct sockaddr_in broadcast_addr, c_addr; 

	if((gp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creating failed");
        exit(EXIT_FAILURE);
    }
	memset(&broadcast_addr, '\0', sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(port);

    memset(&c_addr, '\0', sizeof(c_addr));
    c_addr.sin_family = AF_INET;
    c_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    c_addr.sin_port = htons(port);

	int opt = 1;
    if (setsockopt(gp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("set option error\n");
        close(gp_fd);
        exit(EXIT_FAILURE);
    }

	opt = 1;
    if (setsockopt(gp_fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0){
        perror("set option error\n");
        close(gp_fd);
        exit(EXIT_FAILURE);
    }
	if((bind(gp_fd, (struct sockaddr *) &broadcast_addr, sizeof(broadcast_addr))) < 0){
        perror("binding failed\n");
        exit(EXIT_FAILURE);
    }
	int broadcast_addr_len = sizeof(broadcast_addr);

    fd_set read_fds, write_fds;
    int max_fd = gp_fd;

    signal(SIGALRM, send_hb);
    alarm(1);

	print("\n------------ group chat ------------\n");
    while(TRUE){
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(gp_fd, &read_fds);
        FD_SET(0, &read_fds);
        FD_SET(main_server, &write_fds);
        FD_SET(1, &write_fds);
        if(((select(((main_server >= gp_fd) ? main_server : gp_fd) + 1, &read_fds, &write_fds, NULL, NULL)) < 0) && (errno!=EINTR))
			perror("select error\n");

        if(FD_ISSET(gp_fd, &read_fds)){
            if((readval = recvfrom(gp_fd, buff, sizeof(buff), MSG_WAITALL,
                            (struct sockaddr*)&c_addr, &broadcast_addr_len)) <= 0){ 
                print("Select error\n");
            }
            else{   
                buff[readval] = '\0';
				print(delimit(buff, 0)); print(" : "); print(delimit(buff, 1)); print("\n");
            }
        }
        if(FD_ISSET(0, &read_fds)){ 
            memset(buff, '\0', sizeof(buff));
            read(0, buff, BUFF_SIZE);
            if(buff[strlen(buff)-1] == '\n')
                buff[strlen(buff)-1] = '\0';

			if(strcmp(buff, "quit") == 0) {
				alarm(0);
				break;
			}

            char to_be_sent_buff [BUFF_SIZE];
            memset(to_be_sent_buff, 0, sizeof(to_be_sent_buff));
            strcat(to_be_sent_buff, user_name);
            strcat(to_be_sent_buff, "$");
            strcat(to_be_sent_buff, buff );
            strcat(to_be_sent_buff, "$");
			if((sendto(gp_fd, to_be_sent_buff, strlen(to_be_sent_buff), 0, 
					(struct sockaddr*)&broadcast_addr, broadcast_addr_len) <= 0))
				perror("ERRRROOR");
        }
    }
	memset(buff, '\0', sizeof(buff));
	strcat(buff, MAKE_ME_FREE);
	strcat(buff, "$");
	strcat(buff, user_name);
	strcat(buff, "$");
	send(main_server, buff, BUFF_SIZE, 0);
    alarm(0);
    close(gp_fd);
}


void run_pv_as_client(char server_name[NAME_LEN], int server_port) {

	char buff     [BUFF_SIZE];
	memset(buff, '\0',sizeof(buff));
	int tmp_client, readval;
	if((tmp_client = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("creating socket failed.\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(server_port);

	int address_len = sizeof(address);
	if(connect(tmp_client, (struct sockaddr *) &address, address_len)<0){
        perror("Couldn't connect to the other user as server\n");
        exit(EXIT_FAILURE);
    }
    print("\nconnected to port "); print(itoa(server_port,10)); print("\n");
    print("-------------- chatting to ");
	print(server_name);
    print(" --------------\n");

	fd_set read_fds;
    int max_fd = tmp_client;

    while(TRUE){
        FD_ZERO(&read_fds);
        FD_SET(tmp_client, &read_fds);
        FD_SET(0, &read_fds);
        
        if( ((select(max_fd + 1, &read_fds, NULL, NULL, NULL)) < 0) && (errno!=EINTR) ) 
			 perror("select error");

        if(FD_ISSET(tmp_client, &read_fds)){
            if((readval = read(tmp_client, buff, BUFF_SIZE)) == 0)
                print("The other client is gone\n");
            else{
                buff[readval] = '\0';
				if (buff[strlen(buff) - 1] == '\n')
					buff[strlen(buff) - 1] = '\0';
				
				if (strcmp(buff, "quit") == 0)
					break;
                print(server_name); print(" : "); print(buff); print("\n");
                }
            }
        
        if(FD_ISSET(0, &read_fds)){
            memset(buff, '\0', sizeof(buff));
            read(0, buff, BUFF_SIZE);
            if(buff[strlen(buff)-1] == '\n')
                buff[strlen(buff)-1] = '\0';

			if(strcmp(buff, "quit") == 0) {
				send(tmp_client, buff, BUFF_SIZE, 0);
				break;
			}
			send(tmp_client, buff, BUFF_SIZE, 0);
        }
	}

	memset(buff, '\0', sizeof(buff));
	strcat(buff, MAKE_ME_FREE);
	strcat(buff, "$");
	strcat(buff, user_name);
	strcat(buff, "$");
	send(main_server, buff, BUFF_SIZE, 0);
    close(tmp_client);

}

void run_pv_as_server(char c_name[NAME_LEN], int master_server_fd) {

	char buff[BUFF_SIZE];
	int tmp_server, readval;
	int opt = 1;
	if((tmp_server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("# ERROR with creating tcp socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in s_address, c_address;
    memset(&s_address, '\0', sizeof(s_address));
    s_address.sin_family = AF_INET;
    s_address.sin_addr.s_addr = htonl(INADDR_ANY);
    s_address.sin_port = htons(0);

	int s_address_len = sizeof(s_address);

	if( setsockopt(tmp_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) 
	{ 
		perror("setsockopt\n"); 
		exit(EXIT_FAILURE); 
	}
	if (bind(tmp_server, (struct sockaddr *)&s_address, sizeof(s_address))<0) 
	{ 
		perror("bind failed\n"); 
		exit(EXIT_FAILURE); 
	} 
	if (listen(tmp_server, 5) < 0) 
	{ 
		perror("listen failed\n"); 
		exit(EXIT_FAILURE); 
	} 

    int address_len = sizeof(c_address);
    getsockname(tmp_server, (struct sockaddr *) &c_address, (socklen_t *) &address_len);
    print("\nTemporary being a server with port : "); print(itoa(ntohs(c_address.sin_port), 10)); print("\n");

	char to_be_sent_buff[BUFF_SIZE];
	memset(to_be_sent_buff, '\0', sizeof(to_be_sent_buff));
	strcat(to_be_sent_buff, LET_C_CONNECT);
	strcat(to_be_sent_buff, "$");
	strcat(to_be_sent_buff, itoa(ntohs(c_address.sin_port), 10));
	strcat(to_be_sent_buff, "$");
	strcat(to_be_sent_buff, c_name);
	strcat(to_be_sent_buff, "$");
	send(master_server_fd, to_be_sent_buff, BUFF_SIZE, 0);
	// print("request sent to master\n");
	int new_socket;
    if((new_socket = accept(tmp_server, (struct sockaddr*) &c_address, 
                            (socklen_t *) (&address_len))) < 0){
                perror("accepting failed");
                exit(EXIT_FAILURE);
    }

    print("-------------- chatting to ");
	print(c_name);
    print(" --------------\n");

    fd_set read_fds;
    int max_fd = new_socket;

    while(TRUE){
        FD_ZERO(&read_fds);
        FD_SET(new_socket, &read_fds);
        FD_SET(0, &read_fds);
        
        if(((select(max_fd + 1, &read_fds, NULL, NULL, NULL)) < 0) && (errno!=EINTR))
        perror("select error");

        if(FD_ISSET(new_socket, &read_fds)){
            if((readval = read(new_socket, buff, BUFF_SIZE)) == 0)
                print("The other client is gone\n");
            
            else{
                buff[readval] = '\0';
                if (strcmp(buff, "quit") == 0)
					break;
                print(c_name); print(" : "); print(buff); print("\n");
                }
            }
        

        if(FD_ISSET(0, &read_fds)){
            memset(buff, '\0', sizeof(buff));
            read(0, buff, BUFF_SIZE);
            if(buff[strlen(buff)-1] == '\n')
                buff[strlen(buff)-1] = '\0';

            if(strcmp(buff, "quit") == 0 || strcmp(buff, "quit\n") == 0) {
				send(new_socket, buff, BUFF_SIZE, 0);
				break;
			}
			send(new_socket, buff, BUFF_SIZE, 0);
        }
	}

	memset(buff, '\0', sizeof(buff));
	strcat(buff, MAKE_ME_FREE);
	strcat(buff, "$");
	strcat(buff, user_name);
	strcat(buff, "$");
	send(main_server, buff, BUFF_SIZE, 0);
    close(new_socket);
}


void show_options () {
	print("------------------------------------\n");
	print("Enter 1 to see groups list\n");
	print("Enter 2 to join a group chat\n");
	print("Enter 3 to create a new group\n");
	print("Enter 4 to start a private chat\n");
	print("Enter 5 to quit\n\n");
	print("Enter quit to terminate a chat\n");
	print("------------------------------------\n");
}


void handle_client(int sockfd) 
{
	// '$' is the delimiter. First word is always state.
	char buff     [BUFF_SIZE];
	char tmp_servers_name[NAME_LEN];
	char user_port[20];

	c_state curr_state;

	memset(buff , '\0', sizeof(buff) );
	memset(user_name  , '\0', sizeof(user_name)  );
	memset(user_port, '\0', sizeof(user_port));
	
	int activity;
	fd_set read_fds;

	curr_state = waiting_for_name;
	print("Enter your name : \n"); 

	while(TRUE) {

		FD_ZERO(&read_fds);
		FD_SET(0, &read_fds);
		FD_SET(sockfd, &read_fds);

		activity = select(sockfd + 1, &read_fds, NULL, NULL, NULL);
		if ((activity < 0) && (errno!=EINTR)) 
			printf("select error"); 
		if (FD_ISSET(0, &read_fds))
		{	
            memset(buff, '\0', sizeof(buff));
            read(0, buff, BUFF_SIZE);

            if(buff[strlen(buff)-1] == '\n')
                buff[strlen(buff)-1] = '\0';

			if (curr_state == waiting_for_name)
			{
				char to_be_sent_buff[BUFF_SIZE];
				memset(to_be_sent_buff, '\0', sizeof(to_be_sent_buff));
				strcat(to_be_sent_buff, NAME_ENTERED); // current state
				strcat(to_be_sent_buff, "$");
				strcat(to_be_sent_buff, buff);
				strcat(to_be_sent_buff, "$");
				send(sockfd, to_be_sent_buff, BUFF_SIZE, 0);
				printf("You have entered : %s \n", buff);
				strcpy(user_name, buff);
				memset(buff, '\0', sizeof(buff));
				curr_state = idle;

				show_options();
			}
			else if (curr_state == idle)
			{
				if (strcmp(buff, itoa(1, 10)) == 0)
				{
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', sizeof(to_be_sent_buff));
					strcat(to_be_sent_buff, SHOW_ME_THE_LIST); // current state
					strcat(to_be_sent_buff, "$");
					send(sockfd, to_be_sent_buff, BUFF_SIZE, 0);
					memset(buff, '\0', sizeof(buff));
					curr_state = waiting_for_list;
				}

				if (strcmp(buff, itoa(2, 10)) == 0)
				{
					memset(buff, '\0', sizeof(buff));
					print("Enter the group name that u wanna join : ");
					curr_state = waiting_to_join_gp;
					read(0, buff, BUFF_SIZE);
					if (buff[strlen(buff) - 1] == '\n')
						buff[strlen(buff) - 1] = '\0';
					strcpy(curr_gp_name, buff);
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', sizeof(to_be_sent_buff));
					strcat(to_be_sent_buff, CAN_I_JOIN); // current state
					strcat(to_be_sent_buff, "$");
					strcat(to_be_sent_buff, buff);
					strcat(to_be_sent_buff, "$");
					send(sockfd, to_be_sent_buff, BUFF_SIZE, 0);
					memset(buff, '\0', sizeof(buff));
				}

				if (strcmp(buff, itoa(3, 10)) == 0)
				{
					print("Enter the group name u wanna create : ");
					memset(buff, '\0', sizeof(buff));
					read(0, buff, NAME_LEN);
                    if(buff[strlen(buff)-1] == '\n')
                        buff[strlen(buff)-1] = '\0';
					strcpy(curr_gp_name, buff);
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', sizeof(to_be_sent_buff));
					strcat(to_be_sent_buff, MAKE_A_GP); // current state
					strcat(to_be_sent_buff, "$");
					strcat(to_be_sent_buff, buff);
					strcat(to_be_sent_buff, "$");
					send(sockfd, to_be_sent_buff, BUFF_SIZE, 0);
					memset(buff, '\0', sizeof(buff));
					print("Done !\n");
					curr_state = idle;
					show_options();
				}

				else if (strcmp(buff, itoa(4, 10)) == 0)
				{
					memset(tmp_servers_name, '\0', sizeof(tmp_servers_name));
					print("Enter the user's name you want to chat with : ");
					read(0, tmp_servers_name, NAME_LEN);
                    if(tmp_servers_name[strlen(tmp_servers_name)-1] == '\n')
                        tmp_servers_name[strlen(tmp_servers_name)-1] = '\0';
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', sizeof(to_be_sent_buff));
					strcat(to_be_sent_buff, WANT_TO_CHAT_WITH); // current state
					strcat(to_be_sent_buff, "$");
					strcat(to_be_sent_buff, tmp_servers_name);
					strcat(to_be_sent_buff, "$");
					send(sockfd, to_be_sent_buff, BUFF_SIZE, 0);

					curr_state = waiting_to_start_chat;
				}
				if (strcmp(buff, itoa(5, 10)) == 0) {
					print("Goodbye!\n");
					break;
				}
			}
		}
		else if FD_ISSET(sockfd, &read_fds) {
			memset(buff, '\0', sizeof(buff));
            recv(sockfd, buff, BUFF_SIZE, 0);

            if(buff[strlen(buff)-1] == '\n')
                buff[strlen(buff)-1] = '\0';

			if ((curr_state == waiting_for_list) && (strcmp(delimit(buff, 0), HERE_IS_LIST) == 0))
			{	
				int i = 1;
				while (TRUE)
				{
					if (strcmp(delimit(buff, i), "") == 0)
						break;
					print(delimit(buff, i));
					print("\n");
					i += 1;
				}
				curr_state = idle;
				show_options();
			}
			if (curr_state == waiting_to_start_chat)
			{
				if (strcmp(delimit(buff, 0), SEC_CHAT_NOT_VALID) == 0)
					print("user not accessible right now.\n");
				else
				{
					if (strcmp(delimit(buff, 0), TO_C_START_SEC) == 0)
					{
						int s_port = atoi(delimit(buff, 1));
						print("the port is: ");print(itoa(s_port,10));print("\n");
						run_pv_as_client(tmp_servers_name, s_port);
					}					
				}
				curr_state = idle;
				show_options();
			}
			if (curr_state == idle && (strcmp(delimit(buff, 0), TO_S_LISTEN) == 0))
			{
				char c_name[NAME_LEN];
				memset(c_name, '\0', sizeof(c_name));
				strcpy(c_name, delimit(buff, 1));
				run_pv_as_server(c_name, sockfd);
				curr_state = idle;
				show_options();
			}
			if (curr_state == waiting_to_join_gp)
			{
				if (strcmp(delimit(buff, 0), JOIN_GP_NOT_VALID) == 0)
				{
					print("group not found.\n");
				}
				else if(strcmp(delimit(buff, 0), START_GP_CHAT) == 0)
				{
					join_to_gp(atoi(delimit(buff,1)));
				}
				curr_state = idle;
				show_options();
			}
		}
		else{
			printf("select errorrrrr");
			exit(-1);
		}
	}
} 


int main(int argc , char *argv[]) 
{ 
	int port = atoi(argv[1]);
	int sockfd, connfd; 
	struct sockaddr_in servaddr, cli; 

	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("socket creation failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully created..\n"); 

	bzero(&servaddr, sizeof(servaddr)); 

	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = INADDR_ANY; 
	servaddr.sin_port = htons(port); 

	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) { 
		printf("connection with the server failed...\n"); 
		exit(0); 
	} 
	else
		printf("connected to the server..\n"); 
	char portt[20];
	main_server = sockfd;
	handle_client(sockfd); 

	close(sockfd); 
} 
