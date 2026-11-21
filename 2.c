#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_TALK,
    STATUS_DUPLICATE_ID,
    STATUS_INVALID_TIME,
    STATUS_READ_ERROR
} status_t;

typedef struct {
    int id;
    int start;
    int finish;
} Talk;

static int compare_by_id(const void *a, const void *b)
{
    const Talk *x = (const Talk *)a;
    const Talk *y = (const Talk *)b;

    return (x->id > y->id) - (x->id < y->id);
}

static int compare_by_finish(const void *a, const void *b)
{
    const Talk *x = (const Talk *)a;
    const Talk *y = (const Talk *)b;

    if (x->finish != y->finish) {
        return (x->finish > y->finish) - (x->finish < y->finish);
    }
    return (x->start > y->start) - (x->start < y->start);
}

status_t parse_time(const char *str, int *minutes)
{
    int hours;
    int mins;

    if (str == NULL || minutes == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (strlen(str) != 5 || str[2] != ':') {
        return STATUS_INVALID_TIME;
    }
    if (!isdigit((unsigned char)str[0]) || !isdigit((unsigned char)str[1]) ||
        !isdigit((unsigned char)str[3]) || !isdigit((unsigned char)str[4])) {
        return STATUS_INVALID_TIME;
    }

    hours = (str[0] - '0') * 10 + (str[1] - '0');
    mins = (str[3] - '0') * 10 + (str[4] - '0');

    if (hours > 23 || mins > 59) {
        return STATUS_INVALID_TIME;
    }

    *minutes = hours * 60 + mins;
    return STATUS_OK;
}

status_t max_talks(const Talk *talks, const size_t n, const size_t k, size_t *result)
{
    Talk *sorted;
    int *rooms;
    size_t rooms_count;
    size_t i;
    size_t j;
    size_t best;
    size_t count = 0;

    if (talks == NULL || result == NULL || n == 0 || k == 0) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < n; i++) {
        if (talks[i].id <= 0 || talks[i].start < 0 ||
            talks[i].start >= talks[i].finish) {
            return STATUS_INVALID_TALK;
        }
    }

    sorted = (Talk *)malloc(n * sizeof(Talk));
    if (sorted == NULL) {
        return STATUS_NO_MEMORY;
    }
    memcpy(sorted, talks, n * sizeof(Talk));

    qsort(sorted, n, sizeof(Talk), compare_by_id);
    for (i = 1; i < n; i++) {
        if (sorted[i].id == sorted[i - 1].id) {
            free(sorted);
            return STATUS_DUPLICATE_ID;
        }
    }

    qsort(sorted, n, sizeof(Talk), compare_by_finish);

    rooms_count = k < n ? k : n;
    rooms = (int *)calloc(rooms_count, sizeof(int));
    if (rooms == NULL) {
        free(sorted);
        return STATUS_NO_MEMORY;
    }

    for (i = 0; i < n; i++) {
        best = rooms_count;
        for (j = 0; j < rooms_count; j++) {
            if (rooms[j] <= sorted[i].start) {
                if (best == rooms_count || rooms[j] > rooms[best]) {
                    best = j;
                }
            }
        }
        if (best != rooms_count) {
            rooms[best] = sorted[i].finish;
            count++;
        }
    }

    free(rooms);
    free(sorted);

    *result = count;
    return STATUS_OK;
}

status_t read_talks(const size_t n, Talk *talks)
{
    size_t i;
    int id;
    int start;
    int finish;
    char start_str[16];
    char finish_str[16];
    status_t status;

    if (talks == NULL || n == 0) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < n; i++) {
        if (scanf("%d %15s %15s", &id, start_str, finish_str) != 3) {
            return STATUS_READ_ERROR;
        }
        status = parse_time(start_str, &start);
        if (status != STATUS_OK) {
            return status;
        }
        status = parse_time(finish_str, &finish);
        if (status != STATUS_OK) {
            return status;
        }
        talks[i].id = id;
        talks[i].start = start;
        talks[i].finish = finish;
    }

    return STATUS_OK;
}

int main(void)
{
    int n;
    int k;
    Talk *talks;
    size_t answer;
    status_t status;

    if (scanf("%d %d", &n, &k) != 2 || n <= 0 || k <= 0) {
        printf("Ошибка: неверные n и k\n");
        return 1;
    }

    talks = (Talk *)malloc((size_t)n * sizeof(Talk));
    if (talks == NULL) {
        printf("Ошибка: не удалось выделить память\n");
        return 1;
    }

    status = read_talks((size_t)n, talks);
    if (status == STATUS_OK) {
        status = max_talks(talks, (size_t)n, (size_t)k, &answer);
    }

    switch (status) {
        case STATUS_OK:
            printf("%zu\n", answer);
            break;
        case STATUS_INVALID_ARGUMENT:
            printf("Ошибка: неверные аргументы\n");
            break;
        case STATUS_NO_MEMORY:
            printf("Ошибка: не удалось выделить память\n");
            break;
        case STATUS_INVALID_TALK:
            printf("Ошибка: неверные данные доклада\n");
            break;
        case STATUS_DUPLICATE_ID:
            printf("Ошибка: идентификаторы докладов повторяются\n");
            break;
        case STATUS_INVALID_TIME:
            printf("Ошибка: неверный формат времени\n");
            break;
        case STATUS_READ_ERROR:
            printf("Ошибка: не удалось прочитать данные\n");
            break;
    }

    free(talks);

    return status == STATUS_OK ? 0 : 1;
}