#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define GRADES_COUNT 5
#define NAME_SIZE 64
#define LINE_SIZE 512
#define MAX_GRADE 255
#define EPSILON 1e-9

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_OPEN_ERROR,
    STATUS_READ_ERROR,
    STATUS_WRITE_ERROR,
    STATUS_INVALID_DATA
} status_t;

typedef struct {
    unsigned int id;
    char *name;
    char *surname;
    char *group;
    unsigned char *grades;
} Student;

typedef struct {
    Student *items;
    size_t count;
    size_t capacity;
} StudentList;

typedef enum {
    FIELD_ID,
    FIELD_SURNAME,
    FIELD_NAME,
    FIELD_GROUP
} field_t;

typedef int (*comparator_t)(const void *, const void *);

static status_t copy_string(const char *source, char **target)
{
    size_t len = strlen(source);
    char *copy = (char *)malloc(len + 1);

    if (copy == NULL) {
        return STATUS_NO_MEMORY;
    }
    memcpy(copy, source, len + 1);
    *target = copy;
    return STATUS_OK;
}

static int is_latin(const char *text)
{
    size_t i;

    if (text[0] == '\0') {
        return 0;
    }
    for (i = 0; text[i] != '\0'; i++) {
        if (!((text[i] >= 'a' && text[i] <= 'z') ||
              (text[i] >= 'A' && text[i] <= 'Z'))) {
            return 0;
        }
    }
    return 1;
}

static int is_blank(const char *text)
{
    size_t i;

    for (i = 0; text[i] != '\0'; i++) {
        if (!isspace((unsigned char)text[i])) {
            return 0;
        }
    }
    return 1;
}

void student_destroy(Student *student)
{
    if (student == NULL) {
        return;
    }
    free(student->name);
    free(student->surname);
    free(student->group);
    free(student->grades);
    student->name = NULL;
    student->surname = NULL;
    student->group = NULL;
    student->grades = NULL;
}

