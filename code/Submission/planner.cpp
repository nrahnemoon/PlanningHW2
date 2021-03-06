/*=================================================================
 *
 * planner.c
 *
 *=================================================================*/
#include <ctime>
#include <math.h> // for pow, sqrt, round
#include <map>
#include <queue>
#include <stdlib.h> // for rand
#include <vector>
#include "mex.h"

using namespace std;

/* Input Arguments */
#define	MAP_IN      prhs[0]
#define	ARMSTART_IN	prhs[1]
#define	ARMGOAL_IN     prhs[2]
#define	PLANNER_ID_IN     prhs[3]

/* Planner Ids */
#define RRT         0
#define RRTCONNECT  1
#define RRTSTAR     2
#define PRM         3
#define ALL         4

/* Output Arguments */
#define	PLAN_OUT	plhs[0]
#define	PLANLENGTH_OUT	plhs[1]

#define GETMAPINDEX(X, Y, XSIZE, YSIZE) (Y*XSIZE + X)

#if !defined(MAX)
#define	MAX(A, B)	((A) > (B) ? (A) : (B))
#endif

#if !defined(MIN)
#define	MIN(A, B)	((A) < (B) ? (A) : (B))
#endif

#define PI 3.141592654
#define TIMELIMIT 60

//the length of each link in the arm (should be the same as the one used in runtest.m)
#define LINKLENGTH_CELLS 10

#ifdef _BLAS64_
#define int_F ptrdiff_t
#else
#define int_F int
#endif

typedef struct {
  int X1, Y1;
  int X2, Y2;
  int Increment;
  int UsingYIndex;
  int DeltaX, DeltaY;
  int DTerm;
  int IncrE, IncrNE;
  int XIndex, YIndex;
  int Flipped;
} bresenham_param_t;

// Given continuous x, y returns the discretized grid cell coordinates
void ContXY2Cell(double x, double y, short unsigned int* pX, short unsigned int *pY, int x_size, int y_size)
{
    double cellsize = 1.0;
	//take the nearest cell
	*pX = (int)(x/(double)(cellsize));
	if( x < 0) *pX = 0;
	if( *pX >= x_size) *pX = x_size-1;

	*pY = (int)(y/(double)(cellsize));
	if( y < 0) *pY = 0;
	if( *pY >= y_size) *pY = y_size-1;
}


void get_bresenham_parameters(int p1x, int p1y, int p2x, int p2y, bresenham_param_t *params)
{
  params->UsingYIndex = 0;

  if (fabs((double)(p2y-p1y)/(double)(p2x-p1x)) > 1)
    (params->UsingYIndex)++;

  if (params->UsingYIndex)
    {
      params->Y1=p1x;
      params->X1=p1y;
      params->Y2=p2x;
      params->X2=p2y;
    }
  else
    {
      params->X1=p1x;
      params->Y1=p1y;
      params->X2=p2x;
      params->Y2=p2y;
    }

   if ((p2x - p1x) * (p2y - p1y) < 0)
    {
      params->Flipped = 1;
      params->Y1 = -params->Y1;
      params->Y2 = -params->Y2;
    }
  else
    params->Flipped = 0;

  if (params->X2 > params->X1)
    params->Increment = 1;
  else
    params->Increment = -1;

  params->DeltaX=params->X2-params->X1;
  params->DeltaY=params->Y2-params->Y1;

  params->IncrE=2*params->DeltaY*params->Increment;
  params->IncrNE=2*(params->DeltaY-params->DeltaX)*params->Increment;
  params->DTerm=(2*params->DeltaY-params->DeltaX)*params->Increment;

  params->XIndex = params->X1;
  params->YIndex = params->Y1;
}

void get_current_point(bresenham_param_t *params, int *x, int *y)
{
  if (params->UsingYIndex)
    {
      *y = params->XIndex;
      *x = params->YIndex;
      if (params->Flipped)
        *x = -*x;
    }
  else
    {
      *x = params->XIndex;
      *y = params->YIndex;
      if (params->Flipped)
        *y = -*y;
    }
}

int get_next_point(bresenham_param_t *params)
{
  if (params->XIndex == params->X2)
    {
      return 0;
    }
  params->XIndex += params->Increment;
  if (params->DTerm < 0 || (params->Increment < 0 && params->DTerm <= 0))
    params->DTerm += params->IncrE;
  else
    {
      params->DTerm += params->IncrNE;
      params->YIndex += params->Increment;
    }
  return 1;
}


// Given a line segment going in continuous space from (x0, y0) to (x1, y1), returns
// whether the line segment is valid (inside the map and doesn't collide)
int IsValidLineSegment(double x0, double y0, double x1, double y1, double*	map, int x_size, int y_size) {
    
	bresenham_param_t params;
	int nX, nY; 
    short unsigned int nX0, nY0, nX1, nY1;

    //printf("checking link <%f %f> to <%f %f>\n", x0,y0,x1,y1);
    
	//make sure the line segment is inside the environment
	if(x0 < 0 || x0 >= x_size ||
		x1 < 0 || x1 >= x_size ||
		y0 < 0 || y0 >= y_size ||
		y1 < 0 || y1 >= y_size)
		return 0;

	ContXY2Cell(x0, y0, &nX0, &nY0, x_size, y_size);
	ContXY2Cell(x1, y1, &nX1, &nY1, x_size, y_size);

    //printf("checking link <%d %d> to <%d %d>\n", nX0,nY0,nX1,nY1);

	//iterate through the points on the segment
	get_bresenham_parameters(nX0, nY0, nX1, nY1, &params);
	do {
		get_current_point(&params, &nX, &nY);
		if(map[GETMAPINDEX(nX,nY,x_size,y_size)] == 1)
            return 0;
	} while (get_next_point(&params));

	return 1;
}

// Given a number of arm angles, figures out whether the arm is fully
// in the map and not colliding with any obstacles.
int IsValidArmConfiguration(double* angles, int numofDOFs, double*	map,
		   int x_size, int y_size)
{
    double x0,y0,x1,y1;
    int i;
    
 	//iterate through all the links starting with the base
	x1 = ((double)x_size)/2.0;
    y1 = 0;
	for(i = 0; i < numofDOFs; i++)
	{
		//compute the corresponding line segment
		x0 = x1;
		y0 = y1;
		x1 = x0 + LINKLENGTH_CELLS*cos(2*PI-angles[i]);
		y1 = y0 - LINKLENGTH_CELLS*sin(2*PI-angles[i]);

		//check the validity of the corresponding line segment
		if(!IsValidLineSegment(x0,y0,x1,y1,map,x_size,y_size))
				return 0;
	}
    return 1;
}

