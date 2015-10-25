// Author: Jason Wong 
// in order to run application:
// 1) run make file
// 2) run command: ./testscript.pl ./appserver 10 1000 0

#include "appserver.h"

/*DEFINITIONS*/
#define MAX_COMMAND_SIZE 200
#define NUM_ARGUMENTS 4
#define ARG_FORMAT "appserver num_workers num_accounts out_file"

/*PROTOTYPES*/
int parse_arguments(int argc, char** argv);
int setup_bank();
int setup_command_buffer();
int command_loop();
void incorrect_argument_format();
void * request_handler();

//variable indicated if thread is running
int busy = 1;

account * accounts;

//List of commands to process
LinkedList * cmd_buffer;

int num_workers;
int num_accounts;

//ensure str tok is safe
pthread_mutex_t tok_lock;
pthread_mutex_t bank_lock;

FILE * outputFile;

/**Setup bank and wait for client input**/
int main(int argc, char** argv){
	cmd_buffer = malloc(sizeof(LinkedList));

	if(parse_arguments(argc, argv)){
		return -1;
	}

	accounts = malloc(num_accounts * sizeof(account));

	if(setup_bank()){
		return -1;
	}

	if(setup_command_buffer()){
		return -1;
	}

	//main input loop
	if(command_loop()){
		return -1;
	}

	//Garbage Collection
	free(cmd_buffer);
	free(accounts);
	free_accounts();
	fclose(outputFile);

	return 0;
}

//parse initial program arguments to construct threads and bank
int parse_arguments(int argc, char** argv){
	//check for correct num args
	if(argc != NUM_ARGUMENTS){
		fprintf(stderr, "error (appserver): incorrect # of command " 
			"line arguments\n");
		fprintf(stderr, "appserver expected format: ");
		fprintf(stderr, ARG_FORMAT);
		fprintf(stderr, "\nappserver: exiting program\n");
		return -1;
	}

	//get arugments for num workers and accounts
	if(!sscanf(argv[1], "%d", &num_workers)){
		incorrect_argument_format();
		return -1;
	}

	if(!sscanf(argv[2], "%d", &num_accounts)){
		incorrect_argument_format();
		return -1;
	}

	outputFile = fopen(argv[3], "w");

	//Forced to exit due to output file fail
	if(!outputFile){
		fprintf(stderr, "error (appserver): failed to open out_file\n");
		fprintf(stderr, "appserver: exiting program\n");
		return -1;
	}

	return 0;
}

//setup accounts and bank before program init
int setup_bank(){
	int i;
	initialize_accounts(num_accounts);

	//create account for all in bank
	for(i = 0; i < num_accounts; i++){
		pthread_mutex_init(&(accounts[i].lock), NULL);
		accounts[i].value = 0;
	}

	return 0;
}

//setup data structure for command buffer
int setup_command_buffer(){
	pthread_mutex_init(&(cmd_buffer->lock), NULL);
	cmd_buffer->head = NULL;
	cmd_buffer->tail = NULL;
	cmd_buffer->size = 0;

	return 0;
}

//take in user input loop
int command_loop(){
	pthread_t workers[num_workers];
	int i;
	size_t n = 100;
	ssize_t read_size = 0;
	char * command = malloc(MAX_COMMAND_SIZE * sizeof(char));
	int id = 1;

	//create all worker threads
	for(i = 0; i < num_workers; i++){
		pthread_create(&workers[i], NULL, request_handler, NULL);
	}

	pthread_mutex_init(&tok_lock, NULL);
	pthread_mutex_init(&bank_lock, NULL);

	while(1){
		read_size = getline(&command, &n, stdin);
		command[read_size - 1] = '\0';

		//exit program on this command
		if(strcasecmp(command, "END") == 0){
			busy = 0;
			break;
		}
		printf("ID %d\n", id);
		add_command(command, id);
		id++;
	}

	//make sure all commands finished
	while(cmd_buffer->size > 0){
		usleep(1);
	}

	//wait for workers to finish
	for(i = 0; i < num_workers; i++){
		pthread_join(workers[i], NULL);
	}

	free(command);

	return 0;
}

