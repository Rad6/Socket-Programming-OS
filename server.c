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
#include <time.h>
#include <sys/time.h>


#define TRUE         1 
#define FALSE        0 
#define MAX_CLIENTS  100
#define BUFF_SIZE    1024
#define NAME_LEN     100
#define MAX_GP_USERS 10
#define MAX_GP       10
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


typedef struct{
	int port;
	int fd;
	char name[NAME_LEN];
	bool online;
	bool busy;
} User;


typedef struct{
	char name[NAME_LEN];
	User joined_users[MAX_GP_USERS];
	int mems_cnt;
	bool in_use;
	int port;
	long last_hb;
} Group;


int gp_cnt = 0;
User users[MAX_CLIENTS];
Group groups[MAX_GP];


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


void eliminate_groups() {
	for (int i = 0; i < gp_cnt; i++)
	{
		if (groups[i].in_use == TRUE && ((time(NULL) - groups[i].last_hb) > 30 ) )
		{
			print("group \"");print(groups[i].name);print("\" is eliminated.\n");
			groups[i].in_use = FALSE;
		}
	}
	alarm(30);
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


int main(int argc , char *argv[]) 
{ 
	int port = atoi(argv[1]);
	int opt = 1; 
	int master_socket , addrlen , new_socket ,client_socket[MAX_CLIENTS],
		activity, i , valread , sd, max_sd; 
	struct sockaddr_in s_address, c_address;

	memset(users, 0, sizeof(users));
	memset(groups, 0, sizeof(groups));
	
	char buffer[BUFF_SIZE];
	memset(buffer, '\0', sizeof(buffer));
	fd_set readfds, writefds; 
		
	for (i = 0; i < MAX_CLIENTS; i++) 
		client_socket[i] = 0; 
		
	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
	{ 
		perror("socket failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) 
	{ 
		perror("setsockopt"); 
		exit(EXIT_FAILURE); 
	} 
	
	s_address.sin_family = AF_INET; 
	s_address.sin_addr.s_addr = INADDR_ANY; 
	s_address.sin_port = htons( port ); 
		
	if (bind(master_socket, (struct sockaddr *)&s_address, sizeof(s_address))<0) 
	{ 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	print("Listening on port ");
	print(itoa(port, 10));
	print(" ...\n"); 
		
	if (listen(master_socket, 5) < 0) 
	{ 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	} 
		
	addrlen = sizeof(c_address); 
	signal(SIGALRM, eliminate_groups);
	alarm(1);
	while(TRUE) 
	{ 
		FD_ZERO(&readfds); 
		FD_ZERO(&writefds);
		FD_SET(master_socket, &readfds);
		FD_SET(1, &writefds);
		max_sd = master_socket; 
			
		for ( i = 0 ; i < MAX_CLIENTS ; i++) 
		{ 
			sd = client_socket[i]; 
			if(sd > 0) 
				FD_SET( sd , &readfds); 
			if(sd > max_sd) 
				max_sd = sd; 
		} 
		activity = select( max_sd + 1 , &readfds , &writefds , NULL , NULL); 
	
		if ((activity < 0) && (errno!=EINTR)) 
			print("select error"); 
			
		if (FD_ISSET(master_socket, &readfds)) 
		{ 
			if ( (new_socket = accept(master_socket, (struct sockaddr *)&c_address, (socklen_t*)&addrlen)) < 0 ) 
			{ 
				perror("accept"); 
				exit(EXIT_FAILURE); 
			} 
			User user;
			user.fd = new_socket;
			user.port = ntohs(c_address.sin_port);
			user.online = TRUE;
			user.busy = TRUE;
			print("A user has connected on port: ");
			print(itoa(ntohs(c_address.sin_port), 10));
			print("\n");

			for (i = 0; i < MAX_CLIENTS; i++) 
				if( client_socket[i] == 0 ) 
				{ 
					client_socket[i] = new_socket; 
					users[i] = user; 
					break; 
				}  
		} 
			
		for (i = 0; i < MAX_CLIENTS; i++) 
		{
			sd = client_socket[i]; 
				
			if (FD_ISSET( sd , &readfds)) 
			{
				if ((valread = read( sd , buffer, BUFF_SIZE)) == 0) 
				{ 
					getpeername(sd , (struct sockaddr*)&s_address , (socklen_t*)&addrlen); 
					print("client disconnected on port ");
					print(itoa(ntohs(s_address.sin_port), 10));
					print("\n");
					close( sd ); 
					client_socket[i] = 0;
					memset(&users[i], 0, sizeof(User));
				} 
					
				else if (strcmp(delimit(buffer, 0), NAME_ENTERED) == 0)
				{	
					strcpy(users[i].name, delimit(buffer, 1));
					users[i].busy = FALSE;
					print("User \"");
					print(users[i].name);
					print("\" added.\n");
				}
				else if (strcmp(delimit(buffer, 0), SHOW_ME_THE_LIST) == 0) {
					memset(buffer, '\0', BUFF_SIZE);
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', BUFF_SIZE);
					strcat(to_be_sent_buff, HERE_IS_LIST); // current state
					strcat(to_be_sent_buff, "$");

					for (int i = 0; i < MAX_GP; i++)
						if (groups[i].name != "")
						{
							strcat(to_be_sent_buff, groups[i].name);
							strcat(to_be_sent_buff, "$");
						}
					send(sd, to_be_sent_buff, BUFF_SIZE, 0);
					memset(to_be_sent_buff, '\0', BUFF_SIZE);
				}
				else if (strcmp(delimit(buffer, 0), WANT_TO_CHAT_WITH) == 0)
				{
					bool is_valid = FALSE;
					int tmp_server_fd;
					char c_name[NAME_LEN];
					for (int i = 0; i < MAX_CLIENTS; i++)
						if (strcmp(delimit(buffer, 1), users[i].name) == 0 && users[i].busy == FALSE)
						{
							is_valid = TRUE;
							users[i].busy = TRUE;
							tmp_server_fd = users[i].fd;
							for (int j = 0; j < MAX_CLIENTS; j++)
								if (users[j].fd == sd) {
									users[j].busy = TRUE;
									strcpy(c_name, users[j].name);
									break;
								}
							break;
						}
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', BUFF_SIZE);
					if (is_valid) {
						strcat(to_be_sent_buff, TO_S_LISTEN);
						strcat(to_be_sent_buff, "$");
						strcat(to_be_sent_buff, c_name);
						strcat(to_be_sent_buff, "$");
						
						send(tmp_server_fd, to_be_sent_buff, BUFF_SIZE, 0);
					}
					else {
						strcat(to_be_sent_buff, SEC_CHAT_NOT_VALID);
						strcat(to_be_sent_buff, "$");
						send(sd, to_be_sent_buff, BUFF_SIZE, 0);
					}
					
					memset(to_be_sent_buff, '\0', BUFF_SIZE);
				}
				else if (strcmp(delimit(buffer, 0), LET_C_CONNECT) == 0) {
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', BUFF_SIZE);
					strcat(to_be_sent_buff, TO_C_START_SEC);
					strcat(to_be_sent_buff, "$");
					strcat(to_be_sent_buff, delimit(buffer, 1));
					strcat(to_be_sent_buff, "$");
					for (int i = 0; i < MAX_CLIENTS; i++)
						if (strcmp(delimit(buffer, 2), users[i].name) == 0)
						{
							send(users[i].fd, to_be_sent_buff, BUFF_SIZE, 0);
							memset(to_be_sent_buff, '\0', BUFF_SIZE);
						}
				}
				else if (strcmp(delimit(buffer, 0), MAKE_ME_FREE) == 0) {
					for (int i = 0; i < MAX_CLIENTS; i++)
						if (strcmp(delimit(buffer, 1), users[i].name) == 0)
						{
							print(users[i].name); print(" is no more busy.\n");
							users[i].busy = FALSE;
							break;
						}
				}else if (strcmp(delimit(buffer, 0), MAKE_A_GP) == 0) {
					Group new_group;
					strcpy(new_group.name, delimit(buffer, 1));
					new_group.in_use = FALSE;
					new_group.mems_cnt = 0;
					new_group.port = gp_cnt + 50000; //for being large enough
					groups[gp_cnt] = new_group;
					gp_cnt++;
					print("group ");print("\"");print(new_group.name);print("\"");
					print(" is added on port ");print(itoa(new_group.port, 10));print("\n");
				}
				else if (strcmp(delimit(buffer, 0), CAN_I_JOIN) == 0) {
					int valid = FALSE;
					int gp_port;
					for (int i = 0; i < gp_cnt; i++)
						if (strcmp(delimit(buffer,1), groups[i].name) == 0)
						{
							valid = TRUE;
							gp_port = groups[i].port;
							groups[i].in_use = TRUE;
							for (int j = 0; j < MAX_CLIENTS; j++)
								if (users[j].fd == sd)
								{
									users[j].busy = TRUE;
									print("User \"");print(users[j].name);
									print("\" joined to group \"");print(groups[i].name);
									print("\"\n");
									groups[i].joined_users[groups[i].mems_cnt++] = users[j];
									break;
								}
							break;
						}
					char to_be_sent_buff[BUFF_SIZE];
					memset(to_be_sent_buff, '\0', BUFF_SIZE);
					if (valid == 0)
					{
						strcat(to_be_sent_buff, JOIN_GP_NOT_VALID);
						strcat(to_be_sent_buff, "$");
						send(sd, to_be_sent_buff, BUFF_SIZE, 0);
					}
					else
					{
						strcat(to_be_sent_buff, START_GP_CHAT);
						strcat(to_be_sent_buff, "$");
						strcat(to_be_sent_buff, itoa(gp_port, 10));
						strcat(to_be_sent_buff, "$");
						send(sd, to_be_sent_buff, BUFF_SIZE, 0);
					}
										
				}
				else if (strcmp(delimit(buffer, 0), HB_FROM) == 0) {
					print("new heartbeat from group \"");
					print(delimit(buffer, 1));print("\"\n");
					for (int i = 0; i < gp_cnt; i++)
						if (strcmp(delimit(buffer, 1), groups[i].name) == 0)
							groups[i].last_hb = time(NULL);
				}
				else
					print("Shiiiiiitttt");
			} 
		} 
	} 
	close(sd);
	alarm(0);
	return 0; 
} 
