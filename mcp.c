/* Master Control Program for BareMetal Node */
/* Written by Ian Seyler of Return Infinity */

// Compile: gcc mcp.c -o mcp -Wall

// Ethernet Frame format
// | 00 01 02 03 04 05 | 06 07 08 09 0A 0B | 0C 0D | 0E 0F |
// | Destination MAC   | Source MAC        | Ether | Data  |
// EtherID - 16 bit value
// 0xABBA - Command
// 0xABBB - Data
// Instruction - 16 bit value
// Bits 10 - 0 are the total packet size - max packet size is 1518 in total
// Bits 15 - 12 encode the instuction - space for 16 commands

/* Global Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <fcntl.h>

/* Global defines */
#undef ETH_FRAME_LEN
#define ETH_FRAME_LEN 1518
#define maxnodes 16

/* Global variables */
unsigned char src_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // server address
unsigned char dst_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // node address
unsigned char dst_broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned char node_ID[maxnodes][7];
unsigned short node_Speed[maxnodes];
unsigned short node_NumCores[maxnodes];
unsigned short node_Memory[maxnodes];
unsigned int node_HDDSize[maxnodes];
unsigned long long node_Result[maxnodes];
unsigned char wordcount;
int c, c2, s, i, running=1, filesize, fileoffset, bytecount, args, broadcastflag, tint, tint2;
int nodes=0, nodeid, nodevar;
unsigned long long memlocation, variable, tempresult, finalresult=0;
struct sockaddr_ll sa;
unsigned char* buffer;
char userinput[160], command[20], argument1[20], argument2[20], filename[20], parameters[1024];
char s_discover[] = "discover";
char s_dispatch[] = "dispatch";
char s_execute[] = "execute";
char s_exit[] = "exit";
char s_help[] = "help";
char s_list[] = "list";
char s_stop[] = "stop";
char s_parameters[] = "parameters";
char s_reboot[] = "reboot";
char s_reset[] = "reset";
char s_results[] = "results";
clock_t wait, timer1, timer2;
fd_set selectlist;
struct ifreq ifr;
FILE *file;

