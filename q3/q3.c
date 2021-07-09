#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
/* ________________________________________________________ */
static int NumAcousticStages = 0;    //
static int NumElectricStages = 0;    //
static int NumCoordinaters = 0;      //
static int NumPerformers = 0;        //
static int MaxWaitingTime = 0;       //
static int ExtensionTime = 2;        //
static int tshirtTime = 2;           //
static int PerformedPerformers = 0;  //
static int MinBrowseTime = 0;        //
static int DiffBrowseTime = 0;       //
/* ________________________________________________________ */
/* random sleep times generation functions */
static int getBrowseTime() {
    return rand() % DiffBrowseTime + MinBrowseTime;
}
/* ________________________________________________________ */
typedef struct performer {
    int id;             //
    int stage;          //
    int stageid;        //
    int arrivalTime;    //
    int extendTime;     //
    int performTime;    //
    char *name;         //
    char *instrument;   //
    bool singer;        //
    bool finished;      //
    pthread_t tid;      //
    pthread_mutex_t m;  //
    pthread_cond_t cv;  //
    sem_t tshirt;       //
    // 0 is no choice,
    // 1 is acoustic,
    // -1 is electric
} performer;
typedef struct stage {
    int id;              //
    int type;            //
    int performer;       //
    int performer_type;  //
    bool noExtension;    //
    pthread_t tid;       //
    sem_t sem;           //
    sem_t extension;     //
} stage;
typedef struct coordinator {
    int id;         //
    pthread_t tid;  //
} coordinator;
/* ________________________________________________________ */
pthread_mutex_t lock;  //
static void *Performer(void *);
static void givePerformance(performer *);
static void gettshirt(performer *);
static void *Stage(void *);
static void *Coordinator(void *);
/* ________________________________________________________ */
performer *performers;
stage *stages;
coordinator *coordinators;
/* ________________________________________________________ */
// code to be executed by the performer (musician and singer)
static void *Performer(void *p1) {
    int rc = 0;
    struct timespec ts;
    performer *p = (performer *)p1;
    sleep(p->arrivalTime);
    printf("\033[0;32m%s %s arrived\n", p->name, p->instrument);
    fflush(stdout);
    pthread_mutex_lock(&p->m);
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += MaxWaitingTime;
    p->stageid = -1;
    while (p->stageid == -1 && rc == 0) {
        rc = pthread_cond_timedwait(&p->cv, &p->m, &ts);
    }
    if (rc != 0) {
        p->stageid = 0;
    }
    pthread_mutex_unlock(&p->m);
    if (rc == 0) {
        givePerformance(p);
        sem_post(&stages[p->stageid].sem);
        gettshirt(p);
    } else {
        printf("\033[0;36m%s %s left because of impatience\n", p->name, p->instrument);
        fflush(stdout);
    }
    pthread_mutex_lock(&lock);
    PerformedPerformers++;
    pthread_mutex_unlock(&lock);
    return NULL;
}
static void givePerformance(performer *p) {
    // int performTime = getBrowseTime();
    stages[p->stageid].type == 1 ? printf("\033[01;33m%s performing %s at acoustic stage #%d for %d seconds\n", p->name, p->instrument, p->stageid + 1, p->performTime)
                                 : printf("\033[0;33m%s performing %s at electric stage #%d for %d seconds\n", p->name, p->instrument, p->stageid + 1, p->performTime);
    fflush(stdout);
    sleep(p->performTime);
    sem_wait(&stages[p->stageid].extension);
    sleep(p->extendTime);
    stages[p->stageid].noExtension = true;
    sem_post(&stages[p->stageid].extension);
    stages[p->stageid].type == 1 ? printf("\033[01;33m%s performance at acoustic stage #%d ended\n", p->name, p->stageid + 1)
                                 : printf("\033[0;33m%s performance at electric stage #%d ended\n", p->name, p->stageid + 1);
    fflush(stdout);
}

