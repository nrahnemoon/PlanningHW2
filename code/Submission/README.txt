The following are the final results of my tests:

---------------------------------------------------------------
| Algorithm  | avgPlanningTime | avgNumNodes | avgPlanQuality |
-------------------------------------------------------------
| RRT        | 0.468950        | 496         | 11.032289      |
---------------------------------------------------------------
| RRTConnect | 0.013300        | 176         | 9.158859       |
---------------------------------------------------------------
| RRTStar    | 1.128250        | 2056        | 9.433589       |
---------------------------------------------------------------
| PRM        | 0.591300        | 4695        | 14.583754      |
---------------------------------------------------------------

To run my code, use the following commands:

> mex planner.cpp
> startQ = [pi/2 pi/4 pi/2 pi/4 pi/2]
> goalQ = [pi/8 3*pi/4 pi 0.9*pi 1.5*pi];
> runtest('map1.txt', startQ, goalQ, 0);

In runtest, the last argument defines the planner to run.
0->RRT, 1->RRTConnect, 2->RRTStar, 3->PRM

The #define TIMELIMIT defines how many seconds it will try the planner before it gives up.
For now, I've set it to 5 seconds.

In addition, I added an extra option, 4, which will:
1. Ignore startQ and goalQ
2. Generate a non-colliding start and goal joint configuration
3. Run all the planners, collecting important statistics
4. If any of the planners
5. Repeating step 2 and 3 20 times.
6. Printing all the statistics and averages.

The following is a two paragraph short description of my approach.

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

The following is the raw data which were used to generate the final results above.  Note that in 8 of the 20
iterations the 60 second time limit was triggered.

Running All Planners
Iteration 1
start is  [0.783888, 5.926524, 3.272848, 1.736136, 1.774870]
goal is  [1.860008, 1.743231, 0.135953, 3.905251, 0.819938]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.125000 | 88 | 17 | 12.046484
RRTConnect | 0.031000 | 372 | 23 | 16.853232
RRTStar | 0.141000 | 1107 | 16 | 11.630432
PRM | 1.067000 | 7533 | 23 | 15.647028
-----------------------------------

Iteration 2
start is  [1.715043, 0.760686, 1.567393, 2.612641, 0.331542]
goal is  [0.896447, 2.717722, 0.362222, 4.715409, 1.952434]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.000000 | 6 | 6 | 3.605752
RRTConnect | 0.000000 | 6 | 6 | 3.850760
RRTStar | 0.078000 | 1006 | 6 | 3.605752
PRM | 0.094000 | 2206 | 15 | 9.849992
-----------------------------------

Iteration 3
start is  [1.076120, 2.420120, 1.855982, 0.737292, 5.455002]
goal is  [1.191364, 6.114059, 0.883408, 4.036218, 5.967176]
Running RRT
Running RRTConnect
Running RRTStar
RRTStar took more than 60 seconds, retrying iteration...

Iteration 3
start is  [1.434891, 6.038700, 2.325202, 2.228367, 1.054644]
goal is  [1.001145, 1.034702, 2.331147, 4.470156, 2.718297]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.031000 | 47 | 17 | 12.360140
RRTConnect | 0.109000 | 1526 | 23 | 15.805072
RRTStar | 0.172000 | 1098 | 18 | 12.581810
PRM | 0.813000 | 6643 | 20 | 12.410244
-----------------------------------

Iteration 4
start is  [1.290884, 6.020100, 5.597858, 2.294905, 0.556277]
goal is  [1.727315, 0.721952, 1.778130, 4.571594, 0.049664]
Running RRT
RRT took more than 60 seconds, retrying iteration...

Iteration 4
start is  [0.107190, 1.993277, 5.411666, 0.138254, 1.770843]
goal is  [1.438534, 5.391532, 2.107370, 2.712736, 1.194816]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 2.313000 | 2720 | 39 | 29.007530
RRTConnect | 0.015000 | 169 | 25 | 18.505295
RRTStar | 1.813000 | 2521 | 20 | 13.859194
PRM | 0.938000 | 7180 | 25 | 16.571218
-----------------------------------

Iteration 5
start is  [1.665762, 2.122711, 2.159336, 5.825278, 0.346115]
goal is  [0.442375, 2.669400, 1.684554, 3.232004, 2.894518]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.000000 | 7 | 7 | 4.308234
RRTConnect | 0.000000 | 7 | 7 | 4.000084
RRTStar | 0.078000 | 1007 | 7 | 4.460719
PRM | 0.094000 | 2187 | 30 | 19.629361
-----------------------------------

