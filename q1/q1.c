#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <unistd.h>
#include <wait.h>

void swap(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

int *shareMem(size_t size) {
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (int *)shmat(shm_id, NULL, 0);
}
/* merge function of merge sort */
void merge(int arr[], int l, int m, int r) {
    int i, j, k;
    int n1 = m - l + 1;
    int n2 = r - m;
    int a[n1], b[n2];
    for (i = 0; i < n1; i++) {
        a[i] = arr[l + i];
    }
    for (j = 0; j < n2; j++) {
        b[j] = arr[m + 1 + j];
    }
    i = 0;
    j = 0;
    k = l;
    while (i < n1 && j < n2) {
        if (a[i] <= b[j]) {
            arr[k] = a[i];
            i++;
        } else {
            arr[k] = b[j];
            j++;
        }
        k++;
    }
    while (i < n1) {
        arr[k] = a[i];
        i++;
        k++;
    }
    while (j < n2) {
        arr[k] = b[j];
        j++;
        k++;
    }
}
/* Code for selection sort */
void selectionsort(int arr[], int l, int r) {
    int i, j, min_idx;
    for (i = l; i <= r; i++) {
        min_idx = i;
        for (j = i + 1; j <= r; j++) {
            if (arr[j] < arr[min_idx]) {
                min_idx = j;
            }
        }
        swap(&arr[min_idx], &arr[i]);
    }
}
/* Code for normal merge sort */
void normal_mergesort(int arr[], int l, int r) {
    int m;
    if (r - l >= 4) {
        m = (l + r) / 2;
        normal_mergesort(arr, l, m);
        normal_mergesort(arr, m + 1, r);
        merge(arr, l, m, r);
    } else {
        selectionsort(arr, l, r);
    }
}
/* Code for concurrent merge sort using processes */
void mergesort(int *arr, int low, int high) {
    if (high - low >= 4) {
        int mid = (low + high) / 2;
        int pid1 = fork();
        int pid2;
        if (pid1 == 0) {
            mergesort(arr, low, mid);
            _exit(0);
        } else {
            pid2 = fork();
            if (pid2 == 0) {
                mergesort(arr, mid + 1, high);
                _exit(0);
            } else {
                int status;
                waitpid(pid1, &status, 0);
                waitpid(pid2, &status, 0);
                merge(arr, low, mid, high);
            }
        }
    } else {
        selectionsort(arr, low, high);
    }
}

struct arg {
    int l;
    int r;
    int *arr;
};
/* Code for threaded merge sort */
void *threaded_mergesort(void *a) {
    struct arg *args = (struct arg *)a;

    int l = args->l;
    int r = args->r;
    int *arr = args->arr;
    if (l >= r) {
        return NULL;
    } else if (r - l < 4) {
        selectionsort(arr, l, r);
        return NULL;
    }
    int m = (l + r) / 2;
    struct arg a1;
    a1.l = l;
    a1.r = m;
    a1.arr = arr;
    pthread_t tid1;
    pthread_create(&tid1, NULL, threaded_mergesort, &a1);
    struct arg a2;
    a2.l = m + 1;
    a2.r = r;
    a2.arr = arr;
    pthread_t tid2;
    pthread_create(&tid2, NULL, threaded_mergesort, &a2);
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    merge(arr, l, m, r);
}

/* Analysis by running normal and concurrent merge sort */
void runSorts(int n) {
    struct timespec ts;
    int *arr = shareMem(sizeof(int) * n);
    for (int i = 0; i < n; i++) {
        scanf("%d", arr + i);
    }
    int b[n], c[n];
    for (int i = 0; i < n; i++) {
        b[i] = arr[i];
        c[i] = arr[i];
    }
    printf("Running concurrent mergesort for n = %d\n", n);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    long double st = ts.tv_nsec / (1e9) + ts.tv_sec;
    mergesort(arr, 0, n - 1);
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    long double en = ts.tv_nsec / (1e9) + ts.tv_sec;
    printf("time = %Lf\n", en - st);
    long double t1 = en - st;
    pthread_t tid;
    struct arg a;
    a.l = 0;
    a.r = n - 1;
    a.arr = b;
    printf("Running threaded concurrent mergesort for n = %d\n", n);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    st = ts.tv_nsec / (1e9) + ts.tv_sec;
    pthread_create(&tid, NULL, threaded_mergesort, &a);
    pthread_join(tid, NULL);
    for (int i = 0; i < n; i++) {
        printf("%d ", a.arr[i]);
    }
    printf("\n");
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    en = ts.tv_nsec / (1e9) + ts.tv_sec;
    printf("time = %Lf\n", en - st);
    long double t2 = en - st;
    printf("Running normal mergesort for n = %d\n", n);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    st = ts.tv_nsec / (1e9) + ts.tv_sec;
    normal_mergesort(c, 0, n - 1);
    for (int i = 0; i < n; i++) {
        printf("%d ", c[i]);
    }
    printf("\n");
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    en = ts.tv_nsec / (1e9) + ts.tv_sec;
    printf("time = %Lf\n", en - st);
    long double t3 = en - st;
    printf("Normal mergesort ran:\n\t[ %Lf ] times faster than concurrent mergesort\n\t[ %Lf ] times faster than threaded concurrent mergesort\n", t1 / t3, t2 / t3);
    shmdt(arr);
}

int main(void) {
    int n;
    scanf("%d", &n);
    runSorts(n);
    return 0;
}
