#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <deque>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cstring>

using namespace std;

pthread_mutex_t carIdLock = PTHREAD_MUTEX_INITIALIZER;
int currentCarID = 1;

class Event {
public:
    int time;
    string description;

    Event(int time, string description) {
        this->time = time;
        this->description = description;
    }
};

class Car {
public:
    int waitTime;
    int direction;
    int carid;
    int arrivalTime;
    int crossTime;

    Car() {
        pthread_mutex_lock(&carIdLock);
        carid = currentCarID;
        currentCarID++;
        pthread_mutex_unlock(&carIdLock);
        waitTime = 0;
        crossTime = -1;
    }
};

class Direction {
public:
    int ID;
    string name;
    deque<Car *> cars;
    double probability;
};

ofstream logFile;
time_t startTime = time(0);

Direction *westDirection;
Direction *eastDirection;
Direction *southDirection;
Direction *northDirection;

const int WEST = 0, SOUTH = 1, EAST = 2, NORTH = 3;

vector<Event *> allEvents;
vector<Car *> allCars;
Direction *flowingDirection;
vector<Direction *> directions;
int sTime = 0;
int currentTime = 0;
int logTime = 0;
double probability = 0;


//North sleep
pthread_mutex_t wakeNorthMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wakeNorthCond = PTHREAD_COND_INITIALIZER;
int northSleepingTime = 0;


//Log
pthread_mutex_t logLock = PTHREAD_MUTEX_INITIALIZER;

//Wake Directions
pthread_mutex_t wakeDirectionsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wakeDirectionsCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t wakePoliceOfficerMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wakePoliceOfficerCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t wakeMainMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wakeMainCond = PTHREAD_COND_INITIALIZER;

int directionsInitialized;
bool mainInitialized = true;
int remainingDirectionThreads = 0;
pthread_mutex_t honkPoliceOfficerMutex = PTHREAD_MUTEX_INITIALIZER;
int policeOfficerGamingTime = 0;
bool policeOfficerIsGaming = false;
bool policeOfficerIsNotified = false;

//Current Time Lock
pthread_mutex_t currentTimeMutex = PTHREAD_MUTEX_INITIALIZER;

void readArgs(int argc, char *argv[]);

void *directionThread(void *directionPtr);

void *policeOfficerThread(void *);

int pthread_sleep(int seconds);

void prioritizeDirections();

void incrementWaitTimes();

void threadSafeWriteToLog(const string &str);

void threadSafeWriteToConsole(const string &str);

void init();

