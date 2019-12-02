/* Project 4 template
 * Please check the comments and hints, and fill the codes.
 * You only need to write your code in this file, and you do not 
 * need to write the server and client sides in two files. 
 *
 * You also need to modify the code from project 3 accordingly to 
 * make it adapted to the network communication. 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#define SIZE 10


// you can change the struct as you like
struct move{
    char letter;
    int number;
    char state[20];
    char ship[20];
    int player;
    struct move *next;
};

char ipAddress[200], port[200];
int ourSocket,listenSocket;


void generateShip(char** board, int size, char letter) {
	int noGood = 1;
	int orientation , row, col;
    int curRow, curCol;
	while (noGood) {
		orientation = random() %2;
		if (orientation == 0) { // Horizontal
			row = random() % 10;
			col = random() % (10 - size);
		} else {
			row = random() % (10 - size);
			col = random() % 10;
		}
		int noObstructions = 1;
		for (int i=0;i<size;i++) {
			curRow = row, curCol = col;
			if (orientation == 0) { // Horizontal
				curCol += i;
			} else {
				curRow += i;
			}
			if (board[curRow][curCol] != '-') 
                noObstructions = 0;	
		}
		if (noObstructions == 0) 
            continue;
		noGood = 0;
	}
	printf ("Ship %c at %c%d-%c%d, orientation = %s\n",letter, row+65, col,curRow+65, curCol, orientation==0?"horizontal" : "vertical");
	for (int i=0;i<size;i++) {
		curCol = col, curRow = row;
		if (orientation == 0) { // Horizontal
			curCol += i;
		} else {
			curRow += i;
		}
		board[curRow][curCol] = letter;
	}
}

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void createSendingSocket() {
    /*write the code here, you can refer to the lab9 handout */
    
	int numbytes;
    char buf[30];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo(ipAddress, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((ourSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        
        if (connect(ourSocket, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(ourSocket);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo); // all done with this structure

    //Make the socket non-blocking
    //fcntl(ourSocket, F_SETFL, O_NONBLOCK);
}


void createListenSocket() {
    /*write the code here, you can refer to the lab9 handout */

	printf("\nListening Socket Creation\n");

	int new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

	int backlog = 10;
    
    if ((rv = getaddrinfo("127.0.0.1", port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((listenSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(listenSocket, p->ai_addr, p->ai_addrlen) == -1) {
            close(listenSocket);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    
	if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(listenSocket, backlog) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    sin_size = sizeof their_addr;
    new_fd = accept(listenSocket, (struct sockaddr *)&their_addr, &sin_size);
    
    if (new_fd == -1) {
        perror("accept");
    }
    
    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    
    printf("server: got connection from %s\n", s);

    close(listenSocket);

    listenSocket = new_fd;

    //Make the socket non-blocking
    //fcntl(listenSocket, F_SETFL, O_NONBLOCK);
}



char** initialization(char ***ourMoveBoard){
	if (ipAddress[0] == 0){
        printf("create listen socket");
        /*add function call of create listen socket*/
        createListenSocket();
    }
	else{
        printf("create sending socket");
        /*add function call of create sending socket*/
        createSendingSocket();
    }

	int i, j;
	char **board = (char**)malloc(sizeof(char*)*SIZE);
    *ourMoveBoard = (char**)malloc(sizeof(char*)*SIZE);
	for (i = 0; i < SIZE; i++){
		board[i] = (char*)malloc(sizeof(char)*SIZE);
        (*ourMoveBoard)[i] = (char*)malloc(sizeof(char)*SIZE);
	}
	for(i = 0; i < SIZE; i++){
		for(j = 0; j < SIZE; j++){
			board[i][j] = '-';
			(*ourMoveBoard)[i][j] = '-';
		}
	}

	generateShip(board, 2,'D');
	generateShip(board, 3,'S');
	generateShip(board, 3,'C');
	generateShip(board, 4,'B');
	generateShip(board, 5,'R');
    return board;
}



void insert_move(struct move **head, struct move **tail,struct move *temp){
     if (*head == NULL){
        /* List is currently empty. */
        *head = *tail = temp;
    }
    else{
        (*tail)->next = temp;
        *tail = (*tail)->next;
    }
}

void update_state(char* state, char ** board, struct move** head,struct move** tail, struct move** temp){
	int row, i, j;
    char letter = (*temp)->letter;
    int col = (*temp)->number;
    row = letter % 65;
    if(board[row][col] == '-'){
        strcpy(state, "MISS");
        strcpy((*temp)->state, "MISS");
        strcpy((*temp)->ship, "  ");
        board[row][col]='O';
    }
    else{
        strcpy(state, "HIT");
        strcpy((*temp)->state, "HIT!");
        switch (board[row][col]){
            case 'C':  strcpy((*temp)->ship, "Crusier"); break;
            case 'R':  strcpy((*temp)->ship, "Carrier"); break;
            case 'B':  strcpy((*temp)->ship, "Battleship"); break;
            case 'S':  strcpy((*temp)->ship, "Submarine"); break;
            case 'D':  strcpy((*temp)->ship, "Destroyer"); break;
        }
        board[row][col]='X';
    }

    insert_move(head,tail,*temp);
    int counter = 0;
    for(i=0; i < SIZE; i++){
        for(j=0; j < SIZE; j++){
            if(board[i][j] == '-' || board[i][j] == 'X')
                counter += 1;
        }
    }
    if(counter == SIZE * SIZE)
        strcpy(state, "GAME OVER!");
}

struct move* accept_input(){
    char letter;
    int number;
    bool flag = true;
    do{
        printf("Enter a letter A-J and number 0-9 ex. B4 - enter Z0 to end\n");
        int size = scanf(" %c%d", &letter, &number);
        if(size != 2){
            printf("INVALID INPUT\n");
            continue;
        }
        letter = toupper(letter);
        if(letter == 'Z' && number == 0)
            break;
        if (letter < 65 || letter > 74)
            printf("INVALID INPUT\n");
        else if (number <0 || number >9)
            printf("INVALID INPUT\n");
        else
            flag = false;
	}while(flag);

    struct move *temp;
    temp = (struct move *)malloc(sizeof(struct move));
    temp->letter = letter;
    temp->number = number;
    return temp;
}

void display_state(char* state, char** board, char** ourMoveBoard){
	int i, j;
    printf("  **** %s ****             **** Your Ships ****\n", state);

	printf("  0 1 2 3 4 5 6 7 8 9               0 1 2 3 4 5 6 7 8 9\n");
	for (i = 0; i < SIZE; i++){
        printf("%c ", 65+i);
		for (j = 0; j < SIZE; j++){
			printf("%c ", ourMoveBoard[i][j]);
		}
        
        printf("            %c ", 65+i);
		for (j = 0; j < SIZE; j++){
			printf("%c ", board[i][j]);
		}
		printf("\n");
	}
}

int teardown(char ** board,struct move* head){
	int i;
    struct move* temp;
	for(i = 0; i < SIZE; i++)
		free(board[i]);
	free(board);
	FILE *fptr;

    fptr = fopen("log.txt", "w");
     if (fptr == NULL)
     {
         exit(-1);
     }

    
     if (head==NULL){
         printf("The list is empty");
     }
     else{
         while (head != NULL){
            fprintf(fptr, "Fired at %c%d %s %s \n", head->letter, head->number, head->state, head->ship);
            temp = head;
            head = head->next;
            free(temp);
         }
     }
     fclose(fptr);
	return 0;
}




void main(int argc, char **argv) {
	if (argc != 3 && argc != 2) { printf ("usage: battleship [ipaddress] port\n"); return; }
	if (argc == 3) {
        // if there are two command line arguments, where
        // first is the ipaddress and 
        // second is the port, then we initialize 
        // the client side in initialization() function
		strcpy(ipAddress,argv[1]);
		strcpy(port,argv[2]);
	}
	else {
        // if there is only one command line argument, 
        // then we initialize the server side in initialization
        // function
		memset(ipAddress,0,200);
		strcpy(port,argv[1]);
	}

	srand(time(NULL));
	char **board, **ourMoveBoard;
    char state[] = "GAME START";
    char flag[] = "GAME OVER!";

    struct move *head, *tail, *ourMove, *theirMove;
    head = tail = NULL;
    
    /*modify the initialization function */
	board = initialization(&ourMoveBoard);

    char buff[20];
    char ourMoveStateAndShip[40];
    char theirMoveStateAndShip[40];

	do{
        //ourMove = accept_input();
        
        if(ipAddress[0] != 0){
		    display_state(state, board, ourMoveBoard);
            //make move
		    ourMove = accept_input();
            ourMove->player = 1;
            char num[2];

            sprintf(num, "%d", ourMove->number);
            //num = (ourMove->number);

            printf("The number is %s", num);
            buff[0] = ourMove->letter;
            buff[1] = ' ';
            buff[2] = num[0];

            //send move
            send(ourSocket, buff, sizeof(buff), 0);
            printf("Sent %s\n", buff);

            //wait for state
            recv(ourSocket, ourMoveStateAndShip, sizeof(ourMoveStateAndShip), 0);
            printf("Received %s\n", ourMoveStateAndShip);
            
            printf("The state was: %s", strtok(ourMoveStateAndShip, " "));
            printf("The ship hit was: %s", strtok(NULL, " "));

            //store move
            //strcpy(ourMove->state, strtok(ourMoveStateAndShip, " "));
            //strcpy(ourMove->ship, strtok(NULL, " "));
            
            //insert_move(&head, &tail, ourMove);

            //wait for move
            printf("Waiting for other player to make move...");
            fflush(stdout);
            recv(ourSocket, buff, sizeof(buff), 0);
            printf("Received %s\n", buff);
            theirMove = (struct move *)malloc(sizeof(struct move));
            char letter = buff[0];
            int number = buff[2] - '0';

            theirMove->letter = letter;
            theirMove->number = number;
            theirMove->player = 2;
            update_state(state, board, &head, &tail, &theirMove);

            //send state
            strcpy(theirMoveStateAndShip, theirMove->state);
            strcat(theirMoveStateAndShip, " ");
            strcat(theirMoveStateAndShip, theirMove->ship);
            send(ourSocket, theirMoveStateAndShip, sizeof(theirMoveStateAndShip), 0);
            printf("Sent %s\n", theirMoveStateAndShip);
        }else{
            //wait for move
            printf("Waiting for other player to make move...");
            fflush(stdout);
            recv(listenSocket, buff, sizeof(buff), 0);
            printf("Received %s\n", buff);
            theirMove = (struct move *)malloc(sizeof(struct move));
            char letter = buff[0];
            int number = buff[2] - '0';

            printf("Their move was: %c%d", letter, number);

            theirMove->letter = letter;
            theirMove->number = number;
            theirMove->player = 2;
            update_state(state, board, &head, &tail, &theirMove);
            
            
            //send state
            strcpy(theirMoveStateAndShip, theirMove->state);
            strcat(theirMoveStateAndShip, " ");
            strcat(theirMoveStateAndShip, theirMove->ship);
            send(listenSocket, theirMoveStateAndShip, sizeof(theirMoveStateAndShip), 0);
            printf("Sent %s\n", theirMoveStateAndShip);



		    display_state(state, board, ourMoveBoard);
            //make move
            ourMove = accept_input();
            ourMove->player = 1;
            char num[2];
            
            sprintf(num, "%d", ourMove->number);

            buff[0] = ourMove->letter;
            buff[1] = ' ';
            buff[2] = num[0];

            //send move
            send(listenSocket, buff, sizeof(buff), 0);
            printf("Sent %s\n", buff);

            //wait for state
            recv(listenSocket, ourMoveStateAndShip, sizeof(ourMoveStateAndShip), 0);
            printf("Received %s\n", ourMoveStateAndShip);
            
            
            printf("The state was: %s", strtok(ourMoveStateAndShip, " "));
            printf("The ship hit was: %s", strtok(NULL, " "));
            //store move
            //strcpy(ourMove->state, strtok(ourMoveStateAndShip, " "));
            //strcpy(ourMove->ship, strtok(NULL, " "));
            
            //insert_move(&head, &tail, ourMove);
        }
        
        

        
        
        
        
        /*add code below to send our move to the other player*/
        

        /*
        if (ipAddress[0] != 0){
            //strcpy(buff, "Test Message");
            buff[0] = ourMove->letter;
            buff[1] = toascii(ourMove->number);
            buff[2] = '\0';
            //sprintf(buff, "%s%d\0", ourMove->letter, ourMove->number);
            send(ourSocket, buff, sizeof(buff), 0);
            printf("Sent %s\n", buff);
        }else
        {
            buff[0] = ourMove->letter;
            buff[1] = toascii(ourMove->number);
            buff[2] = '\0';
            //sprintf(buff, "%s%d\0", ourMove->letter, ourMove->number);
            send(listenSocket, buff, sizeof(buff), 0);
            printf("Sent %s\n", buff);
        }
        */
        
        /*add code to receive the state of our move from the other player*/
        /*
        if (ipAddress[0] != 0){
            while(recv(ourSocket, buff, sizeof(buff), 0) > 0){
                //recv(listenSocket, buff, sizeof(buff), 0);
                printf("Received %s\n", buff);
                memset(buff, 0, sizeof(buff));
            }
        }else
        {
            while(recv(listenSocket, buff, sizeof(buff), 0) > 0){
                //recv(listenSocket, buff, sizeof(buff), 0);
                printf("Received %s\n", buff);
                memset(buff, 0, sizeof(buff));
            }
        }
        */
        /*add code to store our moves (letter, number, and result) into linked list*/

		//struct move theirMove;
        /*add code below to receive theirMove from the other player*/
        
        /*
        if (ipAddress[0] != 0){
            //strcpy(buff, "Test Message");
            buff[0] = ourMove->letter;
            buff[1] = toascii(ourMove->number);
            buff[2] = '\0';
            send(ourSocket, buff, sizeof(buff), 0);
            printf("Sent %s", buff);
        }
        */

        /*modify the update_state function to check theirMove is HIT or MISS
         * and send the state back to the other player */
        //Check their move


	} while(strcmp(state, flag));
	teardown(board, head);
	exit(0);
}