/* Main code */
int main(int argc, char *argv[])
{
	printf("\nMaster Control Program for BareMetal Node v0.1\n");
	printf("© 2017 Return Infinity, Inc.\n\n");

	/* first argument needs to be a NIC */
	if (argc < 2)
	{
		printf("Please specify an Ethernet device\n");
		exit(0);
	}

	/* open a socket in raw mode */
	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1)
	{
		printf("Error: Could not open socket! Check permissions\n");
		exit(1);
	}

	/* which interface to use? */
	memset(&ifr, 0, sizeof(struct ifreq)); // sizeof(ifr)?
	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);

	/* does the interface exist? */
	if (ioctl(s, SIOCGIFINDEX, &ifr) == -1)
	{
		printf("No such interface: %s\n", argv[1]);
		close(s);
		exit(1);
	}

	/* is the interface up? */
	ioctl(s, SIOCGIFFLAGS, &ifr);
	if ((ifr.ifr_flags & IFF_UP) == 0)
	{
		printf("Interface %s is down\n", argv[1]);
		close(s);
		exit(1);
	}

	/* configure the port for non-blocking */
	if (-1 == fcntl(s, F_SETFL, O_NONBLOCK))
	{
		printf("fcntl (NonBlocking) Warning\n");
		close(s);
		exit(1);
	}

	/* get the MAC address */
	ioctl(s, SIOCGIFHWADDR, &ifr);
	for (c=0; c<6; c++)
	{
		src_MAC[c] = (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[c]; // WTF
	}

	/* just write in the structure again */
	ioctl(s, SIOCGIFINDEX, &ifr);

	/* well we need this to work */
	memset(&sa, 0, sizeof (sa));
	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = ifr.ifr_ifindex;
	sa.sll_protocol = htons(ETH_P_ALL);

	printf("This server is: %02X:%02X:%02X:%02X:%02X:%02X\n\n", src_MAC[0], src_MAC[1], src_MAC[2], src_MAC[3], src_MAC[4], src_MAC[5]);
	buffer = (void*)malloc(ETH_FRAME_LEN);

	while(running == 1)
	{
		printf("> ");		// Print the prompt

		memset(command, 0, 20);
		fgets(userinput, 100, stdin);	// Get up to 100 chars from the keyboard

		sscanf(userinput, "%s", command);	// grab the first word in the string

// DISCOVER
		if (strcasecmp(s_discover, command) == 0)
		{
			printf("Discovering nodes... Waiting 3 seconds");
			fflush(stdout);
			nodes = 0;
			memcpy((void*)buffer, (void*)dst_broadcast, 6);
			memcpy((void*)(buffer+6), (void*)src_MAC, 6);
			buffer[12] = 0xAB;
			buffer[13] = 0xBA;
			buffer[14] = 0x00;
			buffer[15] = 0x10;
			c = sendto(s, buffer, 64, 0, (struct sockaddr *)&sa, sizeof (sa));
			wait = time(NULL) + 3; // Current time + 3 seconds
			while ((time(NULL) < wait) & (nodes < maxnodes))
			{
				for (c=0; c<ETH_FRAME_LEN; c++)
				{
					buffer[c] = 0x00;
				}
				c = recvfrom(s, buffer, ETH_FRAME_LEN, 0, 0, 0);
				if ((buffer[12] == 0xAB) & (buffer[13] == 0xBA) & (buffer[15] == 0x10))
				{
					node_ID[nodes][0] = buffer[6];
					node_ID[nodes][1] = buffer[7];
					node_ID[nodes][2] = buffer[8];
					node_ID[nodes][3] = buffer[9];
					node_ID[nodes][4] = buffer[10];
					node_ID[nodes][5] = buffer[11];
					memcpy(&node_NumCores[nodes], &buffer[16], 2);
					memcpy(&node_Speed[nodes], &buffer[18], 2);
					memcpy(&node_Memory[nodes], &buffer[20], 2);
					memcpy(&node_HDDSize[nodes], &buffer[22], 4);
					nodes++;
					printf("\nID %02d - MAC %02X:%02X:%02X:%02X:%02X:%02X - %d Cores at %d MHz, %d MiB RAM, %d MiB HDD", nodes, buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11], node_NumCores[nodes-1], node_Speed[nodes-1], node_Memory[nodes-1], node_HDDSize[nodes-1]);
					fflush(stdout);
				}
			}
			printf("\n%d nodes discovered.\n", nodes);
		}
		else if (strcasecmp(s_exit, command) == 0)
		{
//			printf("Exiting.");
			running = 0;
		}
		else if (strcasecmp(s_list, command) == 0)
		{
			if (nodes == 0)
				printf("No nodes. Run discover first.\n");
			else
			{
				printf("Current nodes:\n");
				for (c=0; c<nodes; c++)
				{
					printf("ID %02d - MAC %02X:%02X:%02X:%02X:%02X:%02X - %d Cores at %d MHz, %d MiB RAM, %d MiB HDD\n", c+1, node_ID[c][0], node_ID[c][1], node_ID[c][2], node_ID[c][3], node_ID[c][4], node_ID[c][5], node_NumCores[c], node_Speed[c], node_Memory[c], node_HDDSize[c]);
				}
			}
		}
// DISPATCH
		else if (strcasecmp(s_dispatch, command) == 0)
		{
			args = sscanf(userinput, "%*s %s %s", argument1, filename);
			nodeid = atoi(argument1);
			if (args != 2 || (nodeid <= 0 && argument1[0] != '*'))
			{
				printf("dispatch: Incorrect or insufficient arguments\n");
				printf("          usage is \"dispatch NodeID Filename\"\n");
			}
			else
			{
				if (argument1[0] == '*')
				{
					broadcastflag = 1;
				}
				else
				{
					broadcastflag = 0;
					nodeid--;
				}

				if ((file = fopen(filename, "rb")) == NULL)
				{
					printf("Error opening file '%s'", filename);
				}
				else
				{
					fseek(file, 0, SEEK_END);
					filesize = ftell(file);
					rewind(file);
					memlocation = 0xFFFF800000200000;
					fileoffset = 0;

					if (broadcastflag == 0)
					{
//						printf("Dispatching program to node %d... ", nodeid+1);
//						fflush(stdout);
						memcpy((void*)buffer, (void*)node_ID[nodeid], 6);
					}
					else
					{
//						printf("Dispatching program to all nodes... ");
//						fflush(stdout);
						memcpy((void*)buffer, (void*)dst_broadcast, 6);
					}

					memcpy((void*)(buffer+6), (void*)src_MAC, 6);
					buffer[12] = 0xAB;
					buffer[13] = 0xBA;
					buffer[14] = 0x00;
					buffer[15] = 0x20;

					while (fileoffset < filesize)
					{
						memcpy((void*)(buffer+16), &memlocation, 8);
						memset(buffer+24, 0, 1024);
						fread((char *)(buffer+24), 1024, 1, file);
						c = sendto(s, buffer, 1024+24, 0, (struct sockaddr *)&sa, sizeof (sa));
						memlocation += 1024;
						fileoffset += 1024;
						usleep(10000); // sleep 1 ms
					}

//					printf("Complete");
					fclose(file);
				}
			}
		}
// PARAMETERS
		else if (strcasecmp(s_parameters, command) == 0)
		{
			args = sscanf(userinput, "%*s %d", &nodeid);
			if (args != 1)
			{
				printf("parameters: Incorrect or insufficient arguments\n");
				printf("            usage is \"parameters NodeID\"\n");
			}
			else
			{
				printf("Data: ");
				fgets(userinput, 100, stdin);	// Get up to 100 chars from the keyboard
				// remove extra spaces from userinput
				strcpy(parameters, filename);
				strcat(parameters, " ");
				strcat(parameters, userinput);
				wordcount = 1;
				for (i=0; i<1024; i++)
				{
					if (parameters[i] == 0x20)
					{
						parameters[i] = 0x00;
						wordcount++;
					}
				}
				nodeid--;
				memcpy((void*)buffer, (void*)node_ID[nodeid], 6);
				memcpy((void*)(buffer+6), (void*)src_MAC, 6);
				buffer[12] = 0xAB;
				buffer[13] = 0xBA;
				buffer[14] = 0x00;
				buffer[15] = 0x30;
				buffer[16] = wordcount;			// Word count of string
				memcpy((void*)(buffer+17), &parameters, 100);
//				printf("Sending parameters to node %d... ", nodeid+1);
				c = sendto(s, buffer, 125, 0, (struct sockaddr *)&sa, sizeof (sa));
//				printf("Complete");
			}
		}
// EXECUTE
		else if (strcasecmp(s_execute, command) == 0)
		{
			argument2[0] = 0x00;
			args = sscanf(userinput, "%*s %s %s", argument1, argument2);
			nodeid = atoi(argument1);
			if (args < 1 || (nodeid <= 0 && argument1[0] != '*')) //if (args != 1)
			{
				printf("execute: Incorrect or insufficient arguments\n");
				printf("         usage is \"execute NodeID\"\n");
			}
			else
			{
				if (argument1[0] == '*')
				{
					broadcastflag = 1;
				}
				else
				{
					broadcastflag = 0;
					nodeid--;
				}

				if (broadcastflag == 0)
				{
//					printf("Executing program on node %d... ", nodeid+1);
					memcpy((void*)buffer, (void*)node_ID[nodeid], 6);
				}
				else
				{
//					printf("Executing program on all nodes... ");
					memcpy((void*)buffer, (void*)dst_broadcast, 6);
				}

				memcpy((void*)(buffer+6), (void*)src_MAC, 6);
				buffer[12] = 0xAB;
				buffer[13] = 0xBA;
				buffer[14] = 0x00;
				buffer[15] = 0x40;
				c = sendto(s, buffer, 16, 0, (struct sockaddr *)&sa, sizeof (sa));
//				printf("Complete");

				if (argument2[0] != 'n')
				{
					timer1 = time(NULL);
					if (broadcastflag == 0)
					{
						printf("Waiting for result from node %d... ", nodeid+1);
						fflush(stdout);
						node_Result[nodeid] = 0;
						while (node_Result[nodeid] == 0)
						{
							for (c=0; c<ETH_FRAME_LEN; c++)
							{
								buffer[c] = 0x00;
							}
							c = recvfrom(s, buffer, ETH_FRAME_LEN, 0, 0, 0);
							if ((buffer[12] == 0xAB) & (buffer[13] == 0xBB))
							{
								memcpy(&node_Result[nodeid], &buffer[14], 8);
							}
						}
						printf("Complete\n");
					}
					else
					{
						printf("Waiting for results from all nodes... ");
						fflush(stdout);
						for (tint=0; tint<maxnodes; tint++)
						{
							node_Result[tint] = 0;
						}
						tint = 0;
						while (tint < nodes)
						{
							for (c=0; c<ETH_FRAME_LEN; c++)
							{
								buffer[c] = 0x00;
							}
							c = recvfrom(s, buffer, ETH_FRAME_LEN, 0, 0, 0);
							if ((buffer[12] == 0xAB) & (buffer[13] == 0xBB))
							{
								// Which node is it from?
								for (tint2=0; tint2<nodes; tint2++)
								{
									if ((buffer[6] == node_ID[tint2][0]) & (buffer[7] == node_ID[tint2][1]) & (buffer[8] == node_ID[tint2][2]) & (buffer[9] == node_ID[tint2][3]) & (buffer[10] == node_ID[tint2][4]) & (buffer[11] == node_ID[tint2][5]))
									{
										memcpy(&node_Result[tint2], &buffer[14], 8);
									}
								}
								tint++;
							}
						}
						printf("Complete\n");
					}

					timer2 = time(NULL);
//					printf("\nCompiled result is: %ld", finalresult+1);
					printf("Elapsed time: %.0lf seconds\n", difftime(timer2, timer1));
				}
			}
		}
// STOP
		else if (strcasecmp(s_stop, command) == 0)
		{
			args = sscanf(userinput, "%*s %d", &nodeid);
			if (args != 1)
			{
				printf("stop: Incorrect or insufficient arguments\n");
				printf("      usage is \"stop NodeID\"\n");
			}
			else
			{
				nodeid--;
//				printf("Sending command to node %d... ", nodeid+1);
				memcpy((void*)buffer, (void*)node_ID[nodeid], 6);
				memcpy((void*)(buffer+6), (void*)src_MAC, 6);
				buffer[12] = 0xAB;
				buffer[13] = 0xBA;
				buffer[14] = 0x00;
				buffer[15] = 0x60;
				for (i=0; i<10; i++)
					c = sendto(s, buffer, 64, 0, (struct sockaddr *)&sa, sizeof (sa));
//				printf("Complete");
			}
		}
// RESET
		else if (strcasecmp(s_reset, command) == 0)
		{
			args = sscanf(userinput, "%*s %d", &nodeid);
			if (args != 1)
			{
				printf("reset: Incorrect or insufficient arguments\n");
				printf("       usage is \"reset NodeID\"\n");
			}
			else
			{
				nodeid--;
//				printf("Sending command to node %d... ", nodeid+1);
				memcpy((void*)buffer, (void*)node_ID[nodeid], 6);
				memcpy((void*)(buffer+6), (void*)src_MAC, 6);
				buffer[12] = 0xAB;
				buffer[13] = 0xBA;
				buffer[14] = 0x00;
				buffer[15] = 0x70;
				c = sendto(s, buffer, 16, 0, (struct sockaddr *)&sa, sizeof (sa));
//				printf("Complete");
			}
		}
// REBOOT
		else if (strcasecmp(s_reboot, command) == 0)
		{
			args = sscanf(userinput, "%*s %d", &nodeid);
			if (args != 1)
			{
				printf("reboot: Incorrect or insufficient arguments\n");
				printf("        usage is \"reboot NodeID\"\n");
			}
			else
			{
				nodeid--;
//				printf("Sending command to node %d... ", nodeid+1);
				memcpy((void*)buffer, (void*)node_ID[nodeid], 6);
				memcpy((void*)(buffer+6), (void*)src_MAC, 6);
				buffer[12] = 0xAB;
				buffer[13] = 0xBA;
				buffer[14] = 0x00;
				buffer[15] = 0x90;
				c = sendto(s, buffer, 16, 0, (struct sockaddr *)&sa, sizeof (sa));
//				printf("Complete");
			}
		}
// RESULTS
		else if (strcasecmp(s_results, command) == 0)
		{
			args = sscanf(userinput, "%*s %s", command);	// grab the second word in the string
			if (args < 1)
			{
				if (nodes == 0)
					printf("No nodes. Run discover first.");
				else
				{
					printf("Current results:\n");
					for (c=0; c<nodes; c++)
					{
						printf("ID %02d - Decimal:%lld\n", c+1, node_Result[c]);
					}
				}
			}
			else
			{

			}
		}
// HELP
		else if (strcasecmp(s_help, command) == 0)
		{
			args = sscanf(userinput, "%*s %s", command);	// grab the second word in the string
			if (args < 1)
			{
				printf("Available commands:\n");
				printf("discover, dispatch, execute, exit, list, parameters, reboot, reset, stop\n");
				printf("For more details: 'help command'\n");
			}
			else
			{
				if (strcasecmp(s_discover, command) == 0)
				{
					printf("discover:   Discover available nodes on the network\n");
				}
				else if (strcasecmp(s_dispatch, command) == 0)
				{
					printf("dispatch:   Dispatch a program to a node or all nodes");
					printf("\n            'dispatch * hello.app' - Send program to all nodes");
					printf("\n            'dispatch 2 hello.app' - Send program to node 2\n");
				}
				else if (strcasecmp(s_execute, command) == 0)
				{
					printf("execute:    Execute the program on the node or all nodes");
					printf("\n            'execute *' - Set all nodes to execute");
					printf("\n            'execute 2' - Set node 2 to execute\n");
				}
				else if (strcasecmp(s_parameters, command) == 0)
				{
					printf("parameters: Set the parameters on a node\n");
				}
				else if (strcasecmp(s_reboot, command) == 0)
				{
					printf("reboot:     Reboot a node\n");
				}
				else if (strcasecmp(s_reset, command) == 0)
				{
					printf("reset:      Reset a node\n");
				}
				else
				{
					printf("No details on that command\n");
				}
			}
		}
		else
		{
			if (strlen(userinput) > 1)
				printf("Unknown command\n");
		}
	}

//	printf("\n");
	close(s);
	return 0;
}

// EOF