int main(int argc, char *argv[]) {
    readArgs(argc, argv);

    struct timespec timetoexpire;
    struct timeval tp;

    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + sTime;

    int previousSecond = tp.tv_sec;
    srand(time(NULL));

    init();

    pthread_t threads[5];

    pthread_create(&threads[0], NULL, directionThread, (void *) SOUTH);
    pthread_create(&threads[1], NULL, directionThread, (void *) NORTH);
    pthread_create(&threads[2], NULL, directionThread, (void *) WEST);
    pthread_create(&threads[3], NULL, directionThread, (void *) EAST);
    pthread_create(&threads[4], NULL, policeOfficerThread, (void *) NULL);

    printf("started\n");

    Car *westCar = new Car();
    westCar->direction = WEST;
    westCar->arrivalTime = 0;

    Car *eastCar = new Car();
    eastCar->direction = EAST;
    eastCar->arrivalTime = 0;

    Car *northCar = new Car();
    northCar->direction = NORTH;
    northCar->arrivalTime = 0;

    Car *southCar = new Car();
    southCar->direction = SOUTH;
    southCar->arrivalTime = 0;

    westDirection->cars.emplace_back(westCar);
    eastDirection->cars.emplace_back(eastCar);
    northDirection->cars.emplace_back(northCar);
    southDirection->cars.emplace_back(southCar);

    allEvents = vector<Event *>();
    allCars = vector<Car *>();

    allCars.push_back(westCar);
    allCars.push_back(eastCar);
    allCars.push_back(northCar);
    allCars.push_back(southCar);

    while (true) {
        //main thread stuff

        if (currentTime == sTime) {
            break;
        }

        pthread_sleep(1);

        if (currentTime - logTime < 3 && currentTime - logTime >= 0) {
            threadSafeWriteToConsole("  " + to_string(northDirection->cars.size()));
            threadSafeWriteToConsole(
                    to_string(westDirection->cars.size()) + "   " + to_string(eastDirection->cars.size()));
            threadSafeWriteToConsole("  " + to_string(southDirection->cars.size()));
        }

        printf("-------------------------------------\n");
        cout << "Time  : " << currentTime + 1 << "/" << sTime << "   " <<
             "N: " << northDirection->cars.size() << ", " <<
             "S: " << southDirection->cars.size() << ", " <<
             "E: " << eastDirection->cars.size() << ", " <<
             "W: " << westDirection->cars.size() <<
             endl << endl;

        if (policeOfficerIsGaming) {
            policeOfficerGamingTime--;
        }


        pthread_mutex_lock(&wakeDirectionsMutex);
        if (northSleepingTime > 0) {
            northSleepingTime--;
            pthread_mutex_lock(&wakeNorthMutex);
            pthread_cond_broadcast(&wakeNorthCond);
            pthread_mutex_unlock(&wakeNorthMutex);
        } else {
        }

        directionsInitialized = 4;
        pthread_cond_broadcast(&wakeDirectionsCond);
        pthread_mutex_unlock(&wakeDirectionsMutex);

        if (policeOfficerIsGaming) {
            pthread_mutex_lock(&wakePoliceOfficerMutex);
            pthread_cond_signal(&wakePoliceOfficerCond);
            pthread_mutex_unlock(&wakePoliceOfficerMutex);

            pthread_mutex_lock(&wakeMainMutex);
            while (remainingDirectionThreads != 4)
                pthread_cond_wait(&wakeMainCond, &wakeMainMutex);

            remainingDirectionThreads = 0;
            pthread_mutex_unlock(&wakeMainMutex);
        } else {
            pthread_mutex_lock(&wakeMainMutex);
            while (!mainInitialized)
                pthread_cond_wait(&wakeMainCond, &wakeMainMutex);

            mainInitialized = false;
            pthread_mutex_unlock(&wakeMainMutex);
        }

        pthread_mutex_lock(&currentTimeMutex);
        currentTime++;
        incrementWaitTimes();
        pthread_mutex_unlock(&currentTimeMutex);

    }


    printf("-------------------------------------\n");
    printf("-------------------------------------\n");
    printf("-------------------------------------\n");
    printf("Simulation Over!\n");
    printf("Writing to Log!\n");

    threadSafeWriteToLog("CarID  Direction  Arrival-Time  Cross-Time  Wait-Time");
    threadSafeWriteToLog("-----------------------------------------------------");

    for (auto &car : allCars) {
        time_t arrivalTime = startTime + car->arrivalTime;
        struct tm arrivalTimeTM;
        memset(&arrivalTimeTM, '\0', sizeof(struct tm));
        localtime_r(&arrivalTime, &arrivalTimeTM);

        time_t crossTime = startTime + car->crossTime;
        struct tm crossTimeTM;
        memset(&crossTimeTM, '\0', sizeof(struct tm));
        localtime_r(&crossTime, &crossTimeTM);

        string directionString = "";
        if (car->direction == WEST) {
            directionString = "W";
        } else if (car->direction == NORTH) {
            directionString = "N";
        } else if (car->direction == SOUTH) {
            directionString = "S";
        } else if (car->direction == EAST) {
            directionString = "E";
        }
        string crossTimeString = "X";

        if (car->crossTime == -1) {
            crossTimeString = "X";
        } else {
            crossTimeString = to_string(crossTimeTM.tm_hour) + ":" + to_string(crossTimeTM.tm_min) + ":" +
                              to_string(crossTimeTM.tm_sec);
        }

        logFile << "   " << car->carid << "\t"
                << directionString << "     \t"
                << arrivalTimeTM.tm_hour << ":" << arrivalTimeTM.tm_min << ":" << arrivalTimeTM.tm_sec << "  \t"
                << crossTimeString <<   "\t"
                << car->waitTime
                << endl;
    }

    threadSafeWriteToLog("");
    threadSafeWriteToLog("Time       Event");
    threadSafeWriteToLog("----------------");
    for (auto &event : allEvents) {
        time_t eventTime = startTime + event->time;
        struct tm eventTimeTM;
        memset(&eventTimeTM, '\0', sizeof(struct tm));
        localtime_r(&eventTime, &eventTimeTM);

        logFile << eventTimeTM.tm_hour << ":" << eventTimeTM.tm_min << ":" << eventTimeTM.tm_sec
                << "  \t" << event->description << endl;
    }


    printf("Log finished!\n");

    logFile.close();
    return 0;
}

