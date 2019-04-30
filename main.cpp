#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <queue>
#include <vector>
#include <algorithm>

using namespace std;

class Direction {
public:
    int ID;
    string name;
    queue<string> cars;
    double probability;

};

Direction *westDirection;
Direction *eastDirection;
Direction *southDirection;
Direction *northDirection;

const int WEST = 0, SOUTH = 1, EAST = 2, NORTH = 3;

Direction *currentDirection;
vector<Direction *> directions;
int sTime = 0;
int currentTime = 1;
int seed = 0;
double probability = 0;

pthread_mutex_t logLock = PTHREAD_MUTEX_INITIALIZER;

//Wake Directions
pthread_mutex_t wakeDirectionsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wakeDirectionsCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t wakeMainMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wakeMainCond = PTHREAD_COND_INITIALIZER;
int directionsInitialized;
int mainInitialized = 0;

//Current Time Lock
pthread_mutex_t currentTimeMutex = PTHREAD_MUTEX_INITIALIZER;

void readArgs(int argc, char *argv[]);

void *directionThread(void *directionPtr);

int pthread_sleep(int seconds);

void prioritizeDirections();

void threadSafeLog(string str);

int main(int argc, char *argv[]) {
    readArgs(argc, argv);

    struct timespec timetoexpire;
    struct timeval tp;

    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + sTime;

    int previousSecond = tp.tv_sec;
    srand(time(NULL));

    pthread_t threads[4];

    westDirection = new Direction();
    eastDirection = new Direction();
    northDirection = new Direction();
    southDirection = new Direction();

    directions = vector<Direction *>();
    directions.push_back(westDirection);
    directions.push_back(eastDirection);
    directions.push_back(northDirection);
    directions.push_back(southDirection);

    currentDirection = northDirection;

    westDirection->probability = probability;
    westDirection->cars = queue<string>();
    westDirection->ID = WEST;
    westDirection->name = "WEST";

    southDirection->probability = probability;
    southDirection->cars = queue<string>();
    southDirection->ID = SOUTH;
    southDirection->name = "SOUTH";

    northDirection->probability = 1 - probability;
    northDirection->cars = queue<string>();
    northDirection->ID = NORTH;
    northDirection->name = "NORTH";

    eastDirection->probability = probability;
    eastDirection->cars = queue<string>();
    eastDirection->ID = EAST;
    eastDirection->name = "EAST";

    pthread_create(&threads[0], NULL, directionThread, (void *) SOUTH);
    pthread_create(&threads[1], NULL, directionThread, (void *) NORTH);
    pthread_create(&threads[2], NULL, directionThread, (void *) WEST);
    pthread_create(&threads[3], NULL, directionThread, (void *) EAST);

    printf("started\n");

    westDirection->cars.push("car");
    eastDirection->cars.push("car");
    northDirection->cars.push("car");
    southDirection->cars.push("car");

    while (currentTime <= sTime) {
        //Find current direction flow
        prioritizeDirections();
        if (!currentDirection->cars.empty()) {
            cout << "Direction from " << currentDirection->name << " is moving!" << endl;
            currentDirection->cars.pop();
        }

        pthread_sleep(1);

        //main thread stuff first
        printf("-------------------------------------\n");
        cout << "Time  : " << currentTime << "/" << sTime << "   " <<
             "N: " << northDirection->cars.size() << ", " <<
             "S: " << southDirection->cars.size() << ", " <<
             "E: " << eastDirection->cars.size() << ", " <<
             "W: " << westDirection->cars.size() << ", " <<
             endl << endl;

        switch (currentDirection->ID) {
            case NORTH:
                break;
            case SOUTH:
                break;
            case EAST:
                break;
            case WEST:
                break;
            default:
                printf("Something went horribly wrong!\n");
                exit(0);
        }


        pthread_mutex_lock(&wakeDirectionsMutex);
        directionsInitialized = 4;
        pthread_cond_broadcast(&wakeDirectionsCond);
        pthread_mutex_unlock(&wakeDirectionsMutex);

        if (currentTime == sTime) {
            pthread_join(threads[0], NULL);
            pthread_join(threads[1], NULL);
            pthread_join(threads[2], NULL);
            pthread_join(threads[3], NULL);
            break;
        }

        //Sleep main until all direction threads are done.
        pthread_mutex_lock(&wakeMainMutex);
        while (mainInitialized != 4)
            pthread_cond_wait(&wakeMainCond, &wakeMainMutex);

        pthread_mutex_unlock(&wakeMainMutex);

        mainInitialized = 0;
        pthread_mutex_lock(&currentTimeMutex);
        currentTime++;
        pthread_mutex_unlock(&currentTimeMutex);
    }


    printf("-------------------------------------\n");
    printf("-------------------------------------\n");
    printf("-------------------------------------\n");
    printf("Simulation Over!\n");
    return 0;
}