Iteration 6
start is  [1.646012, 1.131537, 3.132676, 2.368155, 3.668052]
goal is  [1.706606, 2.478989, 0.808433, 0.980435, 1.688581]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.046000 | 158 | 12 | 8.564829
RRTConnect | 0.000000 | 30 | 10 | 6.301439
RRTStar | 0.110000 | 1094 | 13 | 8.154733
PRM | 0.094000 | 2207 | 31 | 19.855587
-----------------------------------

Iteration 7
start is  [0.661741, 2.467675, 3.531907, 1.839491, 5.554714]
goal is  [1.668830, 0.265195, 2.095865, 6.195554, 4.892973]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.430000 | 1098 | 24 | 16.849596
RRTConnect | 0.016000 | 59 | 12 | 8.488004
RRTStar | 3.469000 | 4669 | 13 | 9.063447
PRM | 0.187000 | 3113 | 24 | 15.253337
-----------------------------------

Iteration 8
start is  [1.117923, 2.699697, 5.581176, 2.513312, 0.727896]
goal is  [1.292610, 1.396157, 4.765649, 5.776381, 0.297985]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.016000 | 53 | 14 | 9.914905
RRTConnect | 0.000000 | 28 | 8 | 5.133640
RRTStar | 2.688000 | 4198 | 20 | 13.135860
PRM | 0.203000 | 3265 | 31 | 20.689227
-----------------------------------

Iteration 9
start is  [1.193665, 2.783110, 0.114285, 0.298177, 5.796323]
goal is  [0.699517, 2.209383, 1.836998, 1.058671, 5.628539]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.000000 | 4 | 4 | 2.151167
RRTConnect | 0.000000 | 5 | 5 | 2.907517
RRTStar | 0.078000 | 1009 | 6 | 3.366563
PRM | 0.125000 | 2671 | 23 | 14.589756
-----------------------------------

Iteration 10
start is  [0.994817, 1.711783, 0.401532, 3.733631, 5.469575]
goal is  [1.076887, 1.022046, 2.355691, 5.692201, 0.826266]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.000000 | 8 | 8 | 5.449544
RRTConnect | 0.000000 | 9 | 9 | 5.570097
RRTStar | 0.078000 | 1008 | 8 | 5.449544
PRM | 0.094000 | 2262 | 21 | 13.126818
-----------------------------------

Iteration 11
start is  [0.455606, 0.887243, 1.232016, 4.798630, 5.144937]
goal is  [1.466722, 0.558961, 2.792505, 3.742452, 0.235665]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 4.196000 | 1585 | 42 | 29.276035
RRTConnect | 0.063000 | 853 | 19 | 13.794295
RRTStar | 2.766000 | 2926 | 19 | 12.489225
PRM | 2.369000 | 10952 | 39 | 25.627119
-----------------------------------

Iteration 12
start is  [1.261738, 5.620102, 1.022813, 1.649847, 5.598625]
goal is  [1.795579, 3.117336, 2.920022, 0.035091, 5.065359]
Running RRT
Running RRTConnect
Running RRTStar
RRTStar took more than 60 seconds, retrying iteration...

Iteration 12
start is  [1.726740, 0.097411, 4.373513, 1.232975, 5.162962]
goal is  [0.845441, 2.731528, 0.392136, 2.310246, 0.427035]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.016000 | 71 | 20 | 14.295960
RRTConnect | 0.000000 | 22 | 16 | 11.759792
RRTStar | 0.109000 | 1141 | 14 | 9.431469
PRM | 0.641000 | 5941 | 18 | 11.607804
-----------------------------------

Iteration 13
start is  [1.314853, 1.130003, 4.177157, 1.949749, 4.405152]
goal is  [0.401340, 0.952248, 0.564330, 4.237943, 1.332111]
Running RRT
RRT took more than 60 seconds, retrying iteration...

Iteration 13
start is  [1.426454, 0.953590, 0.357237, 3.524812, 0.743812]
goal is  [1.010924, 2.990779, 0.700475, 3.774283, 0.197890]
Running RRT
Running RRTConnect
Running RRTStar
RRTStar took more than 60 seconds, retrying iteration...