void *policeOfficerThread(void *) {
    while (true) {
        pthread_mutex_lock(&currentTimeMutex);
        if (currentTime == sTime) {
            pthread_mutex_unlock(&currentTimeMutex);
            threadSafeWriteToConsole("Terminating police officer thread!");
            pthread_exit(0);
        }
        pthread_mutex_unlock(&currentTimeMutex);

        //Sleep police officer until all direction threads are done.
        pthread_mutex_lock(&wakePoliceOfficerMutex);
        while (remainingDirectionThreads != 4)
            pthread_cond_wait(&wakePoliceOfficerCond, &wakePoliceOfficerMutex);

        remainingDirectionThreads = 0;
        pthread_mutex_unlock(&wakePoliceOfficerMutex);

        //Cars finished arriving
        if (westDirection->cars.empty() &&
            eastDirection->cars.empty() &&
            northDirection->cars.empty() &&
            southDirection->cars.empty()) {

            pthread_mutex_lock(&wakePoliceOfficerMutex);
            policeOfficerIsGaming = true;
            policeOfficerGamingTime = 999999999;
            allEvents.push_back(new Event(currentTime, "Cell Phone"));
            threadSafeWriteToConsole("Police officer is now gaming");

            while (policeOfficerGamingTime > 0) {
                pthread_cond_wait(&wakePoliceOfficerCond, &wakePoliceOfficerMutex);
                pthread_mutex_unlock(&wakePoliceOfficerMutex);
            }

            policeOfficerGamingTime = 0;
            policeOfficerIsGaming = false;
            policeOfficerIsNotified = false;
        }


        //Find current direction flow
        prioritizeDirections();
        if (!flowingDirection->cars.empty()) {
            cout << "Direction from " << flowingDirection->name << " is moving!" << endl;
            Car *passingCar = flowingDirection->cars.front();
            passingCar->crossTime = currentTime;
            flowingDirection->cars.pop_front();
        }

        pthread_mutex_lock(&wakeMainMutex);
        mainInitialized = true;
        pthread_cond_broadcast(&wakeMainCond);
        pthread_mutex_unlock(&wakeMainMutex);
    }
}

void *directionThread(void *directionPtr) {
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


    while (true) {
        pthread_mutex_lock(&currentTimeMutex);
        if (currentTime == sTime) {
            pthread_mutex_unlock(&currentTimeMutex);
            threadSafeWriteToConsole("Terminating " + currentDirection->name + " thread!");
            pthread_exit(0);
        }
        pthread_mutex_unlock(&currentTimeMutex);

        pthread_mutex_lock(&wakeDirectionsMutex);
        while (directionsInitialized <= 0)
            pthread_cond_wait(&wakeDirectionsCond, &wakeDirectionsMutex);
        directionsInitialized--;
        pthread_mutex_unlock(&wakeDirectionsMutex);

        //threadSafeWriteToConsole(flowingDirection->name + " Thread Initialized!");
        double random = ((double) rand() / (RAND_MAX));
        if (random < currentDirection->probability) {
            threadSafeWriteToConsole(
                    "Car arrives at " + currentDirection->name + " since " + to_string(random) + " < " +
                    to_string(currentDirection->probability));
            Car *car = new Car();
            car->arrivalTime = currentTime;
            car->direction = currentDirection->ID;

            currentDirection->cars.emplace_back(car);
            allCars.push_back(car);

            pthread_mutex_lock(&honkPoliceOfficerMutex);
            if (!policeOfficerIsNotified && policeOfficerIsGaming) {
                allEvents.push_back(new Event(currentTime, "Honk"));
                threadSafeWriteToConsole("Honk!");
                policeOfficerIsNotified = true;
                policeOfficerGamingTime = 3;
            }
            pthread_mutex_unlock(&honkPoliceOfficerMutex);
        } else {
            if (currentDirection->ID == NORTH) {
                northSleepingTime = 20;
                threadSafeWriteToConsole("North is now sleeping for 20 seconds!");
                if (policeOfficerIsGaming) {
                    pthread_mutex_lock(&wakeMainMutex);
                    remainingDirectionThreads++;
                    pthread_cond_broadcast(&wakeMainCond);
                    pthread_mutex_unlock(&wakeMainMutex);
                } else {
                    pthread_mutex_lock(&wakePoliceOfficerMutex);
                    remainingDirectionThreads++;
                    pthread_cond_broadcast(&wakePoliceOfficerCond);
                    pthread_mutex_unlock(&wakePoliceOfficerMutex);
                }

                while (northSleepingTime > 0) {
                    pthread_mutex_lock(&wakeNorthMutex);
                    pthread_cond_wait(&wakeNorthCond, &wakeNorthMutex);
                    threadSafeWriteToConsole("North sleeping for " + to_string(northSleepingTime) + "!");
                    pthread_mutex_unlock(&wakeNorthMutex);
                }
                continue;
            }
        }

        if (policeOfficerIsGaming) {
            pthread_mutex_lock(&wakeMainMutex);
            remainingDirectionThreads++;
            pthread_cond_broadcast(&wakeMainCond);
            pthread_mutex_unlock(&wakeMainMutex);
        } else {
            pthread_mutex_lock(&wakePoliceOfficerMutex);
            remainingDirectionThreads++;
            pthread_cond_broadcast(&wakePoliceOfficerCond);
            pthread_mutex_unlock(&wakePoliceOfficerMutex);
        }
    }
}