static void planner(double*	map, int x_size, int y_size, double* armstart_anglesV_rad,
	double* armgoal_anglesV_rad, int numofDOFs, double*** plan, int* planlength) {
	//no plan by default
	*plan = NULL;
	*planlength = 0;
    
    //for now just do straight interpolation between start and goal checking for the validity of samples

    double distance = 0;
    int i, j;
    for (j = 0; j < numofDOFs; j++){
        if(distance < fabs(armstart_anglesV_rad[j] - armgoal_anglesV_rad[j]))
            distance = fabs(armstart_anglesV_rad[j] - armgoal_anglesV_rad[j]);
    }
    int numofsamples = (int)(distance/(PI/20));
    if(numofsamples < 2){
        printf("the arm is already at the goal\n");
        return;
    }
    *plan = (double**) malloc(numofsamples*sizeof(double*));
    int firstinvalidconf = 1;
    for (i = 0; i < numofsamples; i++){
        (*plan)[i] = (double*) malloc(numofDOFs*sizeof(double)); 
        for(j = 0; j < numofDOFs; j++){
            (*plan)[i][j] = armstart_anglesV_rad[j] + ((double)(i)/(numofsamples-1))*(armgoal_anglesV_rad[j] - armstart_anglesV_rad[j]);
        }
        if(!IsValidArmConfiguration((*plan)[i], numofDOFs, map, x_size, y_size) && firstinvalidconf)
        {
            firstinvalidconf = 1;
            printf("ERROR: Invalid arm configuration!!!\n");
        }
    }    
    *planlength = numofsamples;
    
    return;
}

struct Node {
    double* joint;
    Node* parent;
    int nodeNum;
    double cost;
};

struct ExperimentResult {
    double planningTime;
    int numNodes;
    int planLength;
    double planQuality;
};

static int getAngleDiscretizationFactor(int numofDOFs) {
    return round((2 * PI) / (2 * asin(sqrt(2)/(2 * LINKLENGTH_CELLS * numofDOFs))));
}

// Given currJoint and joints (all the existing joints),
// sets closestNeighbor to the joint within joints that's closest to currJoint
// and also returns the distance between closestNeighbor and currJoint
double getClosestNeighborFromTree(double* currJoint, vector<Node*>* tree, int numofDOFs, Node** closestNeighbor) {

    double closestNeighborDistance = (pow(2 * PI, 2) * numofDOFs);

    for (int i = 0; i < tree->size(); i++) {
        Node* neighbor = (*tree)[i];
        double* neighborJoint = neighbor->joint;
        double currNeighborDistance = 0;
        for (int j = 0; j < numofDOFs; j++) {
            currNeighborDistance += pow(fabs(neighborJoint[j] - currJoint[j]), 2);
        }
        //printf("***neighborJoint is , [%f, %f, %f, %f, %f]\n",
        //            neighborJoint[0], neighborJoint[1], neighborJoint[2], neighborJoint[3], neighborJoint[4]);
        if(currNeighborDistance < closestNeighborDistance) {
            *closestNeighbor = neighbor;
            closestNeighborDistance = currNeighborDistance;
        }
    }
    return sqrt(closestNeighborDistance);
}

// Given currJoint and joints (all the existing joints),
// sets closestNeighbor to the joint within joints that's closest to currJoint
// and also returns the distance between closestNeighbor and currJoint
double getClosestNeighborFromTreeAndNearNodes(double* currJoint, vector<Node*>* tree, int numofDOFs, Node** closestNeighbor,
        vector<Node*>* nearNodes, vector<double>* nearNodeDistances, double radius) {

    radius = pow(radius, 2);
    double closestNeighborDistance = (pow(2 * PI, 2) * numofDOFs);

    for (int i = 0; i < tree->size(); i++) {
        Node* neighbor = (*tree)[i];
        double* neighborJoint = neighbor->joint;
        double currNeighborDistance = 0;
        for (int j = 0; j < numofDOFs; j++) {
            currNeighborDistance += pow(fabs(neighborJoint[j] - currJoint[j]), 2);
        }
        //printf("***neighborJoint is , [%f, %f, %f, %f, %f]\n",
        //            neighborJoint[0], neighborJoint[1], neighborJoint[2], neighborJoint[3], neighborJoint[4]);
        if(currNeighborDistance < closestNeighborDistance) {
            *closestNeighbor = neighbor;
            closestNeighborDistance = currNeighborDistance;
        }
        if (currNeighborDistance <= radius) {
            nearNodes->push_back(neighbor);
            nearNodeDistances->push_back(sqrt(currNeighborDistance));
        }
    }
    return sqrt(closestNeighborDistance);
}

static int isJointTransitionValid(double distance, double discretizationStep, int numofDOFs, double* currJoint, double* closestNeighbor,
        double* worldMap, int x_size, int y_size) {
    double* tempJoint = (double*) malloc(numofDOFs * sizeof(double));
    int numSteps = ((int) (distance/discretizationStep));
    for (int i = 1; i <= numSteps; i++) {
        for (int j = 0; j < numofDOFs; j++) {
            tempJoint[j] = closestNeighbor[j] + (i * discretizationStep) * ((currJoint[j] - closestNeighbor[j])/distance);
        }
        if (!IsValidArmConfiguration(tempJoint, numofDOFs, worldMap, x_size, y_size)) {
            return 0;
        }
    }
    return 1;
}

static void generateRandomJoint(double** joint, int numofDOFs) {
    for (int i = 0; i < numofDOFs; i++) {
        (*joint)[i] = (rand() / (RAND_MAX/(2 * PI )));
    }
}

static double getPlanQuality(double*** plan, int* planlength, int numofDOFs) {
    double distance = 0;
    for (int i = 0; i < *planlength - 1; i++) {
        double* currPlan = (*plan)[i];
        double* nextPlan = (*plan)[i+1];
        double currDistance = 0;
        for (int j = 0; j < numofDOFs; j++) {
            currDistance += pow(fabs(currPlan[j] - nextPlan[j]), 2);
        }
        distance += sqrt(currDistance);
    }
    return distance;
}

