#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include <sys/stat.h>
#include<errno.h>
#include<math.h>
#include<stdbool.h>
#include<pthread.h>
#include<time.h>
#include<sys/time.h>
#include<signal.h>
#include "my402list.h"

double token_arrival_rate = 0.0, packet_arrival_rate = 0.0, service_rate = 0.0;
int token_bucket_depth = 0, packet_requirement, num_packets, tokens_in_bucket = 0; 
int serviced = 0, served_packet_number = 0, packets_entering_filter = 0, tokens_entering_filter = 0, packets_dropped = 0, tokens_dropped = 0;
unsigned int inter_packet_arrival_time = 0, inter_token_arrival_time = 0, inter_service_time = 0;
bool input_file_flag = false;
struct timeval emulation_start_time, emulation_end_time;
int terminate = 0;

unsigned long int *total_time_data = NULL;;
int initial_capacity = 5000;
unsigned int total_packet_inter_arrival_time = 0;
unsigned int total_packet_service_time = 0;
unsigned int total_time_in_Q1 = 0;
unsigned int total_time_in_Q2 = 0;
unsigned int total_time_in_S1 = 0;
unsigned int total_time_in_S2 = 0;
unsigned long int total_time_in_System = 0; 

My402List queue_1, queue_2;
pthread_mutex_t mutex_object = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t conditional_variable = PTHREAD_COND_INITIALIZER;
pthread_t packet_generator_thread, token_depositor_thread, server_1_thread, server_2_thread, cntrl_C_thread;
sigset_t set_interrupt;
FILE *fp;
char *filename;
int line_num = 1;

typedef struct packet {
	int packet_number;
	int packet_token_requirement;
	unsigned int packet_inter_arrival;
	unsigned int packet_service;
	struct timeval packet_created_time;
	struct timeval packet_enters_Q1;
	struct timeval packet_leaves_Q1;
	struct timeval packet_enters_Q2;
	struct timeval packet_leaves_Q2;
	struct timeval packet_enters_server;
	struct timeval packet_leaves_server;
} Packet_structure;

unsigned int calculate_interval(struct timeval *start_time, struct timeval *end_time)
{
	unsigned int seconds_difference = (end_time->tv_sec - start_time->tv_sec);
	unsigned int microseconds_difference = (end_time->tv_usec - start_time->tv_usec);
	return(seconds_difference*1000000 + microseconds_difference);
}

double calculate_interval_in_milliseconds(struct timeval *start_time, struct timeval *end_time)
{
	return(calculate_interval(start_time, end_time)/1000.0);
}

void * handle_interrupt(void * param)
{
	int sig_value = 0;
	sigwait(&set_interrupt, &sig_value);
	if(sig_value != SIGINT)
	{
		sigwait(&set_interrupt, &sig_value);
	}
	if(pthread_mutex_lock(&mutex_object))
	{
		fprintf(stderr, "Error in acquiring mutex lock - %s\n", strerror(errno));
		return(0);
	}
	struct timeval cntrl_c_caught;
	gettimeofday(&cntrl_c_caught, NULL);
	fprintf(stdout, "\n%012.3fms: SIGINT caught, no new packets or tokens will be allowed\n", calculate_interval_in_milliseconds(&emulation_start_time, &cntrl_c_caught));
	terminate = 1;
	served_packet_number = 0;
	num_packets = 0;
	pthread_cond_broadcast(&conditional_variable);
	pthread_cancel(packet_generator_thread);
	pthread_cancel(token_depositor_thread);
	if(pthread_mutex_unlock(&mutex_object))
	{
		fprintf(stderr, "Error in releasing mutex lock - %s\n", strerror(errno));
		return(0);
	}
	pthread_sigmask(SIG_UNBLOCK, &set_interrupt, NULL);
	pthread_exit(NULL);
	return 0;
}

void move_packet_from_Q1_to_Q2(Packet_structure * object)
{
	My402ListUnlink(&queue_1, My402ListFirst(&queue_1));
	gettimeofday(&(object->packet_leaves_Q1), NULL);
	fprintf(stdout, "%012.3fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d token\n", calculate_interval_in_milliseconds(&emulation_start_time, &(object->packet_leaves_Q1)), object -> packet_number, calculate_interval_in_milliseconds(&(object->packet_enters_Q1), &(object->packet_leaves_Q1)), tokens_in_bucket);
	(void)My402ListAppend(&queue_2, object);
	gettimeofday(&(object->packet_enters_Q2), NULL);
	fprintf(stdout, "%012.3fms: p%d enters Q2\n", calculate_interval_in_milliseconds(&emulation_start_time, &(object->packet_enters_Q2)), object->packet_number);
}

void cleanup()
{
	pthread_mutex_lock(&mutex_object);
	fclose(fp);
	fp = NULL;
	pthread_mutex_unlock(&mutex_object);
	exit(-1);
}