void threadSafeWriteToLog(const string &str) {
    pthread_mutex_lock(&logLock);
    logFile << str << endl;
    pthread_mutex_unlock(&logLock);
}

void threadSafeWriteToConsole(const string &str) {
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

            if (previousArg == "-s") {
                sTime = atoi(currentArg.c_str());
            } else if (previousArg == "-t") {
                logTime = atoi(currentArg.c_str());
            } else if (previousArg == "-p") {
                probability = atof(currentArg.c_str());
            }

            previousArg = "";
        }

    }
}


void init() {
    logFile.open("police.log");

    westDirection = new Direction();
    eastDirection = new Direction();
    northDirection = new Direction();
    southDirection = new Direction();

    directions = vector<Direction *>();
    directions.push_back(westDirection);
    directions.push_back(eastDirection);
    directions.push_back(northDirection);
    directions.push_back(southDirection);

    flowingDirection = northDirection;

    westDirection->probability = probability;
    westDirection->cars = deque<Car *>();
    westDirection->ID = WEST;
    westDirection->name = "WEST";

    southDirection->probability = probability;
    southDirection->cars = deque<Car *>();
    southDirection->ID = SOUTH;
    southDirection->name = "SOUTH";

    northDirection->probability = 1 - probability;
    northDirection->cars = deque<Car *>();
    northDirection->ID = NORTH;
    northDirection->name = "NORTH";

    eastDirection->probability = probability;
    eastDirection->cars = deque<Car *>();
    eastDirection->ID = EAST;
    eastDirection->name = "EAST";
}

string logString;

void prioritizeDirections() {
    vector<Direction *>::iterator it;
    vector<Direction *> candidateDirections = vector<Direction *>();

    for (it = directions.begin(); it != directions.end(); it++) {
        Direction *dir = (*it);

        if (flowingDirection == dir) {
            continue;
        }

        for (auto &car : dir->cars) {
            if (car->waitTime >= 20) {
                threadSafeWriteToConsole(
                        "Priority has changed from : " + flowingDirection->name + " to " + dir->name);
                flowingDirection = dir;
                threadSafeWriteToConsole("Reason : The driver is very angry!");
                return;
            }
        }
    }

    if (flowingDirection->cars.empty()) {
        logString = "Current direction is empty!";
        for (it = directions.begin(); it != directions.end(); it++) {
            Direction *dir = (*it);

            if (flowingDirection == dir) {
                continue;
            }
            candidateDirections.push_back(dir);
        }
    } else if (flowingDirection->cars.size() < 5) {
        for (it = directions.begin(); it != directions.end(); it++) {
            Direction *dir = (*it);

            if (flowingDirection == dir) {
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
        threadSafeWriteToConsole(
                "Priority has changed from : " + flowingDirection->name + " to " + candidateDirections.front()->name);
        threadSafeWriteToConsole("Reason : " + logString);
        flowingDirection = candidateDirections.front();
    }
}

void incrementWaitTimes() {
    for (auto &car : westDirection->cars) {
        car->waitTime = car->waitTime + 1;
    }

    for (auto &car : eastDirection->cars) {
        car->waitTime = car->waitTime + 1;
    }

    for (auto &car : northDirection->cars) {
        car->waitTime = car->waitTime + 1;
    }

    for (auto &car : southDirection->cars) {
        car->waitTime = car->waitTime + 1;
    }
}