static ExperimentResult plannerRRT(double* worldMap, int x_size, int y_size,
        double* armstart_anglesV_rad, double* armgoal_anglesV_rad, int numofDOFs,
        double*** plan, int* planlength) {

    clock_t start = clock();
	//no plan by default
	*plan = NULL;
	*planlength = 0;

    int discretizationFactor = getAngleDiscretizationFactor(numofDOFs);
    double discretizationStep = (2 * PI)/discretizationFactor;
    double epsilon = PI/4;
    //printf("Discretization factor is %d and epsilon is %f\n", discretizationFactor, epsilon);

	Node* startNode = (Node*) malloc(sizeof(Node));
    double* startJoint = (double*) malloc(numofDOFs * sizeof(double));
    for (int i = 0; i < numofDOFs; i++) {
        startJoint[i] = armstart_anglesV_rad[i];
    }
    startNode->joint = startJoint;
    startNode->parent = 0;
    startNode->nodeNum = 1;
    vector<Node*>* nodes = new vector<Node*>();
    nodes->push_back(startNode);
    //printf("Created startTree and added startNode to it.\n");

    double* currJoint;
    Node* closestNeighbor;
    int isGoalJoint = 0;
    while (1) {
        if (((clock() - start ) / (double) CLOCKS_PER_SEC) > TIMELIMIT) {
            ExperimentResult result;
            result.planningTime = -1;
            return result;
        }
        if (rand() % 2 == 1) {
            currJoint = (double*) malloc(numofDOFs * sizeof(double));
            for (int i = 0; i < numofDOFs; i++) {
                currJoint[i] = armgoal_anglesV_rad[i];
            }
            isGoalJoint = 1;
        } else {
            currJoint = (double*) malloc(numofDOFs * sizeof(double));
            generateRandomJoint(&currJoint, numofDOFs);
            isGoalJoint = 0;
        }
        if(!IsValidArmConfiguration(currJoint, numofDOFs, worldMap, x_size, y_size))
            continue;

        // Calculate closest neighbor
        double closestNeighborDistance = getClosestNeighborFromTree(currJoint, nodes, numofDOFs, &closestNeighbor);
        //printf("closestNeighbor to currNode is, [%f, %f, %f, %f, %f]\n",
        //            closestNeighbor[0], closestNeighbor[1], closestNeighbor[2], closestNeighbor[3], closestNeighbor[4]);
        //printf("Distance between closestNeighbor and currNode is %f\n", closestNeighborDistance);
        
        if (closestNeighborDistance > epsilon) {
            isGoalJoint = 0;
            for (int j = 0; j < numofDOFs; j++) {
                currJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
            }
            closestNeighborDistance = epsilon;
            //printf("Distance was greater than epsilon(%f), so updated currJoint to [%f, %f, %f, %f, %f]\n",
            //        epsilon, currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
        }
        
        int jointTransitionValid = isJointTransitionValid(closestNeighborDistance, discretizationStep, numofDOFs,
                currJoint, closestNeighbor->joint, worldMap, x_size, y_size);
        
        if (jointTransitionValid) {
            Node* currNode = (Node*) malloc(sizeof(Node));
            currNode->joint = currJoint;
            currNode->parent = closestNeighbor;
            currNode->nodeNum = closestNeighbor->nodeNum + 1;
            nodes->push_back(currNode);

            if (isGoalJoint) {
                //printf("Reached goalJoint -- building plan of length %d.\n", jointPathLength[currJoint]);
                *plan = (double**) malloc(currNode->nodeNum * sizeof(double*));
                *planlength = currNode->nodeNum;

                for (int i = *planlength - 1; i >= 0; i--) {
                    (*plan)[i] = (double*) malloc(numofDOFs * sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i][j] = currNode->joint[j];
                    }
                    currNode = currNode->parent;
                }
                ExperimentResult result;
                result.planningTime = (clock() - start ) / (double) CLOCKS_PER_SEC;
                result.numNodes = nodes->size();
                result.planLength = *planlength;
                result.planQuality = getPlanQuality(plan, planlength, numofDOFs);
                for(int i = 0; i < nodes->size(); i++) {
                    free((*nodes)[i]->joint);
                    free((*nodes)[i]);
                }
                //printf("Path has %d nodes and %d total nodes were generated in %f seconds with planQuality %f.\n", result.planLength, result.numNodes, result.planningTime, result.planQuality);
                return result;
            }
        }
    }
}