void read_file()
{
	char buffer[1030];
	buffer[1024] = '\0';
	
	if(fgets(buffer, 1030, fp))
	{
		line_num++;
		if(buffer[1024] != '\0')
		{
			fprintf(stderr, "Error in the input file:%s, Line no:%d contains more than 1024 characters!\n", filename, line_num);
			cleanup();
		}
		int buf_ptr = 0;
		if(buffer[buf_ptr] == ' ' || buffer[buf_ptr] == '\t')
		{
			fprintf(stderr, "Error in the input file:%s, Line no:%d contains leading spaces or tabs!\n", filename, line_num);
			cleanup();
		}
		while(buffer[buf_ptr] != '\t' && buffer[buf_ptr] != ' ')
		{
			if(buffer[buf_ptr] < '0' || buffer[buf_ptr] > '9')
			{
				fprintf(stderr, "Error in the input file:%s, Line no:%d Inter packet arrival rate value contains characters that are not numbers!\n", filename, line_num);
				cleanup();
			}
			buf_ptr++;
		}
		while(buffer[buf_ptr] == ' ' || buffer[buf_ptr] == '\t')
		{
			buf_ptr++;
		}
		while(buffer[buf_ptr] != '\t' && buffer[buf_ptr] != ' ')
		{
			if(buffer[buf_ptr] < '0' || buffer[buf_ptr] > '9')
			{
				fprintf(stderr, "Error in the input file:%s, Line no:%d Packet requirement value contains characters that are not numbers!\n", filename, line_num);
				cleanup();
			}
			buf_ptr++;
		}
		while(buffer[buf_ptr] == ' ' || buffer[buf_ptr] == '\t')
		{
			buf_ptr++;
		}
		while(buffer[buf_ptr] != '\t' && buffer[buf_ptr] != ' ' && buffer[buf_ptr] != '\n' && buffer[buf_ptr] != '\0' )
		{
			if(buffer[buf_ptr] < '0' || buffer[buf_ptr] > '9')
			{
				fprintf(stderr, "Error in the input file:%s, Line no:%d Service rate value contains characters that are not numbers!\n", filename, line_num);
				cleanup();
			}
			buf_ptr++;
		}
		int arrival_time, service_time;
		unsigned int p;
		sscanf(buffer, "%d %u %d", &arrival_time, &p, &service_time);
		if(p <= 0 || p > 2147483647)
		{
			fprintf(stderr, "Error in the input file:%s, Line no:%d contains incorrect value for Packet requirement(P), should be a positive integer with maximum value 2147483647!\n", filename, line_num);
			cleanup();
		}
		inter_packet_arrival_time = (unsigned int)arrival_time * 1000;
		inter_service_time = (unsigned int)service_time * 1000;
		packet_requirement = (int)p;
	}
	else
	{
		fprintf(stderr, "Error in the input file:%s, Contains insufficient data!\n", filename);
		cleanup();
	}
}

