/*=================================================================
 *
 * planner.c
 *
 *=================================================================*/
#include <math.h> // for pow, sqrt, round
#include <map>
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

//the length of each link in the arm (should be the same as the one used in runtest.m)
#define LINKLENGTH_CELLS 10

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
    int cost;
};

static int getAngleDiscretizationFactor(int numofDOFs) {
    return round((2 * PI) / (2 * asin(sqrt(2)/(2 * LINKLENGTH_CELLS * numofDOFs))));
}

// Given currJoint and joints (all the existing joints),
// sets closestNeighbor to the joint within joints that's closest to currJoint
// and also returns the distance between closestNeighbor and currJoint
double getClosestNeighbor(double* currJoint, vector<double*>* joints, int numofDOFs, double** closestNeighbor) {

    double closestNeighborDistance = (pow(2 * PI, 2) * numofDOFs);

    for (int i = 0; i < joints->size(); i++) {
        double* neighborJoint = (*joints)[i];
        double currNeighborDistance = 0;
        for (int j = 0; j < numofDOFs; j++) {
            currNeighborDistance += pow(fabs(neighborJoint[j] - currJoint[j]), 2);
        }
        //printf("***neighborJoint is , [%f, %f, %f, %f, %f]\n",
        //            neighborJoint[0], neighborJoint[1], neighborJoint[2], neighborJoint[3], neighborJoint[4]);
        if(currNeighborDistance < closestNeighborDistance) {
            *closestNeighbor = neighborJoint;
            closestNeighborDistance = currNeighborDistance;
        }
    }
    return sqrt(closestNeighborDistance);
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

static void plannerRRT(double* worldMap, int x_size, int y_size,
        double* armstart_anglesV_rad, double* armgoal_anglesV_rad, int numofDOFs,
        double*** plan, int* planlength) {

	//no plan by default
	*plan = NULL;
	*planlength = 0;

    int discretizationFactor = getAngleDiscretizationFactor(numofDOFs);
    double discretizationStep = (2 * PI)/discretizationFactor;
    double epsilon = PI/4;
    //printf("Discretization factor is %d and epsilon is %f\n", discretizationFactor, epsilon);

    vector<double*> joints;
    map <double*, double*> jointParents;
    map <double*, int> jointPathLength;
    joints.push_back(armstart_anglesV_rad);
    jointParents[armstart_anglesV_rad] = 0;
    jointPathLength[armstart_anglesV_rad] = 1;
    //printf("Pushed back start node\n");

    double* currJoint;
    double* closestNeighbor;
    int isGoalJoint = 0;
    while (1) {
        if (rand() % 2 == 1) {
            currJoint = (double*) malloc(numofDOFs * sizeof(double));
            for (int i = 0; i < numofDOFs; i++) {
                currJoint[i] = armgoal_anglesV_rad[i];
            }
            isGoalJoint = 1;
            //printf("currJoint is goal node, [%f, %f, %f, %f, %f]\n",
            //        currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
        } else {
            currJoint = (double*) malloc(numofDOFs * sizeof(double));
            for (int i = 0; i < numofDOFs; i++) {
                currJoint[i] = (rand() / (RAND_MAX/(2 * PI )));
            }
            //printf("currJoint is random, [%f, %f, %f, %f, %f]\n",
            //        currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
            isGoalJoint = 0;
        }

        // Calculate closest neighbor
        double closestNeighborDistance = getClosestNeighbor(currJoint, &joints, numofDOFs, &closestNeighbor);
        //printf("closestNeighbor to currNode is, [%f, %f, %f, %f, %f]\n",
        //            closestNeighbor[0], closestNeighbor[1], closestNeighbor[2], closestNeighbor[3], closestNeighbor[4]);
        //printf("Distance between closestNeighbor and currNode is %f\n", closestNeighborDistance);
        
        if (closestNeighborDistance > epsilon) {
            isGoalJoint = 0;
            for (int j = 0; j < numofDOFs; j++) {
                currJoint[j] = closestNeighbor[j] + epsilon * ((currJoint[j] - closestNeighbor[j])/closestNeighborDistance);
            }
            closestNeighborDistance = epsilon;
            //printf("Distance was greater than epsilon(%f), so updated currJoint to [%f, %f, %f, %f, %f]\n",
            //        epsilon, currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
        }
        
        int jointTransitionValid = isJointTransitionValid(closestNeighborDistance, discretizationStep, numofDOFs,
                currJoint, closestNeighbor, worldMap, x_size, y_size);
        
        if (jointTransitionValid) {
            joints.push_back(currJoint);
            jointParents[currJoint] = closestNeighbor;
            jointPathLength[currJoint] = jointPathLength[closestNeighbor] + 1;

            if (joints.size() % 100 == 0) {
                printf("currJoint was valid, so added it to lists -- there are now %d nodes.\n", joints.size());
            }
            if (isGoalJoint) {
                //printf("Reached goalJoint -- building plan of length %d.\n", jointPathLength[currJoint]);
                *plan = (double**) malloc(jointPathLength[currJoint] * sizeof(double*));
                *planlength = jointPathLength[currJoint];
                
                for (int i = jointPathLength[currJoint] - 1; i >= 0; i--) {
                    (*plan)[i] = (double*) malloc(numofDOFs*sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i][j] = currJoint[j];
                    }
                    //printf("Plan[%d] = [%f, %f, %f, %f, %f].\n", i,
                    //        currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
                    currJoint = jointParents[currJoint];
                }
                return;
            }
        }
    }
}

