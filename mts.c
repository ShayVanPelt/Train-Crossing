#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

/* NAME: Shay Van Pelt */

#define WEST 0
#define EAST 1
#define LOW 0
#define HIGH 1

#define ready "is ready to go"
#define on "is ON the main track going"
#define off "is OFF the main track after going"

#define NANOSECOND_CONVERSION 1e9
#define MAX_TRAINS 500


pthread_mutex_t start_timer;
pthread_mutex_t cross_mutex;
pthread_mutex_t sched_mutex;
pthread_mutex_t crit_mutex;
pthread_mutex_t file_mutex;

pthread_cond_t sched_cond;
pthread_cond_t train_ready_to_load;

int ready_to_load = 0; //ready to lond convar
int ready_to_cross = -1; //train ready to cross
int train_to_cross_east = 0; //trains ready cross east
int train_to_cross_west = 0; //trains ready to cross west 
int trains_loaded[MAX_TRAINS]; //ready train array
FILE *output_file; //output file global


struct timespec start_time = { 0 }; //start time 

struct trains_struct {
   int Num;                      /*train number*/
   int Prio;                     /*train prio*/
   int Direction;                /*train direction*/
   int Loading_Time;             /*train load time*/
   int Crossing_Time;            /*train cross time*/
   pthread_cond_t Cross_Cond;    /*train cross convar */
};

struct scheduler_struct {
    struct trains_struct *train_array; /*train array*/
    int num_trains;                    /*number of trains*/
};

//add train to array
void add_train(int index, int value)
{
   trains_loaded[index] = value;
}

//delete train at index
void delete_train(int index)
{
   for (int i = index; i < train_to_cross_east + train_to_cross_west - 1; i++) 
   {
      trains_loaded[i] = trains_loaded[i + 1];
   }
   trains_loaded[train_to_cross_east + train_to_cross_west - 1] = -1;
}

// Convert timespec to seconds
double timespec_to_seconds(struct timespec *ts)
{
	return ((double) ts->tv_sec) + (((double) ts->tv_nsec) / NANOSECOND_CONVERSION);
}

void print_event(char* type, struct trains_struct *train)
{
   struct timespec current_time = { 0 }; 
   //get current time
   clock_gettime(CLOCK_MONOTONIC, &current_time); 
   //get proper secounds
   double seconds_since_start = timespec_to_seconds(&current_time) - timespec_to_seconds(&start_time); 

   int seconds = seconds_since_start; //set seconds
   int hours = (int)(seconds_since_start / 3600); //calculate and set hours
   seconds_since_start -= hours * 3600; //remove hours
   int minutes = (int)(seconds_since_start / 60); //calculate and set min
   seconds_since_start -= minutes * 60;  //remove min
   int tenths = (int)(seconds_since_start * 10) % 10; //calculate and set tenths of seconds

   //print train
   fprintf(output_file, "%02d:%02d:%02d.%d Train %2d %s %4s\n", hours, 
         minutes, seconds, tenths, train->Num, type,
         (train->Direction == EAST) ? "East" : "West");
}

void *train_thread(void *train_array) 
{
   struct trains_struct *train = (struct trains_struct *)train_array;

   pthread_mutex_lock(&start_timer);
	while (!ready_to_load)
	{
		pthread_cond_wait(&train_ready_to_load, &start_timer); //wait for cond veriable
	}
	pthread_mutex_unlock(&start_timer); 

   //sleep loading time
   usleep(train->Loading_Time * 100000);

   //print train ready
   pthread_mutex_lock(&file_mutex);
   print_event(ready, train);
   pthread_mutex_unlock(&file_mutex);
   
   pthread_mutex_lock(&crit_mutex);
   //add train to array
   add_train((train_to_cross_east + train_to_cross_west), train->Num);

   if (train->Direction == EAST)
   {
      train_to_cross_east++; //increase east trains
   }
   else
   {
      train_to_cross_west++; //increase west trains
   }

   //start scheduelr 
   //pthread_cond_signal(&sched_cond);
   pthread_mutex_unlock(&crit_mutex);
   pthread_cond_signal(&sched_cond);
   pthread_mutex_lock(&cross_mutex);
   while (ready_to_cross != train->Num )
   {
      pthread_cond_wait(&train->Cross_Cond, &cross_mutex); //wait cond veriable 
   }
   pthread_mutex_unlock(&cross_mutex);

   //print on track
   pthread_mutex_lock(&file_mutex);
   print_event(on, train);
   pthread_mutex_unlock(&file_mutex);

   //sleep track time 
   usleep(train->Crossing_Time * 100000);

   //print off track
   pthread_mutex_lock(&file_mutex);
   print_event(off, train);
   pthread_mutex_unlock(&file_mutex);

   pthread_mutex_lock(&crit_mutex);

   //set to cross back to default
   ready_to_cross = -1;

   //destroy convar
   pthread_cond_destroy(&train->Cross_Cond);

   pthread_mutex_unlock(&crit_mutex);

   pthread_exit(NULL);
}

