#include <stdlib.h>
#include <stdio.h>
#define __USE_GNU
#include <pthread.h>
#include <sys/time.h>

#define MAX_EVENTS 255
#define MAX_PROCESSORS 5

//#define VERBOSE
#ifdef VERBOSE
	/*verbose logs*/
	#define printV printf
#else
	#define printV
#endif

typedef enum {
	LOCAL_EVENT = 1,
	SEND_EVENT = 2,
	RECIEVE_EVENT =3,
}e_EventType;

typedef struct Event_Node {
	int processor_index;
	e_EventType evt_type;
	char eType[255];
	int evt_source;
	int evt_site_idx;
	int evt_dstn;
	int evt_ptnr_idx;
	int scalar_clk;
}s_evt_node;

typedef struct Processor {
	pthread_mutex_t a_mutex;
	pthread_cond_t  p_cond;
	int proc_index;
	char *proc_name;
	struct Event_Node* evnt_list[MAX_EVENTS];
	int num_events;
	int scalar_clk;
	int events_processing_complete;
	int evnts_processed;
	int send_blocked;
	int rcv_waiting;
}s_prcr;

pthread_cond_t  p_cond_ACK = PTHREAD_COND_INITIALIZER;

/*globals*/
pthread_cond_t  p_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t a_mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
struct Processor* gpNode[MAX_PROCESSORS];
pthread_t proc_thrdID[MAX_PROCESSORS];


static print_processor_info(struct Processor* pNode) {
	int idx2 = 0;
	int num_events = 0;

	printV("\n\t System %d", pNode->proc_index);
	printV("\n\t\t Name of the system\t  : P[%d]",pNode->proc_index);
	printV("\n\t\t Number of events  \t  : %d",pNode->num_events);
	num_events = pNode->num_events;
	for (idx2=1; idx2<= num_events; idx2++) {
		struct Event_Node *tempNode = (struct Event_Node*)pNode->evnt_list[idx2];
		printV("\n\t\t\t\t Event System \t : %d", tempNode->processor_index);
		printV("\n\t\t\t\t Event Type   \t : %d", tempNode->evt_type);
		printV("\n\t\t\t\t Event start  \t : %d", tempNode->evt_source);
		printV("\n\t\t\t\t Event End    \t : %d", tempNode->evt_dstn);
		printV("\n");
	}
}

static print_input_graph(struct Processor* pNode[MAX_PROCESSORS], int num_dsystems) {

	int idx = 0, idx2 = 0;
	int num_events = 0;

	printV("\n INPUT DUMP");
	for (idx=1; idx<=num_dsystems; idx++) {
		print_processor_info(pNode[idx]);
	}
}

#define ARR_SIZE(arr) sizeof(arr)/sizeof(arr[1])
static int is_rx_ready(int src_idx, int evt_idx, int dstn_idx, int ptnr_idx) {

	int availability = 0;

	printV("\n rcv_Wait = %d, src_idx = %d, dstn_idx = %d, evt_idx = %d",gpNode[dstn_idx]->rcv_waiting ,src_idx,dstn_idx,evt_idx);
	if ((gpNode[dstn_idx]->rcv_waiting == 1) &&
		(gpNode[src_idx]->evnt_list[evt_idx]->evt_type == SEND_EVENT) &&
		(gpNode[src_idx]->evnt_list[evt_idx]->evt_dstn == dstn_idx) &&
		((gpNode[dstn_idx]->evnts_processed+1) == ptnr_idx)) {
		availability = 1;
		printV("\n reciever waiting !! \n");
	}

	return availability;
}

static int is_tx_waiting (int src_idx, int evt_idx, int dstn_idx) {

	int availability = 0;

	if ((gpNode[src_idx]->send_blocked == 1) &&
		(gpNode[dstn_idx]->evnt_list[evt_idx]->evt_type == RECIEVE_EVENT) &&
		(gpNode[dstn_idx]->evnt_list[evt_idx]->evt_dstn == dstn_idx))
		availability = 1;
	return availability;
}