static void plannerRRTConnect(double* worldMap, int x_size, int y_size,
        double* armstart_anglesV_rad, double* armgoal_anglesV_rad, int numofDOFs,
        double*** plan, int* planlength) {

	//no plan by default
	*plan = NULL;
	*planlength = 0;

    int discretizationFactor = getAngleDiscretizationFactor(numofDOFs);
    double discretizationStep = (2 * PI)/discretizationFactor;
    double epsilon = PI/4;
    printf("Discretization factor is %d and epsilon is %f\n", discretizationFactor, epsilon);

	Node* startNode = (Node*) malloc(sizeof(Node));
    startNode->joint = armstart_anglesV_rad;
    startNode->parent = 0;
    startNode->nodeNum = 1;
    vector<Node*>* startTree = new vector<Node*>();
    startTree->push_back(startNode);
    printf("Created startTree and added startNode to it.\n");

    Node* goalNode = (Node*) malloc(sizeof(Node));
    goalNode->joint = armgoal_anglesV_rad;
    goalNode->parent = 0;
    goalNode->nodeNum = 1;
    vector<Node*>* goalTree = new vector<Node*>();
    goalTree->push_back(goalNode);
    printf("Created goalTree and added goalNode to it.\n");

    vector<Node*>*  currTree = startTree;
    double* currJoint;
    Node* closestNeighbor;
    int isStartTree = 1;
    while (1) {
        if (isStartTree)
            printf("Start tree iteration!\n");
        else
            printf("Goal tree iteration!\n");

        currJoint = (double*) malloc(numofDOFs * sizeof(double));
        for (int i = 0; i < numofDOFs; i++) {
            currJoint[i] = (2 * PI ) * (((double)(rand() % discretizationFactor))/discretizationFactor);
        }
        printf("currJoint = [%f, %f, %f, %f, %f]\n",
        	currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
        double closestNeighborDistance = getClosestNeighborFromTree(currJoint, currTree, numofDOFs, &closestNeighbor);

        printf("closestNeighbor to currJoint is %f away = [%f, %f, %f, %f, %f]\n", closestNeighborDistance,
                closestNeighbor->joint[0], closestNeighbor->joint[1], closestNeighbor->joint[2],
                closestNeighbor->joint[3], closestNeighbor->joint[4]);

        if (closestNeighborDistance > epsilon) {
            for (int j = 0; j < numofDOFs; j++) {
                currJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
            }
            closestNeighborDistance = epsilon;
        }
        int jointTransitionValid = isJointTransitionValid(closestNeighborDistance, discretizationStep, numofDOFs,
                currJoint, closestNeighbor->joint, worldMap, x_size, y_size);
        printf("currJoint to neighborJoint valid = %d\n", jointTransitionValid);

        Node* currNode;
        if (jointTransitionValid) {
            currNode = (Node*) malloc(sizeof(Node));
            currNode->joint = currJoint;
            currNode->parent = closestNeighbor;
            currNode->nodeNum = closestNeighbor->nodeNum + 1;
            currTree->push_back(currNode);
            printf("currJoint was valid, so added it to lists -- there are now %d nodes.\n",  currTree->size());
        }
        
        if (isStartTree) {
            currTree = goalTree;
            isStartTree = 0;
        } else {
            currTree = startTree;
            isStartTree = 1;
        }
        printf("Swapped trees. isStartTree = %d\n",  isStartTree);
        
        if (jointTransitionValid) {
            
            // Calculate closest neighbor
            closestNeighborDistance = getClosestNeighborFromTree(currJoint, currTree, numofDOFs, &closestNeighbor);
            printf("closestNeighbor to currJoint is %f away = [%f, %f, %f, %f, %f]\n", closestNeighborDistance,
                closestNeighbor->joint[0], closestNeighbor->joint[1], closestNeighbor->joint[2],
                closestNeighbor->joint[3], closestNeighbor->joint[4]);

            double* otherJoint;
            while (closestNeighborDistance > epsilon) {
                otherJoint = (double*) malloc(numofDOFs * sizeof(double));
                for (int j = 0; j < numofDOFs; j++) {
                    otherJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
                }
                printf("otherJoint = [%f, %f, %f, %f, %f]\n",
                    otherJoint[0], otherJoint[1], otherJoint[2], otherJoint[3], otherJoint[4]);
                int jointTransitionValid = isJointTransitionValid(epsilon, discretizationStep, numofDOFs,
                    otherJoint, closestNeighbor->joint, worldMap, x_size, y_size);
                printf("otherJoint to neighborJoint valid = %d\n", jointTransitionValid);

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

                printf("Start side has %d nodes\n", startTreeLength);
                for (int i = startTreeLength - 1; i >= 0; i--) {
                    (*plan)[i] = (double*) malloc(numofDOFs * sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i][j] = startTreeNode->joint[j];
                    }
                    startTreeNode = startTreeNode->parent;
                }

                printf("Goal side has %d nodes\n", goalTreeNode->nodeNum);
                for (int i = 0; i < goalTreeLength; i++) {
                    (*plan)[i + startTreeLength] = (double*) malloc(numofDOFs * sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i + startTreeLength][j] = goalTreeNode->joint[j];
                    }
                    goalTreeNode = goalTreeNode->parent;
                }
                for (int i = 0; i < *planlength; i++) {
                    printf("Plan step %d = [%f, %f, %f, %f, %f]\n", i, (*plan)[i][0], (*plan)[i][1], (*plan)[i][2],
                            (*plan)[i][3], (*plan)[i][4]);
                }
                printf("Total number of nodes = %d\n", startTree->size() + goalTree->size());
                return;
            }
        }
    }
}