static ExperimentResult plannerRRTConnect(double* worldMap, int x_size, int y_size,
        double* armstart_anglesV_rad, double* armgoal_anglesV_rad, int numofDOFs,
        double*** plan, int* planlength) {

    clock_t start = clock();

	//no plan by default
	*plan = NULL;
	*planlength = 0;

    int discretizationFactor = getAngleDiscretizationFactor(numofDOFs);
    double discretizationStep = (2 * PI)/discretizationFactor;
    double epsilon = PI/4;
    //printf("Discretization factor is %d and epsilon is %f\n", discretizationFactor, epsilon);

	Node* startNode = (Node*) malloc(sizeof(Node));
    double* startJoint = (double*) malloc(numofDOFs * sizeof(double));
    for (int i = 0; i < numofDOFs; i++) {
        startJoint[i] = armstart_anglesV_rad[i];
    }
    startNode->joint = startJoint;
    startNode->parent = 0;
    startNode->nodeNum = 1;
    vector<Node*>* startTree = new vector<Node*>();
    startTree->push_back(startNode);
    //printf("Created startTree and added startNode to it.\n");

    Node* goalNode = (Node*) malloc(sizeof(Node));
    double* goalJoint = (double*) malloc(numofDOFs * sizeof(double));
    for (int i = 0; i < numofDOFs; i++) {
        goalJoint[i] = armgoal_anglesV_rad[i];
    }
    goalNode->joint = armgoal_anglesV_rad;
    goalNode->parent = 0;
    goalNode->nodeNum = 1;
    vector<Node*>* goalTree = new vector<Node*>();
    goalTree->push_back(goalNode);
    //printf("Created goalTree and added goalNode to it.\n");

    vector<Node*>*  currTree = startTree;
    double* currJoint;
    Node* closestNeighbor;
    int isStartTree = 1;
    while (1) {
        if (((clock() - start ) / (double) CLOCKS_PER_SEC) > TIMELIMIT) {
            ExperimentResult result;
            result.planningTime = -1;
            return result;
        }
        /*if (isStartTree)
            printf("Start tree iteration!\n");
        else
            printf("Goal tree iteration!\n");*/

        currJoint = (double*) malloc(numofDOFs * sizeof(double));
        for (int i = 0; i < numofDOFs; i++) {
            currJoint[i] = (2 * PI ) * (((double)(rand() % discretizationFactor))/discretizationFactor);
        }
        if(!IsValidArmConfiguration(currJoint, numofDOFs, worldMap, x_size, y_size))
            continue;
        //printf("currJoint = [%f, %f, %f, %f, %f]\n",
        //	currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
        double closestNeighborDistance = getClosestNeighborFromTree(currJoint, currTree, numofDOFs, &closestNeighbor);

        //printf("closestNeighbor to currJoint is %f away = [%f, %f, %f, %f, %f]\n", closestNeighborDistance,
        //        closestNeighbor->joint[0], closestNeighbor->joint[1], closestNeighbor->joint[2],
        //        closestNeighbor->joint[3], closestNeighbor->joint[4]);

        if (closestNeighborDistance > epsilon) {
            for (int j = 0; j < numofDOFs; j++) {
                currJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
            }
            closestNeighborDistance = epsilon;
        }
        int jointTransitionValid = isJointTransitionValid(closestNeighborDistance, discretizationStep, numofDOFs,
                currJoint, closestNeighbor->joint, worldMap, x_size, y_size);
        // printf("currJoint to neighborJoint valid = %d\n", jointTransitionValid);

        Node* currNode;
        if (jointTransitionValid) {
            currNode = (Node*) malloc(sizeof(Node));
            currNode->joint = currJoint;
            currNode->parent = closestNeighbor;
            currNode->nodeNum = closestNeighbor->nodeNum + 1;
            currTree->push_back(currNode);
            // printf("currJoint was valid, so added it to lists -- there are now %d nodes.\n",  currTree->size());
        }
        
        if (isStartTree) {
            currTree = goalTree;
            isStartTree = 0;
        } else {
            currTree = startTree;
            isStartTree = 1;
        }
        //printf("Swapped trees. isStartTree = %d\n",  isStartTree);
        
        if (jointTransitionValid) {
            
            // Calculate closest neighbor
            closestNeighborDistance = getClosestNeighborFromTree(currJoint, currTree, numofDOFs, &closestNeighbor);
            //printf("closestNeighbor to currJoint is %f away = [%f, %f, %f, %f, %f]\n", closestNeighborDistance,
            //    closestNeighbor->joint[0], closestNeighbor->joint[1], closestNeighbor->joint[2],
            //    closestNeighbor->joint[3], closestNeighbor->joint[4]);

            double* otherJoint;
            while (closestNeighborDistance > epsilon) {
                otherJoint = (double*) malloc(numofDOFs * sizeof(double));
                for (int j = 0; j < numofDOFs; j++) {
                    otherJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
                }
                //printf("otherJoint = [%f, %f, %f, %f, %f]\n",
                //    otherJoint[0], otherJoint[1], otherJoint[2], otherJoint[3], otherJoint[4]);
                int jointTransitionValid = isJointTransitionValid(epsilon, discretizationStep, numofDOFs,
                    otherJoint, closestNeighbor->joint, worldMap, x_size, y_size);
                //printf("otherJoint to neighborJoint valid = %d\n", jointTransitionValid);

                if (jointTransitionValid) {
                    Node* otherNode = (Node*) malloc(sizeof(Node));
                    otherNode->joint = otherJoint;
                    otherNode->parent = closestNeighbor;
                    otherNode->nodeNum = closestNeighbor->nodeNum + 1;
                    currTree->push_back(otherNode);
                    closestNeighbor = otherNode;
                    closestNeighborDistance -= epsilon;
                } else {
                    break;
                }
            }

            if (closestNeighborDistance <= epsilon) {
                Node* startTreeNode;
                Node* goalTreeNode;
                if (isStartTree) {
                    startTreeNode = closestNeighbor;
                    goalTreeNode = currNode;
                } else {
                    startTreeNode = currNode;
                    goalTreeNode = closestNeighbor;
                }
                int startTreeLength = startTreeNode->nodeNum;
                int goalTreeLength = goalTreeNode->nodeNum;
                //printf("Connect succeeded!\n");
                *planlength = startTreeLength + goalTreeLength;
                *plan = (double**) malloc(*planlength * sizeof(double*));

                // printf("Start side has %d nodes\n", startTreeLength);
                for (int i = startTreeLength - 1; i >= 0; i--) {
                    (*plan)[i] = (double*) malloc(numofDOFs * sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i][j] = startTreeNode->joint[j];
                    }
                    startTreeNode = startTreeNode->parent;
                }

                // printf("Goal side has %d nodes\n", goalTreeNode->nodeNum);
                for (int i = 0; i < goalTreeLength; i++) {
                    (*plan)[i + startTreeLength] = (double*) malloc(numofDOFs * sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i + startTreeLength][j] = goalTreeNode->joint[j];
                    }
                    goalTreeNode = goalTreeNode->parent;
                }
                /*for (int i = 0; i < *planlength; i++) {
                    printf("Plan step %d = [%f, %f, %f, %f, %f]\n", i, (*plan)[i][0], (*plan)[i][1], (*plan)[i][2],
                            (*plan)[i][3], (*plan)[i][4]);
                }*/
                ExperimentResult result;
                result.planningTime = (clock() - start ) / (double) CLOCKS_PER_SEC;
                result.numNodes = startTree->size() + goalTree->size();
                result.planLength = *planlength;
                result.planQuality = getPlanQuality(plan, planlength, numofDOFs);
                for(int i = 0; i < startTree->size(); i++) {
                    free((*startTree)[i]->joint);
                    free((*startTree)[i]);
                }
                for(int i = 0; i < goalTree->size(); i++) {
                    free((*goalTree)[i]->joint);
                    free((*goalTree)[i]);
                }
                //printf("Path has %d nodes and %d total nodes were generated in %f seconds with planQuality %f.\n", result.planLength, result.numNodes, result.planningTime, result.planQuality);
                return result;
            }
        }
    }
}

static double getRRTStarRadius(int numVertices, int numofDOFs, double epsilon) {
    double calcRad = pow((1000 * log(numVertices)/numVertices), (1.0/numofDOFs));
    return min(calcRad, epsilon);
}