static void process_lamport_clock(struct Processor* pNode, struct Event_Node* eNode, int msgclk, char*tname, int evt_idx) {
	int idx = 0;

	printf("\n\t----------------------------------");
	printf("\n\tlamport's logical clock calculated");
	printf("\n\t----------------------------------");

	if (eNode->evt_type == 1) {
		eNode->scalar_clk ++;
		pNode->scalar_clk += eNode->scalar_clk;
		sprintf(eNode->eType,"%s","LOCAL_EVENT");
	} else if (eNode->evt_type == 2) {
		printV("\n msgclk = %d, pnode_clk = %d",msgclk,pNode->scalar_clk);	
		eNode->scalar_clk ++;
		pNode->scalar_clk += eNode->scalar_clk;
		eNode->scalar_clk = pNode->scalar_clk;
		sprintf(eNode->eType,"%s","SEND_EVENT");
	} else {

		printV("\n msgclk = %d, pnode_clk = %d",msgclk,pNode->scalar_clk);	
		eNode->scalar_clk = pNode->scalar_clk>msgclk?pNode->scalar_clk:msgclk;
		eNode->scalar_clk ++;
		pNode->scalar_clk = eNode->scalar_clk;
		sprintf(eNode->eType,"%s","RECIEVE_EVENT");
	}


	printf("\n\t<site - %s> : <event_number %d> : <%s>    \t : <clock - %d> \n",tname,evt_idx, eNode->eType, pNode->scalar_clk);
}

void *t_func(void *param) {

	int rc = 0, idx = 0;
	char tname[255];
	pthread_t pid;
	int done = 0;
	struct Processor* pNode = (struct Processor*)param;
	int num_events = 0;
	int msgclk = 0;
	int source_idx = 0, event_idx = 0, destn_idx = 0;
	int source = 0, destn = 0;
	
	rc = pthread_mutex_lock(&a_mutex);
	if (rc) { /* an error has occurred */
		printf("\n pthread_mutex_lock \n");
	        pthread_exit(NULL);
	}

	sleep(2);

	pid = pthread_self();
	rc = pthread_getname_np(pid, tname, 255);

	printV("\n %s:: Enter --> \n", tname);

	//print_processor_info(pNode);

	num_events = pNode->num_events;

	for(idx=1; idx<=num_events; idx++) {
		struct Event_Node *eNode = (struct Event_Node*)pNode->evnt_list[idx];
		
		if(idx>1)
			msgclk = pNode->evnt_list[idx-1]->scalar_clk;

		if ((eNode->evt_type == 1)) {
			printV("\n %s :: local event", tname);
			process_lamport_clock(pNode, eNode, msgclk, tname, idx);
		} else if (eNode->evt_type == 3) {
			source = eNode->evt_source;
			event_idx = idx;
			destn = eNode->evt_dstn;
			source_idx = eNode->evt_site_idx;
			destn_idx = eNode->evt_ptnr_idx;

			if (is_tx_waiting(source, event_idx, destn)) {
				printV("\n P%d signalled to execute \n",source);
				pthread_cond_signal(&p_cond_ACK);
			}
			
			printV("\n %s :: recieve event", tname);
			printV("\n %s :: waiting for signal from P[%d] at event %d", tname, source, event_idx);

			pNode->rcv_waiting = 1;
			pthread_cond_wait(&pNode->p_cond,&a_mutex);
			pNode->rcv_waiting = 0;
			printV("\n %s :: unblocked waiting for signal from P[%d] at event %d", tname, source, event_idx);
		        msgclk = gpNode[source]->evnt_list[destn_idx]->scalar_clk;

			process_lamport_clock(pNode, eNode, msgclk, tname, idx);

		} else {

			source = eNode->evt_source;
			event_idx = idx;
			destn = eNode->evt_dstn;
			
			if (!is_rx_ready(source, event_idx, destn, eNode->evt_ptnr_idx)) {
				printV("\n P%d waiting to execute \n",pNode->proc_index);
				pNode->send_blocked = 1;
				pthread_cond_wait(&p_cond_ACK,&a_mutex); 
				pNode->send_blocked = 0;
				printV("\n P%d starting to execute \n",pNode->proc_index);
			}

			printV("\n %s :: send event",tname);
			source = eNode->evt_source;
			destn = eNode->evt_dstn;
			event_idx = idx;
		        if (idx>1)
				msgclk = gpNode[source]->evnt_list[idx-1]->scalar_clk;
			else
				msgclk = 0;
			process_lamport_clock(pNode, eNode, msgclk, tname, idx);
			printV("\n sending signal from %s to P[%d]",tname,destn);
			pthread_cond_signal(&gpNode[destn]->p_cond);

		}
		pNode->evnts_processed++;

	}

	pNode->events_processing_complete = 1;
	printV("\n %s:: <--Exit  \n", tname);
	pthread_mutex_unlock(&a_mutex);
	rc = pthread_cond_destroy(&pNode->p_cond);

	return NULL;
}

