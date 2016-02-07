
#include <pthread.h>
#include "syscalls.h"

#define MAXCITY	200 
#define MAXBOUND	INT_MAX/2
#define MSG_SOFARBEST	2

void insert_node(int st_city, int to_node, int dist);
void *find_path_parallel(void *);
void find_path_from(int mytid, int memb_of[], int locally_bestpath[], int sub_path[], int cur_city, int cost);
int estimate_bound(int from_node, int memb_of[]);

int args[64] = {0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10, 11, 12, 13, 14, 15,
                16, 17, 18, 19 ,20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

int ncities;			// Actual no of cities

int dist_matrix[MAXCITY][MAXCITY];
				// Distance matrix entered
// 30 x 30 distances as linear array
//
int all_distances_inorder[900] = {
  -1  ,37   ,25   ,23    ,9   ,18   ,34   ,30   ,10   ,39   ,31   ,24   ,33   ,36   ,10   ,26   ,28   ,32   ,45   ,38 
  ,37   ,-1  ,19   ,13   ,31   ,20   ,14    ,6   ,31   ,15    ,5   ,25   ,17   ,22   ,28   ,25   ,10   ,10   ,13    ,5 
  ,25   ,19   ,-1  ,14   ,16   ,18    ,9   ,16   ,16   ,14   ,14    ,6    ,8   ,34   ,21   ,30    ,9   ,22   ,32   ,17 
  ,23   ,13   ,14   ,-1  ,18    ,7   ,17    ,7   ,19   ,21    ,9   ,19   ,18   ,20   ,14   ,16    ,8   ,10   ,22   ,16 
   ,9   ,31   ,16   ,18   ,-1  ,16   ,26   ,25    ,1   ,31   ,25   ,15   ,25   ,36   ,12   ,27   ,21   ,28   ,41   ,31 
  ,18   ,20   ,18    ,7   ,16   ,-1  ,23   ,13   ,17   ,28   ,16   ,22   ,24   ,20    ,7   ,12   ,15   ,14   ,27   ,23 
  ,34   ,14    ,9   ,17   ,26   ,23   ,-1  ,14   ,25    ,5   ,11   ,14    ,3   ,34   ,29   ,33    ,8   ,22   ,27   ,11 
  ,30    ,6   ,16    ,7   ,25   ,13   ,14   ,-1  ,25   ,17    ,3   ,22   ,17   ,19   ,21   ,19    ,7    ,7   ,16   ,10 
  ,10   ,31   ,16   ,19    ,1   ,17   ,25   ,25   ,-1  ,30   ,25   ,14   ,24   ,37   ,13   ,28   ,21   ,29   ,42   ,31 
  ,39   ,15   ,14   ,21   ,31   ,28    ,5   ,17   ,30   ,-1  ,13   ,19    ,7   ,36   ,34   ,37   ,13   ,24   ,26   ,10 
  ,31    ,5   ,14    ,9   ,25   ,16   ,11    ,3   ,25   ,13   ,-1  ,20   ,13   ,23   ,23   ,23    ,5   ,10   ,18    ,7 
  ,24   ,25    ,6   ,19   ,15   ,22   ,14   ,22   ,14   ,19   ,20   ,-1  ,12   ,39   ,23   ,34   ,15   ,28   ,38   ,23 
  ,33   ,17    ,8   ,18   ,25   ,24    ,3   ,17   ,24    ,7   ,13   ,12   ,-1  ,37   ,29   ,35   ,10   ,24   ,30   ,14 
  ,36   ,22   ,34   ,20   ,36   ,20   ,34   ,19   ,37   ,36   ,23   ,39   ,37   ,-1  ,25   ,10   ,26   ,12   ,18   ,27 
  ,10   ,28   ,21   ,14   ,12    ,7   ,29   ,21   ,13   ,34   ,23   ,23   ,29   ,25   ,-1  ,16   ,21   ,21   ,35   ,30 
  ,26   ,25   ,30   ,16   ,27   ,12   ,33   ,19   ,28   ,37   ,23   ,34   ,35   ,10   ,16   ,-1  ,24   ,15   ,26   ,29 
  ,28   ,10    ,9    ,8   ,21   ,15    ,8    ,7   ,21   ,13    ,5   ,15   ,10   ,26   ,21   ,24   ,-1  ,14   ,23   ,10 
  ,32   ,10   ,22   ,10   ,28   ,14   ,22    ,7   ,29   ,24   ,10   ,28   ,24   ,12   ,21   ,15   ,14   ,-1  ,13   ,15 
  ,45   ,13   ,32   ,22   ,41   ,27   ,27   ,16   ,42   ,26   ,18   ,38   ,30   ,18   ,35   ,26   ,23   ,13   ,-1  ,16 
  ,38    ,5   ,17   ,16   ,31   ,23   ,11   ,10   ,31   ,10    ,7   ,23   ,14   ,27   ,30   ,29   ,10   ,15   ,16   ,-1
};

/*
 -1,  -1,  -1,   146 , 190 , 240 ,-1  ,-1  ,-1   ,206,
  430 ,-1 , -1 ,  489, -1,   378,  464,  109,   90, -1, 
   91,  117, -1,    65,  321,  144, -1,   137,   95,  180,
  476,  367,  102, -1,   477,   83,  325,  171,  265,  388,
 -1,     3,  499,  439, -1,   137,  324, -1,   350,  383,
  377,  315,  108,  497, -1,  -1,   168,   41,   92, -1, 
 -1,   216,   95,  172,   82,  417, -1,   198,  423,  456,
 -1,  -1,   221, -1,  -1,   258, -1,  -1,  -1,   381,
  194, -1,   410,   51,  442,    0,  405, -1,  -1,   132,
  405, -1,   488,    2, -1,   361, -1,  -1,    83, -1 } ; */

/*
  -1,  19   ,15   ,12   ,13    ,7   ,12   ,15   ,25   ,13   ,10   ,14   ,10   ,20   ,12   ,11   ,19   ,10   ,11    ,0    ,5   ,28   ,23   ,18    ,6 
  ,19   ,-1  ,34    ,7   ,14   ,18   ,13   ,30   ,45   ,33   ,12    ,7   ,26    ,7   ,30   ,17   ,18   ,18   ,10   ,19   ,23   ,32   ,18   ,28   ,16 
  ,15   ,34   ,-1  ,27   ,23   ,21   ,27   ,10   ,14   ,10   ,23   ,28    ,9   ,36   ,13   ,25   ,33   ,24   ,26   ,15   ,15   ,38   ,37   ,17   ,21 
  ,12    ,7   ,27   ,-1   ,9   ,11    ,8   ,23   ,37   ,25    ,6    ,4   ,19   ,10   ,22   ,11   ,16   ,12    ,4   ,12   ,15   ,29   ,18   ,23    ,9 
  ,13   ,14   ,23    ,9   ,-1  ,17   ,17   ,17   ,36   ,26    ,4    ,7   ,14   ,19   ,25   ,19   ,25   ,19   ,13   ,13   ,18   ,37   ,27   ,14   ,14 
   ,7   ,18   ,21   ,11   ,17   ,-1   ,6   ,22   ,29   ,16   ,13   ,15   ,17   ,16   ,12    ,4   ,12    ,3    ,7    ,7    ,6   ,21   ,16   ,25    ,2 
  ,12   ,13   ,27    ,8   ,17    ,6   ,-1  ,27   ,35   ,22   ,12   ,12   ,22   ,10   ,19    ,4    ,8    ,5    ,3   ,12   ,13   ,21   ,11   ,28    ,6 
  ,15   ,30   ,10   ,23   ,17   ,22   ,27   ,-1  ,24   ,18   ,18   ,23    ,5   ,33   ,20   ,26   ,34   ,25   ,25   ,15   ,17   ,42   ,38    ,7   ,21
,25   ,45   ,14   ,37   ,36   ,29   ,35   ,24   ,-1  ,12   ,35   ,39   ,23   ,45   ,16   ,32   ,38   ,30   ,36, 25   ,22   ,38   ,42   ,31   ,29    ,13  
,33   ,10   ,25   ,26   ,16   ,22   ,18   ,12   ,-1  ,24   ,28   ,15   ,32    ,4   ,19   ,26   ,18   ,23   ,13   ,10   ,28   ,30   ,25   ,17 
  ,10   ,12   ,23    ,6    ,4   ,13   ,12   ,18   ,35   ,24   ,-1   ,5   ,14   ,16   ,22   ,15   ,21   ,15    ,9   ,10   ,15   ,33   ,23   ,17   ,10 
  ,14    ,7   ,28    ,4    ,7   ,15   ,12   ,23   ,39   ,28    ,5   ,-1  ,19   ,12   ,25   ,15   ,20   ,16    ,8   ,14   ,18   ,33   ,22   ,21   ,13 
  ,10   ,26    ,9   ,19   ,14   ,17   ,22    ,5   ,23   ,15   ,14   ,19   ,-1  ,29   ,16   ,21   ,29   ,20   ,20   ,10   ,13   ,37   ,33    ,9   ,16 
  ,20    ,7   ,36   ,10   ,19   ,16   ,10   ,33   ,45   ,32   ,16   ,12   ,29   ,-1  ,29   ,13   ,13   ,15    ,9   ,20   ,22   ,26   ,12   ,33   ,15 
  ,12   ,30   ,13   ,22   ,25   ,12   ,19   ,20   ,16    ,4   ,22   ,25   ,16   ,29   ,-1  ,15   ,21   ,14   ,20   ,12    ,7   ,24   ,26   ,26   ,13 
  ,11   ,17   ,25   ,11   ,19    ,4    ,4   ,26   ,32   ,19   ,15   ,15   ,21   ,13   ,15   ,-1   ,8    ,1    ,7   ,11   ,10   ,18   ,12   ,28    ,5 
  ,19   ,18   ,33   ,16   ,25   ,12    ,8   ,34   ,38   ,26   ,21   ,20   ,29   ,13   ,21    ,8   ,-1   ,9   ,12   ,19   ,18   ,13    ,4   ,36   ,13 
  ,10   ,18   ,24   ,12   ,19    ,3    ,5   ,25   ,30   ,18   ,15   ,16   ,20   ,15   ,14    ,1    ,9   ,-1   ,8   ,10    ,9   ,18   ,13   ,28    ,5 
  ,11   ,10   ,26    ,4   ,13    ,7    ,3   ,25   ,36   ,23    ,9    ,8   ,20    ,9   ,20    ,7   ,12    ,8   ,-1  ,11   ,13   ,24   ,14   ,25    ,6 
   ,0   ,19   ,15   ,12   ,13    ,7   ,12   ,15   ,25   ,13   ,10   ,14   ,10   ,20   ,12   ,11   ,19   ,10   ,11   ,-1   ,5   ,28   ,23   ,18    ,6 
   ,5   ,23   ,15   ,15   ,18    ,6   ,13   ,17   ,22   ,10   ,15   ,18   ,13   ,22    ,7   ,10   ,18    ,9   ,13    ,5   ,-1  ,24   ,22   ,21    ,7 
  ,28   ,32   ,38   ,29   ,37   ,21   ,21   ,42   ,38   ,28   ,33   ,33   ,37   ,26   ,24   ,18   ,13   ,18   ,24   ,28   ,24   ,-1  ,15   ,46   ,23 
  ,23   ,18   ,37   ,18   ,27   ,16   ,11   ,38   ,42   ,30   ,23   ,22   ,33   ,12   ,26   ,12    ,4   ,13   ,14   ,23   ,22   ,15   ,-1  ,39   ,17 
  ,18   ,28   ,17   ,23   ,14   ,25   ,28    ,7   ,31   ,25   ,17   ,21    ,9   ,33   ,26   ,28   ,36   ,28   ,25   ,18   ,21   ,46   ,39   ,-1  ,23 
   ,6   ,16   ,21    ,9   ,14    ,2    ,6   ,21   ,29   ,17   ,10   ,13   ,16   ,15   ,13    ,5   ,13    ,5    ,6    ,6    ,7   ,23   ,17   ,23   ,-1
};
*/

short int sorted_nodes[MAXCITY][MAXCITY];	
short int sorted_list_lengths[MAXCITY];		
				// For each of the cities number of
 				// neighbours.
				// Sorted order of cities
pthread_mutex_t gbound_mut;
pthread_barrier_t barr;
int globalbound = MAXBOUND;
int solution_task = -1;		// The task that found the solution
int ntasks;			// Total number of tasks

int main(int argc, char **argv)
{
	int i,j, k, mcmodel;
	char *arg_list[3];
	pthread_t thr[64];
	int taskno; 
	int rbound;
	int t1, t2, t3, t4;


	if (argc != 3)
	{
		printf("Usage:\n\npbrbnd <NTASKS> <NCITIES>\n\n");
		exit(0);
	}

	ntasks = atoi(argv[1]);
	ncities = atoi(argv[2]);
	pthread_barrier_init(&barr, NULL, ntasks);

	k = 0;
	for (i=0; i<ncities; i++)
		for (j=0; j<ncities; j++)
			dist_matrix[i][j] = all_distances_inorder[k++];

	// Ordering the neighbours according to their distances
	// simple insertion sort is used.
	for (i=0; i<ncities; i++)
	{
		for (j=0; j<ncities; j++)
		{
			if (dist_matrix[i][j] >= 0) // Valid distance
				insert_node(i, j, dist_matrix[i][j]);
		}
	}

	pthread_mutex_init(&gbound_mut, NULL);

	t3 = time(NULL);
	for (i=1; i<ntasks; i++)
		pthread_create(&thr[i], NULL, find_path_parallel, &args[i]);

	t1 = time(NULL);
	find_path_parallel(&args[0]);	// Find path from city 0 initial cost 0
	// Wait till all others have completed the find_path_parallel.
	
	t2 = time(NULL);
	for (i=1; i<ntasks; i++)
		pthread_join(thr[i], NULL);
	t4 = time(NULL);
	// Check the messages if any waiting to be received
	// and process them

	printf("\nTime taken is : create %d , find path : %d, joining : %d\n\n",t1 - t3, t2-t1, t4-t2);


	printf("\nCost is : %6d\n",globalbound);

	return 0;
}

void insert_node(int st_city, int end_city, int dist)
{
	int n; 

	// Find the position where t is to be inserted
	n = sorted_list_lengths[st_city];

	while ((n > 0) && (dist_matrix[st_city][sorted_nodes[st_city][n-1]] > dist))
	{
		sorted_nodes[st_city][n] = sorted_nodes[st_city][n-1];
		n--;
	}
	
	sorted_nodes[st_city][n] = end_city;	
	sorted_list_lengths[st_city]++;

	return;
}
	
void * find_path_parallel(void *arg)
{
	int nedges = sorted_list_lengths[0];
	int edge_to;
	int edge_cost;
	int rbound;
	int taskno;
	int i, j;

	pthread_barrier_wait(&barr);
int mytid = *(int *)arg;			// My task no

int memb_of[MAXCITY];		// Node is in the so far path or 
				// not. If i is not in the so far
 				// path, then memb_of[i] = -1


int sub_path[MAXCITY];		// Cities in the so far path
				// in the visited order

int locally_bestpath[MAXCITY];		// Best complete tour remembered so far

	// Place all the cities as members of -1
	// i.e. not included into the so far found path
	for (i=0; i<ncities; i++)
	{
		memb_of[i] = -1;
		sub_path[i] = -1;
	}
	// Loop that iterates over different starting edges.
	for (i = mytid; i < nedges; i = i + ntasks)
	{
		edge_to = sorted_nodes[0][i];
		edge_cost = dist_matrix[0][edge_to];
		sub_path[0] = edge_to;
		memb_of[edge_to] = 1;	// is a member
		find_path_from(mytid, memb_of, locally_bestpath, sub_path, 1, edge_cost);
		// Remove this edge from the solution and
		// with other alternatives.
		memb_of[edge_to] = -1;
	}
	if (mytid == solution_task)
	{
		printf("[  1]-");
		j = 0;
		for (i=0; i<ncities; i++)
		{
			printf("[%3d]-",locally_bestpath[j]+1);
			j = locally_bestpath[j];
		}
	}
}

void find_path_from(int mytid, int memb_of[], int locally_bestpath[], int sub_path[], int cur_city, int cost)
{
	int nedges;
	int i, j, cycle_len, p1;
	int edge_cost, estb;
	int edge_to;

	nedges = sorted_list_lengths[cur_city];
	estb = estimate_bound(cur_city+1, memb_of);

	for (p1 = 0; p1 < nedges; p1++)
	{
		// Display the path found so far	
//		printf("Nodes included so far ...\n");
//		for (i=0; i < cur_city; i++)
//			printf(" [%3d %3d]-", i+1, sub_path[i]+1);
//		printf("... cost : %6d\n",cost);

		edge_to = sorted_nodes[cur_city][p1];
		edge_cost = dist_matrix[cur_city][edge_to];
//		printf(" Considering : [%3d %3d] - %6d OK...\n", cur_city+1, edge_to + 1, edge_cost);
//		getchar();
		if ((estb + cost + edge_cost) >= globalbound)
		{
			//printf("Bound exceeded ... %d ... %d\n", estb+cost+edge_cost, globalbound);
			return;
		}
		if (memb_of[edge_to] == -1) // Is it not redundant
		{
			// Determine whether it is forming a cycle or not.
			j = edge_to;
			cycle_len = 0;
			while (j != -1)
			{
				j = sub_path[j];
				cycle_len++;
				if (j == cur_city) break;
			}
			// if Cycle formed,  and is not a 
			// complete solution, then go for next edge
			if (j != -1 && cycle_len < ncities - 1) 
					continue;

//			printf(" Including ....\n");
			memb_of[edge_to] = 1; 	// is a visited city
			sub_path[cur_city] = edge_to;
			cost = cost + edge_cost;

			if (cur_city == ncities-1) 
			{
				// solution found copy it into 
				// best path and best bound
				for (i=0; i<ncities; i++)
				{
					locally_bestpath[i] = sub_path[i];
				}

				printf("Current bound : %6d\n",cost);
				
				pthread_mutex_lock(&gbound_mut);
				if (cost < globalbound)
				{
					globalbound = cost;
					solution_task = mytid;
				}
				pthread_mutex_unlock(&gbound_mut);

				// Go back and find other 
				// alternatives
				// Remove last visited node
//				printf("Trying other alternatives ...\n");
				memb_of[edge_to] = -1;
				sub_path[cur_city] = -1;
				return;
			}
			else
			{ 	// find path from the new city
				find_path_from(mytid, memb_of, locally_bestpath,sub_path, cur_city + 1, cost);
				// Remove the last node added 
				// to the path. Excluding that
				// edge continue with other
				// alternative edges
//				printf("Removing : [%3d %3d]\n",cur_city+ 1,edge_to + 1);
				memb_of[edge_to] = -1;
				sub_path[cur_city] = -1;
				cost = cost - edge_cost;
			}
		}
//		else printf(" Visited already ...\n");
	}
//	printf("No more alternatives and backtraking ...\n");
}

int estimate_bound(int from_node, int memb_of[])
{
	int ub;
	int j, k, n1;


	ub = 0;
	for (j = from_node; j<ncities; j++)
	{
		for (k = 0; k< sorted_list_lengths[j]; k++)
		{
			n1 = sorted_nodes[j][k];
			if (memb_of[n1] == -1)
			{
				ub += dist_matrix[j][n1];
				break;
			}
		}

		// If no edge can be selected in this
		// column then the previously considered
		// edge cannot be included.
		if ( k == sorted_list_lengths[j])
		{
			ub = MAXBOUND;
			break;
		}
	}

	return ub;
}