static void plannerRRTStar(double* worldMap, int x_size, int y_size,
        double* armstart_anglesV_rad, double* armgoal_anglesV_rad, int numofDOFs,
        double*** plan, int* planlength) {

	//no plan by default
	*plan = NULL;
	*planlength = 0;

    int discretizationFactor = getAngleDiscretizationFactor(numofDOFs);
    double discretizationStep = (2 * PI)/discretizationFactor;
    double epsilon = PI/4;
    printf("Discretization factor is %d and epsilon is %f\n", discretizationFactor, epsilon);

	Node* startNode = (Node*) malloc(sizeof(Node));
    startNode->joint = armstart_anglesV_rad;
    startNode->parent = 0;
    startNode->nodeNum = 1;
    vector<Node*>* startTree = new vector<Node*>();
    startTree->push_back(startNode);
    printf("Created startTree and added startNode to it.\n");

    Node* goalNode = (Node*) malloc(sizeof(Node));
    goalNode->joint = armgoal_anglesV_rad;
    goalNode->parent = 0;
    goalNode->nodeNum = 1;
    vector<Node*>* goalTree = new vector<Node*>();
    goalTree->push_back(goalNode);
    printf("Created goalTree and added goalNode to it.\n");

    vector<Node*>*  currTree = startTree;
    double* currJoint;
    Node* closestNeighbor;
    int isStartTree = 1;
    while (1) {
        if (isStartTree)
            printf("Start tree iteration!\n");
        else
            printf("Goal tree iteration!\n");

        currJoint = (double*) malloc(numofDOFs * sizeof(double));
        for (int i = 0; i < numofDOFs; i++) {
            currJoint[i] = (2 * PI ) * (((double)(rand() % discretizationFactor))/discretizationFactor);
        }
        printf("currJoint = [%f, %f, %f, %f, %f]\n",
        	currJoint[0], currJoint[1], currJoint[2], currJoint[3], currJoint[4]);
        double closestNeighborDistance = getClosestNeighborFromTree(currJoint, currTree, numofDOFs, &closestNeighbor);

        printf("closestNeighbor to currJoint is %f away = [%f, %f, %f, %f, %f]\n", closestNeighborDistance,
                closestNeighbor->joint[0], closestNeighbor->joint[1], closestNeighbor->joint[2],
                closestNeighbor->joint[3], closestNeighbor->joint[4]);

        if (closestNeighborDistance > epsilon) {
            for (int j = 0; j < numofDOFs; j++) {
                currJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
            }
            closestNeighborDistance = epsilon;
        }
        int jointTransitionValid = isJointTransitionValid(closestNeighborDistance, discretizationStep, numofDOFs,
                currJoint, closestNeighbor->joint, worldMap, x_size, y_size);
        printf("currJoint to neighborJoint valid = %d\n", jointTransitionValid);

        Node* currNode;
        if (jointTransitionValid) {
            currNode = (Node*) malloc(sizeof(Node));
            currNode->joint = currJoint;
            currNode->parent = closestNeighbor;
            currNode->nodeNum = closestNeighbor->nodeNum + 1;
            currTree->push_back(currNode);
            printf("currJoint was valid, so added it to lists -- there are now %d nodes.\n",  currTree->size());
        }
        
        if (isStartTree) {
            currTree = goalTree;
            isStartTree = 0;
        } else {
            currTree = startTree;
            isStartTree = 1;
        }
        printf("Swapped trees. isStartTree = %d\n",  isStartTree);
        
        if (jointTransitionValid) {
            
            // Calculate closest neighbor
            closestNeighborDistance = getClosestNeighborFromTree(currJoint, currTree, numofDOFs, &closestNeighbor);
            printf("closestNeighbor to currJoint is %f away = [%f, %f, %f, %f, %f]\n", closestNeighborDistance,
                closestNeighbor->joint[0], closestNeighbor->joint[1], closestNeighbor->joint[2],
                closestNeighbor->joint[3], closestNeighbor->joint[4]);

            double* otherJoint;
            while (closestNeighborDistance > epsilon) {
                otherJoint = (double*) malloc(numofDOFs * sizeof(double));
                for (int j = 0; j < numofDOFs; j++) {
                    otherJoint[j] = closestNeighbor->joint[j] + epsilon * ((currJoint[j] - closestNeighbor->joint[j])/closestNeighborDistance);
                }
                printf("otherJoint = [%f, %f, %f, %f, %f]\n",
                    otherJoint[0], otherJoint[1], otherJoint[2], otherJoint[3], otherJoint[4]);
                int jointTransitionValid = isJointTransitionValid(epsilon, discretizationStep, numofDOFs,
                    otherJoint, closestNeighbor->joint, worldMap, x_size, y_size);
                printf("otherJoint to neighborJoint valid = %d\n", jointTransitionValid);

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

                printf("Start side has %d nodes\n", startTreeLength);
                for (int i = startTreeLength - 1; i >= 0; i--) {
                    (*plan)[i] = (double*) malloc(numofDOFs * sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i][j] = startTreeNode->joint[j];
                    }
                    startTreeNode = startTreeNode->parent;
                }

                printf("Goal side has %d nodes\n", goalTreeNode->nodeNum);
                for (int i = 0; i < goalTreeLength; i++) {
                    (*plan)[i + startTreeLength] = (double*) malloc(numofDOFs * sizeof(double));
                    for(int j = 0; j < numofDOFs; j++){
                        (*plan)[i + startTreeLength][j] = goalTreeNode->joint[j];
                    }
                    goalTreeNode = goalTreeNode->parent;
                }
                for (int i = 0; i < *planlength; i++) {
                    printf("Plan step %d = [%f, %f, %f, %f, %f]\n", i, (*plan)[i][0], (*plan)[i][1], (*plan)[i][2],
                            (*plan)[i][3], (*plan)[i][4]);
                }
                printf("Total number of nodes = %d\n", startTree->size() + goalTree->size());
                return;
            }
        }
    }
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
    if(planner_id < 0 || planner_id > 3){
	    mexErrMsgIdAndTxt( "MATLAB:planner:invalidplanner_id",
                "planner id should be between 0 and 3 inclusive");         
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
    } else {
        printf("Running Dummy Planner\n");
        //dummy planner which only computes interpolated path
        planner(map,x_size,y_size, armstart_anglesV_rad, armgoal_anglesV_rad, numofDOFs, &plan, &planlength);
    }
    
    printf("planner returned plan of length=%d\n", planlength); 
    
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




