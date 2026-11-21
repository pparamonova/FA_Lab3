#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NAME_SIZE 64
#define LINE_SIZE 256
#define EPSILON 1e-9

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_OPEN_ERROR,
    STATUS_READ_ERROR,
    STATUS_WRITE_ERROR,
    STATUS_INVALID_DATA,
    STATUS_NOT_FOUND
} status_t;

typedef struct {
    char surname[NAME_SIZE];
    char name[NAME_SIZE];
    char patronymic[NAME_SIZE];
    int day;
    int month;
    int year;
    char gender;
    double income;
} Liver;

typedef struct LiverNode {
    Liver liver;
    struct LiverNode *next;
} LiverNode;

typedef enum {
    OP_ADD,
    OP_REMOVE,
    OP_MODIFY
} op_type_t;

typedef struct Operation {
    op_type_t type;
    Liver before;
    Liver after;
    struct Operation *next;
} Operation;

typedef struct {
    LiverNode *head;
    size_t count;
    Operation *history;
    size_t history_count;
} Registry;

typedef struct {
    int use_surname;
    char surname[NAME_SIZE];
    int use_name;
    char name[NAME_SIZE];
    int use_patronymic;
    char patronymic[NAME_SIZE];
    int use_birth;
    int day;
    int month;
    int year;
    int use_gender;
    char gender;
    int use_income;
    double income;
} LiverFilter;

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

static int days_in_month(int year, int month)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        return 29;
    }
    return days[month - 1];
}

static int numbers_equal(double first, double second, double epsilon)
{
    return first - second <= epsilon && second - first <= epsilon;
}

status_t liver_create(Liver *liver, const char *surname, const char *name,
                      const char *patronymic, int day, int month, int year,
                      char gender, double income, double epsilon)
{
    if (liver == NULL || surname == NULL || name == NULL || patronymic == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (strlen(surname) >= NAME_SIZE || strlen(name) >= NAME_SIZE ||
        strlen(patronymic) >= NAME_SIZE) {
        return STATUS_INVALID_DATA;
    }
    if (!is_latin(surname) || !is_latin(name) ||
        (patronymic[0] != '\0' && !is_latin(patronymic))) {
        return STATUS_INVALID_DATA;
    }
    if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 ||
        day > days_in_month(year, month)) {
        return STATUS_INVALID_DATA;
    }
    if (gender != 'M' && gender != 'W') {
        return STATUS_INVALID_DATA;
    }
    if (income < -epsilon) {
        return STATUS_INVALID_DATA;
    }

    strcpy(liver->surname, surname);
    strcpy(liver->name, name);
    strcpy(liver->patronymic, patronymic);
    liver->day = day;
    liver->month = month;
    liver->year = year;
    liver->gender = gender;
    liver->income = income;
    return STATUS_OK;
}

static int liver_equal(const Liver *first, const Liver *second, double epsilon)
{
    return strcmp(first->surname, second->surname) == 0 &&
           strcmp(first->name, second->name) == 0 &&
           strcmp(first->patronymic, second->patronymic) == 0 &&
           first->day == second->day && first->month == second->month &&
           first->year == second->year && first->gender == second->gender &&
           numbers_equal(first->income, second->income, epsilon);
}

static long long liver_key(const Liver *liver)
{
    return ((long long)liver->year * 100 + liver->month) * 100 + liver->day;
}

void registry_init(Registry *reg)
{
    reg->head = NULL;
    reg->count = 0;
    reg->history = NULL;
    reg->history_count = 0;
}

static void clear_history(Registry *reg)
{
    Operation *op;

    while (reg->history != NULL) {
        op = reg->history;
        reg->history = op->next;
        free(op);
    }
    reg->history_count = 0;
}

void registry_destroy(Registry *reg)
{
    LiverNode *node;

    if (reg == NULL) {
        return;
    }
    while (reg->head != NULL) {
        node = reg->head;
        reg->head = node->next;
        free(node);
    }
    reg->count = 0;
    clear_history(reg);
}