static ExperimentResult plannerRRTStar(double* worldMap, int x_size, int y_size,
        double* armstart_anglesV_rad, double* armgoal_anglesV_rad, int numofDOFs,
        double*** plan, int* planlength) {

    clock_t start = clock();

	//no plan by default
	*plan = NULL;
	*planlength = 0;

    int discretizationFactor = getAngleDiscretizationFactor(numofDOFs);
    double discretizationStep = (2 * PI)/discretizationFactor;
    double epsilon = PI/4;

	Node* startNode = (Node*) malloc(sizeof(Node));
    double* startJoint = (double*) malloc(numofDOFs * sizeof(double));
    for (int i = 0; i < numofDOFs; i++) {
        startJoint[i] = armstart_anglesV_rad[i];
    }
    startNode->joint = startJoint;
    startNode->parent = 0;
    startNode->nodeNum = 1;
    startNode->cost = 0;
    vector<Node*>* nodes = new vector<Node*>();
    nodes->push_back(startNode);
    //printf("Created startTree and added startNode to it.\n");

    double* currJoint;
    Node* closestNeighbor;
    Node* goalNode;
    int isGoalJoint = 0;
    int numAfterGoal = -1;
    while (1) {
        if (((clock() - start ) / (double) CLOCKS_PER_SEC) > TIMELIMIT) {
            ExperimentResult result;
            result.planningTime = -1;
            return result;
        }
        if (rand() % 2 == 1 && numAfterGoal == -1) {
            currJoint = (double*) malloc(numofDOFs * sizeof(double));
            for (int i = 0; i < numofDOFs; i++) {
                currJoint[i] = armgoal_anglesV_rad[i];
            }
            isGoalJoint = 1;
        } else {
            currJoint = (double*) malloc(numofDOFs * sizeof(double));
            generateRandomJoint(&currJoint, numofDOFs);
            isGoalJoint = 0;
        }
        if(!IsValidArmConfiguration(currJoint, numofDOFs, worldMap, x_size, y_size)) {
            free(currJoint);
            continue;
        }

        // Calculate closest neighbor
        double radius = getRRTStarRadius(nodes->size(), numofDOFs, epsilon);
        vector<Node*>* nearNodes = new vector<Node*>();
        vector<double>* nearNodeDistances = new vector<double>();

        double closestNeighborDistance = getClosestNeighborFromTreeAndNearNodes(
                currJoint, nodes, numofDOFs, &closestNeighbor, nearNodes, nearNodeDistances, radius);

        //printf("Radius = %f, Num nearest nodes = %d, total num nodes = %d\n", radius, nearNodes->size(), nodes->size());

        if (closestNeighborDistance > epsilon) {
            isGoalJoint = 0;
            for (int j = 0; j < numofDOFs; j++) {
                currJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
            }
            closestNeighborDistance = epsilon;
            //printf("Distance was greater than epsilon(%f), so updated currJoint to [%f, %f, %f, %f, %f]\n",
            //        epsilon, currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
        }

        int jointTransitionValid = isJointTransitionValid(closestNeighborDistance, discretizationStep, numofDOFs,
                currJoint, closestNeighbor->joint, worldMap, x_size, y_size);

        if (jointTransitionValid) {

            vector<int>* nearNodeObstacleFree = new vector<int>();

            Node* minNode = closestNeighbor;
            double minCost = closestNeighbor->cost + closestNeighborDistance;
            for (int i = 0; i < nearNodes->size(); i++) {
                int jointTransitionValid = isJointTransitionValid((*nearNodeDistances)[i], discretizationStep, numofDOFs,
                    currJoint, (*nearNodes)[i]->joint, worldMap, x_size, y_size);
                
                nearNodeObstacleFree->push_back(jointTransitionValid);
                if (jointTransitionValid) {
                    double currCost = (*nearNodes)[i]->cost + (*nearNodeDistances)[i];
                    if (currCost < minCost) {
                        minNode = (*nearNodes)[i];
                        minCost = currCost;
                    }
                }
            }
            
            Node* currNode = (Node*) malloc(sizeof(Node));
            currNode->joint = currJoint;
            currNode->parent = minNode;
            currNode->nodeNum = minNode->nodeNum + 1;
            currNode->cost = minCost;
            nodes->push_back(currNode);

            for (int i = 0; i < nearNodes->size(); i++) {
                if ((*nearNodes)[i] == minNode)
                    continue;
                double currCost = currNode->cost + (*nearNodeDistances)[i];
                if ((*nearNodeObstacleFree)[i] && currCost < (*nearNodes)[i]->cost) {
                    (*nearNodes)[i]->cost = currCost;
                    (*nearNodes)[i]->parent = currNode;
                    (*nearNodes)[i]->nodeNum = currNode->nodeNum + 1;
                }
            }
            
            if (numAfterGoal > 0)
                numAfterGoal--;
            if (numAfterGoal == 0)
                break;

            if (isGoalJoint) {
                numAfterGoal = 1000; // Start the countdown!
                //printf("Reached goalJoint -- expanding %d more nodes to improve path quality.\n", numAfterGoal);
                goalNode = currNode;
            }
        }
    }
    Node* tempNode = goalNode;
    int planLength = 1;
    while(tempNode->parent != 0) {
        planLength++;
        tempNode = tempNode->parent;
    }
    //printf("Reached goalJoint -- building plan of length %d.\n", goalNode->nodeNum);
    *plan = (double**) malloc(planLength * sizeof(double*));
    *planlength = planLength;

    //printf("startNode = [%f, %f, %f, %f, %f]\n",
    //        startNode->joint[0], startNode->joint[1], startNode->joint[2],
    //        startNode->joint[3], startNode->joint[4]);
    for (int i = *planlength - 1; i >= 0; i--) {
        //printf("plan[%d] = [%f, %f, %f, %f, %f]\n",
        //    i, goalNode->joint[0], goalNode->joint[1], goalNode->joint[2],
        //        goalNode->joint[3], goalNode->joint[4]);
        (*plan)[i] = (double*) malloc(numofDOFs * sizeof(double));
        for(int j = 0; j < numofDOFs; j++){
            (*plan)[i][j] = goalNode->joint[j];
        }
        goalNode = goalNode->parent;
    }
    ExperimentResult result;
    result.planningTime = (clock() - start ) / (double) CLOCKS_PER_SEC;
    result.numNodes = nodes->size();
    result.planLength = *planlength;
    result.planQuality = getPlanQuality(plan, planlength, numofDOFs);
    for(int i = 0; i < nodes->size(); i++) {
        free((*nodes)[i]->joint);
        free((*nodes)[i]);
    }
    //printf("Path has %d nodes and %d total nodes were generated in %f seconds with planQuality %f.\n", result.planLength, result.numNodes, result.planningTime, result.planQuality);
    return result;
}