string logString;

void prioritizeDirections() {
    vector<Direction *>::iterator it;
    vector<Direction *> candidateDirections = vector<Direction *>();

    if (currentDirection->cars.empty()) {
        logString = "Current direction is empty!";
        for (it = directions.begin(); it != directions.end(); it++) {
            Direction *dir = (*it);

            if (currentDirection == dir) {
                continue;
            }
            candidateDirections.push_back(dir);
        }
    } else if (currentDirection->cars.size() < 5) {
        for (it = directions.begin(); it != directions.end(); it++) {
            Direction *dir = (*it);

            if (currentDirection == dir) {
                continue;
            }

            if (dir->cars.size() >= 5) {
                logString = "Another direction is too full while this one has less than 5!";
                candidateDirections.push_back(dir);
            }

        }
    }

    sort(candidateDirections.begin(), candidateDirections.end(), [](const Direction *lhs, const Direction *rhs) {
        if (lhs->cars.size() == rhs->cars.size()) {
            return lhs->ID > rhs->ID;
        } else {
            return lhs->cars.size() > rhs->cars.size();
        }

    });

    if (!candidateDirections.empty()) {
        threadSafeLog(
                "Priority has changed from : " + currentDirection->name + " to " + candidateDirections.front()->name);
        threadSafeLog("Reason : " + logString);
        currentDirection = candidateDirections.front();
    }
}

void *directionThread(void *directionPtr) {
    while (true) {
        pthread_mutex_lock(&wakeDirectionsMutex);
        while (directionsInitialized <= 0)
            pthread_cond_wait(&wakeDirectionsCond, &wakeDirectionsMutex);
        directionsInitialized--;
        pthread_mutex_unlock(&wakeDirectionsMutex);

        Direction *currentDirection;
        long direction = (long) directionPtr;

        switch (direction) {
            case NORTH:
                currentDirection = northDirection;
                break;
            case SOUTH:
                currentDirection = southDirection;
                break;
            case EAST:
                currentDirection = eastDirection;
                break;
            case WEST:
                currentDirection = westDirection;
                break;
            default:
                printf("Something went horribly wrong\n!");
                exit(0);
        }

        //threadSafeLog(currentDirection->name + " Thread Initialized!");
        double random = ((double) rand() / (RAND_MAX));
        if (random < currentDirection->probability) {
            threadSafeLog("Car arrives at " + currentDirection->name + " since " + to_string(random) + " < " +
                          to_string(currentDirection->probability));
            currentDirection->cars.push("car");
        }

        pthread_mutex_lock(&currentTimeMutex);
        if (currentTime == sTime) {
            pthread_mutex_unlock(&currentTimeMutex);
            //threadSafeLog("Terminating " + currentDirection->name + " thread!");
            pthread_exit(0);
        }
        pthread_mutex_unlock(&currentTimeMutex);

        pthread_mutex_lock(&wakeMainMutex);
        mainInitialized++;
        pthread_cond_broadcast(&wakeMainCond);
        pthread_mutex_unlock(&wakeMainMutex);

    }
}

void threadSafeLog(string str) {
    pthread_mutex_lock(&logLock);
    cout << str << endl;
    pthread_mutex_unlock(&logLock);
}

int pthread_sleep(int seconds) {
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if (pthread_mutex_init(&mutex, NULL)) {
        return -1;
    }
    if (pthread_cond_init(&conditionvar, NULL)) {
        return -1;
    }
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add //it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds;
    timetoexpire.tv_nsec = tp.tv_usec * 1000;

    pthread_mutex_lock(&mutex);
    int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);

    //Upon successful completion, a value of zero shall be returned
    return res;

}

void readArgs(int argc, char *argv[]) {
    string previousArg;

    for (int i = 0; i < argc; i++) {
        string currentArg = argv[i];

        if (previousArg.empty()) {
            if (currentArg == "-t" || currentArg == "-s" || currentArg == "-p")
                previousArg = currentArg;
        } else {

            if (previousArg == "-t") {
                sTime = atoi(currentArg.c_str());
            } else if (previousArg == "-s") {
                seed = atoi(currentArg.c_str());
            } else if (previousArg == "-p") {
                probability = atof(currentArg.c_str());
            }

            previousArg = "";
        }

    }
}