//print out standard argument info
void incorrect_argument_format(){
	fprintf(stderr, "Error! Incorrect argument format\n");
	fprintf(stderr, "Expected format: ");
	fprintf(stderr, ARG_FORMAT);
	fprintf(stderr, "\nExiting...\n");
}

//worker threads method
void * request_handler(){
	LinkedCommand cmd;

	/*string of arguments*/
	char ** cmd_tokens = malloc(21 * sizeof(char*));
	char * cur_tok;

	int num_tokens = 0;
	int check_account;
	int amount;
	int i, j;

	//insuffient funds if 1, else 0
	int ISF = 0;

	struct timeval timestamp2;

	while(busy || cmd_buffer->size > 0)
	{
		//performance, make sure not using valuable resources
		while(cmd_buffer->size == 0 && busy)
		{
			usleep(1);
		}

		cmd = next_command();

		if(!cmd.command)
		{
			continue;
		}

		//put command into usuable format
		pthread_mutex_lock(&tok_lock);
		cur_tok = strtok(cmd.command, " ");
		while(cur_tok){
			cmd_tokens[num_tokens] = malloc(21 * sizeof(char));
			strncpy(cmd_tokens[num_tokens], cur_tok, 21);
			num_tokens++;
			cur_tok = strtok(NULL, " ");
		}

		pthread_mutex_unlock(&tok_lock);

		//check command
		if(strcasecmp(cmd_tokens[0], "CHECK") == 0 && num_tokens == 2){
			pthread_mutex_lock(&tok_lock);
			check_account = atoi(cmd_tokens[1]);
			pthread_mutex_unlock(&tok_lock);
			pthread_mutex_lock(&(accounts[i-1].lock));
			amount = read_account(check_account);
			pthread_mutex_unlock(&(accounts[i-1].lock));
			gettimeofday(&timestamp2, NULL);
			
			//ensure file writes are in order
			flockfile(outputFile);
			fprintf(outputFile, "%d BAL %d TIME %d.%06d %d.%06d\n", 
				cmd.id, amount, cmd.timestamp.tv_sec, 
				cmd.timestamp.tv_usec, timestamp2.tv_sec, 
				timestamp2.tv_usec);
			funlockfile(outputFile);
		}
		//trans command
		else if(strcasecmp(cmd_tokens[0], "TRANS") == 0 && num_tokens % 2 
			&& num_tokens > 1){

			int num_trans = (num_tokens - 1) / 2;
			int trans_accounts[num_trans];
			int trans_amounts[num_trans];
			int trans_balances[num_trans];
			int temp;

			//create data structure for tranactions
			for(i = 0; i < num_trans; i++)
			{
				pthread_mutex_lock(&tok_lock);
				trans_accounts[i] = atoi(cmd_tokens[i*2+1]);
				trans_amounts[i] = atoi(cmd_tokens[i*2+2]);
				pthread_mutex_unlock(&tok_lock);
			}

			//order transactions by account number
			for(i = 0; i < num_trans; i++)
			{
				for(j = i; j < num_trans; j++)
				{
					if(trans_accounts[j] < 
						trans_accounts[i])
					{
						temp = trans_accounts[i];
						trans_accounts[i] = 
							trans_accounts[j];
						trans_accounts[j] = temp;
						temp = trans_amounts[i];
						trans_amounts[i] = 
							trans_amounts[j];
						trans_amounts[j] = temp;
					}
				}
			}

			//mutex lock
			for(i = 0; i < num_trans; i++)
				pthread_mutex_lock(&(accounts[
					trans_accounts[i]-1].lock));

			//make sure all commands have sufficient funds
			for(i = 0; i < num_trans; i++)
			{
				trans_balances[i] = 
					read_account(trans_accounts[i]);
				if(trans_balances[i] + trans_amounts[i] < 0)
				{
					//write isf to file
					gettimeofday(&timestamp2, NULL);

					//ensure file writes are in order
					flockfile(outputFile);
					fprintf(outputFile, "%d ISF %d TIME " 
						"%d.06%d %d.06%d\n", 
						cmd.id, trans_accounts[i], 
						cmd.timestamp.tv_sec, 
						cmd.timestamp.tv_usec, 
						timestamp2.tv_sec, 
						timestamp2.tv_usec);
					funlockfile(outputFile);
					ISF = 1;
					break;
				}
			}
			//sufficient funds so execute commands
			if(!ISF)
			{
				for(i = 0; i < num_trans; i++)
				{
					write_account(trans_accounts[i], 
						(trans_balances[i] + 
						trans_amounts[i]));
				}
				//print to file of summary
				gettimeofday(&timestamp2, NULL);

				//ensure file writes are in order
				flockfile(outputFile);
				fprintf(outputFile, "%d OK TIME %d.%06d %d.%06d\n", 
					cmd.id, cmd.timestamp.tv_sec, 
					cmd.timestamp.tv_usec, 
					timestamp2.tv_sec, timestamp2.tv_usec);
				funlockfile(outputFile);
			}
			//mutex unlock
			for(i = num_trans - 1; i >=0; i--)
				pthread_mutex_unlock(&(accounts[
					trans_accounts[i]-1].lock));
		}
		else
		{
			fprintf(stderr, "%d INVALID REQUEST FORMAT\n", cmd.id);
		}

		//free up command
		free(cmd.command);
		for(i = 0; i < num_tokens; i++)
		{
			free(cmd_tokens[i]);
		}
		num_tokens = 0;
		ISF = 0;
	}

	free(cmd_tokens);

	/*return*/
	return;
}