status_t student_create(Student *student, unsigned int id, const char *name,
                        const char *surname, const char *group,
                        const unsigned char *grades)
{
    status_t status;

    if (student == NULL || name == NULL || surname == NULL || group == NULL ||
        grades == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (!is_latin(name) || !is_latin(surname) || group[0] == '\0') {
        return STATUS_INVALID_DATA;
    }

    student->name = NULL;
    student->surname = NULL;
    student->group = NULL;
    student->grades = NULL;

    status = copy_string(name, &student->name);
    if (status == STATUS_OK) {
        status = copy_string(surname, &student->surname);
    }
    if (status == STATUS_OK) {
        status = copy_string(group, &student->group);
    }
    if (status == STATUS_OK) {
        student->grades = (unsigned char *)malloc(GRADES_COUNT);
        if (student->grades == NULL) {
            status = STATUS_NO_MEMORY;
        }
    }
    if (status != STATUS_OK) {
        student_destroy(student);
        return status;
    }
    memcpy(student->grades, grades, GRADES_COUNT);
    student->id = id;
    return STATUS_OK;
}

void list_destroy(StudentList *list)
{
    size_t i;

    if (list == NULL) {
        return;
    }
    for (i = 0; i < list->count; i++) {
        student_destroy(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

status_t list_add(StudentList *list, Student *student)
{
    Student *new_items;
    size_t new_capacity;

    if (list == NULL || student == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (list->count == list->capacity) {
        new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        new_items = (Student *)realloc(list->items, new_capacity * sizeof(Student));
        if (new_items == NULL) {
            return STATUS_NO_MEMORY;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count] = *student;
    list->count++;
    memset(student, 0, sizeof(Student));
    return STATUS_OK;
}

status_t parse_student(const char *line, Student *student)
{
    long long id;
    char name[NAME_SIZE];
    char surname[NAME_SIZE];
    char group[NAME_SIZE];
    int values[GRADES_COUNT];
    unsigned char grades[GRADES_COUNT];
    int consumed = 0;
    int i;

    if (line == NULL || student == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (sscanf(line, "%lld %63s %63s %63s %d %d %d %d %d %n", &id, name, surname,
               group, &values[0], &values[1], &values[2], &values[3],
               &values[4], &consumed) != 9 || line[consumed] != '\0') {
        return STATUS_INVALID_DATA;
    }
    if (id < 0 || id > UINT_MAX) {
        return STATUS_INVALID_DATA;
    }
    for (i = 0; i < GRADES_COUNT; i++) {
        if (values[i] < 0 || values[i] > MAX_GRADE) {
            return STATUS_INVALID_DATA;
        }
        grades[i] = (unsigned char)values[i];
    }
    return student_create(student, (unsigned int)id, name, surname, group, grades);
}

status_t load_students(const char *path, StudentList *list)
{
    FILE *file;
    char line[LINE_SIZE];
    size_t len;
    Student student;
    status_t status = STATUS_OK;
    int ch;

    if (path == NULL || list == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;

    file = fopen(path, "r");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }

    while (status == STATUS_OK && fgets(line, LINE_SIZE, file) != NULL) {
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        } else if (!feof(file)) {
            status = STATUS_INVALID_DATA;
            while ((ch = fgetc(file)) != '\n' && ch != EOF) {
            }
        }
        if (status == STATUS_OK && len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        if (status == STATUS_OK && !is_blank(line)) {
            status = parse_student(line, &student);
            if (status == STATUS_OK) {
                status = list_add(list, &student);
                student_destroy(&student);
            }
        }
    }
    if (status == STATUS_OK && ferror(file)) {
        status = STATUS_READ_ERROR;
    }

    fclose(file);
    if (status != STATUS_OK) {
        list_destroy(list);
    }
    return status;
}

static int match_student(const Student *student, field_t field,
                         const char *text, unsigned int id)
{
    switch (field) {
        case FIELD_ID:
            return student->id == id;
        case FIELD_SURNAME:
            return strcmp(student->surname, text) == 0;
        case FIELD_NAME:
            return strcmp(student->name, text) == 0;
        case FIELD_GROUP:
            return strcmp(student->group, text) == 0;
    }
    return 0;
}

status_t find_students(const StudentList *list, field_t field, const char *text,
                       const Student ***result, size_t *result_count)
{
    const Student **found;
    unsigned long long number = 0;
    unsigned int id = 0;
    char *end;
    size_t count = 0;
    size_t i;

    if (list == NULL || text == NULL || result == NULL || result_count == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *result = NULL;
    *result_count = 0;

    if (field == FIELD_ID) {
        if (!isdigit((unsigned char)text[0])) {
            return STATUS_INVALID_DATA;
        }
        number = strtoull(text, &end, 10);
        if (*end != '\0' || number > UINT_MAX) {
            return STATUS_INVALID_DATA;
        }
        id = (unsigned int)number;
    }

    if (list->count == 0) {
        return STATUS_OK;
    }
    found = (const Student **)malloc(list->count * sizeof(const Student *));
    if (found == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (i = 0; i < list->count; i++) {
        if (match_student(&list->items[i], field, text, id)) {
            found[count] = &list->items[i];
            count++;
        }
    }
    if (count == 0) {
        free(found);
        return STATUS_OK;
    }
    *result = found;
    *result_count = count;
    return STATUS_OK;
}

static unsigned long student_sum(const Student *student)
{
    unsigned long sum = 0;
    int i;

    for (i = 0; i < GRADES_COUNT; i++) {
        sum += student->grades[i];
    }
    return sum;
}

status_t student_average(const Student *student, double *average)
{
    if (student == NULL || average == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *average = (double)student_sum(student) / GRADES_COUNT;
    return STATUS_OK;
}

status_t find_above_average(const StudentList *list, double epsilon,
                            const Student ***result, size_t *result_count)
{
    const Student **found;
    unsigned long total = 0;
    double overall;
    double average;
    size_t count = 0;
    size_t i;

    if (list == NULL || result == NULL || result_count == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *result = NULL;
    *result_count = 0;
    if (list->count == 0) {
        return STATUS_OK;
    }

    for (i = 0; i < list->count; i++) {
        total += student_sum(&list->items[i]);
    }
    overall = (double)total / ((double)list->count * GRADES_COUNT);

    found = (const Student **)malloc(list->count * sizeof(const Student *));
    if (found == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (i = 0; i < list->count; i++) {
        average = (double)student_sum(&list->items[i]) / GRADES_COUNT;
        if (average > overall + epsilon) {
            found[count] = &list->items[i];
            count++;
        }
    }
    if (count == 0) {
        free(found);
        return STATUS_OK;
    }
    *result = found;
    *result_count = count;
    return STATUS_OK;
}

status_t sort_students(StudentList *list, comparator_t comparator)
{
    if (list == NULL || comparator == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (list->count > 1) {
        qsort(list->items, list->count, sizeof(Student), comparator);
    }
    return STATUS_OK;
}

static int compare_by_id(const void *first, const void *second)
{
    const Student *a = (const Student *)first;
    const Student *b = (const Student *)second;

    return (a->id > b->id) - (a->id < b->id);
}

static int compare_by_surname(const void *first, const void *second)
{
    const Student *a = (const Student *)first;
    const Student *b = (const Student *)second;

    return strcmp(a->surname, b->surname);
}

static int compare_by_name(const void *first, const void *second)
{
    const Student *a = (const Student *)first;
    const Student *b = (const Student *)second;

    return strcmp(a->name, b->name);
}

static int compare_by_group(const void *first, const void *second)
{
    const Student *a = (const Student *)first;
    const Student *b = (const Student *)second;

    return strcmp(a->group, b->group);
}

status_t write_student_trace(FILE *file, const Student *student)
{
    double average;
    status_t status;

    if (file == NULL || student == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    status = student_average(student, &average);
    if (status != STATUS_OK) {
        return status;
    }
    if (fprintf(file, "%s %s, группа %s, средняя оценка %.2f\n", student->surname,
                student->name, student->group, average) < 0) {
        return STATUS_WRITE_ERROR;
    }
    return STATUS_OK;
}

status_t write_names_trace(FILE *file, const Student *const *list, size_t count)
{
    size_t i;

    if (file == NULL || (list == NULL && count > 0)) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (count == 0) {
        return STATUS_OK;
    }
    if (fprintf(file, "Студенты со средним баллом выше общего:\n") < 0) {
        return STATUS_WRITE_ERROR;
    }
    for (i = 0; i < count; i++) {
        if (fprintf(file, "%s %s\n", list[i]->surname, list[i]->name) < 0) {
            return STATUS_WRITE_ERROR;
        }
    }
    if (fflush(file) != 0) {
        return STATUS_WRITE_ERROR;
    }
    return STATUS_OK;
}

static void print_error(status_t status)
{
    switch (status) {
        case STATUS_OK:
            break;
        case STATUS_INVALID_ARGUMENT:
            printf("Ошибка: неверные аргументы\n");
            break;
        case STATUS_NO_MEMORY:
            printf("Ошибка: не удалось выделить память\n");
            break;
        case STATUS_OPEN_ERROR:
            printf("Ошибка: не удалось открыть файл\n");
            break;
        case STATUS_READ_ERROR:
            printf("Ошибка: не удалось прочитать данные\n");
            break;
        case STATUS_WRITE_ERROR:
            printf("Ошибка: не удалось записать данные в файл\n");
            break;
        case STATUS_INVALID_DATA:
            printf("Ошибка: некорректные данные\n");
            break;
    }
}

static void print_student(const Student *student)
{
    int i;

    printf("%u %s %s %s |", student->id, student->name, student->surname,
           student->group);
    for (i = 0; i < GRADES_COUNT; i++) {
        printf(" %d", student->grades[i]);
    }
    printf("\n");
}

static void print_found(const Student *const *list, size_t count)
{
    size_t i;

    if (count == 0) {
        printf("Ничего не найдено\n");
        return;
    }
    for (i = 0; i < count; i++) {
        print_student(list[i]);
    }
}

static status_t read_line(const char *prompt, char *buffer, size_t size)
{
    size_t len;
    int ch;

    printf("%s", prompt);
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return STATUS_READ_ERROR;
    }
    len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
        return STATUS_OK;
    }
    if (feof(stdin)) {
        return STATUS_OK;
    }
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
    return STATUS_INVALID_DATA;
}

static status_t read_choice(int *choice)
{
    char buffer[LINE_SIZE];
    char *end;
    long number;
    status_t status;

    status = read_line("Выбор: ", buffer, LINE_SIZE);
    if (status != STATUS_OK) {
        return status;
    }
    number = strtol(buffer, &end, 10);
    if (end == buffer || *end != '\0' || number < 0 || number > 1000) {
        return STATUS_INVALID_DATA;
    }
    *choice = (int)number;
    return STATUS_OK;
}

static void dialog_find(const StudentList *list, field_t field, const char *prompt)
{
    char text[LINE_SIZE];
    const Student **found = NULL;
    size_t count = 0;
    status_t status;

    status = read_line(prompt, text, LINE_SIZE);
    if (status == STATUS_OK) {
        status = find_students(list, field, text, &found, &count);
    }
    if (status != STATUS_OK) {
        print_error(status);
        return;
    }
    print_found(found, count);
    free(found);
}

static void dialog_sort(StudentList *list, comparator_t comparator)
{
    status_t status = sort_students(list, comparator);

    if (status == STATUS_OK) {
        printf("Список отсортирован\n");
    } else {
        print_error(status);
    }
}

static void dialog_show_all(const StudentList *list)
{
    size_t i;

    if (list->count == 0) {
        printf("Список пуст\n");
        return;
    }
    for (i = 0; i < list->count; i++) {
        print_student(&list->items[i]);
    }
}

static void dialog_trace_by_id(const StudentList *list, FILE *trace)
{
    char text[LINE_SIZE];
    const Student **found = NULL;
    size_t count = 0;
    size_t i;
    status_t status;

    status = read_line("Введите id: ", text, LINE_SIZE);
    if (status == STATUS_OK) {
        status = find_students(list, FIELD_ID, text, &found, &count);
    }
    if (status != STATUS_OK) {
        print_error(status);
        return;
    }
    if (count == 0) {
        printf("Студент с таким id не найден\n");
        return;
    }
    for (i = 0; i < count && status == STATUS_OK; i++) {
        status = write_student_trace(trace, found[i]);
    }
    if (status == STATUS_OK && fflush(trace) != 0) {
        status = STATUS_WRITE_ERROR;
    }
    free(found);
    if (status == STATUS_OK) {
        printf("Данные записаны в трассировочный файл\n");
    } else {
        print_error(status);
    }
}

static void dialog_trace_above_average(const StudentList *list, FILE *trace)
{
    const Student **found = NULL;
    size_t count = 0;
    status_t status;

    status = find_above_average(list, EPSILON, &found, &count);
    if (status == STATUS_OK) {
        status = write_names_trace(trace, found, count);
    }
    free(found);
    if (status == STATUS_OK) {
        if (count == 0) {
            printf("Нет студентов с баллом выше среднего\n");
        } else {
            printf("Найдено студентов: %zu. Данные записаны в трассировочный файл\n",
                   count);
        }
    } else {
        print_error(status);
    }
}

static void print_menu(void)
{
    printf("\n1. Найти по id\n");
    printf("2. Найти по фамилии\n");
    printf("3. Найти по имени\n");
    printf("4. Найти по группе\n");
    printf("5. Сортировать по id\n");
    printf("6. Сортировать по фамилии\n");
    printf("7. Сортировать по имени\n");
    printf("8. Сортировать по группе\n");
    printf("9. Показать всех студентов\n");
    printf("10. В трассировочный файл: студент по id\n");
    printf("11. В трассировочный файл: студенты с баллом выше среднего\n");
    printf("0. Выход\n");
}

static void run_menu(StudentList *list, FILE *trace)
{
    int choice = -1;
    status_t status;

    while (choice != 0) {
        print_menu();
        status = read_choice(&choice);
        if (status == STATUS_READ_ERROR) {
            break;
        }
        if (status != STATUS_OK) {
            print_error(status);
            choice = -1;
            continue;
        }
        switch (choice) {
            case 1:
                dialog_find(list, FIELD_ID, "Введите id: ");
                break;
            case 2:
                dialog_find(list, FIELD_SURNAME, "Введите фамилию: ");
                break;
            case 3:
                dialog_find(list, FIELD_NAME, "Введите имя: ");
                break;
            case 4:
                dialog_find(list, FIELD_GROUP, "Введите группу: ");
                break;
            case 5:
                dialog_sort(list, compare_by_id);
                break;
            case 6:
                dialog_sort(list, compare_by_surname);
                break;
            case 7:
                dialog_sort(list, compare_by_name);
                break;
            case 8:
                dialog_sort(list, compare_by_group);
                break;
            case 9:
                dialog_show_all(list);
                break;
            case 10:
                dialog_trace_by_id(list, trace);
                break;
            case 11:
                dialog_trace_above_average(list, trace);
                break;
            case 0:
                break;
            default:
                printf("Неизвестный пункт меню\n");
                break;
        }
    }
}

int main(int argc, char *argv[])
{
    StudentList list;
    FILE *trace;
    status_t status;

    if (argc != 3) {
        printf("Использование: %s <файл со студентами> <трассировочный файл>\n",
               argv[0]);
        return 1;
    }

    status = load_students(argv[1], &list);
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }

    trace = fopen(argv[2], "w");
    if (trace == NULL) {
        print_error(STATUS_OPEN_ERROR);
        list_destroy(&list);
        return 1;
    }

    run_menu(&list, trace);

    fclose(trace);
    list_destroy(&list);

    return 0;
}