static void link_node(Registry *reg, LiverNode *node)
{
    LiverNode *prev = NULL;
    LiverNode *current = reg->head;
    long long key = liver_key(&node->liver);

    while (current != NULL && liver_key(&current->liver) >= key) {
        prev = current;
        current = current->next;
    }
    node->next = current;
    if (prev == NULL) {
        reg->head = node;
    } else {
        prev->next = node;
    }
    reg->count++;
}

static LiverNode *node_at(const Registry *reg, size_t index)
{
    LiverNode *node = reg->head;
    size_t i;

    if (index >= reg->count) {
        return NULL;
    }
    for (i = 0; i < index; i++) {
        node = node->next;
    }
    return node;
}

static LiverNode *unlink_at(Registry *reg, size_t index)
{
    LiverNode *prev = NULL;
    LiverNode *node = reg->head;
    size_t i;

    if (index >= reg->count) {
        return NULL;
    }
    for (i = 0; i < index; i++) {
        prev = node;
        node = node->next;
    }
    if (prev == NULL) {
        reg->head = node->next;
    } else {
        prev->next = node->next;
    }
    node->next = NULL;
    reg->count--;
    return node;
}

static LiverNode *unlink_equal(Registry *reg, const Liver *target, double epsilon)
{
    LiverNode *prev = NULL;
    LiverNode *node = reg->head;

    while (node != NULL && !liver_equal(&node->liver, target, epsilon)) {
        prev = node;
        node = node->next;
    }
    if (node == NULL) {
        return NULL;
    }
    if (prev == NULL) {
        reg->head = node->next;
    } else {
        prev->next = node->next;
    }
    node->next = NULL;
    reg->count--;
    return node;
}

static status_t insert_liver(Registry *reg, const Liver *liver)
{
    LiverNode *node = (LiverNode *)malloc(sizeof(LiverNode));

    if (node == NULL) {
        return STATUS_NO_MEMORY;
    }
    node->liver = *liver;
    link_node(reg, node);
    return STATUS_OK;
}

static status_t create_operation(op_type_t type, const Liver *before,
                                 const Liver *after, Operation **result)
{
    Operation *op = (Operation *)malloc(sizeof(Operation));

    if (op == NULL) {
        return STATUS_NO_MEMORY;
    }
    op->type = type;
    memset(&op->before, 0, sizeof(Liver));
    memset(&op->after, 0, sizeof(Liver));
    if (before != NULL) {
        op->before = *before;
    }
    if (after != NULL) {
        op->after = *after;
    }
    op->next = NULL;
    *result = op;
    return STATUS_OK;
}

static void push_operation(Registry *reg, Operation *op)
{
    op->next = reg->history;
    reg->history = op;
    reg->history_count++;
}

