#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY
} status_t;

static unsigned long long bit_add(unsigned long long a, unsigned long long b)
{
    unsigned long long carry;

    while (b != 0) {
        carry = (a & b) << 1;
        a = a ^ b;
        b = carry;
    }
    return a;
}

status_t decimal_to_base_2r(const long long number, const int r, char **result)
{
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
    char buffer[66];
    unsigned long long value;
    unsigned long long mask;
    unsigned long long len = 0;
    unsigned long long i;
    int negative;
    char *str;

    if (result == NULL || r < 1 || r > 5) {
        return STATUS_INVALID_ARGUMENT;
    }
    *result = NULL;

    negative = number < 0;
    if (negative) {
        value = bit_add(~(unsigned long long)number, 1);
    } else {
        value = (unsigned long long)number;
    }

    mask = ~(~0ULL << r);

    if (value == 0) {
        buffer[len] = '0';
        len = bit_add(len, 1);
    }

    while (value != 0) {
        buffer[len] = digits[value & mask];
        len = bit_add(len, 1);
        value = value >> r;
    }

    if (negative) {
        buffer[len] = '-';
        len = bit_add(len, 1);
    }

    str = (char *)malloc(bit_add(len, 1));
    if (str == NULL) {
        return STATUS_NO_MEMORY;
    }

    for (i = 0; i < len; i = bit_add(i, 1)) {
        str[i] = buffer[bit_add(~i, len)];
    }
    str[len] = '\0';

    *result = str;
    return STATUS_OK;
}

int main(void)
{
    const long long numbers[5] = {0, 10, 255, -255, 123456789};
    int r;
    int k;
    char *res;
    status_t status;

    for (k = 0; k < 5; k = (int)bit_add((unsigned long long)k, 1)) {
        printf("Число %lld:\n", numbers[k]);
        for (r = 1; r <= 5; r = (int)bit_add((unsigned long long)r, 1)) {
            status = decimal_to_base_2r(numbers[k], r, &res);
            switch (status) {
                case STATUS_OK:
                    printf("  основание 2^%d: %s\n", r, res);
                    free(res);
                    break;
                case STATUS_INVALID_ARGUMENT:
                    printf("Ошибка: неверные аргументы\n");
                    return 1;
                case STATUS_NO_MEMORY:
                    printf("Ошибка: не удалось выделить память\n");
                    return 1;
            }
        }
    }

    return 0;
}