void *scheduler_thread(void *args) 
{
   struct scheduler_struct *struct_args = (struct scheduler_struct *)args;
   struct trains_struct *trains = struct_args->train_array;
   int Number_trains = struct_args->num_trains;
   
   int i = 0;
   int last = -1; //last train through track
   int last2 = -1; //last last train through track
   int Chosen_train = -1; //train to schdule next
   int to_delete = -1; //train to delete next

   while(i < Number_trains)
   {
      usleep(1000); //small sleep to allow trains to enter properly
      pthread_mutex_lock(&sched_mutex);
      while (train_to_cross_east == 0 && train_to_cross_west == 0)
      {
         pthread_cond_wait(&sched_cond, &sched_mutex);
      }
      pthread_mutex_lock(&crit_mutex);
      Chosen_train = trains[trains_loaded[0]].Num; //default train to schedule 
      to_delete = 0; //default train to delete
      int total_trains = train_to_cross_east + train_to_cross_west; //total number trains
      if (total_trains == 1)
      {
         //do nothing 
      }
      else 
      {
         if ((last != -1) && (last2 != -1) && (last == last2) && ((train_to_cross_west > 0 && last == EAST) || (train_to_cross_east > 0 && last == WEST))) //Direction last 2 same
         {
            if (train_to_cross_west > 0 && last == EAST)
            {
               for (int x = 1; x < total_trains; x++)
               {
                  if ((trains[trains_loaded[x]].Direction) == WEST)
                  {
                     if (trains[Chosen_train].Direction == EAST)
                     {
                        Chosen_train = trains_loaded[x];
                        to_delete = x;
                     }
                     else if (trains[trains_loaded[x]].Prio > trains[Chosen_train].Prio)
                     {
                        Chosen_train = trains_loaded[x];
                        to_delete = x;
                     }
                     else if (trains[trains_loaded[x]].Prio == trains[Chosen_train].Prio)
                     {  
                        if (trains[trains_loaded[x]].Loading_Time < trains[Chosen_train].Loading_Time)
                        {
                           Chosen_train = trains_loaded[x];
                           to_delete = x;
                        }
                        else if (trains[trains_loaded[x]].Loading_Time == trains[Chosen_train].Loading_Time)
                        {
                           if (trains[trains_loaded[x]].Num < trains[Chosen_train].Num)
                           {
                              Chosen_train = trains_loaded[x];
                              to_delete = x;
                           }
                        }
                     }
                  }
               }
            }
            else if (train_to_cross_east > 0 && last == WEST)
            {
               for (int x = 1; x < total_trains; x++)
               {
                  if ((trains[trains_loaded[x]].Direction) == EAST)
                  {
                     if (trains[Chosen_train].Direction == WEST)
                     {
                        Chosen_train = trains_loaded[x];
                        to_delete = x;
                     }
                     else if (trains[trains_loaded[x]].Prio > trains[Chosen_train].Prio)
                     {
                        Chosen_train = trains_loaded[x];
                        to_delete = x;
                     }
                     else if (trains[trains_loaded[x]].Prio == trains[Chosen_train].Prio)
                     {  
                        if (trains[trains_loaded[x]].Loading_Time < trains[Chosen_train].Loading_Time)
                        {
                           Chosen_train = trains_loaded[x];
                           to_delete = x;
                        }
                        else if (trains[trains_loaded[x]].Loading_Time == trains[Chosen_train].Loading_Time)
                        {
                           if (trains[trains_loaded[x]].Num < trains[Chosen_train].Num)
                           {
                              Chosen_train = trains_loaded[x];
                              to_delete = x;
                           }
                        }
                     }
                  }
               }
            }
         }
         else //not same 
         {
            for (int x = 1; x < total_trains; x++)
            {
               if (trains[trains_loaded[x]].Prio > trains[Chosen_train].Prio)
               {
                  Chosen_train = trains_loaded[x];
                  to_delete = x;
               }
               else if (trains[trains_loaded[x]].Prio == trains[Chosen_train].Prio)
               {
                  if (trains[trains_loaded[x]].Direction == trains[Chosen_train].Direction)
                  {
                     if (trains[trains_loaded[x]].Loading_Time < trains[Chosen_train].Loading_Time)
                     {
                        Chosen_train = trains_loaded[x];
                        to_delete = x;
                     }
                     else if (trains[trains_loaded[x]].Loading_Time == trains[Chosen_train].Loading_Time)
                     {
                        if (trains[trains_loaded[x]].Num < trains[Chosen_train].Num)
                        {
                           Chosen_train = trains_loaded[x];
                           to_delete = x;
                        }
                     }
                  }
                  else
                  {
                     if (last == -1)
                     {
                        if (trains[trains_loaded[x]].Direction == WEST)
                        {
                           Chosen_train = trains_loaded[x];
                           to_delete = x;
                        }
                     }
                     else
                     {
                        if (trains[trains_loaded[x]].Direction != last)
                        {
                           Chosen_train = trains_loaded[x];
                           to_delete = x;
                        }
                     }
                  }
               }
            }
         }
      }
      //control that forces only when ready to schdule next
      if (ready_to_cross == -1) 
      {
         ready_to_cross = Chosen_train; //set to chosen train
         delete_train(to_delete); //delete chosen
         if (trains[Chosen_train].Direction == EAST)
         {
            train_to_cross_east--; //decrease east trains
         }
         else
         {
            train_to_cross_west--; //decrease west trains
         }
         last2 = last; 
         last = trains[Chosen_train].Direction;
         pthread_cond_signal(&trains[Chosen_train].Cross_Cond); //signal cond veriable to start train
         i++; 
      }
      pthread_mutex_unlock(&crit_mutex);
      pthread_mutex_unlock(&sched_mutex);
   }
   pthread_exit(NULL);
}

