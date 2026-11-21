#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define NAME_SIZE 64
#define TOKEN_SIZE 16
#define LINE_SIZE 512
#define EPSILON 1e-9
#define FIRST_YEAR 1970

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_OPEN_ERROR,
    STATUS_READ_ERROR,
    STATUS_INVALID_DATA,
    STATUS_EMPTY
} status_t;

typedef struct {
    char *vehicle;
    double x;
    double y;
    long long arrival;
    long long departure;
    char marker;
} Record;

typedef struct {
    Record *items;
    size_t count;
    size_t capacity;
} RecordArray;

typedef struct StopNode {
    double x;
    double y;
    long long arrival;
    long long departure;
    char marker;
    struct StopNode *next;
} StopNode;

typedef struct RouteNode {
    char *vehicle;
    StopNode *stops;
    struct RouteNode *next;
} RouteNode;

typedef struct {
    const char *vehicle;
    double value;
} Answer;

typedef double (*metric_t)(const RouteNode *head, const RouteNode *route);

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

static long long leap_years_before(int year)
{
    int n = year - 1;

    return n / 4 - n / 100 + n / 400;
}

status_t parse_datetime(const char *date, const char *time_text, long long *seconds)
{
    int day;
    int month;
    int year;
    int hour;
    int minute;
    int second;
    int m;
    int i;
    long long days;

    if (date == NULL || time_text == NULL || seconds == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (strlen(date) != 10 || strlen(time_text) != 8) {
        return STATUS_INVALID_DATA;
    }
    if (date[2] != '.' || date[5] != '.' || time_text[2] != ':' ||
        time_text[5] != ':') {
        return STATUS_INVALID_DATA;
    }
    for (i = 0; i < 10; i++) {
        if (i != 2 && i != 5 && !isdigit((unsigned char)date[i])) {
            return STATUS_INVALID_DATA;
        }
    }
    for (i = 0; i < 8; i++) {
        if (i != 2 && i != 5 && !isdigit((unsigned char)time_text[i])) {
            return STATUS_INVALID_DATA;
        }
    }

    day = two_digits(date);
    month = two_digits(date + 3);
    year = two_digits(date + 6) * 100 + two_digits(date + 8);
    hour = two_digits(time_text);
    minute = two_digits(time_text + 3);
    second = two_digits(time_text + 6);

    if (year < FIRST_YEAR || month < 1 || month > 12 || day < 1 ||
        day > days_in_month(year, month) || hour > 23 || minute > 59 ||
        second > 59) {
        return STATUS_INVALID_DATA;
    }

    days = (long long)(year - FIRST_YEAR) * 365 + leap_years_before(year) -
           leap_years_before(FIRST_YEAR);
    for (m = 1; m < month; m++) {
        days += days_in_month(year, m);
    }
    days += day - 1;

    *seconds = days * 86400 + hour * 3600 + minute * 60 + second;
    return STATUS_OK;
}

void free_records(RecordArray *records)
{
    size_t i;

    if (records == NULL) {
        return;
    }
    for (i = 0; i < records->count; i++) {
        free(records->items[i].vehicle);
    }
    free(records->items);
    records->items = NULL;
    records->count = 0;
    records->capacity = 0;
}

status_t add_record(RecordArray *records, const char *vehicle, double x, double y,
                    long long arrival, long long departure, char marker)
{
    Record *new_items;
    size_t new_capacity;
    char *name;
    size_t len;

    if (records == NULL || vehicle == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (records->count == records->capacity) {
        new_capacity = records->capacity == 0 ? 16 : records->capacity * 2;
        new_items = (Record *)realloc(records->items, new_capacity * sizeof(Record));
        if (new_items == NULL) {
            return STATUS_NO_MEMORY;
        }
        records->items = new_items;
        records->capacity = new_capacity;
    }
    len = strlen(vehicle);
    name = (char *)malloc(len + 1);
    if (name == NULL) {
        return STATUS_NO_MEMORY;
    }
    memcpy(name, vehicle, len + 1);

    records->items[records->count].vehicle = name;
    records->items[records->count].x = x;
    records->items[records->count].y = y;
    records->items[records->count].arrival = arrival;
    records->items[records->count].departure = departure;
    records->items[records->count].marker = marker;
    records->count++;
    return STATUS_OK;
}

static int compare_records(const void *first, const void *second)
{
    const Record *a = (const Record *)first;
    const Record *b = (const Record *)second;
    int result = strcmp(a->vehicle, b->vehicle);

    if (result != 0) {
        return result;
    }
    return (a->arrival > b->arrival) - (a->arrival < b->arrival);
}

status_t sort_records(RecordArray *records)
{
    if (records == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (records->count > 1) {
        qsort(records->items, records->count, sizeof(Record), compare_records);
    }
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

static status_t parse_coordinates(const char *line, double *x, double *y)
{
    int consumed = 0;

    if (sscanf(line, "%lf %lf %n", x, y, &consumed) != 2 || line[consumed] != '\0') {
        return STATUS_INVALID_DATA;
    }
    return STATUS_OK;
}

static status_t parse_record_line(const char *line, double x, double y,
                                  RecordArray *records)
{
    char vehicle[NAME_SIZE];
    char arrival_date[TOKEN_SIZE];
    char arrival_time[TOKEN_SIZE];
    char departure_date[TOKEN_SIZE];
    char departure_time[TOKEN_SIZE];
    char marker[2];
    long long arrival;
    long long departure;
    int consumed = 0;
    status_t status;

    if (sscanf(line, "%63s %15s %15s %15s %15s %1s %n", vehicle, arrival_date,
               arrival_time, departure_date, departure_time, marker,
               &consumed) != 6 || line[consumed] != '\0') {
        return STATUS_INVALID_DATA;
    }
    if (marker[0] != 'S' && marker[0] != 'I' && marker[0] != 'E') {
        return STATUS_INVALID_DATA;
    }
    status = parse_datetime(arrival_date, arrival_time, &arrival);
    if (status != STATUS_OK) {
        return status;
    }
    status = parse_datetime(departure_date, departure_time, &departure);
    if (status != STATUS_OK) {
        return status;
    }
    if (departure < arrival) {
        return STATUS_INVALID_DATA;
    }
    return add_record(records, vehicle, x, y, arrival, departure, marker[0]);
}

status_t load_file(const char *path, RecordArray *records)
{
    FILE *file;
    char line[LINE_SIZE];
    int got = 1;
    int have_coordinates = 0;
    double x = 0.0;
    double y = 0.0;
    status_t status = STATUS_OK;

    if (path == NULL || records == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }

    while (status == STATUS_OK && got) {
        status = next_line(file, line, LINE_SIZE, &got);
        if (status == STATUS_OK && got && !is_blank(line)) {
            if (!have_coordinates) {
                status = parse_coordinates(line, &x, &y);
                if (status == STATUS_OK) {
                    have_coordinates = 1;
                }
            } else {
                status = parse_record_line(line, x, y, records);
            }
        }
    }
    if (status == STATUS_OK && !have_coordinates) {
        status = STATUS_INVALID_DATA;
    }

    fclose(file);
    return status;
}

void destroy_routes(RouteNode *head)
{
    RouteNode *route;
    StopNode *stop;
    StopNode *next_stop;

    while (head != NULL) {
        route = head;
        head = head->next;
        stop = route->stops;
        while (stop != NULL) {
            next_stop = stop->next;
            free(stop);
            stop = next_stop;
        }
        free(route->vehicle);
        free(route);
    }
}

static status_t create_route(const char *vehicle, RouteNode **route)
{
    RouteNode *node = (RouteNode *)malloc(sizeof(RouteNode));
    size_t len = strlen(vehicle);

    if (node == NULL) {
        return STATUS_NO_MEMORY;
    }
    node->vehicle = (char *)malloc(len + 1);
    if (node->vehicle == NULL) {
        free(node);
        return STATUS_NO_MEMORY;
    }
    memcpy(node->vehicle, vehicle, len + 1);
    node->stops = NULL;
    node->next = NULL;
    *route = node;
    return STATUS_OK;
}

static status_t create_stop(const Record *record, StopNode **stop)
{
    StopNode *node = (StopNode *)malloc(sizeof(StopNode));

    if (node == NULL) {
        return STATUS_NO_MEMORY;
    }
    node->x = record->x;
    node->y = record->y;
    node->arrival = record->arrival;
    node->departure = record->departure;
    node->marker = record->marker;
    node->next = NULL;
    *stop = node;
    return STATUS_OK;
}

status_t build_routes(const RecordArray *records, RouteNode **routes)
{
    RouteNode *head = NULL;
    RouteNode *tail = NULL;
    RouteNode *current = NULL;
    RouteNode *route;
    StopNode *last = NULL;
    StopNode *stop;
    const Record *record;
    size_t i;
    status_t status = STATUS_OK;

    if (records == NULL || routes == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *routes = NULL;

    for (i = 0; i < records->count && status == STATUS_OK; i++) {
        record = &records->items[i];
        if (current != NULL && strcmp(current->vehicle, record->vehicle) != 0) {
            status = STATUS_INVALID_DATA;
        } else if (record->marker == 'S' && current != NULL) {
            status = STATUS_INVALID_DATA;
        } else if (record->marker != 'S' && current == NULL) {
            status = STATUS_INVALID_DATA;
        }

        if (status == STATUS_OK && record->marker == 'S') {
            status = create_route(record->vehicle, &route);
            if (status == STATUS_OK) {
                if (tail == NULL) {
                    head = route;
                } else {
                    tail->next = route;
                }
                tail = route;
                current = route;
                last = NULL;
            }
        }

        if (status == STATUS_OK) {
            status = create_stop(record, &stop);
            if (status == STATUS_OK) {
                if (last == NULL) {
                    current->stops = stop;
                } else {
                    last->next = stop;
                }
                last = stop;
                if (record->marker == 'E') {
                    current = NULL;
                }
            }
        }
    }
    if (status == STATUS_OK && current != NULL) {
        status = STATUS_INVALID_DATA;
    }

    if (status != STATUS_OK) {
        destroy_routes(head);
        return status;
    }
    *routes = head;
    return STATUS_OK;
}

static double stop_duration(const StopNode *stop)
{
    return (double)(stop->departure - stop->arrival);
}

static double route_downtime(const RouteNode *route)
{
    const StopNode *stop;
    double sum = 0.0;

    for (stop = route->stops; stop != NULL; stop = stop->next) {
        sum += stop_duration(stop);
    }
    return sum;
}

static double metric_vehicle_routes(const RouteNode *head, const RouteNode *route)
{
    const RouteNode *node;
    double count = 0.0;

    for (node = head; node != NULL; node = node->next) {
        if (strcmp(node->vehicle, route->vehicle) == 0) {
            count += 1.0;
        }
    }
    return count;
}

static double metric_vehicle_downtime(const RouteNode *head, const RouteNode *route)
{
    const RouteNode *node;
    double sum = 0.0;

    for (node = head; node != NULL; node = node->next) {
        if (strcmp(node->vehicle, route->vehicle) == 0) {
            sum += route_downtime(node);
        }
    }
    return sum;
}

static double metric_route_distance(const RouteNode *head, const RouteNode *route)
{
    const StopNode *stop;
    double dx;
    double dy;
    double sum = 0.0;

    (void)head;
    for (stop = route->stops; stop != NULL && stop->next != NULL; stop = stop->next) {
        dx = stop->next->x - stop->x;
        dy = stop->next->y - stop->y;
        sum += sqrt(dx * dx + dy * dy);
    }
    return sum;
}

static double metric_route_stops(const RouteNode *head, const RouteNode *route)
{
    const StopNode *stop;
    double count = 0.0;

    (void)head;
    for (stop = route->stops; stop != NULL; stop = stop->next) {
        count += 1.0;
    }
    return count;
}

static double metric_route_longest_stop(const RouteNode *head, const RouteNode *route)
{
    const StopNode *stop;
    double best = 0.0;

    (void)head;
    for (stop = route->stops; stop != NULL; stop = stop->next) {
        if (stop_duration(stop) > best) {
            best = stop_duration(stop);
        }
    }
    return best;
}

static double metric_route_shortest_stop(const RouteNode *head, const RouteNode *route)
{
    const StopNode *stop;
    double best;

    (void)head;
    best = stop_duration(route->stops);
    for (stop = route->stops->next; stop != NULL; stop = stop->next) {
        if (stop_duration(stop) < best) {
            best = stop_duration(stop);
        }
    }
    return best;
}

static int is_first_of_vehicle(const RouteNode *head, const RouteNode *route)
{
    const RouteNode *node;

    for (node = head; node != route; node = node->next) {
        if (strcmp(node->vehicle, route->vehicle) == 0) {
            return 0;
        }
    }
    return 1;
}

status_t find_extreme(const RouteNode *head, metric_t metric, int per_vehicle,
                      int want_max, double epsilon, Answer *answer)
{
    const RouteNode *node;
    double value;
    int found = 0;

    if (metric == NULL || answer == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (node = head; node != NULL; node = node->next) {
        if (per_vehicle && !is_first_of_vehicle(head, node)) {
            continue;
        }
        value = metric(head, node);
        if (!found || (want_max && value > answer->value + epsilon) ||
            (!want_max && value < answer->value - epsilon)) {
            answer->vehicle = node->vehicle;
            answer->value = value;
            found = 1;
        }
    }
    if (!found) {
        return STATUS_EMPTY;
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
        case STATUS_INVALID_DATA:
            printf("Ошибка: некорректные данные\n");
            break;
        case STATUS_EMPTY:
            printf("Хранилище маршрутов пусто\n");
            break;
    }
}

static status_t read_choice(int *choice)
{
    char buffer[LINE_SIZE];
    char *end;
    long number;
    int got;
    status_t status;

    printf("Выбор: ");
    status = next_line(stdin, buffer, LINE_SIZE, &got);
    if (status != STATUS_OK) {
        return status;
    }
    if (!got) {
        return STATUS_READ_ERROR;
    }
    number = strtol(buffer, &end, 10);
    if (end == buffer || *end != '\0' || number < 0 || number > 1000) {
        return STATUS_INVALID_DATA;
    }
    *choice = (int)number;
    return STATUS_OK;
}

static void dialog_query(const RouteNode *head, metric_t metric, int per_vehicle,
                         int want_max, const char *title, const char *unit)
{
    Answer answer;
    status_t status;

    status = find_extreme(head, metric, per_vehicle, want_max, EPSILON, &answer);
    if (status == STATUS_OK) {
        printf("%s: %s (%.2f %s)\n", title, answer.vehicle, answer.value, unit);
    } else {
        print_error(status);
    }
}

static void dialog_show_routes(const RouteNode *head)
{
    const RouteNode *route;
    int number = 1;

    if (head == NULL) {
        printf("Хранилище маршрутов пусто\n");
        return;
    }
    for (route = head; route != NULL; route = route->next) {
        printf("%d. %s: остановок %.0f, длина пути %.2f\n", number, route->vehicle,
               metric_route_stops(head, route), metric_route_distance(head, route));
        number++;
    }
}

static void print_menu(void)
{
    printf("\n1. Транспорт с наибольшим числом маршрутов\n");
    printf("2. Транспорт с наименьшим числом маршрутов\n");
    printf("3. Транспорт, проехавший самый длинный путь\n");
    printf("4. Транспорт, проехавший самый короткий путь\n");
    printf("5. Транспорт с самым длинным маршрутом (по числу остановок)\n");
    printf("6. Транспорт с самым коротким маршрутом (по числу остановок)\n");
    printf("7. Транспорт с самой долгой остановкой\n");
    printf("8. Транспорт с самой короткой остановкой\n");
    printf("9. Транспорт с наибольшим временем простоя\n");
    printf("10. Показать все маршруты\n");
    printf("0. Выход\n");
}

static void run_menu(const RouteNode *head)
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
                dialog_query(head, metric_vehicle_routes, 1, 1,
                             "Больше всех маршрутов", "маршрутов");
                break;
            case 2:
                dialog_query(head, metric_vehicle_routes, 1, 0,
                             "Меньше всех маршрутов", "маршрутов");
                break;
            case 3:
                dialog_query(head, metric_route_distance, 0, 1,
                             "Самый длинный путь", "ед. длины");
                break;
            case 4:
                dialog_query(head, metric_route_distance, 0, 0,
                             "Самый короткий путь", "ед. длины");
                break;
            case 5:
                dialog_query(head, metric_route_stops, 0, 1,
                             "Самый длинный маршрут", "остановок");
                break;
            case 6:
                dialog_query(head, metric_route_stops, 0, 0,
                             "Самый короткий маршрут", "остановок");
                break;
            case 7:
                dialog_query(head, metric_route_longest_stop, 0, 1,
                             "Самая долгая остановка", "сек");
                break;
            case 8:
                dialog_query(head, metric_route_shortest_stop, 0, 0,
                             "Самая короткая остановка", "сек");
                break;
            case 9:
                dialog_query(head, metric_vehicle_downtime, 1, 1,
                             "Наибольшее время простоя", "сек");
                break;
            case 10:
                dialog_show_routes(head);
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
    RecordArray records = {NULL, 0, 0};
    RouteNode *routes = NULL;
    status_t status = STATUS_OK;
    int i;

    if (argc < 2) {
        printf("Использование: %s <файл1> [файл2 ...]\n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc && status == STATUS_OK; i++) {
        status = load_file(argv[i], &records);
        if (status != STATUS_OK) {
            printf("Файл %s: ", argv[i]);
        }
    }
    if (status == STATUS_OK) {
        status = sort_records(&records);
    }
    if (status == STATUS_OK) {
        status = build_routes(&records, &routes);
    }
    free_records(&records);

    if (status != STATUS_OK) {
        print_error(status);
        destroy_routes(routes);
        return 1;
    }

    run_menu(routes);
    destroy_routes(routes);

    return 0;
}