static void gettshirt(performer *p) {
    // singers will also get t-shirt
    p->finished = true;
    sem_wait(&p->tshirt);
}
/* ________________________________________________________ */
// code to be executed by the stage
static void *Stage(void *s1) {
    stage *s = (stage *)s1;
    while (PerformedPerformers != NumPerformers) {
        time_t start_time, perform_time;
        bool success = false;
        while (!success && PerformedPerformers != NumPerformers) {
            for (int i = 0; i < NumPerformers && PerformedPerformers != NumPerformers; i++) {
                if (performers[i].stageid != -1) {
                    continue;
                }
                pthread_mutex_lock(&performers[i].m);
                if (performers[i].stageid == -1 &&
                    (performers[i].stage == 0 ||
                     performers[i].stage == s->type)) {
                    s->noExtension = false;
                    s->performer = performers[i].id;
                    s->performer_type = performers[i].singer;
                    start_time = time(NULL);
                    performers[i].stageid = s->id;
                    performers[i].performTime = getBrowseTime();
                    perform_time = performers[i].performTime;
                    success = true;
                }
                pthread_mutex_unlock(&performers[i].m);
                if (success) {
                    pthread_cond_signal(&performers[i].cv);
                    break;
                }
            }
        }
        if (!success) {
            break;
        }
        // solo singer
        if (s->performer_type > 0) {
            sem_wait(&s->sem);
            continue;
        }
        // performance joining by singer
        int secondPerformer = -1;
        while (secondPerformer == -1 && PerformedPerformers != NumPerformers && !s->noExtension) {
            for (int i = 0; i < NumPerformers && PerformedPerformers != NumPerformers && !s->noExtension; i++) {
                if (performers[i].stageid != -1) {
                    continue;
                }
                pthread_mutex_lock(&performers[i].m);
                if (performers[i].stageid == -1 &&
                    performers[i].singer) {
                    sem_wait(&s->extension);
                    if (!s->noExtension) {
                        printf("\033[1;32m%s joined %s's performance, performance extended by %d seconds\n", performers[i].name, performers[s->performer].name, ExtensionTime);
                        fflush(stdout);
                        performers[i].stageid = s->id;
                        performers[i].performTime = ExtensionTime + perform_time - (time(NULL) - start_time);
                        performers[s->performer].extendTime = ExtensionTime;
                        secondPerformer++;
                    }
                    secondPerformer++;
                    sem_post(&s->extension);
                }
                pthread_mutex_unlock(&performers[i].m);
                if (secondPerformer != -1) {
                    if (secondPerformer > 0) {
                        pthread_cond_signal(&performers[i].cv);
                    }
                    break;
                }
            }
        }
        if (secondPerformer > 0) {
            sem_wait(&s->sem);
        }
        sem_wait(&s->sem);
    }
    return NULL;
}
/* ________________________________________________________ */
// code to be executed by the coordinator
static void *Coordinator(void *c1) {
    coordinator *c = (coordinator *)c1;
    while (PerformedPerformers != NumPerformers) {
        for (int i = 0; i < NumPerformers && PerformedPerformers != NumPerformers; i++) {
            if (!performers[i].finished) {
                continue;
            }
            pthread_mutex_lock(&performers[i].m);
            if (performers[i].finished) {
                printf("\033[0;35m%s collecting t-shirt\n", performers[i].name);
                fflush(stdout);
                performers[i].finished = false;
                sleep(tshirtTime);
                sem_post(&performers[i].tshirt);
            }
            pthread_mutex_unlock(&performers[i].m);
        }
    }
    return NULL;
}
/* ________________________________________________________ */
int main(void) {
    srand(time(0));
    scanf("%d%d%d%d%d%d%d", &NumPerformers, &NumAcousticStages, &NumElectricStages,
          &NumCoordinaters, &MinBrowseTime, &DiffBrowseTime, &MaxWaitingTime);
    if (NumCoordinaters == 0) {
        printf("\033[0;31mFinished (there is no coordintors)\n");
        fflush(stdout);
        return 0;
    }
    DiffBrowseTime = DiffBrowseTime - MinBrowseTime + 1;
    performers = malloc(NumPerformers * sizeof(performer));
    stages = malloc((NumAcousticStages + NumElectricStages) * sizeof(stage));
    coordinators = malloc(NumCoordinaters * sizeof(coordinator));
    pthread_mutex_init(&lock, NULL);
    for (int i = 0; i < NumPerformers; i++) {
        char s[100];
        char instrument;
        int arrivalTime;
        scanf("%s %c%d", s, &instrument, &arrivalTime);
        performers[i].name = malloc(strlen(s) + 1);
        strcpy(performers[i].name, s);
        performers[i].id = i;
        performers[i].singer = false;
        performers[i].stageid = 0;
        performers[i].extendTime = 0;
        performers[i].finished = false;
        sem_init(&performers[i].tshirt, 0, 0);
        pthread_mutex_init(&performers[i].m, NULL);
        pthread_cond_init(&performers[i].cv, NULL);
        performers[i].arrivalTime = arrivalTime;
        if (instrument == 'p') {
            performers[i].stage = 0;
            performers[i].instrument = malloc(strlen("piano") + 1);
            strcpy(performers[i].instrument, "piano");
        } else if (instrument == 'g') {
            performers[i].stage = 0;
            performers[i].instrument = malloc(strlen("guitar") + 1);
            strcpy(performers[i].instrument, "guitar");
        } else if (instrument == 'v') {
            performers[i].stage = 1;
            performers[i].instrument = malloc(strlen("violin") + 1);
            strcpy(performers[i].instrument, "violin");
        } else if (instrument == 'b') {
            performers[i].stage = -1;
            performers[i].instrument = malloc(strlen("bass") + 1);
            strcpy(performers[i].instrument, "bass");
        } else {
            performers[i].singer = true;
            performers[i].stage = 0;
            performers[i].instrument = malloc(strlen("singer") + 1);
            strcpy(performers[i].instrument, "singer");
        }
    }
    for (int i = 0; i < NumAcousticStages; i++) {
        stages[i].id = i;
        stages[i].type = 1;
        sem_init(&stages[i].sem, 0, 0);
        sem_init(&stages[i].extension, 0, 1);
    }
    for (int i = NumAcousticStages; i < NumAcousticStages + NumElectricStages; i++) {
        stages[i].id = i;
        stages[i].type = -1;
        sem_init(&stages[i].sem, 0, 0);
        sem_init(&stages[i].extension, 0, 1);
    }
    for (int i = 0; i < NumCoordinaters; i++) {
        coordinators[i].id = i;
    }
    for (int i = 0; i < NumPerformers; i++) {
        pthread_create(&performers[i].tid, NULL, Performer, (void *)&performers[i]);
    }
    for (int i = 0; i < NumAcousticStages + NumElectricStages; i++) {
        pthread_create(&stages[i].tid, NULL, Stage, (void *)&stages[i]);
    }
    for (int i = 0; i < NumCoordinaters; i++) {
        pthread_create(&coordinators[i].tid, NULL, Coordinator, (void *)&coordinators[i]);
    }
    for (int i = 0; i < NumCoordinaters; i++) {
        pthread_join(coordinators[i].tid, NULL);
    }
    for (int i = 0; i < NumAcousticStages + NumElectricStages; i++) {
        pthread_join(stages[i].tid, NULL);
        sem_destroy(&stages[i].sem);
        sem_destroy(&stages[i].extension);
    }
    for (int i = 0; i < NumPerformers; i++) {
        pthread_join(performers[i].tid, NULL);
        pthread_mutex_destroy(&performers[i].m);
        pthread_cond_destroy(&performers[i].cv);
        sem_destroy(&performers[i].tshirt);
    }
    pthread_mutex_destroy(&lock);
    printf("\033[0;31mFinished\n");
    fflush(stdout);
    return 0;
}
