# CSC 360 P2
### NAME: Shay Van Pelt 
#
The **MTS** is a multiple threaded train crossing simulator that takes N number of trains coming from an EAST and WEST station.
The trains run one at a time through the crossing.


## Input File: 
* input.txt

    e 10 6  
    6 7  
    E 3 10   

## Algorithm:
* The rules enforced by the automated control system as defined by the assignment are:

-   Only one train is on the main track at any given time. 
-   Only loaded trains can cross the main track. 
-   If there are multiple loaded trains, the one with the high priority crosses. 
-   If two loaded trains have the same priority, then: 
    -   If they are both traveling in the same direction, the train which finished loading first gets the clearance to cross first. If they finished loading at the same time, the one that appeared first in the input file gets the clearance to cross first. 
    -   If they are traveling in opposite directions, pick the train which will travel in the direction opposite of which the last train to cross the main track traveled. If no trains have crossed the main track yet, the Westbound train has the priority. 
-   To avoid starvation, if there are two trains in the same direction traveling through the main track **back to back**, the trains waiting in the opposite direction get a chance to dispatch one train if any. 
## Output:
* example output:

    00:00:00.3 Train  2 is ready to go East  
    00:00:00.3 Train  2 is ON the main track going East  
    00:00:00.6 Train  1 is ready to go West  
    00:00:01.0 Train  0 is ready to go East  
    00:00:01.3 Train  2 is OFF the main track after going East  
    00:00:01.3 Train  1 is ON the main track going West  
    00:00:02.0 Train  1 is OFF the main track after going West  
    00:00:02.0 Train  0 is ON the main track going East  
    00:00:02.6 Train  0 is OFF the main track after going East  

* On the UVIC Linux computer the program ran in 2.618 sec. 

## How to use:
* run make
* run ./mts input.txt
* output will be in output.txt