void * generate_packets(void * param)
{
	int count_of_packets = 0;
	unsigned int packet_inter_arrival_total = 0;
	struct timeval current_time;
	struct timeval last_inter_arrival_time = emulation_start_time;
	int oldstate;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

	while(count_of_packets < num_packets)
	{	
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);

		if(input_file_flag)
		{
			read_file();
		}

		gettimeofday(&current_time, NULL);
		unsigned int emulation_to_packet_start = calculate_interval(&emulation_start_time, &current_time);
		packet_inter_arrival_total += inter_packet_arrival_time;
		if(input_file_flag)
		{
			if(emulation_to_packet_start < packet_inter_arrival_total)
			{
				/*subtract time elapsed from emulation start to present time from inter arrival time, do not sleep for actual inter arrival time*/
				unsigned int sleep_time = packet_inter_arrival_total - emulation_to_packet_start;
				usleep(sleep_time);
			}
		}
		else
		{
			if(packet_arrival_rate <= 10000)
			{
				if(emulation_to_packet_start < packet_inter_arrival_total)
				{
					/*subtract time elapsed from emulation start to present time from inter arrival time, do not sleep for actual inter arrival time*/
					unsigned int sleep_time = packet_inter_arrival_total - emulation_to_packet_start;
					usleep(sleep_time);
				}
			}
		}
		
		

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

		if(pthread_mutex_lock(&mutex_object))
		{
			fprintf(stderr, "Error in acquiring mutex lock - %s\n", strerror(errno));
			return(0);
		}

		packets_entering_filter++;
		struct timeval packet_generated_time;
		gettimeofday(&packet_generated_time, NULL);
		unsigned int current_inter_arrival_time = calculate_interval(&last_inter_arrival_time, &packet_generated_time);
		total_packet_inter_arrival_time += current_inter_arrival_time;

		if(packet_requirement > token_bucket_depth)  /*Drop the packet*/
		{
			packets_dropped++;
			last_inter_arrival_time = packet_generated_time;
			fprintf(stdout, "%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms, dropped\n", calculate_interval_in_milliseconds(&emulation_start_time, &packet_generated_time), packets_entering_filter, packet_requirement, ((double)current_inter_arrival_time)/1000.0);
			served_packet_number--;
			if(served_packet_number == 0)
			{
				pthread_cond_broadcast(&conditional_variable);
				pthread_cancel(token_depositor_thread);
			}
			if(My402ListEmpty(&queue_1) && (count_of_packets == num_packets-1))
			{
				pthread_cond_broadcast(&conditional_variable);
				pthread_cancel(token_depositor_thread);
			}
		}
		else
		{
			Packet_structure *packet_object = NULL;
			if((packet_object = (Packet_structure*)malloc(sizeof(Packet_structure))) == NULL)
			{
				fprintf(stderr, "Error in allocating space for packet structure handle: %s\n", strerror(errno));
				return(0);
			}

			packet_object -> packet_number = packets_entering_filter;
			packet_object -> packet_created_time = packet_generated_time;
			packet_object -> packet_inter_arrival = current_inter_arrival_time;
			packet_object -> packet_token_requirement = packet_requirement;
			packet_object -> packet_service = inter_service_time;
			last_inter_arrival_time = packet_generated_time;

			fprintf(stdout, "%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n", calculate_interval_in_milliseconds(&emulation_start_time, &packet_generated_time), packet_object -> packet_number, packet_object -> packet_token_requirement, ((double)packet_object -> packet_inter_arrival)/1000.0);
			gettimeofday(&(packet_object->packet_enters_Q1), NULL);
			
			if(!My402ListEmpty(&queue_1))
			{
				(void)My402ListAppend(&queue_1, packet_object);
				fprintf(stdout, "%012.3fms: p%d enters Q1\n", calculate_interval_in_milliseconds(&emulation_start_time, &(packet_object -> packet_enters_Q1)), packet_object -> packet_number);
			}
			else
			{
				(void)My402ListAppend(&queue_1, packet_object);
				fprintf(stdout, "%012.3fms: p%d enters Q1\n", calculate_interval_in_milliseconds(&emulation_start_time, &(packet_object -> packet_enters_Q1)), packet_object -> packet_number);
				if(packet_object->packet_token_requirement <= tokens_in_bucket)
				{
					tokens_in_bucket -= packet_object -> packet_token_requirement;
					//Move Packet from Q1 to Q2
					move_packet_from_Q1_to_Q2(packet_object);
					pthread_cond_broadcast(&conditional_variable);   /*Wake up servers*/
					if(My402ListEmpty(&queue_1) && (count_of_packets == num_packets-1))
					{
						pthread_cancel(token_depositor_thread);
					}
				}
			}
		}
		count_of_packets++;
		if(pthread_mutex_unlock(&mutex_object))
		{
			fprintf(stderr, "Error in releasing mutex lock - %s\n", strerror(errno));
			return(0);
		}
	}
	return 0;
}