//mutex lock for accounts
int lock_account(account * to_lock){
	if(pthread_mutex_trylock(&(to_lock->lock)))
	{
		return -1;
	}

	return 0;
}

//mutex unlock for accounts
int unlock_account(account * to_unlock){
	pthread_mutex_unlock(&(to_unlock->lock));
	return 0;
}

//add command to buffer
int add_command(char * given_command, int id){
	LinkedCommand * new_tail = malloc(sizeof(LinkedCommand));

	//create command
	new_tail->command = malloc(MAX_COMMAND_SIZE * sizeof(char));
	strncpy(new_tail->command, given_command, MAX_COMMAND_SIZE);
	new_tail->id = id;
	gettimeofday(&(new_tail->timestamp), NULL);
	new_tail->next = NULL;

	pthread_mutex_lock(&(cmd_buffer->lock));

	//check size of list
	if(cmd_buffer->size > 0){
		cmd_buffer->tail->next = new_tail;

		cmd_buffer->tail = new_tail;
		cmd_buffer->size = cmd_buffer->size + 1;
	}
	else{
		//only one command
		cmd_buffer->head = new_tail;
		cmd_buffer->tail = new_tail;
		cmd_buffer->size = 1;
	}

	pthread_mutex_unlock(&(cmd_buffer->lock));

	return 0;
}

//retrieve next command from buffer
LinkedCommand next_command(){
	//temporary pointer used to free head
	LinkedCommand * temp_head;
	LinkedCommand retCommand;

	pthread_mutex_lock(&(cmd_buffer->lock));

	//Check for new command
	if(cmd_buffer->size > 0){
		retCommand.id = cmd_buffer->head->id;
		retCommand.command = malloc(MAX_COMMAND_SIZE * sizeof(char));
		retCommand.timestamp = cmd_buffer->head->timestamp;
		strncpy(retCommand.command, cmd_buffer->head->command, 
			MAX_COMMAND_SIZE);
		retCommand.next = NULL;

		//reassign head
		temp_head = cmd_buffer->head;
		cmd_buffer->head = cmd_buffer->head->next;
		free(temp_head->command);
		free(temp_head);

		if(!cmd_buffer->head)
		{
			cmd_buffer->tail = NULL;
		}

		//update size of linked list
		cmd_buffer->size = cmd_buffer->size - 1;
	}
	else{
		pthread_mutex_unlock(&(cmd_buffer->lock));

		retCommand.command = NULL;

		//no commands needed to be buffered
		return retCommand;
	}

	pthread_mutex_unlock(&(cmd_buffer->lock));

	return retCommand;
}