status_t registry_add(Registry *reg, const Liver *liver)
{
    Operation *op;
    status_t status;

    if (reg == NULL || liver == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    status = create_operation(OP_ADD, NULL, liver, &op);
    if (status != STATUS_OK) {
        return status;
    }
    status = insert_liver(reg, liver);
    if (status != STATUS_OK) {
        free(op);
        return status;
    }
    push_operation(reg, op);
    return STATUS_OK;
}

status_t registry_remove(Registry *reg, size_t index)
{
    LiverNode *node;
    Operation *op;
    status_t status;

    if (reg == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    node = node_at(reg, index);
    if (node == NULL) {
        return STATUS_NOT_FOUND;
    }
    status = create_operation(OP_REMOVE, &node->liver, NULL, &op);
    if (status != STATUS_OK) {
        return status;
    }
    node = unlink_at(reg, index);
    free(node);
    push_operation(reg, op);
    return STATUS_OK;
}

status_t registry_modify(Registry *reg, size_t index, const Liver *updated)
{
    LiverNode *node;
    Operation *op;
    status_t status;

    if (reg == NULL || updated == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    node = node_at(reg, index);
    if (node == NULL) {
        return STATUS_NOT_FOUND;
    }
    status = create_operation(OP_MODIFY, &node->liver, updated, &op);
    if (status != STATUS_OK) {
        return status;
    }
    node = unlink_at(reg, index);
    node->liver = *updated;
    link_node(reg, node);
    push_operation(reg, op);
    return STATUS_OK;
}

status_t registry_get(const Registry *reg, size_t index, Liver *liver)
{
    const LiverNode *node;

    if (reg == NULL || liver == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    node = node_at(reg, index);
    if (node == NULL) {
        return STATUS_NOT_FOUND;
    }
    *liver = node->liver;
    return STATUS_OK;
}

static status_t apply_undo(Registry *reg, const Operation *op, double epsilon)
{
    LiverNode *node;
    status_t status;

    switch (op->type) {
        case OP_ADD:
            node = unlink_equal(reg, &op->after, epsilon);
            if (node == NULL) {
                return STATUS_NOT_FOUND;
            }
            free(node);
            return STATUS_OK;
        case OP_REMOVE:
            return insert_liver(reg, &op->before);
        case OP_MODIFY:
            status = insert_liver(reg, &op->before);
            if (status != STATUS_OK) {
                return status;
            }
            node = unlink_equal(reg, &op->after, epsilon);
            if (node == NULL) {
                return STATUS_NOT_FOUND;
            }
            free(node);
            return STATUS_OK;
    }
    return STATUS_INVALID_ARGUMENT;
}

status_t registry_undo(Registry *reg, double epsilon, size_t *undone)
{
    Operation *op;
    size_t target;
    status_t status;

    if (reg == NULL || undone == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *undone = 0;
    target = reg->history_count / 2;
    while (*undone < target) {
        op = reg->history;
        status = apply_undo(reg, op, epsilon);
        if (status != STATUS_OK) {
            return status;
        }
        reg->history = op->next;
        free(op);
        reg->history_count--;
        (*undone)++;
    }
    if (target > 0) {
        clear_history(reg);
    }
    return STATUS_OK;
}

static int matches(const Liver *liver, const LiverFilter *filter, double epsilon)
{
    if (filter->use_surname && strcmp(liver->surname, filter->surname) != 0) {
        return 0;
    }
    if (filter->use_name && strcmp(liver->name, filter->name) != 0) {
        return 0;
    }
    if (filter->use_patronymic && strcmp(liver->patronymic, filter->patronymic) != 0) {
        return 0;
    }
    if (filter->use_birth && (liver->day != filter->day ||
                              liver->month != filter->month ||
                              liver->year != filter->year)) {
        return 0;
    }
    if (filter->use_gender && liver->gender != filter->gender) {
        return 0;
    }
    if (filter->use_income && !numbers_equal(liver->income, filter->income, epsilon)) {
        return 0;
    }
    return 1;
}

status_t registry_find(const Registry *reg, const LiverFilter *filter,
                       double epsilon, const Liver ***result, size_t *result_count)
{
    const Liver **found;
    const LiverNode *node;
    size_t count = 0;

    if (reg == NULL || filter == NULL || result == NULL || result_count == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *result = NULL;
    *result_count = 0;
    if (reg->count == 0) {
        return STATUS_OK;
    }
    found = (const Liver **)malloc(reg->count * sizeof(const Liver *));
    if (found == NULL) {
        return STATUS_NO_MEMORY;
    }
    for (node = reg->head; node != NULL; node = node->next) {
        if (matches(&node->liver, filter, epsilon)) {
            found[count] = &node->liver;
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

static status_t next_line(FILE *file, char *line, size_t size, int *got)
{
    size_t len;

    *got = 0;
    if (fgets(line, (int)size, file) == NULL) {
        if (ferror(file)) {
            return STATUS_READ_ERROR;
        }
        return STATUS_OK;
    }
    len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
        len--;
    } else if (!feof(file)) {
        return STATUS_INVALID_DATA;
    }
    if (len > 0 && line[len - 1] == '\r') {
        line[len - 1] = '\0';
    }
    *got = 1;
    return STATUS_OK;
}

static status_t parse_liver_line(const char *line, Liver *liver)
{
    char surname[NAME_SIZE];
    char name[NAME_SIZE];
    char patronymic[NAME_SIZE];
    char gender[4];
    int day;
    int month;
    int year;
    double income;
    int consumed = 0;
    int fields;

    patronymic[0] = '\0';
    fields = sscanf(line, "%63s %63s %63s %d %d %d %3s %lf %n", surname, name,
                    patronymic, &day, &month, &year, gender, &income, &consumed);
    if (fields != 8 || line[consumed] != '\0') {
        patronymic[0] = '\0';
        consumed = 0;
        fields = sscanf(line, "%63s %63s %d %d %d %3s %lf %n", surname, name, &day,
                        &month, &year, gender, &income, &consumed);
        if (fields != 7 || line[consumed] != '\0') {
            return STATUS_INVALID_DATA;
        }
    }
    if (strlen(gender) != 1) {
        return STATUS_INVALID_DATA;
    }
    return liver_create(liver, surname, name, patronymic, day, month, year,
                        gender[0], income, EPSILON);
}

status_t registry_load(Registry *reg, const char *path)
{
    FILE *file;
    char line[LINE_SIZE];
    int got = 1;
    Liver liver;
    status_t status = STATUS_OK;

    if (reg == NULL || path == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }
    while (status == STATUS_OK && got) {
        status = next_line(file, line, LINE_SIZE, &got);
        if (status == STATUS_OK && got && !is_blank(line)) {
            status = parse_liver_line(line, &liver);
            if (status == STATUS_OK) {
                status = insert_liver(reg, &liver);
            }
        }
    }
    fclose(file);
    if (status != STATUS_OK) {
        registry_destroy(reg);
    }
    return status;
}

status_t registry_save(const Registry *reg, const char *path)
{
    FILE *file;
    const LiverNode *node;
    int written;

    if (reg == NULL || path == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    file = fopen(path, "w");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }
    for (node = reg->head; node != NULL; node = node->next) {
        if (node->liver.patronymic[0] != '\0') {
            written = fprintf(file, "%s %s %s %02d %02d %04d %c %.2f\n",
                              node->liver.surname, node->liver.name,
                              node->liver.patronymic, node->liver.day,
                              node->liver.month, node->liver.year,
                              node->liver.gender, node->liver.income);
        } else {
            written = fprintf(file, "%s %s %02d %02d %04d %c %.2f\n",
                              node->liver.surname, node->liver.name,
                              node->liver.day, node->liver.month,
                              node->liver.year, node->liver.gender,
                              node->liver.income);
        }
        if (written < 0) {
            fclose(file);
            return STATUS_WRITE_ERROR;
        }
    }
    if (fclose(file) != 0) {
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
            printf("Ошибка: не удалось записать данные\n");
            break;
        case STATUS_INVALID_DATA:
            printf("Ошибка: некорректные данные\n");
            break;
        case STATUS_NOT_FOUND:
            printf("Ошибка: житель не найден\n");
            break;
    }
}

static void print_liver(const Liver *liver)
{
    printf("%s %s", liver->surname, liver->name);
    if (liver->patronymic[0] != '\0') {
        printf(" %s", liver->patronymic);
    }
    printf(", %02d.%02d.%04d, %c, доход %.2f\n", liver->day, liver->month,
           liver->year, liver->gender, liver->income);
}

static status_t read_line(const char *prompt, char *buffer, size_t size)
{
    int got;
    int ch;
    status_t status;

    printf("%s", prompt);
    status = next_line(stdin, buffer, size, &got);
    if (status == STATUS_INVALID_DATA) {
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
        return status;
    }
    if (status != STATUS_OK) {
        return status;
    }
    if (!got) {
        return STATUS_READ_ERROR;
    }
    return STATUS_OK;
}

static status_t parse_int(const char *text, int *value)
{
    char *end;
    long number = strtol(text, &end, 10);

    if (end == text || *end != '\0' || number < -1000000 || number > 1000000) {
        return STATUS_INVALID_DATA;
    }
    *value = (int)number;
    return STATUS_OK;
}

static status_t parse_double(const char *text, double *value)
{
    char *end;

    *value = strtod(text, &end);
    if (end == text || *end != '\0') {
        return STATUS_INVALID_DATA;
    }
    return STATUS_OK;
}

static status_t read_int(const char *prompt, int *value)
{
    char buffer[LINE_SIZE];
    status_t status = read_line(prompt, buffer, LINE_SIZE);

    if (status != STATUS_OK) {
        return status;
    }
    return parse_int(buffer, value);
}

static status_t read_double(const char *prompt, double *value)
{
    char buffer[LINE_SIZE];
    status_t status = read_line(prompt, buffer, LINE_SIZE);

    if (status != STATUS_OK) {
        return status;
    }
    return parse_double(buffer, value);
}

static status_t read_keep_text(const char *prompt, char *value)
{
    char buffer[LINE_SIZE];
    status_t status = read_line(prompt, buffer, LINE_SIZE);

    if (status != STATUS_OK) {
        return status;
    }
    if (buffer[0] != '\0') {
        strcpy(value, buffer);
    }
    return STATUS_OK;
}

static status_t read_keep_int(const char *prompt, int *value)
{
    char buffer[LINE_SIZE];
    status_t status = read_line(prompt, buffer, LINE_SIZE);

    if (status != STATUS_OK || buffer[0] == '\0') {
        return status;
    }
    return parse_int(buffer, value);
}

static status_t read_keep_double(const char *prompt, double *value)
{
    char buffer[LINE_SIZE];
    status_t status = read_line(prompt, buffer, LINE_SIZE);

    if (status != STATUS_OK || buffer[0] == '\0') {
        return status;
    }
    return parse_double(buffer, value);
}

static status_t read_index(const char *prompt, size_t *index)
{
    int number;
    status_t status = read_int(prompt, &number);

    if (status != STATUS_OK) {
        return status;
    }
    if (number < 1) {
        return STATUS_INVALID_DATA;
    }
    *index = (size_t)(number - 1);
    return STATUS_OK;
}

static status_t read_new_liver(Liver *liver)
{
    char surname[LINE_SIZE];
    char name[LINE_SIZE];
    char patronymic[LINE_SIZE];
    char gender[LINE_SIZE];
    int day = 0;
    int month = 0;
    int year = 0;
    double income = 0.0;
    status_t status;

    status = read_line("Фамилия: ", surname, LINE_SIZE);
    if (status == STATUS_OK) {
        status = read_line("Имя: ", name, LINE_SIZE);
    }
    if (status == STATUS_OK) {
        status = read_line("Отчество (можно пусто): ", patronymic, LINE_SIZE);
    }
    if (status == STATUS_OK) {
        status = read_int("День рождения: ", &day);
    }
    if (status == STATUS_OK) {
        status = read_int("Месяц рождения: ", &month);
    }
    if (status == STATUS_OK) {
        status = read_int("Год рождения: ", &year);
    }
    if (status == STATUS_OK) {
        status = read_line("Пол (M/W): ", gender, LINE_SIZE);
    }
    if (status == STATUS_OK && strlen(gender) != 1) {
        status = STATUS_INVALID_DATA;
    }
    if (status == STATUS_OK) {
        status = read_double("Средний доход за месяц: ", &income);
    }
    if (status != STATUS_OK) {
        return status;
    }
    return liver_create(liver, surname, name, patronymic, day, month, year,
                        gender[0], income, EPSILON);
}

static status_t read_filter_text(const char *prompt, int *use, char *value)
{
    char buffer[LINE_SIZE];
    status_t status = read_line(prompt, buffer, LINE_SIZE);

    if (status != STATUS_OK) {
        return status;
    }
    if (buffer[0] == '\0') {
        *use = 0;
        return STATUS_OK;
    }
    if (strlen(buffer) >= NAME_SIZE) {
        return STATUS_INVALID_DATA;
    }
    strcpy(value, buffer);
    *use = 1;
    return STATUS_OK;
}

static status_t read_filter(LiverFilter *filter)
{
    char buffer[LINE_SIZE];
    int consumed = 0;
    status_t status;

    memset(filter, 0, sizeof(LiverFilter));
    printf("Оставьте поле пустым, если оно не важно\n");
    status = read_filter_text("Фамилия: ", &filter->use_surname, filter->surname);
    if (status == STATUS_OK) {
        status = read_filter_text("Имя: ", &filter->use_name, filter->name);
    }
    if (status == STATUS_OK) {
        status = read_filter_text("Отчество: ", &filter->use_patronymic,
                                  filter->patronymic);
    }
    if (status == STATUS_OK) {
        status = read_line("Дата рождения (день месяц год): ", buffer, LINE_SIZE);
    }
    if (status == STATUS_OK && buffer[0] != '\0') {
        if (sscanf(buffer, "%d %d %d %n", &filter->day, &filter->month,
                   &filter->year, &consumed) != 3 || buffer[consumed] != '\0') {
            status = STATUS_INVALID_DATA;
        } else {
            filter->use_birth = 1;
        }
    }
    if (status == STATUS_OK) {
        status = read_line("Пол (M/W): ", buffer, LINE_SIZE);
    }
    if (status == STATUS_OK && buffer[0] != '\0') {
        if (strlen(buffer) != 1 || (buffer[0] != 'M' && buffer[0] != 'W')) {
            status = STATUS_INVALID_DATA;
        } else {
            filter->gender = buffer[0];
            filter->use_gender = 1;
        }
    }
    if (status == STATUS_OK) {
        status = read_line("Средний доход: ", buffer, LINE_SIZE);
    }
    if (status == STATUS_OK && buffer[0] != '\0') {
        status = parse_double(buffer, &filter->income);
        if (status == STATUS_OK) {
            filter->use_income = 1;
        }
    }
    return status;
}

static void dialog_show_all(const Registry *reg)
{
    const LiverNode *node;
    size_t number = 1;

    if (reg->count == 0) {
        printf("Список пуст\n");
        return;
    }
    for (node = reg->head; node != NULL; node = node->next) {
        printf("%zu. ", number);
        print_liver(&node->liver);
        number++;
    }
}

static void dialog_find(const Registry *reg)
{
    LiverFilter filter;
    const Liver **found = NULL;
    size_t count = 0;
    size_t i;
    status_t status;

    status = read_filter(&filter);
    if (status == STATUS_OK) {
        status = registry_find(reg, &filter, EPSILON, &found, &count);
    }
    if (status != STATUS_OK) {
        print_error(status);
        return;
    }
    if (count == 0) {
        printf("Ничего не найдено\n");
    }
    for (i = 0; i < count; i++) {
        print_liver(found[i]);
    }
    free(found);
}

static void dialog_add(Registry *reg)
{
    Liver liver;
    status_t status;

    status = read_new_liver(&liver);
    if (status == STATUS_OK) {
        status = registry_add(reg, &liver);
    }
    if (status == STATUS_OK) {
        printf("Житель добавлен\n");
    } else {
        print_error(status);
    }
}

static void dialog_remove(Registry *reg)
{
    size_t index;
    status_t status;

    dialog_show_all(reg);
    status = read_index("Номер жителя для удаления: ", &index);
    if (status == STATUS_OK) {
        status = registry_remove(reg, index);
    }
    if (status == STATUS_OK) {
        printf("Житель удалён\n");
    } else {
        print_error(status);
    }
}

static void dialog_modify(Registry *reg)
{
    Liver current;
    Liver updated;
    char surname[LINE_SIZE];
    char name[LINE_SIZE];
    char patronymic[LINE_SIZE];
    char gender[LINE_SIZE];
    int day;
    int month;
    int year;
    double income;
    size_t index;
    status_t status;

    dialog_show_all(reg);
    status = read_index("Номер жителя для изменения: ", &index);
    if (status == STATUS_OK) {
        status = registry_get(reg, index, &current);
    }
    if (status != STATUS_OK) {
        print_error(status);
        return;
    }

    strcpy(surname, current.surname);
    strcpy(name, current.name);
    strcpy(patronymic, current.patronymic);
    gender[0] = current.gender;
    gender[1] = '\0';
    day = current.day;
    month = current.month;
    year = current.year;
    income = current.income;

    printf("Enter — оставить прежнее значение, для отчества \"-\" — очистить\n");
    status = read_keep_text("Фамилия: ", surname);
    if (status == STATUS_OK) {
        status = read_keep_text("Имя: ", name);
    }
    if (status == STATUS_OK) {
        status = read_keep_text("Отчество: ", patronymic);
    }
    if (status == STATUS_OK) {
        status = read_keep_int("День рождения: ", &day);
    }
    if (status == STATUS_OK) {
        status = read_keep_int("Месяц рождения: ", &month);
    }
    if (status == STATUS_OK) {
        status = read_keep_int("Год рождения: ", &year);
    }
    if (status == STATUS_OK) {
        status = read_keep_text("Пол (M/W): ", gender);
    }
    if (status == STATUS_OK) {
        status = read_keep_double("Средний доход: ", &income);
    }
    if (status == STATUS_OK && strcmp(patronymic, "-") == 0) {
        patronymic[0] = '\0';
    }
    if (status == STATUS_OK && strlen(gender) != 1) {
        status = STATUS_INVALID_DATA;
    }
    if (status == STATUS_OK) {
        status = liver_create(&updated, surname, name, patronymic, day, month,
                              year, gender[0], income, EPSILON);
    }
    if (status == STATUS_OK) {
        status = registry_modify(reg, index, &updated);
    }
    if (status == STATUS_OK) {
        printf("Данные жителя изменены\n");
    } else {
        print_error(status);
    }
}

static void dialog_undo(Registry *reg)
{
    size_t undone;
    status_t status = registry_undo(reg, EPSILON, &undone);

    if (status == STATUS_OK) {
        printf("Отменено модификаций: %zu\n", undone);
    } else {
        print_error(status);
    }
}

static void dialog_save(const Registry *reg)
{
    char path[LINE_SIZE];
    status_t status;

    status = read_line("Путь к файлу: ", path, LINE_SIZE);
    if (status == STATUS_OK && path[0] == '\0') {
        status = STATUS_INVALID_DATA;
    }
    if (status == STATUS_OK) {
        status = registry_save(reg, path);
    }
    if (status == STATUS_OK) {
        printf("Данные сохранены\n");
    } else {
        print_error(status);
    }
}

static void print_menu(void)
{
    printf("\n1. Показать всех жителей\n");
    printf("2. Найти жителя по параметрам\n");
    printf("3. Добавить жителя\n");
    printf("4. Изменить жителя\n");
    printf("5. Удалить жителя\n");
    printf("6. Отменить последние N/2 модификаций\n");
    printf("7. Выгрузить данные в файл\n");
    printf("0. Выход\n");
}

static void run_menu(Registry *reg)
{
    int choice = -1;
    status_t status;

    while (choice != 0) {
        print_menu();
        status = read_int("Выбор: ", &choice);
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
                dialog_show_all(reg);
                break;
            case 2:
                dialog_find(reg);
                break;
            case 3:
                dialog_add(reg);
                break;
            case 4:
                dialog_modify(reg);
                break;
            case 5:
                dialog_remove(reg);
                break;
            case 6:
                dialog_undo(reg);
                break;
            case 7:
                dialog_save(reg);
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
    Registry reg;
    status_t status;

    if (argc != 2) {
        printf("Использование: %s <файл с жителями>\n", argv[0]);
        return 1;
    }

    registry_init(&reg);
    status = registry_load(&reg, argv[1]);
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }

    run_menu(&reg);
    registry_destroy(&reg);

    return 0;
}