#ifndef APPSERVER
#define APPSERVER

#ifndef STDLIB
#define STDLIB
#include <stdlib.h>
#endif

#ifndef STDIO
#define STDIO
#include <stdio.h>
#endif

#ifndef STRING
#define STRING
#include <string.h>
#endif

#ifndef PTHREAD
#define PTHREAD
#include <pthread.h>
#endif

#ifndef BANK
#define BANK
#include "Bank.h"
#endif

#ifndef TIME
#define TIME
#include <sys/time.h>
#endif

//structure to hold account info
typedef struct account_struct
{
	pthread_mutex_t lock;
	int value;
}account;

//element in linked list structure
typedef struct LinkedCommand_struct
{
	char * command;
	int id;
	struct timeval timestamp;
	struct LinkedCommand_struct * next;
	
}LinkedCommand;

//structure needed to store linked list
typedef struct LinkedList_struct
{
	pthread_mutex_t lock;
	LinkedCommand * head;
	LinkedCommand * tail;
	int size;
}LinkedList;

/*PROTOTYPES*/
int lock_account(account * to_lock);
int unlock_account(account * to_unlock);
LinkedCommand next_command();
int add_command(char * given_command, int id);

#endif