struct PRMNode {
    double* joint;
    vector<PRMNode*>* neighbors;
    int connectedToStart;
    int connectedToGoal;

    PRMNode* neighborToStart;
    int nodeNum;
};

static void getNearPRMNodes(double* joint, vector<PRMNode*>* nodes, vector<PRMNode*>* nearNodes, vector<double>* nearNodeDistances, double radius, int numofDOFs) {
    radius = pow(radius, 2);
    for (int i = 0; i < nodes->size(); i++) {
        PRMNode* neighbor = (*nodes)[i];
        double* neighborJoint = neighbor->joint;
        double currNeighborDistance = 0;
        for (int j = 0; j < numofDOFs; j++) {
            currNeighborDistance += pow(fabs(neighborJoint[j] - joint[j]), 2);
        }
        
        if (currNeighborDistance <= radius) {
            nearNodes->push_back(neighbor);
            //printf("Added neighbor [%f, %f, %f, %f, %f]\n",
            //    neighbor->joint[0], neighbor->joint[1], neighbor->joint[2], neighbor->joint[3], neighbor->joint[4]);
            nearNodeDistances->push_back(sqrt(currNeighborDistance));
        }
    }
}

static int propagateStartGoalConnected(PRMNode* node) {
    int startGoalConnected = (node->connectedToStart && node->connectedToGoal);
    PRMNode* currNeighbor;
    int currNeighborConnectedToStart;
    int currNeighborConnectedToGoal;
    for (int i = 0; i < node->neighbors->size(); i++) {
        currNeighbor = (*(node->neighbors))[i];
        currNeighborConnectedToStart = currNeighbor->connectedToStart;
        currNeighborConnectedToGoal = currNeighbor->connectedToGoal;
        currNeighbor->connectedToStart = (currNeighbor->connectedToStart || node->connectedToStart);
        currNeighbor->connectedToGoal = (currNeighbor->connectedToGoal || node->connectedToGoal);
        if (currNeighborConnectedToStart != node->connectedToStart || currNeighborConnectedToGoal != node->connectedToGoal) {
            if(propagateStartGoalConnected(currNeighbor)) {
                startGoalConnected = 1;
            }
        }
    }
    return startGoalConnected;
}

static ExperimentResult plannerPRM(double* worldMap, int x_size, int y_size,
        double* armstart_anglesV_rad, double* armgoal_anglesV_rad, int numofDOFs,
        double*** plan, int* planlength) {

    clock_t start = clock();

	//no plan by default
	*plan = NULL;
	*planlength = 0;

    int discretizationFactor = getAngleDiscretizationFactor(numofDOFs);
    double discretizationStep = (2 * PI)/discretizationFactor;
    double epsilon = PI/4;

    vector<PRMNode*>* nodes = new vector<PRMNode*>();

    
    PRMNode* startNode = (PRMNode*) malloc(sizeof(PRMNode));
    double* startJoint = (double*) malloc(numofDOFs * sizeof(double));
    for (int i = 0; i < numofDOFs; i++) {
        startJoint[i] = armstart_anglesV_rad[i];
    }
    startNode->joint = startJoint;
    startNode->connectedToStart = 1;
    startNode->connectedToGoal = 0;
    startNode->neighbors = new vector<PRMNode*>();
    startNode->nodeNum = 1;
    nodes->push_back(startNode);

    PRMNode* goalNode = (PRMNode*) malloc(sizeof(PRMNode));
    double* goalJoint = (double*) malloc(numofDOFs * sizeof(double));
    for (int i = 0; i < numofDOFs; i++) {
        goalJoint[i] = armgoal_anglesV_rad[i];
    }
    goalNode->joint = goalJoint;
    goalNode->connectedToStart = 0;
    goalNode->connectedToGoal = 1;
    goalNode->neighbors = new vector<PRMNode*>();
    goalNode->nodeNum = -1;
    nodes->push_back(goalNode);

    double* currJoint;
    while(1) {
        if (((clock() - start ) / (double) CLOCKS_PER_SEC) > TIMELIMIT) {
            ExperimentResult result;
            result.planningTime = -1;
            return result;
        }
        currJoint = (double*) malloc(numofDOFs * sizeof(double));
        generateRandomJoint(&currJoint, numofDOFs);
        if(!IsValidArmConfiguration(currJoint, numofDOFs, worldMap, x_size, y_size))
            continue;
        //printf("currJoint is , [%f, %f, %f, %f, %f]\n",
        //    currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);

        vector<PRMNode*>* nearNodes = new vector<PRMNode*>();
        vector<double>* nearNodeDistances = new vector<double>();
        double radius = getRRTStarRadius(nodes->size(), numofDOFs, epsilon);
        getNearPRMNodes(currJoint, nodes, nearNodes, nearNodeDistances, radius, numofDOFs);
        // printf("Radius = %f, Num nearest nodes = %d, total num nodes = %d\n", radius, nearNodes->size(), nodes->size());

        PRMNode* currNode = (PRMNode*) malloc(sizeof(PRMNode));
        currNode->joint = currJoint;
        currNode->connectedToStart = 0;
        currNode->connectedToGoal = 0;
        currNode->neighbors = new vector<PRMNode*>();
        currNode->nodeNum = -1;
        nodes->push_back(currNode);
        // printf("currNode added to nodes\n");

        PRMNode* neighbor;
        double neighborDistance;
        for(int i = 0; i < nearNodes->size(); i++) {
            neighbor = (*nearNodes)[i];
            neighborDistance = (*nearNodeDistances)[i];
            //printf("neighbor is , [%f, %f, %f, %f, %f]\n",
            //    neighbor->joint[0], neighbor->joint[1], neighbor->joint[2], neighbor->joint[3], neighbor->joint[4]);
            if(!isJointTransitionValid(neighborDistance, discretizationStep, numofDOFs, currNode->joint, neighbor->joint, worldMap, x_size, y_size))
                continue;
            neighbor->neighbors->push_back(currNode);
            currNode->neighbors->push_back(neighbor);
        }
        if (propagateStartGoalConnected(currNode))
            break;
    }
    // printf("Start goal connected!  %d nodes expanded!", nodes->size());
    
    queue<PRMNode*> prmQueue;
    prmQueue.push(startNode);
            
    PRMNode* currNode;
    while(prmQueue.size() != 0) {
        currNode = prmQueue.front();
        prmQueue.pop();
        if (currNode == goalNode) {
            //printf("Found path to goalNode!\n");
            break;
        }
        PRMNode* neighbor;
        for(int i = 0; i < currNode->neighbors->size(); i++) {
            neighbor = (*(currNode->neighbors))[i];
            if (neighbor->nodeNum == -1) {
                neighbor->neighborToStart = currNode;
                neighbor->nodeNum = currNode->nodeNum + 1;
                prmQueue.push(neighbor);
            }
        }
    }

    *plan = (double**) malloc(currNode->nodeNum * sizeof(double*));
    *planlength = currNode->nodeNum;

    for (int i = *planlength - 1; i >= 0; i--) {
        (*plan)[i] = (double*) malloc(numofDOFs * sizeof(double));
        for(int j = 0; j < numofDOFs; j++){
            (*plan)[i][j] = currNode->joint[j];
        }
        currNode = currNode->neighborToStart;
    }
    
    ExperimentResult result;
    result.planningTime = (clock() - start ) / (double) CLOCKS_PER_SEC;
    result.numNodes = nodes->size();
    result.planLength = *planlength;
    result.planQuality = getPlanQuality(plan, planlength, numofDOFs);
    //printf("Path has %d nodes and %d total nodes were generated in %f seconds with planQuality %f.\n", result.planLength, result.numNodes, result.planningTime, result.planQuality);
    for(int i = 0; i < nodes->size(); i++) {
        free((*nodes)[i]->joint);
        free((*nodes)[i]);
    }
    return result;
}