Iteration 13
start is  [1.073436, 2.017246, 0.198657, 1.471132, 4.211289]
goal is  [1.581007, 3.166808, 3.030088, 0.361455, 0.960110]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.484000 | 1162 | 11 | 7.483169
RRTConnect | 0.000000 | 27 | 14 | 9.765951
RRTStar | 0.344000 | 1700 | 17 | 11.409972
PRM | 0.922000 | 7158 | 13 | 8.166239
-----------------------------------

Iteration 14
start is  [0.396546, 1.590978, 2.896628, 0.693189, 0.146691]
goal is  [0.609392, 2.958756, 1.621467, 0.904309, 2.022615]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.016000 | 6 | 6 | 3.632617
RRTConnect | 0.000000 | 10 | 7 | 4.047954
RRTStar | 0.109000 | 1143 | 10 | 6.582860
PRM | 0.078000 | 1883 | 10 | 6.334533
-----------------------------------

Iteration 15
start is  [0.285329, 0.143815, 2.429708, 2.981383, 6.093733]
goal is  [1.752243, 2.706408, 3.057892, 4.777537, 1.173723]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 1.377000 | 2236 | 23 | 15.839840
RRTConnect | 0.016000 | 57 | 19 | 14.094522
RRTStar | 8.352000 | 7362 | 23 | 13.995200
PRM | 1.157000 | 7924 | 23 | 14.312313
-----------------------------------

Iteration 16
start is  [1.510058, 1.380625, 2.230860, 1.150521, 0.835278]
goal is  [1.691074, 2.297973, 5.233143, 1.019553, 2.999407]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.094000 | 340 | 13 | 9.153358
RRTConnect | 0.000000 | 17 | 8 | 5.355011
RRTStar | 0.078000 | 1059 | 10 | 6.471102
PRM | 0.078000 | 2037 | 38 | 24.921007
-----------------------------------

Iteration 17
start is  [1.310251, 1.112745, 0.975833, 3.080327, 2.591931]
goal is  [0.230104, 0.930579, 4.769867, 3.015323, 6.192678]
Running RRT
Running RRTConnect
Running RRTStar
RRTStar took more than 60 seconds, retrying iteration...

Iteration 17
start is  [1.211690, 2.345720, 2.131531, 2.890875, 5.781558]
goal is  [1.564708, 2.955304, 4.782331, 3.306788, 1.972376]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.016000 | 59 | 15 | 10.580277
RRTConnect | 0.000000 | 25 | 12 | 8.474647
RRTStar | 1.351000 | 3074 | 19 | 12.476182
PRM | 2.499000 | 11212 | 14 | 9.382072
-----------------------------------

Iteration 18
start is  [0.074784, 2.172950, 5.447907, 2.120218, 3.236415]
goal is  [0.226844, 0.962027, 4.935159, 0.763179, 2.294330]
Running RRT
RRT took more than 60 seconds, retrying iteration...

Iteration 18
start is  [1.509483, 2.712544, 5.506200, 1.697402, 0.324063]
goal is  [0.316585, 2.262115, 1.549368, 6.278967, 2.685507]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.000000 | 31 | 16 | 11.710481
RRTConnect | 0.000000 | 19 | 15 | 10.317187
RRTStar | 0.094000 | 1045 | 18 | 12.623432
PRM | 0.093000 | 2274 | 22 | 13.687079
-----------------------------------

Iteration 19
start is  [0.119079, 0.486287, 3.339770, 2.607272, 0.780820]
goal is  [1.358190, 1.686088, 3.122321, 4.682428, 2.704491]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.219000 | 251 | 15 | 10.134731
RRTConnect | 0.016000 | 284 | 18 | 13.269796
RRTStar | 0.594000 | 1956 | 21 | 13.603150
PRM | 0.187000 | 3072 | 21 | 13.336356
-----------------------------------

Iteration 20
start is  [0.433171, 1.806318, 1.789635, 3.849834, 2.092989]
goal is  [1.359532, 0.865000, 4.557213, 1.333645, 3.703334]
Running RRT
Running RRTConnect
Running RRTStar
Running PRM
Algorithm | planningTime | numNodes | planLength | planQuality
RRT | 0.000000 | 7 | 7 | 4.281136
RRTConnect | 0.000000 | 11 | 8 | 4.882878
RRTStar | 0.063000 | 1007 | 7 | 4.281136
PRM | 0.093000 | 2199 | 10 | 6.677982
-----------------------------------