static void construct_input_graph(struct Processor* pNode[MAX_PROCESSORS], pthread_t proc_thrd[MAX_PROCESSORS], int num_dsystems) {
	
	int num_events[MAX_EVENTS];
	int idx = 0, idx2 = 0;
	int evt_type = 0, start = 0, end = 0;
	int ptnr_idx = 0;
	struct Event_Node *eNode;
	int rc = 0;
	char tname[255];

	printf("\nconstructing graph ...\n");

	for (idx=1; idx<=num_dsystems; idx++) {

		pNode[idx] = (struct Processor*) malloc(sizeof(struct Processor));
		if(!pNode[idx]) {
			printf("\n Failed to allocate Processor - Exit");
			exit(1);
		}

		printf("\n Enter the number of events for P[%d]     \t : ",idx);
		scanf("%d",&num_events[idx]);

		for (idx2=1; idx2<=num_events[idx]; idx2++){
			printf(" Enter the type of Event (LOCAL_EVENT = 1, SEND_EVENT = 2, RECIEVE_EVENT = 3)\n");
			eNode = (struct Event_Node*) malloc(sizeof(struct Event_Node));
			if (!eNode) {
				printf("\n Failed to allocate Event nodes - Exit");
				exit(1);
			}

			eNode->processor_index = idx;
			printf("\tEvent Type \t\t: ");
			scanf("%d",&evt_type);
			eNode->evt_type = evt_type;
			if (evt_type == LOCAL_EVENT) {
				eNode->evt_source = idx;
				eNode->evt_dstn = idx;
			} else {
				printf("\tEvent Start\t\t: ");
				scanf("%d",&start);
				eNode->evt_source = start;
				printf("\tEvent End  \t\t: ");
				scanf("%d",&end);
				eNode->evt_dstn = end;
			}
			eNode->evt_site_idx = idx2;
			if (evt_type == LOCAL_EVENT) {
				eNode->evt_ptnr_idx = idx2;
			} else {
				printf("\t--please enter the associated event index where this event coincides with another site-- \n");
				printf("\tPartner Event num\t: ");
				scanf("%d",&ptnr_idx);
				eNode->evt_ptnr_idx = ptnr_idx;
			}

			pNode[idx]->evnt_list[idx2] = eNode;
		}
		pNode[idx]->proc_index = idx;
		pNode[idx]->num_events = num_events[idx];

	}

	for (idx=1; idx<=num_dsystems; idx++) {

		/*create individual thread for every processor*/
		rc = pthread_create(&proc_thrd[idx], NULL, t_func, pNode[idx]);
		if (rc) {
			printf("\n thread creation failed");
			exit(1);
		}

	        sprintf(tname,"P%d",idx);
		printV("\n tname = %s",tname);
		rc = pthread_setname_np(proc_thrd[idx], tname);
		//sleep(2);

	}

	printf("\n\n \t Please wait -- clock is getting generated!! Dont press ENTER !! \n");
}

static void free_graph_nodes(struct Processor* pNode[MAX_PROCESSORS], pthread_t proc_thrd[MAX_PROCESSORS],int num_dsystems) {

	int idx = 0, idx2 = 0;
	int num_events = 0;
	
	printf("\n Freeing graph nodes..\n");
	
	for (idx=1; idx<=num_dsystems; idx++) {
		num_events = pNode[idx]->num_events;
		for (idx2=1; idx2<= num_events; idx2++) {
			struct Event_Node *tempNode = (struct Event_Node*)pNode[idx]->evnt_list[idx2];
			free(tempNode);
		}
		free(pNode[idx]);
	}
}

int main()
{
	int num_dsystems = 0;
	int num_events[MAX_EVENTS];
	int idx = 0, idx2 = 0;
	int evt_type = 0, start = 0, end = 0;
	struct Event_Node *eNode;

	printf("\n\nLamport's Logical clock **START** \n\n");

	printf("\nEnter the number of distributed systems needed \t : ");
	scanf("%d",&num_dsystems);

	construct_input_graph(gpNode, proc_thrdID, num_dsystems);
	
	print_input_graph(gpNode, num_dsystems);

	getchar();
	getchar();
	
	free_graph_nodes(gpNode, proc_thrdID, num_dsystems);
	
	printf("\n\n\n Lamport's Logical clock **END** \n\n");
	return 0;
}
