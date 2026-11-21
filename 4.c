#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#define EPSILON 1e-9
#define LINE_SIZE 256

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_INVALID_DATA,
    STATUS_NOT_FOUND,
    STATUS_DUPLICATE,
    STATUS_READ_ERROR,
    STATUS_TIME_ERROR
} status_t;

typedef struct {
    char *data;
    int length;
} String;

typedef struct {
    String city;
    String street;
    int house;
    String building;
    int apartment;
    String index;
} Address;

typedef struct {
    Address address;
    double weight;
    String id;
    String created;
    String delivery_time;
    long long created_key;
    long long delivery_key;
    int delivered;
} Mail;

typedef struct {
    Address *address;
    Mail *mails;
    size_t count;
    size_t capacity;
} Post;

typedef enum {
    SELECT_DELIVERED,
    SELECT_EXPIRED
} select_mode_t;

status_t string_create(String *str, const char *source)
{
    size_t len;
    char *data;

    if (str == NULL || source == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    len = strlen(source);
    if (len >= INT_MAX) {
        return STATUS_INVALID_ARGUMENT;
    }
    data = (char *)malloc(len + 1);
    if (data == NULL) {
        return STATUS_NO_MEMORY;
    }
    memcpy(data, source, len + 1);
    str->data = data;
    str->length = (int)len;
    return STATUS_OK;
}

void string_destroy(String *str)
{
    if (str == NULL) {
        return;
    }
    free(str->data);
    str->data = NULL;
    str->length = 0;
}

status_t string_compare(const String *first, const String *second, int *result)
{
    int cmp;

    if (first == NULL || second == NULL || result == NULL ||
        first->data == NULL || second->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (first->length != second->length) {
        *result = first->length < second->length ? -1 : 1;
        return STATUS_OK;
    }
    cmp = strcmp(first->data, second->data);
    *result = (cmp > 0) - (cmp < 0);
    return STATUS_OK;
}

status_t string_equal(const String *first, const String *second, int *equal)
{
    if (first == NULL || second == NULL || equal == NULL ||
        first->data == NULL || second->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *equal = strcmp(first->data, second->data) == 0;
    return STATUS_OK;
}

status_t string_copy(String *destination, const String *source)
{
    char *data;

    if (destination == NULL || source == NULL || source->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    data = (char *)malloc((size_t)source->length + 1);
    if (data == NULL) {
        return STATUS_NO_MEMORY;
    }
    memcpy(data, source->data, (size_t)source->length + 1);
    free(destination->data);
    destination->data = data;
    destination->length = source->length;
    return STATUS_OK;
}

status_t string_clone(const String *source, String **clone)
{
    String *result;
    status_t status;

    if (source == NULL || clone == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    result = (String *)malloc(sizeof(String));
    if (result == NULL) {
        return STATUS_NO_MEMORY;
    }
    result->data = NULL;
    result->length = 0;
    status = string_copy(result, source);
    if (status != STATUS_OK) {
        free(result);
        return status;
    }
    *clone = result;
    return STATUS_OK;
}

status_t string_concat(String *first, const String *second)
{
    char *data;
    int first_length;
    int second_length;

    if (first == NULL || second == NULL || first->data == NULL ||
        second->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    first_length = first->length;
    second_length = second->length;
    if (second_length > INT_MAX - 1 - first_length) {
        return STATUS_INVALID_ARGUMENT;
    }
    data = (char *)realloc(first->data, (size_t)first_length + second_length + 1);
    if (data == NULL) {
        return STATUS_NO_MEMORY;
    }
    first->data = data;
    memcpy(first->data + first_length, second->data, (size_t)second_length + 1);
    first->length = first_length + second_length;
    return STATUS_OK;
}

static void string_init(String *str)
{
    str->data = NULL;
    str->length = 0;
}

static int is_digits(const char *text, size_t count)
{
    size_t i;

    if (strlen(text) != count) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }
    return 1;
}

static int two_digits(const char *text)
{
    return (text[0] - '0') * 10 + (text[1] - '0');
}

static int days_in_month(int year, int month)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        return 29;
    }
    return days[month - 1];
}

static long long make_key(int year, int month, int day, int hour, int minute,
                          int second)
{
    long long key = year;

    key = key * 100 + month;
    key = key * 100 + day;
    key = key * 100 + hour;
    key = key * 100 + minute;
    key = key * 100 + second;
    return key;
}

status_t parse_datetime(const char *text, long long *key)
{
    static const int digit_positions[14] = {0, 1, 3, 4, 6, 7, 8, 9, 11, 12, 14, 15, 17, 18};
    int i;
    int day;
    int month;
    int year;
    int hour;
    int minute;
    int second;

    if (text == NULL || key == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (strlen(text) != 19) {
        return STATUS_INVALID_DATA;
    }
    if (text[2] != ':' || text[5] != ':' || text[10] != ' ' ||
        text[13] != ':' || text[16] != ':') {
        return STATUS_INVALID_DATA;
    }
    for (i = 0; i < 14; i++) {
        if (!isdigit((unsigned char)text[digit_positions[i]])) {
            return STATUS_INVALID_DATA;
        }
    }

    day = two_digits(text);
    month = two_digits(text + 3);
    year = two_digits(text + 6) * 100 + two_digits(text + 8);
    hour = two_digits(text + 11);
    minute = two_digits(text + 14);
    second = two_digits(text + 17);

    if (year < 1 || month < 1 || month > 12 || day < 1 ||
        day > days_in_month(year, month) || hour > 23 || minute > 59 ||
        second > 59) {
        return STATUS_INVALID_DATA;
    }

    *key = make_key(year, month, day, hour, minute, second);
    return STATUS_OK;
}

status_t get_current_key(long long *key)
{
    time_t now;
    const struct tm *local;

    if (key == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return STATUS_TIME_ERROR;
    }
    local = localtime(&now);
    if (local == NULL) {
        return STATUS_TIME_ERROR;
    }
    *key = make_key(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                    local->tm_hour, local->tm_min, local->tm_sec);
    return STATUS_OK;
}

void address_destroy(Address *address)
{
    if (address == NULL) {
        return;
    }
    string_destroy(&address->city);
    string_destroy(&address->street);
    string_destroy(&address->building);
    string_destroy(&address->index);
}

static void address_init(Address *address)
{
    string_init(&address->city);
    string_init(&address->street);
    string_init(&address->building);
    string_init(&address->index);
    address->house = 0;
    address->apartment = 0;
}

status_t address_create(Address *address, const char *city, const char *street,
                        int house, const char *building, int apartment,
                        const char *index)
{
    status_t status;

    if (address == NULL || city == NULL || street == NULL || building == NULL ||
        index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (city[0] == '\0' || street[0] == '\0' || house <= 0 || apartment <= 0 ||
        !is_digits(index, 6)) {
        return STATUS_INVALID_DATA;
    }

    address_init(address);
    status = string_create(&address->city, city);
    if (status == STATUS_OK) {
        status = string_create(&address->street, street);
    }
    if (status == STATUS_OK) {
        status = string_create(&address->building, building);
    }
    if (status == STATUS_OK) {
        status = string_create(&address->index, index);
    }
    if (status != STATUS_OK) {
        address_destroy(address);
        return status;
    }
    address->house = house;
    address->apartment = apartment;
    return STATUS_OK;
}

status_t address_copy(Address *destination, const Address *source)
{
    status_t status;

    if (destination == NULL || source == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    address_init(destination);
    status = string_copy(&destination->city, &source->city);
    if (status == STATUS_OK) {
        status = string_copy(&destination->street, &source->street);
    }
    if (status == STATUS_OK) {
        status = string_copy(&destination->building, &source->building);
    }
    if (status == STATUS_OK) {
        status = string_copy(&destination->index, &source->index);
    }
    if (status != STATUS_OK) {
        address_destroy(destination);
        return status;
    }
    destination->house = source->house;
    destination->apartment = source->apartment;
    return STATUS_OK;
}

void mail_destroy(Mail *mail)
{
    if (mail == NULL) {
        return;
    }
    address_destroy(&mail->address);
    string_destroy(&mail->id);
    string_destroy(&mail->created);
    string_destroy(&mail->delivery_time);
}

status_t mail_create(Mail *mail, const Address *address, double weight,
                     const char *id, const char *created,
                     const char *delivery_time, double epsilon)
{
    long long created_key;
    long long delivery_key;
    status_t status;

    if (mail == NULL || address == NULL || id == NULL || created == NULL ||
        delivery_time == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (weight < -epsilon || !is_digits(id, 14)) {
        return STATUS_INVALID_DATA;
    }
    status = parse_datetime(created, &created_key);
    if (status != STATUS_OK) {
        return status;
    }
    status = parse_datetime(delivery_time, &delivery_key);
    if (status != STATUS_OK) {
        return status;
    }
    if (delivery_key < created_key) {
        return STATUS_INVALID_DATA;
    }

    string_init(&mail->id);
    string_init(&mail->created);
    string_init(&mail->delivery_time);
    status = address_copy(&mail->address, address);
    if (status != STATUS_OK) {
        return status;
    }
    status = string_create(&mail->id, id);
    if (status == STATUS_OK) {
        status = string_create(&mail->created, created);
    }
    if (status == STATUS_OK) {
        status = string_create(&mail->delivery_time, delivery_time);
    }
    if (status != STATUS_OK) {
        mail_destroy(mail);
        return status;
    }
    mail->weight = weight;
    mail->created_key = created_key;
    mail->delivery_key = delivery_key;
    mail->delivered = 0;
    return STATUS_OK;
}

static int mail_compare(const Mail *first, const Mail *second)
{
    int result = strcmp(first->address.index.data, second->address.index.data);

    if (result != 0) {
        return result;
    }
    return strcmp(first->id.data, second->id.data);
}

static int compare_by_created(const void *first, const void *second)
{
    const Mail *a = *(const Mail *const *)first;
    const Mail *b = *(const Mail *const *)second;

    if (a->created_key != b->created_key) {
        return (a->created_key > b->created_key) - (a->created_key < b->created_key);
    }
    return strcmp(a->id.data, b->id.data);
}

status_t post_create(Post *post, const Address *address)
{
    status_t status;

    if (post == NULL || address == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    post->address = (Address *)malloc(sizeof(Address));
    if (post->address == NULL) {
        return STATUS_NO_MEMORY;
    }
    status = address_copy(post->address, address);
    if (status != STATUS_OK) {
        free(post->address);
        post->address = NULL;
        return status;
    }
    post->mails = NULL;
    post->count = 0;
    post->capacity = 0;
    return STATUS_OK;
}

void post_destroy(Post *post)
{
    size_t i;

    if (post == NULL) {
        return;
    }
    for (i = 0; i < post->count; i++) {
        mail_destroy(&post->mails[i]);
    }
    free(post->mails);
    post->mails = NULL;
    post->count = 0;
    post->capacity = 0;
    if (post->address != NULL) {
        address_destroy(post->address);
        free(post->address);
        post->address = NULL;
    }
}

status_t post_find(const Post *post, const char *id, size_t *position)
{
    size_t i;

    if (post == NULL || id == NULL || position == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (i = 0; i < post->count; i++) {
        if (strcmp(post->mails[i].id.data, id) == 0) {
            *position = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

status_t post_add(Post *post, Mail *mail)
{
    Mail *new_mails;
    size_t new_capacity;
    size_t position;
    size_t found;

    if (post == NULL || mail == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (post_find(post, mail->id.data, &found) == STATUS_OK) {
        return STATUS_DUPLICATE;
    }

    if (post->count == post->capacity) {
        new_capacity = post->capacity == 0 ? 4 : post->capacity * 2;
        new_mails = (Mail *)realloc(post->mails, new_capacity * sizeof(Mail));
        if (new_mails == NULL) {
            return STATUS_NO_MEMORY;
        }
        post->mails = new_mails;
        post->capacity = new_capacity;
    }

    position = 0;
    while (position < post->count &&
           mail_compare(&post->mails[position], mail) < 0) {
        position++;
    }
    memmove(&post->mails[position + 1], &post->mails[position],
            (post->count - position) * sizeof(Mail));
    post->mails[position] = *mail;
    post->count++;
    memset(mail, 0, sizeof(Mail));
    return STATUS_OK;
}

status_t post_remove(Post *post, const char *id)
{
    size_t position;
    status_t status;

    status = post_find(post, id, &position);
    if (status != STATUS_OK) {
        return status;
    }
    mail_destroy(&post->mails[position]);
    memmove(&post->mails[position], &post->mails[position + 1],
            (post->count - position - 1) * sizeof(Mail));
    post->count--;
    return STATUS_OK;
}

status_t post_mark_delivered(Post *post, const char *id)
{
    size_t position;
    status_t status;

    status = post_find(post, id, &position);
    if (status != STATUS_OK) {
        return status;
    }
    post->mails[position].delivered = 1;
    return STATUS_OK;
}

status_t post_select(const Post *post, select_mode_t mode, long long now_key,
                     const Mail ***result, size_t *result_count)
{
    const Mail **list;
    const Mail *mail;
    size_t count = 0;
    size_t i;
    int suitable;

    if (post == NULL || result == NULL || result_count == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *result = NULL;
    *result_count = 0;
    if (post->count == 0) {
        return STATUS_OK;
    }

    list = (const Mail **)malloc(post->count * sizeof(const Mail *));
    if (list == NULL) {
        return STATUS_NO_MEMORY;
    }

    for (i = 0; i < post->count; i++) {
        mail = &post->mails[i];
        if (mode == SELECT_DELIVERED) {
            suitable = mail->delivered;
        } else {
            suitable = !mail->delivered && mail->delivery_key < now_key;
        }
        if (suitable) {
            list[count] = mail;
            count++;
        }
    }

    if (count == 0) {
        free(list);
        return STATUS_OK;
    }
    qsort(list, count, sizeof(const Mail *), compare_by_created);
    *result = list;
    *result_count = count;
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
        case STATUS_INVALID_DATA:
            printf("Ошибка: введены некорректные данные\n");
            break;
        case STATUS_NOT_FOUND:
            printf("Ошибка: отправление не найдено\n");
            break;
        case STATUS_DUPLICATE:
            printf("Ошибка: отправление с таким идентификатором уже есть\n");
            break;
        case STATUS_READ_ERROR:
            printf("Ошибка: не удалось прочитать ввод\n");
            break;
        case STATUS_TIME_ERROR:
            printf("Ошибка: не удалось получить текущее время\n");
            break;
    }
}

static void print_mail(const Mail *mail)
{
    printf("Идентификатор: %s\n", mail->id.data);
    printf("Адрес: %s, г. %s, ул. %s, д. %d", mail->address.index.data,
           mail->address.city.data, mail->address.street.data,
           mail->address.house);
    if (mail->address.building.length > 0) {
        printf(", корп. %s", mail->address.building.data);
    }
    printf(", кв. %d\n", mail->address.apartment);
    printf("Вес: %.2f\n", mail->weight);
    printf("Создано: %s\n", mail->created.data);
    printf("Срок вручения: %s\n", mail->delivery_time.data);
    printf("Статус: %s\n", mail->delivered ? "вручено" : "не вручено");
    printf("-----\n");
}

static void print_mail_list(const Mail *const *list, size_t count)
{
    size_t i;

    if (count == 0) {
        printf("Ничего не найдено\n");
        return;
    }
    for (i = 0; i < count; i++) {
        print_mail(list[i]);
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

static status_t read_int(const char *prompt, int *value)
{
    char buffer[LINE_SIZE];
    char *end;
    long number;
    status_t status;

    status = read_line(prompt, buffer, LINE_SIZE);
    if (status != STATUS_OK) {
        return status;
    }
    number = strtol(buffer, &end, 10);
    if (end == buffer || *end != '\0' || number <= INT_MIN || number >= INT_MAX) {
        return STATUS_INVALID_DATA;
    }
    *value = (int)number;
    return STATUS_OK;
}

static status_t read_double(const char *prompt, double *value)
{
    char buffer[LINE_SIZE];
    char *end;
    status_t status;

    status = read_line(prompt, buffer, LINE_SIZE);
    if (status != STATUS_OK) {
        return status;
    }
    *value = strtod(buffer, &end);
    if (end == buffer || *end != '\0') {
        return STATUS_INVALID_DATA;
    }
    return STATUS_OK;
}

static status_t read_address(Address *address)
{
    char city[LINE_SIZE];
    char street[LINE_SIZE];
    char building[LINE_SIZE];
    char index[LINE_SIZE];
    int house = 0;
    int apartment = 0;
    status_t status;

    status = read_line("Город: ", city, LINE_SIZE);
    if (status == STATUS_OK) {
        status = read_line("Улица: ", street, LINE_SIZE);
    }
    if (status == STATUS_OK) {
        status = read_int("Номер дома: ", &house);
    }
    if (status == STATUS_OK) {
        status = read_line("Корпус (можно оставить пустым): ", building, LINE_SIZE);
    }
    if (status == STATUS_OK) {
        status = read_int("Номер квартиры: ", &apartment);
    }
    if (status == STATUS_OK) {
        status = read_line("Индекс (6 цифр): ", index, LINE_SIZE);
    }
    if (status != STATUS_OK) {
        return status;
    }
    return address_create(address, city, street, house, building, apartment,
                          index);
}

static void dialog_add(Post *post)
{
    char id[LINE_SIZE];
    char created[LINE_SIZE];
    char delivery_time[LINE_SIZE];
    double weight = 0.0;
    Address address;
    Mail mail;
    status_t status;

    status = read_address(&address);
    if (status != STATUS_OK) {
        print_error(status);
        return;
    }

    status = read_double("Вес посылки: ", &weight);
    if (status == STATUS_OK) {
        status = read_line("Идентификатор (14 цифр): ", id, LINE_SIZE);
    }
    if (status == STATUS_OK) {
        status = read_line("Время создания (dd:MM:yyyy hh:mm:ss): ", created, LINE_SIZE);
    }
    if (status == STATUS_OK) {
        status = read_line("Срок вручения (dd:MM:yyyy hh:mm:ss): ", delivery_time, LINE_SIZE);
    }
    if (status == STATUS_OK) {
        status = mail_create(&mail, &address, weight, id, created,
                             delivery_time, EPSILON);
    }
    address_destroy(&address);
    if (status != STATUS_OK) {
        print_error(status);
        return;
    }

    status = post_add(post, &mail);
    mail_destroy(&mail);
    if (status == STATUS_OK) {
        printf("Отправление добавлено\n");
    } else {
        print_error(status);
    }
}

static void dialog_remove(Post *post)
{
    char id[LINE_SIZE];
    status_t status;

    status = read_line("Идентификатор: ", id, LINE_SIZE);
    if (status == STATUS_OK) {
        status = post_remove(post, id);
    }
    if (status == STATUS_OK) {
        printf("Отправление удалено\n");
    } else {
        print_error(status);
    }
}

static void dialog_find(const Post *post)
{
    char id[LINE_SIZE];
    size_t position;
    status_t status;

    status = read_line("Идентификатор: ", id, LINE_SIZE);
    if (status == STATUS_OK) {
        status = post_find(post, id, &position);
    }
    if (status == STATUS_OK) {
        print_mail(&post->mails[position]);
    } else {
        print_error(status);
    }
}

static void dialog_mark(Post *post)
{
    char id[LINE_SIZE];
    status_t status;

    status = read_line("Идентификатор: ", id, LINE_SIZE);
    if (status == STATUS_OK) {
        status = post_mark_delivered(post, id);
    }
    if (status == STATUS_OK) {
        printf("Отправление отмечено как врученное\n");
    } else {
        print_error(status);
    }
}

static void dialog_list(const Post *post, select_mode_t mode)
{
    const Mail **list = NULL;
    size_t count = 0;
    long long now_key = 0;
    status_t status;

    status = get_current_key(&now_key);
    if (status == STATUS_OK) {
        status = post_select(post, mode, now_key, &list, &count);
    }
    if (status != STATUS_OK) {
        print_error(status);
        return;
    }
    print_mail_list(list, count);
    free(list);
}

static void dialog_show_all(const Post *post)
{
    size_t i;

    if (post->count == 0) {
        printf("Отправлений нет\n");
        return;
    }
    for (i = 0; i < post->count; i++) {
        print_mail(&post->mails[i]);
    }
}

static void print_menu(void)
{
    printf("\n1. Добавить отправление\n");
    printf("2. Удалить отправление\n");
    printf("3. Найти отправление по идентификатору\n");
    printf("4. Отметить отправление врученным\n");
    printf("5. Показать все врученные\n");
    printf("6. Показать все с истёкшим сроком\n");
    printf("7. Показать все отправления\n");
    printf("0. Выход\n");
}

static void run_menu(Post *post)
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
                dialog_add(post);
                break;
            case 2:
                dialog_remove(post);
                break;
            case 3:
                dialog_find(post);
                break;
            case 4:
                dialog_mark(post);
                break;
            case 5:
                dialog_list(post, SELECT_DELIVERED);
                break;
            case 6:
                dialog_list(post, SELECT_EXPIRED);
                break;
            case 7:
                dialog_show_all(post);
                break;
            case 0:
                break;
            default:
                printf("Неизвестный пункт меню\n");
                break;
        }
    }
}

static void demo_strings(void)
{
    String first = {NULL, 0};
    String second = {NULL, 0};
    String target = {NULL, 0};
    String *clone = NULL;
    int order = 0;
    int equal = 0;
    status_t status;

    status = string_create(&first, "abc");
    if (status == STATUS_OK) {
        status = string_create(&second, "abd");
    }
    if (status == STATUS_OK) {
        status = string_create(&target, "temp");
    }
    if (status == STATUS_OK) {
        status = string_compare(&first, &second, &order);
    }
    if (status == STATUS_OK) {
        status = string_equal(&first, &second, &equal);
    }
    if (status == STATUS_OK) {
        printf("Сравнение \"%s\" и \"%s\": порядок %d, равны %d\n",
               first.data, second.data, order, equal);
        status = string_copy(&target, &first);
    }
    if (status == STATUS_OK) {
        printf("После копирования: %s\n", target.data);
        status = string_concat(&target, &second);
    }
    if (status == STATUS_OK) {
        printf("После конкатенации: %s (длина %d)\n", target.data, target.length);
        status = string_clone(&target, &clone);
    }
    if (status == STATUS_OK) {
        printf("Копия в динамической памяти: %s\n", clone->data);
    } else {
        print_error(status);
    }

    string_destroy(&first);
    string_destroy(&second);
    string_destroy(&target);
    if (clone != NULL) {
        string_destroy(clone);
        free(clone);
    }
}

int main(void)
{
    Address address;
    Post post;
    status_t status;

    demo_strings();

    printf("\nВведите адрес почтового отделения\n");
    status = read_address(&address);
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }
    status = post_create(&post, &address);
    address_destroy(&address);
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }

    run_menu(&post);
    post_destroy(&post);

    return 0;
}