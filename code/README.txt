The following are the final results of my tests:



To run my code, use the following commands:

> mex planner.cpp
> startQ = [pi/2 pi/4 pi/2 pi/4 pi/2]
> goalQ = [pi/8 3*pi/4 pi 0.9*pi 1.5*pi];
> runtest('map1.txt', startQ, goalQ, 0);

In runtest, the last argument defines the planner to run.
0->RRT, 1->RRTConnect, 2->RRTStar, 3->PRM

In addition, I added an extra option, 4, which will:
1. Ignore startQ and goalQ
2. Generate a non-colliding start and goal joint configuration
3. Run all the planners, collecting important statistics
4. If any of the planners
5. Repeating step 2 and 3 20 times.
6. Printing all the statistics and averages.

The epsilon I used was pi/2.  I also calculate the minimum angle resolution for the given
grid size and the length of the links.  For the default configuration that's given by Matlab
5 joints and each link being 10 cells long, the minimum angle resolution ends up being around
pi/111.  Whenever I extend by epsilon, I check all the links at pi/111 resolution to ensure
nowhere along the length of the epsilon link (or any size link for that matter) is there an
obstacle that I'll miss.

Most of the algorithms follow the provided pseudocode to the T.  The only exception being
RRTStar which expands 1,000 more nodes after it hits the goal (so the path becomes more accurate).
Also, in PRM I do a trick where I add goal and start to the list of nodes at the beginning
and then stop adding nodes once the graph containing start and the graph containing goal connect
to one another.  For the algorithms which involve a radius, I use 1000 for the hyperparameter which
contains the terms gamma, the hyberball volume and 1/log(2).  I.e., 1000 = (gamma/hyperBallVolume * log(2)).