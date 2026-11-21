#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define MAX_EXP 10000
#define NAME_SIZE 16
#define MAX_ARGS 2

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_OPEN_ERROR,
    STATUS_READ_ERROR,
    STATUS_SYNTAX,
    STATUS_OVERFLOW,
    STATUS_DIVISION_BY_ZERO,
    STATUS_NOT_DIVISIBLE
} status_t;

typedef struct Term {
    long long coef;
    int exp;
    struct Term *next;
} Term;

typedef struct {
    Term *head;
} Polynomial;

typedef struct {
    const char *start;
    size_t length;
} Span;

typedef struct {
    int is_value;
    long long value;
} Outcome;

typedef status_t (*binary_fn)(const Polynomial *, const Polynomial *, Polynomial *);

typedef struct {
    const char *name;
    binary_fn function;
} BinaryOperation;

static status_t checked_add(long long a, long long b, long long *result)
{
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) {
        return STATUS_OVERFLOW;
    }
    *result = a + b;
    return STATUS_OK;
}

static status_t checked_sub(long long a, long long b, long long *result)
{
    if ((b < 0 && a > LLONG_MAX + b) || (b > 0 && a < LLONG_MIN + b)) {
        return STATUS_OVERFLOW;
    }
    *result = a - b;
    return STATUS_OK;
}

static status_t checked_neg(long long a, long long *result)
{
    if (a == LLONG_MIN) {
        return STATUS_OVERFLOW;
    }
    *result = -a;
    return STATUS_OK;
}

static status_t checked_mul(long long a, long long b, long long *result)
{
    if (a == 0 || b == 0) {
        *result = 0;
        return STATUS_OK;
    }
    if ((a == -1 && b == LLONG_MIN) || (b == -1 && a == LLONG_MIN)) {
        return STATUS_OVERFLOW;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > LLONG_MAX / b) {
                return STATUS_OVERFLOW;
            }
        } else if (b < LLONG_MIN / a) {
            return STATUS_OVERFLOW;
        }
    } else {
        if (b > 0) {
            if (a < LLONG_MIN / b) {
                return STATUS_OVERFLOW;
            }
        } else if (a < LLONG_MAX / b) {
            return STATUS_OVERFLOW;
        }
    }
    *result = a * b;
    return STATUS_OK;
}

static status_t checked_pow(long long base, int n, long long *result)
{
    long long value = 1;
    int i;
    status_t status = STATUS_OK;

    for (i = 0; i < n && status == STATUS_OK; i++) {
        status = checked_mul(value, base, &value);
    }
    if (status == STATUS_OK) {
        *result = value;
    }
    return status;
}

void poly_init(Polynomial *poly)
{
    poly->head = NULL;
}

void poly_destroy(Polynomial *poly)
{
    Term *term;

    if (poly == NULL) {
        return;
    }
    while (poly->head != NULL) {
        term = poly->head;
        poly->head = term->next;
        free(term);
    }
}

static void poly_replace(Polynomial *target, Polynomial *source)
{
    poly_destroy(target);
    target->head = source->head;
    source->head = NULL;
}

static status_t append_term(Polynomial *poly, Term **tail, long long coef, int exp)
{
    Term *term = (Term *)malloc(sizeof(Term));

    if (term == NULL) {
        return STATUS_NO_MEMORY;
    }
    term->coef = coef;
    term->exp = exp;
    term->next = NULL;
    if (*tail == NULL) {
        poly->head = term;
    } else {
        (*tail)->next = term;
    }
    *tail = term;
    return STATUS_OK;
}