//prhs contains input parameters (3): 
//1st is matrix with all the obstacles
//2nd is a row vector of start angles for the arm 
//3nd is a row vector of goal angles for the arm 
//plhs should contain output parameters (2): 
//1st is a 2D matrix plan when each plan[i][j] is the value of jth angle at the ith step of the plan
//(there are D DoF of the arm (that is, D angles). So, j can take values from 0 to D-1
//2nd is planlength (int)
void mexFunction( int nlhs, mxArray *plhs[], 
		  int nrhs, const mxArray*prhs[])    
{
    
    /* Check for proper number of arguments */    
    if (nrhs != 4) { 
	    mexErrMsgIdAndTxt( "MATLAB:planner:invalidNumInputs",
                "Four input arguments required."); 
    } else if (nlhs != 2) {
	    mexErrMsgIdAndTxt( "MATLAB:planner:maxlhs",
                "One output argument required."); 
    } 
        
    /* get the dimensions of the map and the map matrix itself*/     
    int x_size = (int) mxGetM(MAP_IN);
    int y_size = (int) mxGetN(MAP_IN);
    double* map = mxGetPr(MAP_IN);
    
    /* get the start and goal angles*/     
    int numofDOFs = (int) (MAX(mxGetM(ARMSTART_IN), mxGetN(ARMSTART_IN)));
    if(numofDOFs <= 1){
	    mexErrMsgIdAndTxt( "MATLAB:planner:invalidnumofdofs",
                "it should be at least 2");         
    }
    double* armstart_anglesV_rad = mxGetPr(ARMSTART_IN);
    if (numofDOFs != MAX(mxGetM(ARMGOAL_IN), mxGetN(ARMGOAL_IN))){
        	    mexErrMsgIdAndTxt( "MATLAB:planner:invalidnumofdofs",
                "numofDOFs in startangles is different from goalangles");         
    }
    double* armgoal_anglesV_rad = mxGetPr(ARMGOAL_IN);
 
    //get the planner id
    int planner_id = (int)*mxGetPr(PLANNER_ID_IN);
    if(planner_id < 0 || planner_id > 4){
	    mexErrMsgIdAndTxt( "MATLAB:planner:invalidplanner_id",
                "planner id should be between 0 and 4 inclusive");         
    }
    
    //call the planner
    double** plan = NULL;
    int planlength = 0;
    
    //you can may be call the corresponding planner function here
    if (planner_id == RRT) {
        printf("Running RRT Planner\n");
        plannerRRT(map,x_size,y_size, armstart_anglesV_rad, armgoal_anglesV_rad, numofDOFs, &plan, &planlength);
    } else if (planner_id == RRTCONNECT) {
        printf("Running RRT Connect Planner\n");
        plannerRRTConnect(map,x_size,y_size, armstart_anglesV_rad, armgoal_anglesV_rad, numofDOFs, &plan, &planlength);
    } else if (planner_id == RRTSTAR) {
        printf("Running RRT Star Planner\n");
        plannerRRTStar(map,x_size,y_size, armstart_anglesV_rad, armgoal_anglesV_rad, numofDOFs, &plan, &planlength);
    } else if (planner_id == PRM) {
        printf("Running PRM Planner\n");
        plannerPRM(map,x_size,y_size, armstart_anglesV_rad, armgoal_anglesV_rad, numofDOFs, &plan, &planlength);
    } else if (planner_id == ALL) {
        printf("Running All Planners\n");
        int numIterations = 20;
        
        double rrtPlanningTime = 0;
        int rrtNumNodes = 0;
        double rrtPlanQuality = 0;

        double rrtConnectPlanningTime = 0;
        int rrtConnectNumNodes = 0;
        double rrtConnectPlanQuality = 0;

        double rrtStarPlanningTime = 0;
        int rrtStarNumNodes = 0;
        double rrtStarPlanQuality = 0;

        double prmPlanningTime = 0;
        int prmNumNodes = 0;
        double prmPlanQuality = 0;
        
        srand(time(NULL));
        int i = 1;
        while (i <= numIterations) {
            printf("Iteration %d\n", i);
            double* start = (double*) malloc(numofDOFs * sizeof(double));
            while(1) {
                generateRandomJoint(&start, numofDOFs);
                if(IsValidArmConfiguration(start, numofDOFs, map, x_size, y_size))
                    break;
            }
            double* goal = (double*) malloc(numofDOFs * sizeof(double));
            while(1) {
                generateRandomJoint(&goal, numofDOFs);
                if(IsValidArmConfiguration(goal, numofDOFs, map, x_size, y_size))
                    break;
            }
            printf("start is  [");
            for (int i = 0; i < numofDOFs-1; i++) {
                printf("%f, ", start[i]);
            }
            printf("%f]\n", start[numofDOFs-1]);

            printf("goal is  [");
            for (int i = 0; i < numofDOFs-1; i++) {
                printf("%f, ", goal[i]);
            }
            printf("%f]\n", goal[numofDOFs-1]);

            printf("Running RRT\n");
            ExperimentResult rrtResult = plannerRRT(map,x_size,y_size, start, goal, numofDOFs, &plan, &planlength);
            if (rrtResult.planningTime == -1) {
                printf("RRT took more than %d seconds, retrying iteration...\n\n", TIMELIMIT);
                continue;
            }
            printf("Running RRTConnect\n");
            ExperimentResult rrtConnectResult = plannerRRTConnect(map,x_size,y_size, start, goal, numofDOFs, &plan, &planlength);
            if (rrtConnectResult.planningTime == -1) {
                printf("RRTConnect took more than %d seconds, retrying iteration...\n\n", TIMELIMIT);
                continue;
            }
            printf("Running RRTStar\n");
            ExperimentResult rrtStarResult = plannerRRTStar(map,x_size,y_size, start, goal, numofDOFs, &plan, &planlength);
            if (rrtStarResult.planningTime == -1) {
                printf("RRTStar took more than %d seconds, retrying iteration...\n\n", TIMELIMIT);
                continue;
            }
            printf("Running PRM\n");
            ExperimentResult prmResult = plannerPRM(map,x_size,y_size, start, goal, numofDOFs, &plan, &planlength);
            if (prmResult.planningTime == -1) {
                printf("PRM took more than %d seconds, retrying iteration...\n\n", TIMELIMIT);
                continue;
            }

            printf("Algorithm | planningTime | numNodes | planLength | planQuality\n");
            rrtPlanningTime += rrtResult.planningTime;
            rrtNumNodes += rrtResult.numNodes;
            rrtPlanQuality += rrtResult.planQuality;
            printf("RRT | %f | %d | %d | %f\n", rrtResult.planningTime, rrtResult.numNodes, rrtResult.planLength, rrtResult.planQuality);
            
            rrtConnectPlanningTime += rrtConnectResult.planningTime;
            rrtConnectNumNodes += rrtConnectResult.numNodes;
            rrtConnectPlanQuality += rrtConnectResult.planQuality;
            printf("RRTConnect | %f | %d | %d | %f\n", rrtConnectResult.planningTime, rrtConnectResult.numNodes, rrtConnectResult.planLength, rrtConnectResult.planQuality);

            rrtStarPlanningTime += rrtStarResult.planningTime;
            rrtStarNumNodes += rrtStarResult.numNodes;
            rrtStarPlanQuality += rrtStarResult.planQuality;
            printf("RRTStar | %f | %d | %d | %f\n", rrtStarResult.planningTime, rrtStarResult.numNodes, rrtStarResult.planLength, rrtStarResult.planQuality);

            prmPlanningTime += prmResult.planningTime;
            prmNumNodes += prmResult.numNodes;
            prmPlanQuality += prmResult.planQuality;
            printf("PRM | %f | %d | %d | %f\n", prmResult.planningTime, prmResult.numNodes, prmResult.planLength, prmResult.planQuality);
            printf("-----------------------------------\n\n");

            i++;
        }
        rrtPlanningTime /= numIterations;
        rrtNumNodes /= numIterations;
        rrtPlanQuality /= numIterations;
        rrtConnectPlanningTime /= numIterations;
        rrtConnectNumNodes /= numIterations;
        rrtConnectPlanQuality /= numIterations;
        rrtStarPlanningTime /= numIterations;
        rrtStarNumNodes /= numIterations;
        rrtStarPlanQuality /= numIterations;
        prmPlanningTime /= numIterations;
        prmNumNodes /= numIterations;
        prmPlanQuality /= numIterations;
        printf("Final Results!\n");
        printf("Algorithm | avgPlanningTime | avgNumNodes |avgPlanQuality\n");
        printf("RRT | %f | %d | %f\n", rrtPlanningTime, rrtNumNodes, rrtPlanQuality);
        printf("RRTConnect | %f | %d | %f\n", rrtConnectPlanningTime, rrtConnectNumNodes, rrtConnectPlanQuality);
        printf("RRTStar | %f | %d | %f\n", rrtStarPlanningTime, rrtStarNumNodes, rrtStarPlanQuality);
        printf("PRM | %f | %d | %f\n", prmPlanningTime, prmNumNodes, prmPlanQuality);
        printf("-----------------------------------\n\n");
    } else {
        printf("Running Dummy Planner\n");
        //dummy planner which only computes interpolated path
        planner(map,x_size,y_size, armstart_anglesV_rad, armgoal_anglesV_rad, numofDOFs, &plan, &planlength);
    }
    
    /* Create return values */
    if(planlength > 0)
    {
        PLAN_OUT = mxCreateNumericMatrix( (mwSize)planlength, (mwSize)numofDOFs, mxDOUBLE_CLASS, mxREAL); 
        double* plan_out = mxGetPr(PLAN_OUT);        
        //copy the values
        int i,j;
        for(i = 0; i < planlength; i++)
        {
            for (j = 0; j < numofDOFs; j++)
            {
                plan_out[j*planlength + i] = plan[i][j];
            }
            free(plan[i]);
        }
        free(plan);
    }
    else
    {
        PLAN_OUT = mxCreateNumericMatrix( (mwSize)1, (mwSize)numofDOFs, mxDOUBLE_CLASS, mxREAL); 
        double* plan_out = mxGetPr(PLAN_OUT);
        //copy the values
        int j;
        for(j = 0; j < numofDOFs; j++)
        {
                plan_out[j] = armstart_anglesV_rad[j];
        }     
    }
    PLANLENGTH_OUT = mxCreateNumericMatrix( (mwSize)1, (mwSize)1, mxUINT16_CLASS, mxREAL);
    unsigned short* planlength_out = (unsigned short*)mxGetData(PLANLENGTH_OUT);
    *planlength_out = planlength;

    
    return;
    
}





