#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
/* ________________________________________________________ */
#define minimum(a, b, c) (a < b ? (a < c ? a : c) : (b < c ? b : c))
/* ________________________________________________________ */
// declarations and definations of global variables
pthread_mutex_t lock;
static int NumCompanies;
static int NumVaccinationZones;
static int NumStudents;
static int VaccinatedStudents = 0;
static int WaitingStudents = 0;
static const int MinBrowseTime = 0;
static const int DiffBrowseTime = 6;
static const int MinPrepTime = 2;
static const int DiffPrepTime = 4;
static const int MinVaccinateCapacity = 10;
static const int DiffVaccinateCapacity = 11;
static const int MinNumBatches = 1;
static const int DiffNumBatches = 5;
// static const int MinSlotSize = 1;
// static const int DiffSlotSize = 8;
static double *VaccineSuccessProb;
/* ________________________________________________________ */
/* functions to generate random sleep times and model the variations in
* execution that come with a real, concurrent, mostly unpredictable system */
static int getBrowseTime() {
    return rand() % DiffBrowseTime + MinBrowseTime;
}
static int getPrepTime() {
    return rand() % DiffPrepTime + MinPrepTime;
}
static int getVaccinationCapacity() {
    return rand() % DiffVaccinateCapacity + MinVaccinateCapacity;
}
static int getSlotSize() {
    return 8;
}
static int getNumBatches() {
    return rand() % DiffNumBatches + MinNumBatches;
}
/* ________________________________________________________ */
/* Some necessary structs for coordination between students, 
vaccination zones, and pharmaceutical companies */
typedef struct vaccination_zone {
    int id;
    int capacity;
    int batchCompany;
    int NumSlots;
    bool ready;
    pthread_t tid;
    pthread_mutex_t m;
} vaccination_zone;
typedef struct company {
    int id;
    int NumBatches;
    pthread_t tid;
    pthread_mutex_t m;
} company;
typedef struct student {
    int id;
    int uid;
    int cid;
    bool vaccinated;
    pthread_t tid;
    pthread_mutex_t m;
} student;
student *students;
vaccination_zone *vaccination_zones;
company *companies;
/* Declarations of functions used in the code */
static void *Student(void *);
static void getSlot(student *);
static bool testStudent(int);
/* ________________________________________________________ */
static void *Vaccination(void *);
static void getBatch(vaccination_zone *);
static void allocateSlots(vaccination_zone *);
static void giveVaccination(vaccination_zone *);
/* ________________________________________________________ */
static void *Company(void *);
static bool prepBatches(company *);
static bool allocateBatch(company *);
/* ________________________________________________________ */
// code to be executed by a student thread
static void *Student(void *s1) {
    student *s = (student *)s1;
    sleep(getBrowseTime());
    int numTests = 0;
    while (numTests < 3) {
        printf("\033[1;33mStudent #%d has arrived for his #%d round of vaccination\n\n", s->id + 1, numTests + 1);
        fflush(stdout);
        pthread_mutex_lock(&lock);
        WaitingStudents++;
        pthread_mutex_unlock(&lock);
        getSlot(s);
        bool test = testStudent(s->cid);
        const char *verb = test == 0 ? "NEGATIVE" : "POSITIVE";
        printf("\033[1;33mStudent #%d has tested %s for antibodies\n\n", s->id + 1, verb);
        fflush(stdout);
        if (test) {
            pthread_mutex_lock(&lock);
            VaccinatedStudents++;
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        numTests++;
    }
    pthread_mutex_lock(&lock);
    VaccinatedStudents++;
    pthread_mutex_unlock(&lock);
    return NULL;
}
// code for getting a slot for a student
static void getSlot(student *s) {
    printf("\033[1;33mStudent #%d is waiting to be allocated a slot on a vaccination zone\n\n", s->id + 1);
    fflush(stdout);
    bool success = false;
    while (!success) {
        for (int i = 0; i < NumVaccinationZones; i++) {
            if (!vaccination_zones[i].ready) {
                continue;
            }
            pthread_mutex_lock(&vaccination_zones[i].m);
            if (vaccination_zones[i].NumSlots > 0 && vaccination_zones[i].ready) {
                printf("\033[1;33mStudent #%d assigned a slot on the vaccination zone #%d and waiting to be vaccinated\n\n", s->id + 1, vaccination_zones[i].id + 1);
                fflush(stdout);
                s->uid = vaccination_zones[i].id;
                vaccination_zones[i].NumSlots--;
                success = true;
                pthread_mutex_unlock(&vaccination_zones[i].m);
                break;
            }
            pthread_mutex_unlock(&vaccination_zones[i].m);
        }
    }
    s->vaccinated = false;
    while (!s->vaccinated) {
        sleep(1);
    }
}
// code to test anti-bodies
static bool testStudent(int cid) {
    // 0 <= value <= 1
    double value = (double)rand() / RAND_MAX;
    return value <= VaccineSuccessProb[cid];
}
/* ________________________________________________________ */
// code to be executed by a vaccination zone thread
static void *Vaccination(void *v1) {
    vaccination_zone *v = (vaccination_zone *)v1;
    while (true) {
        if (v->capacity == 0) {
            printf("\033[1;32mVaccination zone #%d has run out of vaccines\n\n", v->id + 1);
            fflush(stdout);
            getBatch(v);
        }
        pthread_mutex_lock(&lock);
        int slots = getSlotSize();
        v->NumSlots = minimum(slots, WaitingStudents, v->capacity);
        if (VaccinatedStudents == NumStudents) {
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        WaitingStudents -= v->NumSlots;
        v->capacity -= v->NumSlots;
        pthread_mutex_unlock(&lock);
        if (v->NumSlots > 0) {
            allocateSlots(v);
            giveVaccination(v);
        }
    }
}
// code for getting a batch of vaccinations for a vaccination zone
static void getBatch(vaccination_zone *v) {
    if (v->batchCompany >= 0) {
        pthread_mutex_lock(&companies[v->batchCompany].m);
        companies[v->batchCompany].NumBatches--;
        pthread_mutex_unlock(&companies[v->batchCompany].m);
    }
    v->batchCompany = -1;
    while (v->batchCompany == -1 && VaccinatedStudents != NumStudents) {
        sleep(1);
    }
}
// code for allocating slots
static void allocateSlots(vaccination_zone *v) {
    printf("\033[1;32mVaccination zone #%d is ready to vaccinate with #%d slots\n\n", v->id + 1, v->NumSlots);
    fflush(stdout);
    v->ready = true;
    while (v->NumSlots > 0) {
        sleep(1);
    }
    v->ready = false;
}
// code to give vaccinations to students
static void giveVaccination(vaccination_zone *v) {
    printf("\033[1;32mVaccination zone #%d entering vaccination phase\n\n", v->id + 1);
    fflush(stdout);
    for (int i = 0; i < NumStudents; i++) {
        if (students[i].uid != v->id) {
            continue;
        }
        pthread_mutex_lock(&students[i].m);
        if (students[i].uid == v->id && !students[i].vaccinated) {
            students[i].cid = v->batchCompany;
            students[i].uid = -1;
            sleep(1);  // to make the simulation realistic
            printf("\033[1;32mStudent #%d on vaccination zone #%d has been vaccinated which has success probability %0.2lf\n\n", students[i].id + 1, v->id + 1, VaccineSuccessProb[v->batchCompany]);
            fflush(stdout);
            students[i].vaccinated = true;
        }
        pthread_mutex_unlock(&students[i].m);
    }
}
/* ________________________________________________________ */
// code to be executed by a pharmaceutical company thread
static void *Company(void *c1) {
    company *c = (company *)c1;
    bool success = true;
    while (success) {
        success = prepBatches(c);
        success = allocateBatch(c);
    }
    return NULL;
}
// code to prepare a set of new batches
static bool prepBatches(company *c) {
    c->NumBatches = getNumBatches();
    int prep_time = getPrepTime();
    if (VaccinatedStudents == NumStudents) {
        return false;
    }
    printf("\033[1;34mPharmaceutical company #%d is preparing #%d batches of vaccines which have success probability %0.2lf\n\n", c->id + 1, c->NumBatches, VaccineSuccessProb[c->id]);
    fflush(stdout);
    sleep(prep_time);
    printf("\033[1;34mPharmaceutical company #%d has prepared #%d batches of vaccines which have success probability %0.2lf\n\n", c->id + 1, c->NumBatches, VaccineSuccessProb[c->id]);
    fflush(stdout);
    return true;
}
// code to allocate the prepared set of batches
static bool allocateBatch(company *c) {
    int totalBathches = c->NumBatches;
    while (totalBathches > 0) {
        for (int i = 0; i < NumVaccinationZones && totalBathches > 0; i++) {
            if (VaccinatedStudents == NumStudents) {
                return false;
            }
            vaccination_zone *v = &vaccination_zones[i];
            if (v->batchCompany != -1) {
                continue;
            }
            pthread_mutex_lock(&v->m);
            if (v->batchCompany == -1 && v->capacity == 0) {
                printf("\033[1;34mPharmaceutical company #%d is delivering a vaccine batch to vaccination zone #%d which has success probability %0.2lf\n\n", c->id + 1, v->id + 1, VaccineSuccessProb[c->id]);
                fflush(stdout);
                sleep(getBrowseTime());
                v->capacity = getVaccinationCapacity();
                printf("\033[1;34mPharmaceutical company #%d has delivered vaccines to vaccination zone #%d, resuming vaccinations now\n\n", c->id + 1, v->id + 1);
                fflush(stdout);
                v->batchCompany = c->id;
                totalBathches--;
            }
            pthread_mutex_unlock(&v->m);
        }
    }
    while (c->NumBatches > 0) {
        if (VaccinatedStudents == NumStudents) {
            return false;
        }
        sleep(1);
    }
    printf("\033[1;34mAll the vaccines prepared by pharmaceutical company #%d are emptied, resuming production now\n\n", c->id + 1);
    fflush(stdout);
    return true;
}
/* ________________________________________________________ */
int main(void) {
    srand(time(0));
    /* ____________________________________________________ */
    printf("Enter the number of pharmaceutical companies: ");
    fflush(stdout);
    scanf("%d", &NumCompanies);
    printf("Enter the number of vaccination zones: ");
    fflush(stdout);
    scanf("%d", &NumVaccinationZones);
    printf("Enter the number of students to be vaccinated: ");
    fflush(stdout);
    scanf("%d", &NumStudents);
    /* ____________________________________________________ */
    // code for initializations and joining of threads
    printf("Enter the probabilities of success of each pharmaceutical company:\n");
    fflush(stdout);
    double totalProb = 0;
    VaccineSuccessProb = malloc(NumCompanies * sizeof(double));
    for (int i = 0; i < NumCompanies; i++) {
        scanf("%lf", &VaccineSuccessProb[i]);
        totalProb += VaccineSuccessProb[i];
    }
    if (NumCompanies == 0 || NumVaccinationZones == 0 || totalProb == 0) {
        printf("\033[1;31mSimulation over, thank you!\n\n");
        fflush(stdout);
        return 0;
    }
    /* ____________________________________________________ */
    pthread_mutex_init(&lock, NULL);
    students = malloc(NumStudents * sizeof(student));
    vaccination_zones = malloc(NumVaccinationZones * sizeof(vaccination_zone));
    companies = malloc(NumCompanies * sizeof(company));
    /* ____________________________________________________ */
    for (int i = 0; i < NumStudents; i++) {
        students[i].uid = -1;
        pthread_mutex_init(&students[i].m, NULL);
    }
    for (int i = 0; i < NumVaccinationZones; i++) {
        vaccination_zones[i].capacity = 0;
        vaccination_zones[i].batchCompany = INT_MIN;
        vaccination_zones[i].ready = false;
        pthread_mutex_init(&vaccination_zones[i].m, NULL);
    }
    for (int i = 0; i < NumCompanies; i++) {
        pthread_mutex_init(&companies[i].m, NULL);
    }
    for (int i = 0; i < NumCompanies; i++) {
        companies[i].id = i;
        pthread_create(&companies[i].tid, NULL, Company, (void *)&companies[i]);
    }
    for (int i = 0; i < NumVaccinationZones; i++) {
        vaccination_zones[i].id = i;
        pthread_create(&vaccination_zones[i].tid, NULL, Vaccination, (void *)&vaccination_zones[i]);
    }
    for (int i = 0; i < NumStudents; i++) {
        students[i].id = i;
        pthread_create(&students[i].tid, NULL, Student, (void *)&students[i]);
    }
    /* ____________________________________________________ */
    for (int i = 0; i < NumCompanies; i++) {
        pthread_join(companies[i].tid, NULL);
        pthread_mutex_destroy(&companies[i].m);
    }
    for (int i = 0; i < NumVaccinationZones; i++) {
        pthread_join(vaccination_zones[i].tid, NULL);
        pthread_mutex_destroy(&vaccination_zones[i].m);
    }
    for (int i = 0; i < NumStudents; i++) {
        pthread_join(students[i].tid, NULL);
        pthread_mutex_destroy(&students[i].m);
    }
    /* ____________________________________________________ */
    pthread_mutex_destroy(&lock);
    printf("\033[1;31mSimulation over, thank you!\n\n");
    fflush(stdout);
    return 0;
}