void * deposit_tokens(void * param)
{
	unsigned int token_inter_arrival_total = 0;
	struct timeval current_time;
	bool flag = false;
	int oldstate;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

	while(1)
	{
		if(served_packet_number == 0)
		{
			break;
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
		gettimeofday(&current_time, NULL);
		unsigned int emulation_to_token_start = calculate_interval(&emulation_start_time, &current_time);
		token_inter_arrival_total += inter_token_arrival_time;

		if(emulation_to_token_start < token_inter_arrival_total)
		{
			unsigned int sleep_time = token_inter_arrival_total - emulation_to_token_start;
			usleep(sleep_time);
		}
			
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

		if(pthread_mutex_lock(&mutex_object))
		{
			fprintf(stderr, "Error in acquiring mutex lock - %s\n", strerror(errno));
			return(0);
		}
		tokens_entering_filter++;
		struct timeval token_generated_time;
		gettimeofday(&token_generated_time, NULL);

		if(tokens_in_bucket < token_bucket_depth)
		{
			tokens_in_bucket++;
			fprintf(stdout, "%012.3fms: token t%d arrives, token bucket now has %d token\n", calculate_interval_in_milliseconds(&emulation_start_time, &token_generated_time), tokens_entering_filter, tokens_in_bucket);
			while(!My402ListEmpty(&queue_1))
			{
				My402ListElem * element_object = My402ListFirst(&queue_1);
				if(((Packet_structure*)(element_object -> obj)) -> packet_token_requirement <= tokens_in_bucket)
				{
					tokens_in_bucket -= ((Packet_structure*)(element_object -> obj)) -> packet_token_requirement;
					move_packet_from_Q1_to_Q2((Packet_structure*)(element_object -> obj));
					if(My402ListEmpty(&queue_1) && (packets_entering_filter == num_packets))
					{
						pthread_cond_broadcast(&conditional_variable);
						flag = true;
						break;
					}
					pthread_cond_broadcast(&conditional_variable);
				}
				else
				{
					break;
				}
			}
			if(flag)
			{
				if(pthread_mutex_unlock(&mutex_object))
				{
					fprintf(stderr, "Error in releasing mutex lock - %s\n", strerror(errno));
					return(0);
				}
				break;
			}
		}
		else /*Drop the token*/
		{
			tokens_dropped++;
			fprintf(stdout, "%012.3fms: token %d arrives, dropped\n", calculate_interval_in_milliseconds(&emulation_start_time, &token_generated_time), tokens_entering_filter);
		}

		if(served_packet_number == 0)
		{
			break;
		}

		if(pthread_mutex_unlock(&mutex_object))
		{
			fprintf(stderr, "Error in releasing mutex lock - %s\n", strerror(errno));
			return(0);
		}
	}
	return 0;
}

static void cleanup_mutex(void *mutex)
{
	pthread_mutex_unlock(mutex);
}

void * service_packets(void * param)
{
	bool serve_packet_flag;
	My402ListElem* element_object = NULL;
	Packet_structure *packet_object;
	unsigned int sleep_time;
	while(1)
	{
		if(pthread_mutex_lock(&mutex_object))
		{
			fprintf(stderr, "Error in acquiring mutex lock - %s\n", strerror(errno));
			return(0);
		}
		pthread_cleanup_push(cleanup_mutex, &mutex_object);
      	
		while(My402ListEmpty(&queue_2))
		{
			if(served_packet_number <= 0 || terminate == 1)
			{
				break;
			}
			pthread_cond_wait(&conditional_variable, &mutex_object);
		}

		serve_packet_flag = false;

		if(served_packet_number)
		{
			element_object = My402ListFirst(&queue_2);
			packet_object = (Packet_structure*)(element_object -> obj);
			sleep_time = packet_object->packet_service;
			My402ListUnlink(&queue_2, element_object);
			gettimeofday(&(packet_object->packet_leaves_Q2), NULL);
			fprintf(stdout, "%012.3fms: p%d leaves Q2, time in Q2 = %.3fms\n", calculate_interval_in_milliseconds(&emulation_start_time, &(packet_object->packet_leaves_Q2)), packet_object->packet_number, calculate_interval_in_milliseconds(&(packet_object->packet_enters_Q2), &(packet_object->packet_leaves_Q2)));
			serve_packet_flag = true;
		}

		pthread_cleanup_pop(0);
		if(pthread_mutex_unlock(&mutex_object))
		{
			fprintf(stderr, "Error in releasing mutex lock - %s\n", strerror(errno));
			return(0);
		}

		if(serve_packet_flag)
		{

			if(pthread_mutex_lock(&mutex_object))
			{
				fprintf(stderr, "Error in acquiring mutex lock - %s\n", strerror(errno));
				return(0);
			}
			pthread_cleanup_push(cleanup_mutex, &mutex_object);
			gettimeofday(&(packet_object->packet_enters_server), NULL);
			fprintf(stdout, "%012.3fms: p%d begins service at S%d, requesting %ums of service\n", calculate_interval_in_milliseconds(&emulation_start_time, &(packet_object->packet_enters_server)), packet_object->packet_number, (int)param, (packet_object -> packet_service)/1000);
			pthread_cleanup_pop(0);
			if(pthread_mutex_unlock(&mutex_object))
			{
				fprintf(stderr, "Error in releasing mutex lock - %s\n", strerror(errno));
				return(0);
			}
			if(input_file_flag)
			{
				usleep(sleep_time);
			}
			else
			{
				if(service_rate <= 10000)
				{
					usleep(sleep_time);
				}
			}
			
			if(pthread_mutex_lock(&mutex_object))
			{
				fprintf(stderr, "Error in acquiring mutex lock - %s\n", strerror(errno));
				return(0);
			}
			pthread_cleanup_push(cleanup_mutex, &mutex_object);
			gettimeofday(&(packet_object->packet_leaves_server), NULL);
			fprintf(stdout, "%012.3fms: p%d departs from S%d, service time = %.3fms, time in system = %.3fms\n", calculate_interval_in_milliseconds(&emulation_start_time, &(packet_object->packet_leaves_server)), packet_object->packet_number, (int)param, calculate_interval_in_milliseconds(&(packet_object->packet_enters_server), &(packet_object->packet_leaves_server)), calculate_interval_in_milliseconds(&(packet_object->packet_enters_Q1), &(packet_object->packet_leaves_server)));
		
			total_packet_service_time += calculate_interval(&(packet_object->packet_enters_server),&(packet_object->packet_leaves_server));
      		total_time_in_Q1 += calculate_interval(&(packet_object->packet_enters_Q1), &(packet_object->packet_leaves_Q1));
      		total_time_in_Q2 += calculate_interval(&(packet_object->packet_enters_Q2), &(packet_object->packet_leaves_Q2));
      		if((int)param == 1)
      		{
      			total_time_in_S1 += calculate_interval(&(packet_object->packet_enters_server),&(packet_object->packet_leaves_server));
      		}
      		else
      		{
      			total_time_in_S2 += calculate_interval(&(packet_object->packet_enters_server),&(packet_object->packet_leaves_server));
      		}
      		total_time_in_System += calculate_interval(&(packet_object->packet_created_time), &(packet_object->packet_leaves_server));
      		if(serviced > initial_capacity)
      		{
      			initial_capacity += 1000;
      			total_time_data = (unsigned long int*)realloc(total_time_data, sizeof(unsigned long int) * initial_capacity);
      		}
      		*(total_time_data+serviced) = calculate_interval(&(packet_object->packet_created_time), &(packet_object->packet_leaves_server));

      		serviced++;
      		free(packet_object);
      		served_packet_number--;
      	
      		pthread_cleanup_pop(0);
      		if(pthread_mutex_unlock(&mutex_object))
			{
				fprintf(stderr, "Error in releasing mutex lock - %s\n", strerror(errno));
				return(0);
			}
		}

		if(served_packet_number <= 0)
      	{
      		pthread_cond_broadcast(&conditional_variable);
      		pthread_cancel(token_depositor_thread);
      		break;
      	}
	}
	return 0;
}

void read_number_of_packets()
{
	char buffer[1030];
	buffer[1024] = '\0';

	fgets(buffer, 1030, fp);
	if(buffer[1024] != '\0')
	{
		fprintf(stderr, "Error in the input file:%s, Line contains more than 1024 characters!\n", filename);
		cleanup();
	}
	int buf_ptr = 0;
	if(buffer[buf_ptr] == ' ' || buffer[buf_ptr] == '\t')
	{
		fprintf(stderr, "Error in line 1 of input file:%s, Line should not contain leading spaces or tabs!\n", filename);
		cleanup();
	}

	while(buffer[buf_ptr] != '\0' && buffer[buf_ptr] != '\n')
	{
		if(buffer[buf_ptr] > '9' || buffer[buf_ptr] < '0')
		{
			fprintf(stderr, "Error in line 1 of input file:%s, It contains characters other than numbers!\n", filename);
			cleanup();
		}
		buf_ptr++;
	}
	unsigned int n;
	sscanf(buffer, "%u", &n);
	if(n <= 0 || n > 2147483647)
	{
		fprintf(stderr, "Error in the file:%s with value of number of packets\nIt should be a positive integer with maximum value 2147483647!\n", filename);
		cleanup();
	}
	num_packets = (int)n;
}

void read_commandline_arguments(int argc, char *argv[])
{
	int i, j;
	bool default_r = true, default_b = true, default_lambda = true, default_mu = true, default_p = true, default_n = true;
	j = 1;
	while(j < argc)
	{
		if(token_arrival_rate && token_bucket_depth)
		{
			break;
		}
		if(strcmp(argv[j], "-r") == 0)
		{
			default_r = false;
			if(j == argc-1 || argv[j+1][0] == '-')
			{
				fprintf(stderr, "Error in the command: Missing value for -r(token arrival rate) or the value is negative!\n");
				exit(-1);
			}
			int arg = 0;
			while(argv[j+1][arg] != '\0')
			{
				if(argv[j+1][arg] > '9' || argv[j+1][arg] < '0')
				{
					if(argv[j+1][arg] != '.')
					{
						fprintf(stderr, "Error in the command: Token arrival rate value should contain real numbers and not any other character!\n");
						exit(-1);
					}
				}
				arg++;
			}
			sscanf(argv[j+1], "%lf", &token_arrival_rate);
			if(token_arrival_rate < 0.0)
			{
				fprintf(stderr, "Error in the command: Token arrival rate should be positive value!\n");
				exit(-1);
			}

			double r = 1/token_arrival_rate;
			if(r > 10.0)
			{
				r = 10.0;
			}
			inter_token_arrival_time = (unsigned int)(r * 1000000);
			j += 2;
		}
		else if(strcmp(argv[j], "-B") == 0)
		{
			default_b = false;
			if(j == argc-1 || argv[j+1][0] == '-')
			{
				fprintf(stderr, "Error in the command: Missing value for -B(token bucket depth) or the value is negative!\n");
				exit(-1);
			}
			int arg = 0;
			while(argv[j+1][arg] != '\0')
			{
				if(argv[j+1][arg] > '9' || argv[j+1][arg] < '0')
				{
					fprintf(stderr, "Error in the command: Token bucket depth value should be an integer!\n");
					exit(-1);
				}
				arg++;
			}
			unsigned int b;
			sscanf(argv[j+1], "%u", &b);
			if(b <= 0 || b > 2147483647)
			{
				fprintf(stderr, "Error in command: Token bucket depth value should be a positive integer with maximum value 2147483647!\n");
				exit(-1);
			}
			token_bucket_depth = (int)b;
			j += 2;
		}
		else
		{
			j++;
		}
	}
	for(i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "-t") == 0)
		{
			input_file_flag = true;
			break;
		}
	}
	if(input_file_flag)
	{
		filename = argv[i+1];
		struct stat file_status;
		stat(filename, &file_status);
		if(S_ISDIR(file_status.st_mode))
        {
            fprintf(stderr, "Error: Input file %s is a directory!\n", filename);
            exit(-1);
        }
		if((fp = fopen(filename, "r")) == NULL)
		{
			fprintf(stderr, "Error in opening the file:%s\n", filename);
			perror("");
			exit(-1);
		}
		read_number_of_packets();
	}
	else
	{
		int c = 1;
		while(c < argc)
		{
			if(strcmp(argv[c], "-lambda") == 0)
			{
				default_lambda = false;
				if(c == argc-1 || argv[c+1][0] == '-')
				{
					fprintf(stderr, "Error in the command: Missing value for -lambda(packet arrival rate) or the value is negative!\nUsage Information: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
					exit(-1);
				}
				int arg = 0;
				while(argv[c+1][arg] != '\0')
				{
					if(argv[c+1][arg] > '9' || argv[c+1][arg] < '0')
					{
						if(argv[c+1][arg] != '.')
						{
							fprintf(stderr, "Error in the command: Packet arrival rate value should contain real numbers and not any other character!\n");
							exit(-1);
						}
					}
					arg++;
				}
				sscanf(argv[c+1], "%lf", &packet_arrival_rate);
				if(packet_arrival_rate < 0.0)
				{
					fprintf(stderr, "Error in the command: Packet arrival rate should be positive value!\n");
					exit(-1);
				}
				
				double lambda = 1/packet_arrival_rate;
				if(lambda > 10.0)
				{
					lambda = 10.0;
				}
				inter_packet_arrival_time = (unsigned int)(lambda * 1000000);
				
				c += 2;
			}
			else if(strcmp(argv[c], "-mu") == 0)
			{
				default_mu = false;
				if(c == argc-1 || argv[c+1][0] == '-')
				{
					fprintf(stderr, "Error in the command: Missing value for -mu(service rate) or the value is negative!\nUsage Information: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
					exit(-1);
				}
				int arg = 0;
				while(argv[c+1][arg] != '\0')
				{
					if(argv[c+1][arg] > '9' || argv[c+1][arg] < '0')
					{
						if(argv[c+1][arg] != '.')
						{
							fprintf(stderr, "Error in the command: Service rate value should contain real numbers and not any other character!\n");
							exit(-1);
						}
					}
					arg++;
				}
				sscanf(argv[c+1], "%lf", &service_rate);
				if(service_rate < 0.0)
				{
					fprintf(stderr, "Error in the command: Service rate should be positive value!\n");
					exit(-1);
				}
				
				double mu = 1/service_rate;
				if(mu > 10.0)
				{
					mu = 10.0;
				}
				inter_service_time = (unsigned int)(mu * 1000000);
				
				c += 2;
			}
			else if(strcmp(argv[c], "-P") == 0)
			{
				default_p = false;
				if(c == argc-1 || argv[c+1][0] == '-')
				{
					fprintf(stderr, "Error in the command: Missing value for -p(packet requirement) or the value is negative!\nUsage Information: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
					exit(-1);
				}
				int arg = 0;
				while(argv[c+1][arg] != '\0')
				{
					if(argv[c+1][arg] > '9' || argv[c+1][arg] < '0')
					{
						fprintf(stderr, "Error in the command: Packet requirement value should contain only numbers and not any other character!\n");
						exit(-1);
					}
					arg++;
				}
				unsigned int req;
				sscanf(argv[c+1], "%u", &req);
				if(req <= 0 || req > 2147483647)
				{
					fprintf(stderr, "Error in the command: Packet requirement should be a positive integer with maximum value 2147483647!\n");
					exit(-1);
				}
				packet_requirement = (int)req;
				c += 2;
			}
			else if(strcmp(argv[c], "-n") == 0)
			{
				default_n = false;
				if(c == argc-1 || argv[c+1][0] == '-')
				{
					fprintf(stderr, "Error in the command: Missing value for -n(number of packets) or the value is negative!\nUsage Information: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
					exit(-1);
				}
				int arg = 0;
				while(argv[c+1][arg] != '\0')
				{
					if(argv[c+1][arg] > '9' || argv[c+1][arg] < '0')
					{
						fprintf(stderr, "Error in the command: Number of packets value should contain only numbers and not any other character!\n");
						exit(-1);
					}
					arg++;
				}
				unsigned int n;
				sscanf(argv[c+1], "%u", &n);
				if(n <= 0 || n > 2147483647)
				{
					fprintf(stderr, "Error in the command: Number of packets value should be a positive integer with maximum value 2147483647!\n");
					exit(-1);
				}
				num_packets = (int)n;
				c += 2;
			}
			else
			{
				if(strcmp(argv[c], "-r") == 0 || strcmp(argv[c], "-B") == 0)
				{
					c += 2;
				}
				else
				{
					fprintf(stderr, "Error in the command: Incorrect Usage.\nUsage Information: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
					exit(-1);
				}	
			}
		}
	}

	if(default_r)
	{
		token_arrival_rate = 1.5;
		double r = 1/token_arrival_rate;
		inter_token_arrival_time = (unsigned int)(r * 1000000);
	}
	if(default_b)
	{
		token_bucket_depth = 10;
	}
	
	if(input_file_flag)
	{
		fprintf(stdout, "Emulation Parameters:\n\tnumber to arrive = %d\n\tr = %g\n\tB = %d\n\ttsfile = %s\n", num_packets, token_arrival_rate, token_bucket_depth, filename);
	}
	else
	{
		if(default_n)
		{
			num_packets = 20;
		}
		if(default_p)
		{
			packet_requirement = 3;
		}
		if(default_lambda)
		{
			packet_arrival_rate = 1.0;
			double lambda = 1/packet_arrival_rate;
			inter_packet_arrival_time = (unsigned int)(lambda * 1000000);
		}
		if(default_mu)
		{
			service_rate = 0.35;
			double mu = 1/service_rate;
			inter_service_time = (unsigned int)(mu * 1000000);
		}
		fprintf(stdout, "Emulation Parameters:\n\tnumber to arrive = %d\n\tlambda = %g\n\tmu = %g\n\tr = %g\n\tB = %d\n\tP = %d\n", num_packets, packet_arrival_rate, service_rate, token_arrival_rate, token_bucket_depth, packet_requirement);
	}
	served_packet_number = num_packets;
	total_time_data = (unsigned long int*)malloc(sizeof(unsigned long int) * initial_capacity);
}

double standardDeviation()
{
	double average = (total_time_in_System / (double)serviced)/1000000.0;
 	double x_square_average = 0.0;
  	int i = 0;
  	for(i = 0; i < serviced; i++){
  		double x_square =  ((double)(*(total_time_data+i))/1000000.0 - average)*((double)(*(total_time_data+i))/1000000.0 - average);
    	x_square_average = x_square_average + x_square;
  	}
  	double std;
  	if(x_square_average > 0)
  	{
  		std = sqrt(x_square_average/(double)serviced);
  	}
  	else
  	{
  		std = 0;
  	}
  	return std;
}

void calculate_and_print_statistics()
{
	unsigned int total_simulation_time = calculate_interval(&emulation_start_time, &emulation_end_time);
	if(packets_entering_filter != 0)
	{
		double average_packet_inter_arrival_time = 0.00;
		average_packet_inter_arrival_time = (total_packet_inter_arrival_time/packets_entering_filter)/1000000.0;
		fprintf(stdout, "\taverage packet inter-arrival time = %.6gs\n", average_packet_inter_arrival_time);
	}
	else
	{
		fprintf(stdout, "\taverage packet inter-arrival time = N/A (no packet arrived)\n");;
	}
	if(serviced != 0)
	{
		double average_packet_service_time = 0.00;
		average_packet_service_time = (total_packet_service_time/serviced)/1000000.0;
		fprintf(stdout, "\taverage packet service time = %.6gs\n\n", average_packet_service_time);
	}
	else
	{
		fprintf(stdout, "\taverage packet service time = N/A (no packet served)\n");
	}
	if(packets_entering_filter != 0)
	{
		double average_packets_in_Q1 = 0.00;
		average_packets_in_Q1 = (total_time_in_Q1/(double)total_simulation_time);
		fprintf(stdout, "\taverage number of packets in Q1 = %.6g\n", average_packets_in_Q1);
	
		double average_packets_in_Q2 = 0.00;
		average_packets_in_Q2 = (total_time_in_Q2/(double)total_simulation_time);
		fprintf(stdout, "\taverage number of packets in Q2 = %.6g\n", average_packets_in_Q2);

		if(serviced != 0)
		{
			double average_packets_in_S1 = 0.00;
			average_packets_in_S1 = (total_time_in_S1/(double)total_simulation_time);
			fprintf(stdout, "\taverage number of packets in S1 = %.6g\n", average_packets_in_S1);
		
			double average_packets_in_S2 = 0.00;
			average_packets_in_S2 = (total_time_in_S2/(double)total_simulation_time);
			fprintf(stdout, "\taverage number of packets in S2 = %.6g\n\n", average_packets_in_S2);
	
			double average_time_spent_in_system = 0.00;
			average_time_spent_in_system = (total_time_in_System/(double)serviced);
			fprintf(stdout, "\taverage time a packet spent in system = %.6g\n", average_time_spent_in_system/1000000.0);
	
			fprintf(stdout, "\tstandard deviation for time spent in system = %.6g\n\n", standardDeviation());
		}
		else
		{
			fprintf(stdout, "\taverage number of packets in S1 = N/A (no packet served)\n");
			fprintf(stdout, "\taverage number of packets in S2 = N/A (no packet served)\n");
			fprintf(stdout, "\taverage time a packet spent in system = N/A (no packet served)\n");
			fprintf(stdout, "\tstandard deviation for time spent in system = N/A (no packet served)\n\n");
		}		
	}
	else
	{
		fprintf(stdout, "\taverage number of packets in Q1 = %.6g\n", (double)(0));
		fprintf(stdout, "\taverage number of packets in Q2 = %.6g\n", (double)(0));
		fprintf(stdout, "\taverage number of packets in S1 = %.6g\n", (double)(0));
		fprintf(stdout, "\taverage number of packets in S2 = %.6g\n\n", (double)(0));
		fprintf(stdout, "\taverage time a packet spent in system = N/A (no packet arrived)\n");
		fprintf(stdout, "\tstandard deviation for time spent in system = N/A (no packet arrived)\n\n");
	}
	
	if(tokens_entering_filter != 0)
	{
		fprintf(stdout, "\ttoken drop probability = %.6g\n", (tokens_dropped/(double)tokens_entering_filter));
	}
	else
	{
		fprintf(stdout, "\ttoken drop probability = N/A (no token has arrived)\n");
	}
	if(packets_entering_filter != 0)
	{
		fprintf(stdout, "\tpacket drop probability = %.6g\n", (packets_dropped/(double)packets_entering_filter));
	}
	else
	{
		fprintf(stdout, "\tpacket drop probability = N/A (no packet has arrived)\n");
	}
}

int main(int argc, char *argv[])
{
	memset(&queue_1, 0, sizeof(My402List));
	(void)My402ListInit(&queue_1);
	memset(&queue_2, 0, sizeof(My402List));
	(void)My402ListInit(&queue_2);

	sigemptyset(&set_interrupt);
	sigaddset(&set_interrupt, SIGINT);
	sigprocmask(SIG_BLOCK, &set_interrupt, 0);

	read_commandline_arguments(argc, argv);
	gettimeofday(&emulation_start_time, NULL);
	fprintf(stdout, "%012.3fms: emulation begins\n", (double)calculate_interval(&emulation_start_time, &emulation_start_time)/1000.0);

	if(pthread_create(&packet_generator_thread, 0, generate_packets, NULL))
	{
		fprintf(stderr, "Thread Creation Failure! - %s\n", strerror(errno));
		exit(-1);
	}

	if(pthread_create(&token_depositor_thread, 0, deposit_tokens, NULL))
	{
		fprintf(stderr, "Thread Creation Failure! - %s\n", strerror(errno));
		exit(-1);
	}

	if(pthread_create(&server_1_thread, 0, service_packets, (void *)1))
	{
		fprintf(stderr, "Thread Creation Failure! - %s\n", strerror(errno));
		exit(-1);
	}

	if(pthread_create(&server_2_thread, 0, service_packets, (void *)2))
	{
		fprintf(stderr, "Thread Creation Failure! - %s\n", strerror(errno));
		exit(-1);
	}

	if(pthread_create(&cntrl_C_thread, 0, handle_interrupt, 0))
	{
		fprintf(stderr, "Thread Creation Failure! - %s\n", strerror(errno));
		exit(-1);
	}

	pthread_join(packet_generator_thread, NULL);
	pthread_join(token_depositor_thread, NULL);
	pthread_join(server_1_thread, NULL);
	pthread_join(server_2_thread, NULL);

	if(terminate)
	{
		My402ListElem *element_object = NULL;
    	for(element_object = My402ListFirst(&queue_2); element_object != NULL; element_object = My402ListNext(&queue_2, element_object))
    	{
    		struct timeval present_time;
    		gettimeofday(&present_time, NULL);
      		fprintf(stdout, "%012.3fms: p%d removed from Q2\n",calculate_interval_in_milliseconds(&emulation_start_time, &present_time), ((Packet_structure*)(element_object->obj))->packet_number);
    	}
    	My402ListUnlinkAll(&queue_2);
    	for(element_object = My402ListFirst(&queue_1); element_object != NULL; element_object = My402ListNext(&queue_1, element_object))
    	{
    		struct timeval present_time;
    		gettimeofday(&present_time, NULL);
      		fprintf(stdout, "%012.3fms: p%d removed from Q1\n",calculate_interval_in_milliseconds(&emulation_start_time, &present_time), ((Packet_structure*)(element_object->obj))->packet_number);
    	}
    	My402ListUnlinkAll(&queue_1);
	}

	gettimeofday(&emulation_end_time, NULL);
	fprintf(stdout, "%012.3fms: emulation ends\n\n", calculate_interval_in_milliseconds(&emulation_start_time, &emulation_end_time));
	fprintf(stdout, "Statistics:\n\n");
	calculate_and_print_statistics();
	if(fp != NULL)
	{
		fclose(fp);
	}
	pthread_mutex_destroy(&mutex_object);
	pthread_cond_destroy(&conditional_variable);
	free(total_time_data);
}