status_t poly_copy(const Polynomial *source, Polynomial *copy)
{
    const Term *term;
    Term *tail = NULL;
    status_t status = STATUS_OK;

    if (source == NULL || copy == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    poly_init(copy);
    for (term = source->head; term != NULL && status == STATUS_OK; term = term->next) {
        status = append_term(copy, &tail, term->coef, term->exp);
    }
    if (status != STATUS_OK) {
        poly_destroy(copy);
    }
    return status;
}

static status_t poly_from_constant(Polynomial *poly, long long value)
{
    Term *tail = NULL;

    poly_init(poly);
    if (value == 0) {
        return STATUS_OK;
    }
    return append_term(poly, &tail, value, 0);
}

static status_t poly_combine(const Polynomial *a, const Polynomial *b, int subtract,
                             Polynomial *result)
{
    const Term *x = a->head;
    const Term *y = b->head;
    Term *tail = NULL;
    long long coef = 0;
    int exp = 0;
    status_t status = STATUS_OK;

    poly_init(result);
    while (status == STATUS_OK && (x != NULL || y != NULL)) {
        if (y == NULL || (x != NULL && x->exp > y->exp)) {
            coef = x->coef;
            exp = x->exp;
            x = x->next;
        } else if (x == NULL || y->exp > x->exp) {
            exp = y->exp;
            if (subtract) {
                status = checked_neg(y->coef, &coef);
            } else {
                coef = y->coef;
            }
            y = y->next;
        } else {
            exp = x->exp;
            if (subtract) {
                status = checked_sub(x->coef, y->coef, &coef);
            } else {
                status = checked_add(x->coef, y->coef, &coef);
            }
            x = x->next;
            y = y->next;
        }
        if (status == STATUS_OK && coef != 0) {
            status = append_term(result, &tail, coef, exp);
        }
    }
    if (status != STATUS_OK) {
        poly_destroy(result);
    }
    return status;
}

static status_t poly_add_term(Polynomial *poly, long long coef, int exp)
{
    Polynomial single;
    Polynomial sum;
    Term *tail = NULL;
    status_t status = STATUS_OK;

    poly_init(&single);
    if (coef != 0) {
        status = append_term(&single, &tail, coef, exp);
    }
    if (status == STATUS_OK) {
        status = poly_combine(poly, &single, 0, &sum);
    }
    poly_destroy(&single);
    if (status == STATUS_OK) {
        poly_replace(poly, &sum);
    }
    return status;
}

status_t poly_add(const Polynomial *a, const Polynomial *b, Polynomial *result)
{
    if (a == NULL || b == NULL || result == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    return poly_combine(a, b, 0, result);
}

status_t poly_sub(const Polynomial *a, const Polynomial *b, Polynomial *result)
{
    if (a == NULL || b == NULL || result == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    return poly_combine(a, b, 1, result);
}

static status_t mul_monomial(const Polynomial *poly, long long coef, int exp,
                             Polynomial *result)
{
    const Term *term;
    Term *tail = NULL;
    long long value;
    status_t status = STATUS_OK;

    poly_init(result);
    for (term = poly->head; term != NULL && status == STATUS_OK; term = term->next) {
        status = checked_mul(coef, term->coef, &value);
        if (status == STATUS_OK && term->exp > MAX_EXP - exp) {
            status = STATUS_OVERFLOW;
        }
        if (status == STATUS_OK) {
            status = append_term(result, &tail, value, term->exp + exp);
        }
    }
    if (status != STATUS_OK) {
        poly_destroy(result);
    }
    return status;
}

status_t poly_mul(const Polynomial *a, const Polynomial *b, Polynomial *result)
{
    const Term *term;
    Polynomial total;
    Polynomial part;
    Polynomial sum;
    status_t status = STATUS_OK;

    if (a == NULL || b == NULL || result == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    poly_init(&total);
    for (term = a->head; term != NULL && status == STATUS_OK; term = term->next) {
        status = mul_monomial(b, term->coef, term->exp, &part);
        if (status == STATUS_OK) {
            status = poly_combine(&total, &part, 0, &sum);
            poly_destroy(&part);
            if (status == STATUS_OK) {
                poly_replace(&total, &sum);
            }
        }
    }
    if (status != STATUS_OK) {
        poly_destroy(&total);
        poly_init(result);
        return status;
    }
    result->head = total.head;
    return STATUS_OK;
}

status_t poly_divide(const Polynomial *a, const Polynomial *b, Polynomial *quotient,
                     Polynomial *remainder)
{
    Polynomial rem;
    Polynomial quo;
    Polynomial product;
    Polynomial next;
    Term *quo_tail = NULL;
    long long lead_a;
    long long lead_b;
    long long coef = 0;
    int exp;
    status_t status;

    if (a == NULL || b == NULL || quotient == NULL || remainder == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    poly_init(quotient);
    poly_init(remainder);
    if (b->head == NULL) {
        return STATUS_DIVISION_BY_ZERO;
    }

    poly_init(&quo);
    status = poly_copy(a, &rem);
    while (status == STATUS_OK && rem.head != NULL && rem.head->exp >= b->head->exp) {
        lead_a = rem.head->coef;
        lead_b = b->head->coef;
        if (lead_b == -1) {
            status = checked_neg(lead_a, &coef);
        } else if (lead_a % lead_b != 0) {
            status = STATUS_NOT_DIVISIBLE;
        } else {
            coef = lead_a / lead_b;
        }
        if (status == STATUS_OK) {
            exp = rem.head->exp - b->head->exp;
            status = mul_monomial(b, coef, exp, &product);
            if (status == STATUS_OK) {
                status = poly_combine(&rem, &product, 1, &next);
                poly_destroy(&product);
                if (status == STATUS_OK) {
                    poly_replace(&rem, &next);
                }
            }
            if (status == STATUS_OK) {
                status = append_term(&quo, &quo_tail, coef, exp);
            }
        }
    }

    if (status != STATUS_OK) {
        poly_destroy(&rem);
        poly_destroy(&quo);
        return status;
    }
    quotient->head = quo.head;
    remainder->head = rem.head;
    return STATUS_OK;
}

status_t poly_quotient(const Polynomial *a, const Polynomial *b, Polynomial *result)
{
    Polynomial rem;
    status_t status;

    poly_init(&rem);
    status = poly_divide(a, b, result, &rem);
    poly_destroy(&rem);
    return status;
}

status_t poly_remainder(const Polynomial *a, const Polynomial *b, Polynomial *result)
{
    Polynomial quo;
    status_t status;

    poly_init(&quo);
    status = poly_divide(a, b, &quo, result);
    poly_destroy(&quo);
    return status;
}

status_t poly_eval(const Polynomial *poly, long long x, long long *value)
{
    const Term *term;
    long long result = 0;
    long long power;
    int last_exp = 0;
    int first = 1;
    status_t status = STATUS_OK;

    if (poly == NULL || value == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (term = poly->head; term != NULL && status == STATUS_OK; term = term->next) {
        if (first) {
            result = term->coef;
            first = 0;
        } else {
            status = checked_pow(x, last_exp - term->exp, &power);
            if (status == STATUS_OK) {
                status = checked_mul(result, power, &result);
            }
            if (status == STATUS_OK) {
                status = checked_add(result, term->coef, &result);
            }
        }
        last_exp = term->exp;
    }
    if (status == STATUS_OK && !first) {
        status = checked_pow(x, last_exp, &power);
        if (status == STATUS_OK) {
            status = checked_mul(result, power, &result);
        }
    }
    if (status == STATUS_OK) {
        *value = result;
    }
    return status;
}

status_t poly_derivative(const Polynomial *poly, Polynomial *result)
{
    const Term *term;
    Term *tail = NULL;
    long long coef;
    status_t status = STATUS_OK;

    if (poly == NULL || result == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    poly_init(result);
    for (term = poly->head; term != NULL && status == STATUS_OK; term = term->next) {
        if (term->exp > 0) {
            status = checked_mul(term->coef, term->exp, &coef);
            if (status == STATUS_OK) {
                status = append_term(result, &tail, coef, term->exp - 1);
            }
        }
    }
    if (status != STATUS_OK) {
        poly_destroy(result);
    }
    return status;
}

status_t poly_compose(const Polynomial *f, const Polynomial *g, Polynomial *result)
{
    const Term *term;
    Polynomial current;
    Polynomial next;
    Polynomial constant;
    int last_exp = 0;
    int first = 1;
    int i;
    status_t status = STATUS_OK;

    if (f == NULL || g == NULL || result == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    poly_init(&current);
    if (f->head == NULL) {
        result->head = NULL;
        return STATUS_OK;
    }
    for (term = f->head; term != NULL && status == STATUS_OK; term = term->next) {
        if (first) {
            status = poly_from_constant(&current, term->coef);
            first = 0;
        } else {
            for (i = 0; i < last_exp - term->exp && status == STATUS_OK; i++) {
                status = poly_mul(&current, g, &next);
                if (status == STATUS_OK) {
                    poly_replace(&current, &next);
                }
            }
            if (status == STATUS_OK) {
                status = poly_from_constant(&constant, term->coef);
            }
            if (status == STATUS_OK) {
                status = poly_combine(&current, &constant, 0, &next);
                poly_destroy(&constant);
                if (status == STATUS_OK) {
                    poly_replace(&current, &next);
                }
            }
        }
        last_exp = term->exp;
    }
    if (!first) {
        for (i = 0; i < last_exp && status == STATUS_OK; i++) {
            status = poly_mul(&current, g, &next);
            if (status == STATUS_OK) {
                poly_replace(&current, &next);
            }
        }
    }
    if (status != STATUS_OK) {
        poly_destroy(&current);
        poly_init(result);
        return status;
    }
    result->head = current.head;
    return STATUS_OK;
}

status_t parse_polynomial(const char *text, size_t length, Polynomial *result)
{
    size_t i = 0;
    int sign;
    int exp;
    int has_coef;
    int has_digits;
    long long coef;
    status_t status = STATUS_OK;

    if (text == NULL || result == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    poly_init(result);
    if (length == 0) {
        return STATUS_SYNTAX;
    }

    while (i < length && status == STATUS_OK) {
        sign = 1;
        if (text[i] == '+' || text[i] == '-') {
            if (text[i] == '-') {
                sign = -1;
            }
            i++;
        } else if (i > 0) {
            status = STATUS_SYNTAX;
        }

        coef = 0;
        has_coef = 0;
        while (status == STATUS_OK && i < length && isdigit((unsigned char)text[i])) {
            status = checked_mul(coef, 10, &coef);
            if (status == STATUS_OK) {
                status = checked_add(coef, text[i] - '0', &coef);
            }
            has_coef = 1;
            i++;
        }

        exp = 0;
        if (status == STATUS_OK) {
            if (i < length && text[i] == 'x') {
                i++;
                exp = 1;
                if (!has_coef) {
                    coef = 1;
                }
                if (i < length && text[i] == '^') {
                    i++;
                    exp = 0;
                    has_digits = 0;
                    while (status == STATUS_OK && i < length &&
                           isdigit((unsigned char)text[i])) {
                        status = checked_mul(exp, 10, &exp);
                        if (status == STATUS_OK) {
                            status = checked_add(exp, text[i] - '0', &exp);
                        }
                        if (status == STATUS_OK && exp > MAX_EXP) {
                            status = STATUS_OVERFLOW;
                        }
                        has_digits = 1;
                        i++;
                    }
                    if (status == STATUS_OK && !has_digits) {
                        status = STATUS_SYNTAX;
                    }
                }
            } else if (!has_coef) {
                status = STATUS_SYNTAX;
            }
        }
        if (status == STATUS_OK) {
            status = poly_add_term(result, sign * coef, exp);
        }
    }

    if (status != STATUS_OK) {
        poly_destroy(result);
    }
    return status;
}

static status_t parse_integer(const char *text, size_t length, long long *value)
{
    size_t i = 0;
    long long sign = 1;
    long long number = 0;
    status_t status = STATUS_OK;

    if (length == 0) {
        return STATUS_SYNTAX;
    }
    if (text[0] == '+' || text[0] == '-') {
        if (text[0] == '-') {
            sign = -1;
        }
        i = 1;
    }
    if (i >= length) {
        return STATUS_SYNTAX;
    }
    for (; i < length && status == STATUS_OK; i++) {
        if (!isdigit((unsigned char)text[i])) {
            status = STATUS_SYNTAX;
        } else {
            status = checked_mul(number, 10, &number);
            if (status == STATUS_OK) {
                status = checked_add(number, text[i] - '0', &number);
            }
        }
    }
    if (status == STATUS_OK) {
        *value = sign * number;
    }
    return status;
}

static status_t split_instruction(const char *instr, char *name, size_t name_size,
                                  Span *args, size_t *count)
{
    const char *open = strchr(instr, '(');
    size_t length = strlen(instr);
    size_t name_length;
    size_t inner_start;
    size_t inner_end;
    size_t start;
    size_t i;

    *count = 0;
    if (open == NULL || open == instr || instr[length - 1] != ')') {
        return STATUS_SYNTAX;
    }
    name_length = (size_t)(open - instr);
    if (name_length >= name_size) {
        return STATUS_SYNTAX;
    }
    memcpy(name, instr, name_length);
    name[name_length] = '\0';

    inner_start = name_length + 1;
    inner_end = length - 1;
    if (inner_start == inner_end) {
        return STATUS_OK;
    }
    start = inner_start;
    for (i = inner_start; i <= inner_end; i++) {
        if (i == inner_end || instr[i] == ',') {
            if (i == start || *count >= MAX_ARGS) {
                return STATUS_SYNTAX;
            }
            args[*count].start = instr + start;
            args[*count].length = i - start;
            (*count)++;
            start = i + 1;
        }
    }
    return STATUS_OK;
}

static binary_fn find_binary(const char *name)
{
    static const BinaryOperation operations[6] = {
        {"Add", poly_add},
        {"Sub", poly_sub},
        {"Mult", poly_mul},
        {"Div", poly_quotient},
        {"Mod", poly_remainder},
        {"Cmps", poly_compose}
    };
    int i;

    for (i = 0; i < 6; i++) {
        if (strcmp(operations[i].name, name) == 0) {
            return operations[i].function;
        }
    }
    return NULL;
}

static status_t run_binary(binary_fn function, const Span *args, size_t count,
                           Polynomial *acc, Outcome *outcome)
{
    Polynomial first;
    Polynomial second;
    Polynomial result;
    const Polynomial *left = NULL;
    status_t status = STATUS_OK;

    poly_init(&first);
    poly_init(&second);
    poly_init(&result);

    if (count == 2) {
        status = parse_polynomial(args[0].start, args[0].length, &first);
        if (status == STATUS_OK) {
            status = parse_polynomial(args[1].start, args[1].length, &second);
        }
        left = &first;
    } else if (count == 1) {
        status = parse_polynomial(args[0].start, args[0].length, &second);
        left = acc;
    } else {
        status = STATUS_SYNTAX;
    }
    if (status == STATUS_OK) {
        status = function(left, &second, &result);
    }
    if (status == STATUS_OK) {
        poly_replace(acc, &result);
        outcome->is_value = 0;
    }

    poly_destroy(&first);
    poly_destroy(&second);
    poly_destroy(&result);
    return status;
}

static status_t run_eval(const Span *args, size_t count, Polynomial *acc,
                         Outcome *outcome)
{
    Polynomial first;
    const Polynomial *left = NULL;
    long long point = 0;
    long long value = 0;
    status_t status = STATUS_OK;

    poly_init(&first);
    if (count == 2) {
        status = parse_polynomial(args[0].start, args[0].length, &first);
        if (status == STATUS_OK) {
            status = parse_integer(args[1].start, args[1].length, &point);
        }
        left = &first;
    } else if (count == 1) {
        status = parse_integer(args[0].start, args[0].length, &point);
        left = acc;
    } else {
        status = STATUS_SYNTAX;
    }
    if (status == STATUS_OK) {
        status = poly_eval(left, point, &value);
    }
    if (status == STATUS_OK) {
        outcome->is_value = 1;
        outcome->value = value;
    }

    poly_destroy(&first);
    return status;
}

static status_t run_diff(const Span *args, size_t count, Polynomial *acc,
                         Outcome *outcome)
{
    Polynomial first;
    Polynomial result;
    const Polynomial *left = NULL;
    status_t status = STATUS_OK;

    poly_init(&first);
    poly_init(&result);
    if (count == 1) {
        status = parse_polynomial(args[0].start, args[0].length, &first);
        left = &first;
    } else if (count == 0) {
        left = acc;
    } else {
        status = STATUS_SYNTAX;
    }
    if (status == STATUS_OK) {
        status = poly_derivative(left, &result);
    }
    if (status == STATUS_OK) {
        poly_replace(acc, &result);
        outcome->is_value = 0;
    }

    poly_destroy(&first);
    poly_destroy(&result);
    return status;
}

status_t run_instruction(const char *instr, Polynomial *acc, Outcome *outcome)
{
    char name[NAME_SIZE];
    Span args[MAX_ARGS];
    size_t count = 0;
    binary_fn function;
    status_t status;

    if (instr == NULL || acc == NULL || outcome == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    outcome->is_value = 0;
    outcome->value = 0;

    status = split_instruction(instr, name, NAME_SIZE, args, &count);
    if (status != STATUS_OK) {
        return status;
    }
    if (strcmp(name, "Eval") == 0) {
        return run_eval(args, count, acc, outcome);
    }
    if (strcmp(name, "Diff") == 0) {
        return run_diff(args, count, acc, outcome);
    }
    function = find_binary(name);
    if (function == NULL) {
        return STATUS_SYNTAX;
    }
    return run_binary(function, args, count, acc, outcome);
}

status_t next_instruction(const char *text, size_t length, size_t *position,
                          char *buffer, size_t buffer_size, int *found)
{
    size_t i = *position;
    size_t used = 0;

    *found = 0;
    buffer[0] = '\0';
    while (i < length) {
        if (text[i] == '%') {
            while (i < length && text[i] != '\n') {
                i++;
            }
        } else if (text[i] == '[') {
            i++;
            while (i < length && text[i] != ']') {
                i++;
            }
            if (i >= length) {
                return STATUS_SYNTAX;
            }
            i++;
        } else if (text[i] == ']') {
            return STATUS_SYNTAX;
        } else if (text[i] == ';') {
            buffer[used] = '\0';
            *position = i + 1;
            *found = 1;
            return STATUS_OK;
        } else {
            if (!isspace((unsigned char)text[i])) {
                if (used + 1 >= buffer_size) {
                    return STATUS_SYNTAX;
                }
                buffer[used] = text[i];
                used++;
            }
            i++;
        }
    }
    *position = i;
    if (used > 0) {
        return STATUS_SYNTAX;
    }
    return STATUS_OK;
}

status_t read_file(const char *path, char **content, size_t *length)
{
    FILE *file;
    char *data = NULL;
    char *new_data;
    size_t capacity = 0;
    size_t size = 0;
    size_t got;

    if (path == NULL || content == NULL || length == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }
    do {
        if (size + 4097 > capacity) {
            capacity = capacity == 0 ? 4097 : capacity * 2;
            new_data = (char *)realloc(data, capacity);
            if (new_data == NULL) {
                free(data);
                fclose(file);
                return STATUS_NO_MEMORY;
            }
            data = new_data;
        }
        got = fread(data + size, 1, capacity - size - 1, file);
        size += got;
    } while (got > 0);

    if (ferror(file)) {
        free(data);
        fclose(file);
        return STATUS_READ_ERROR;
    }
    fclose(file);
    data[size] = '\0';
    *content = data;
    *length = size;
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
            printf("Ошибка: не удалось прочитать файл\n");
            break;
        case STATUS_SYNTAX:
            printf("Ошибка: синтаксическая ошибка\n");
            break;
        case STATUS_OVERFLOW:
            printf("Ошибка: переполнение (слишком большой коэффициент или степень)\n");
            break;
        case STATUS_DIVISION_BY_ZERO:
            printf("Ошибка: деление на нулевой многочлен\n");
            break;
        case STATUS_NOT_DIVISIBLE:
            printf("Ошибка: коэффициенты не делятся нацело\n");
            break;
    }
}

static void print_polynomial(const Polynomial *poly)
{
    const Term *term;
    unsigned long long magnitude;

    if (poly->head == NULL) {
        printf("0");
        return;
    }
    for (term = poly->head; term != NULL; term = term->next) {
        if (term->coef < 0) {
            magnitude = 0ULL - (unsigned long long)term->coef;
            if (term == poly->head) {
                printf("-");
            } else {
                printf(" - ");
            }
        } else {
            magnitude = (unsigned long long)term->coef;
            if (term != poly->head) {
                printf(" + ");
            }
        }
        if (magnitude != 1 || term->exp == 0) {
            printf("%llu", magnitude);
        }
        if (term->exp >= 1) {
            printf("x");
        }
        if (term->exp > 1) {
            printf("^%d", term->exp);
        }
    }
}

static void print_outcome(const char *instr, const Polynomial *acc,
                          const Outcome *outcome)
{
    printf("%s = ", instr);
    if (outcome->is_value) {
        printf("%lld", outcome->value);
    } else {
        print_polynomial(acc);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    char *text = NULL;
    char *buffer;
    size_t length = 0;
    size_t position = 0;
    size_t number = 0;
    int found = 1;
    Polynomial acc;
    Outcome outcome;
    status_t status;

    if (argc != 2) {
        printf("Использование: %s <файл с инструкциями>\n", argv[0]);
        return 1;
    }

    status = read_file(argv[1], &text, &length);
    if (status != STATUS_OK) {
        print_error(status);
        return 1;
    }
    buffer = (char *)malloc(length + 1);
    if (buffer == NULL) {
        free(text);
        print_error(STATUS_NO_MEMORY);
        return 1;
    }

    poly_init(&acc);
    while (status == STATUS_OK && found) {
        status = next_instruction(text, length, &position, buffer, length + 1, &found);
        if (status == STATUS_OK && found && buffer[0] != '\0') {
            number++;
            status = run_instruction(buffer, &acc, &outcome);
            if (status == STATUS_OK) {
                print_outcome(buffer, &acc, &outcome);
            } else {
                printf("Инструкция %zu (%s): ", number, buffer);
            }
        }
    }
    if (status != STATUS_OK) {
        print_error(status);
    }

    poly_destroy(&acc);
    free(buffer);
    free(text);

    return status == STATUS_OK ? 0 : 1;
}