//start all trains to start loading at once
void start_trains()
{
	pthread_mutex_lock(&start_timer); 
	ready_to_load = 1; //set condition veriable
	pthread_cond_broadcast(&train_ready_to_load); //wakeup all trains to  load
	pthread_mutex_unlock(&start_timer);
}

int main(int argc, char *argv[]) 
{
   if (argc != 2) 
   {
      printf("too little or too many arguments\n");
      return 0;
   }
   //open input.txt
   FILE *file = fopen(argv[1], "r"); 
   if (file == NULL) 
   {
      perror("Error opening file");
      return 1;
   }
   //open output.txt
   output_file = fopen("output.txt", "w");
   if (output_file == NULL) 
   {
      perror("Error opening file");
      return 1;
   }
   fclose(output_file);

   //open output.txt in append mode for threads
   output_file = fopen("output.txt", "a");
   if (output_file == NULL) 
   {
      perror("Error opening file in append mode");
      return 1;
   }

   char line[1024]; 
   int NumTrains = 0; //number of trains in file
   struct trains_struct train_array[MAX_TRAINS];

   while (fgets(line, sizeof(line), file) != NULL) 
   {
      char* one = strtok(line, " ");
      char* two = strtok(NULL, " ");
      char* three = strtok(NULL, " \n");

      if (one == NULL || two == NULL || three == NULL) 
      {
         printf("Invalid input format on line %d\n", NumTrains);
         return 1;
      }

      train_array[NumTrains].Num = NumTrains; //set num

      if (isupper(one[0]))
      {
         train_array[NumTrains].Prio = HIGH; //set prio HIGH
      }
      else
      {
         train_array[NumTrains].Prio = LOW; //set prio LOW
      }
      if (strcmp(one, "W") == 0 || strcmp(one, "w") == 0)
      {
         train_array[NumTrains].Direction = WEST; //set direction WEST
      }
      else 
      {
         train_array[NumTrains].Direction = EAST; //set direction EAST
      }

      train_array[NumTrains].Loading_Time = atoi(two); //set loading time
      train_array[NumTrains].Crossing_Time = atoi(three); //set crossing time
      pthread_cond_init(&train_array[NumTrains].Cross_Cond, NULL); //create cond veriable for train 
      NumTrains++; 
   }
   
   if (NumTrains == 0)
   {
      printf("empty file \n");
      return 0;
   }

   //close file
   fclose(file);

   pthread_t threads[MAX_TRAINS];
   pthread_t scheduler; 

   //create struct for schduler 
   struct scheduler_struct struct_args;
   struct_args.train_array = train_array;
   struct_args.num_trains = NumTrains;

   //create scheduler thread 
   pthread_create(&scheduler, NULL, scheduler_thread, &struct_args);

   //Make threads for each train
   for (int i = 0; i < NumTrains; i++) 
   {
      pthread_create(&threads[i], NULL, train_thread, &train_array[i]);
   }

   //Get start time right before loading 
   clock_gettime(CLOCK_MONOTONIC, &start_time);
   
   //Start train loading
	start_trains();

   //Join threads for each train
	for(int i = 0; i < NumTrains; i++)
	{
		pthread_join(threads[i], NULL);
	}

   //Join scheduler thread
   pthread_join(scheduler, NULL);

   //Close output.txt
   fclose(output_file);

   //destroy threads
   pthread_mutex_destroy(&start_timer);
   pthread_mutex_destroy(&cross_mutex);
   pthread_mutex_destroy(&sched_mutex);
   pthread_mutex_destroy(&crit_mutex);
   pthread_mutex_destroy(&file_mutex);

   //destroy convars
   pthread_cond_destroy(&sched_cond);
   pthread_cond_destroy(&train_ready_to_load);

   return 0;
}