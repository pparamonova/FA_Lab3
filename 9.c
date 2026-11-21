#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define LINE_SIZE 1024
#define INITIAL_CAP 64

typedef enum {
    ST_OK = 0,
    ST_ERR_ARGS,
    ST_ERR_NULL,
    ST_ERR_MEMORY,
    ST_ERR_FILE_OPEN,
    ST_ERR_FILE_IO,
    ST_ERR_FORMAT,
    ST_ERR_EMPTY,
    ST_ERR_NOT_FOUND,
    ST_ERR_INVALID,
    ST_ERR_EOF
} Status;

typedef struct Node {
    char *word;
    unsigned long count;
    struct Node *left;
    struct Node *right;
} Node;

static const char *status_message(Status st)
{
    switch (st) {
    case ST_OK:           return "Успешно";
    case ST_ERR_ARGS:     return "Неверные аргументы";
    case ST_ERR_NULL:     return "Передан нулевой указатель";
    case ST_ERR_MEMORY:   return "Не удалось выделить память";
    case ST_ERR_FILE_OPEN:return "Не удалось открыть файл";
    case ST_ERR_FILE_IO:  return "Ошибка чтения или записи файла";
    case ST_ERR_FORMAT:   return "Файл повреждён или имеет неверный формат";
    case ST_ERR_EMPTY:    return "Дерево пусто";
    case ST_ERR_NOT_FOUND:return "Слово не найдено";
    case ST_ERR_INVALID:  return "Некорректный ввод";
    case ST_ERR_EOF:      return "Конец ввода";
    }
    return "Неизвестная ошибка";
}

static Node *node_create(const char *word)
{
    Node *node = (Node *)malloc(sizeof(Node));
    size_t len;

    if (node == NULL) {
        return NULL;
    }
    len = strlen(word);
    node->word = (char *)malloc(len + 1);
    if (node->word == NULL) {
        free(node);
        return NULL;
    }
    memcpy(node->word, word, len + 1);
    node->count = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

static void tree_free(Node *root)
{
    Node *left;
    Node *next;

    while (root != NULL) {
        if (root->left != NULL) {
            left = root->left;
            root->left = left->right;
            left->right = root;
            root = left;
        } else {
            next = root->right;
            free(root->word);
            free(root);
            root = next;
        }
    }
}

static Status tree_insert(Node **root, const char *word)
{
    Node **cur;
    int cmp;

    if (root == NULL || word == NULL) {
        return ST_ERR_NULL;
    }
    cur = root;
    while (*cur != NULL) {
        cmp = strcmp(word, (*cur)->word);
        if (cmp == 0) {
            (*cur)->count++;
            return ST_OK;
        }
        if (cmp < 0) {
            cur = &(*cur)->left;
        } else {
            cur = &(*cur)->right;
        }
    }
    *cur = node_create(word);
    if (*cur == NULL) {
        return ST_ERR_MEMORY;
    }
    return ST_OK;
}

static Status tree_find(const Node *root, const char *word, unsigned long *count)
{
    int cmp;

    if (word == NULL || count == NULL) {
        return ST_ERR_NULL;
    }
    while (root != NULL) {
        cmp = strcmp(word, root->word);
        if (cmp == 0) {
            *count = root->count;
            return ST_OK;
        }
        root = (cmp < 0) ? root->left : root->right;
    }
    return ST_ERR_NOT_FOUND;
}

static int tree_depth(const Node *root)
{
    int l;
    int r;

    if (root == NULL) {
        return 0;
    }
    l = tree_depth(root->left);
    r = tree_depth(root->right);
    return 1 + (l > r ? l : r);
}

static size_t count_nodes(const Node *root)
{
    if (root == NULL) {
        return 0;
    }
    return 1 + count_nodes(root->left) + count_nodes(root->right);
}

static void collect_nodes(const Node *root, const Node **arr, size_t *idx)
{
    if (root == NULL) {
        return;
    }
    collect_nodes(root->left, arr, idx);
    arr[*idx] = root;
    (*idx)++;
    collect_nodes(root->right, arr, idx);
}

static int cmp_by_count(const void *a, const void *b)
{
    const Node *x = *(const Node *const *)a;
    const Node *y = *(const Node *const *)b;

    if (x->count > y->count) {
        return -1;
    }
    if (x->count < y->count) {
        return 1;
    }
    return strcmp(x->word, y->word);
}

static Status tree_sorted_by_count(const Node *root, const Node ***out, size_t *size)
{
    const Node **arr;
    size_t n;
    size_t idx = 0;

    if (out == NULL || size == NULL) {
        return ST_ERR_NULL;
    }
    n = count_nodes(root);
    if (n == 0) {
        return ST_ERR_EMPTY;
    }
    arr = (const Node **)malloc(n * sizeof(const Node *));
    if (arr == NULL) {
        return ST_ERR_MEMORY;
    }
    collect_nodes(root, arr, &idx);
    qsort(arr, n, sizeof(const Node *), cmp_by_count);
    *out = arr;
    *size = n;
    return ST_OK;
}

static void extremes_walk(const Node *root, const Node **longest, const Node **shortest)
{
    size_t len;

    if (root == NULL) {
        return;
    }
    len = strlen(root->word);
    if (*longest == NULL || len > strlen((*longest)->word)) {
        *longest = root;
    }
    if (*shortest == NULL || len < strlen((*shortest)->word)) {
        *shortest = root;
    }
    extremes_walk(root->left, longest, shortest);
    extremes_walk(root->right, longest, shortest);
}

static Status tree_find_extremes(const Node *root, const Node **longest, const Node **shortest)
{
    if (longest == NULL || shortest == NULL) {
        return ST_ERR_NULL;
    }
    if (root == NULL) {
        return ST_ERR_EMPTY;
    }
    *longest = NULL;
    *shortest = NULL;
    extremes_walk(root, longest, shortest);
    return ST_OK;
}

static int trees_equal(const Node *a, const Node *b)
{
    if (a == NULL && b == NULL) {
        return 1;
    }
    if (a == NULL || b == NULL) {
        return 0;
    }
    if (a->count != b->count || strcmp(a->word, b->word) != 0) {
        return 0;
    }
    return trees_equal(a->left, b->left) && trees_equal(a->right, b->right);
}

static int is_separator(const char *seps, int c)
{
    if (c == 0) {
        return 1;
    }
    return strchr(seps, (char)c) != NULL;
}

static int contains_separator(const char *seps, const char *word)
{
    size_t i;

    for (i = 0; word[i] != '\0'; i++) {
        if (strchr(seps, word[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

static Status flush_word(Node **root, char *buf, size_t *len)
{
    Status st = ST_OK;

    if (*len > 0) {
        buf[*len] = '\0';
        st = tree_insert(root, buf);
        *len = 0;
    }
    return st;
}

static Status build_tree_from_file(const char *path, const char *seps, Node **root)
{
    FILE *f;
    Node *tmp = NULL;
    char *buf;
    char *bigger;
    size_t cap = INITIAL_CAP;
    size_t len = 0;
    int c;
    Status st = ST_OK;

    if (path == NULL || seps == NULL || root == NULL) {
        return ST_ERR_NULL;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return ST_ERR_FILE_OPEN;
    }
    buf = (char *)malloc(cap);
    if (buf == NULL) {
        fclose(f);
        return ST_ERR_MEMORY;
    }

    while (st == ST_OK && (c = fgetc(f)) != EOF) {
        if (is_separator(seps, c)) {
            st = flush_word(&tmp, buf, &len);
        } else {
            if (len + 1 >= cap) {
                bigger = (char *)realloc(buf, cap * 2);
                if (bigger == NULL) {
                    st = ST_ERR_MEMORY;
                    break;
                }
                buf = bigger;
                cap *= 2;
            }
            buf[len] = (char)c;
            len++;
        }
    }
    if (st == ST_OK && ferror(f)) {
        st = ST_ERR_FILE_IO;
    }
    if (st == ST_OK) {
        st = flush_word(&tmp, buf, &len);
    }

    free(buf);
    fclose(f);
    if (st != ST_OK) {
        tree_free(tmp);
        return st;
    }
    *root = tmp;
    return ST_OK;
}

static Status save_node(FILE *f, const Node *node)
{
    size_t len;
    Status st;

    if (node == NULL) {
        if (fputc(0, f) == EOF) {
            return ST_ERR_FILE_IO;
        }
        return ST_OK;
    }
    len = strlen(node->word);
    if (fputc(1, f) == EOF
        || fwrite(&node->count, sizeof(node->count), 1, f) != 1
        || fwrite(&len, sizeof(len), 1, f) != 1
        || fwrite(node->word, 1, len, f) != len) {
        return ST_ERR_FILE_IO;
    }
    st = save_node(f, node->left);
    if (st != ST_OK) {
        return st;
    }
    return save_node(f, node->right);
}

static Status tree_save(const char *path, const Node *root)
{
    FILE *f;
    Status st;

    if (path == NULL) {
        return ST_ERR_NULL;
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        return ST_ERR_FILE_OPEN;
    }
    st = save_node(f, root);
    if (fclose(f) != 0 && st == ST_OK) {
        st = ST_ERR_FILE_IO;
    }
    return st;
}

static Status load_node(FILE *f, Node **out)
{
    int marker;
    unsigned long count;
    size_t len;
    Node *node;
    Status st;

    *out = NULL;
    marker = fgetc(f);
    if (marker == EOF) {
        return ST_ERR_FORMAT;
    }
    if (marker == 0) {
        return ST_OK;
    }
    if (marker != 1) {
        return ST_ERR_FORMAT;
    }
    if (fread(&count, sizeof(count), 1, f) != 1
        || fread(&len, sizeof(len), 1, f) != 1) {
        return ST_ERR_FORMAT;
    }
    if (count == 0 || len == 0 || len == SIZE_MAX) {
        return ST_ERR_FORMAT;
    }
    node = (Node *)malloc(sizeof(Node));
    if (node == NULL) {
        return ST_ERR_MEMORY;
    }
    node->count = count;
    node->left = NULL;
    node->right = NULL;
    node->word = (char *)malloc(len + 1);
    if (node->word == NULL) {
        free(node);
        return ST_ERR_MEMORY;
    }
    if (fread(node->word, 1, len, f) != len) {
        free(node->word);
        free(node);
        return ST_ERR_FORMAT;
    }
    node->word[len] = '\0';
    if (strlen(node->word) != len) {
        free(node->word);
        free(node);
        return ST_ERR_FORMAT;
    }
    st = load_node(f, &node->left);
    if (st == ST_OK) {
        st = load_node(f, &node->right);
    }
    if (st != ST_OK) {
        tree_free(node);
        return st;
    }
    *out = node;
    return ST_OK;
}

static Status tree_load(const char *path, Node **root)
{
    FILE *f;
    Node *tmp = NULL;
    Status st;

    if (path == NULL || root == NULL) {
        return ST_ERR_NULL;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return ST_ERR_FILE_OPEN;
    }
    st = load_node(f, &tmp);
    if (st == ST_OK && fgetc(f) != EOF) {
        st = ST_ERR_FORMAT;
    }
    fclose(f);
    if (st != ST_OK) {
        tree_free(tmp);
        return st;
    }
    *root = tmp;
    return ST_OK;
}

static Status parse_separators(int argc, char *argv[], char **seps)
{
    char *res;
    int i;

    if (argv == NULL || seps == NULL) {
        return ST_ERR_NULL;
    }
    if (argc < 3) {
        return ST_ERR_ARGS;
    }
    res = (char *)malloc((size_t)(argc - 2) + 1);
    if (res == NULL) {
        return ST_ERR_MEMORY;
    }
    for (i = 2; i < argc; i++) {
        if (argv[i] == NULL || strlen(argv[i]) != 1) {
            free(res);
            return ST_ERR_ARGS;
        }
        res[i - 2] = argv[i][0];
    }
    res[argc - 2] = '\0';
    *seps = res;
    return ST_OK;
}

static Status parse_uint(const char *s, int *out)
{
    int value = 0;
    int digit;
    size_t i;

    if (s == NULL || out == NULL) {
        return ST_ERR_NULL;
    }
    if (s[0] == '\0') {
        return ST_ERR_INVALID;
    }
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return ST_ERR_INVALID;
        }
        digit = s[i] - '0';
        if (value > (INT_MAX - digit) / 10) {
            return ST_ERR_INVALID;
        }
        value = value * 10 + digit;
    }
    *out = value;
    return ST_OK;
}

static Status read_line(char *buf, size_t size)
{
    size_t len;
    int c;

    if (fgets(buf, (int)size, stdin) == NULL) {
        return ST_ERR_EOF;
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        len--;
        if (len > 0 && buf[len - 1] == '\r') {
            buf[len - 1] = '\0';
        }
        return ST_OK;
    }
    if (feof(stdin)) {
        return ST_OK;
    }
    while ((c = getchar()) != EOF && c != '\n') {
    }
    return ST_ERR_INVALID;
}

static void print_words(const Node *root)
{
    if (root == NULL) {
        return;
    }
    print_words(root->left);
    printf("%s : %lu\n", root->word, root->count);
    print_words(root->right);
}

static void print_menu(void)
{
    printf("\n===== Меню =====\n");
    printf("1. Все слова и их частоты\n");
    printf("2. Сколько раз встречается слово\n");
    printf("3. Первые n самых частых слов\n");
    printf("4. Самое длинное и самое короткое слово\n");
    printf("5. Глубина дерева\n");
    printf("6. Сохранить дерево в файл\n");
    printf("7. Загрузить дерево из файла\n");
    printf("8. Проверка: сохранить, восстановить и сравнить\n");
    printf("0. Выход\n");
    printf("Ваш выбор: ");
}

static void handle_all(const Node *root)
{
    if (root == NULL) {
        printf("%s\n", status_message(ST_ERR_EMPTY));
        return;
    }
    print_words(root);
}

static void handle_count_word(const Node *root, const char *seps)
{
    char line[LINE_SIZE];
    unsigned long count;
    Status st;

    printf("Введите слово: ");
    st = read_line(line, sizeof(line));
    if (st != ST_OK || line[0] == '\0' || contains_separator(seps, line)) {
        printf("%s (слово пустое, слишком длинное или содержит сепаратор)\n",
               status_message(ST_ERR_INVALID));
        return;
    }
    st = tree_find(root, line, &count);
    switch (st) {
    case ST_OK:
        printf("Слово \"%s\" встречается %lu раз(а)\n", line, count);
        break;
    case ST_ERR_NOT_FOUND:
        printf("Слово \"%s\" в файле не встречается (0 раз)\n", line);
        break;
    default:
        printf("Ошибка: %s\n", status_message(st));
        break;
    }
}

static void handle_top(const Node *root)
{
    char line[LINE_SIZE];
    const Node **arr = NULL;
    size_t size = 0;
    size_t i;
    size_t limit;
    int n = 0;
    Status st;

    printf("Введите n: ");
    st = read_line(line, sizeof(line));
    if (st == ST_OK) {
        st = parse_uint(line, &n);
    }
    if (st != ST_OK || n <= 0) {
        printf("%s (нужно положительное целое число)\n", status_message(ST_ERR_INVALID));
        return;
    }
    st = tree_sorted_by_count(root, &arr, &size);
    if (st != ST_OK) {
        printf("Ошибка: %s\n", status_message(st));
        return;
    }
    limit = ((size_t)n < size) ? (size_t)n : size;
    for (i = 0; i < limit; i++) {
        printf("%lu. %s : %lu\n", (unsigned long)(i + 1), arr[i]->word, arr[i]->count);
    }
    free(arr);
}

static void handle_extremes(const Node *root)
{
    const Node *longest = NULL;
    const Node *shortest = NULL;
    Status st = tree_find_extremes(root, &longest, &shortest);

    if (st != ST_OK) {
        printf("Ошибка: %s\n", status_message(st));
        return;
    }
    printf("Самое длинное слово: %s (длина %lu)\n", longest->word,
           (unsigned long)strlen(longest->word));
    printf("Самое короткое слово: %s (длина %lu)\n", shortest->word,
           (unsigned long)strlen(shortest->word));
}

static void handle_depth(const Node *root)
{
    printf("Глубина дерева: %d\n", tree_depth(root));
}

static Status ask_path(char *line, size_t size)
{
    Status st;

    printf("Введите путь к файлу: ");
    st = read_line(line, size);
    if (st == ST_OK && line[0] == '\0') {
        st = ST_ERR_INVALID;
    }
    return st;
}

static void handle_save(const Node *root)
{
    char path[LINE_SIZE];
    Status st = ask_path(path, sizeof(path));

    if (st != ST_OK) {
        printf("%s\n", status_message(ST_ERR_INVALID));
        return;
    }
    st = tree_save(path, root);
    printf("%s\n", status_message(st));
}

static void handle_load(Node **root)
{
    char path[LINE_SIZE];
    Node *loaded = NULL;
    Status st = ask_path(path, sizeof(path));

    if (st != ST_OK) {
        printf("%s\n", status_message(ST_ERR_INVALID));
        return;
    }
    st = tree_load(path, &loaded);
    if (st != ST_OK) {
        printf("Ошибка: %s. Текущее дерево не изменено.\n", status_message(st));
        return;
    }
    tree_free(*root);
    *root = loaded;
    printf("Дерево загружено\n");
}

static void handle_check(const Node *root)
{
    char path[LINE_SIZE];
    Node *restored = NULL;
    Status st = ask_path(path, sizeof(path));

    if (st != ST_OK) {
        printf("%s\n", status_message(ST_ERR_INVALID));
        return;
    }
    st = tree_save(path, root);
    if (st == ST_OK) {
        st = tree_load(path, &restored);
    }
    if (st != ST_OK) {
        printf("Ошибка: %s\n", status_message(st));
        return;
    }
    if (trees_equal(root, restored)) {
        printf("Восстановленное дерево совпадает с исходным (глубина %d)\n",
               tree_depth(restored));
    } else {
        printf("Восстановленное дерево ОТЛИЧАЕТСЯ от исходного\n");
    }
    tree_free(restored);
}

static void run_menu(Node **root, const char *seps)
{
    char line[LINE_SIZE];
    int choice;
    int running = 1;
    Status st;

    while (running) {
        print_menu();
        st = read_line(line, sizeof(line));
        if (st == ST_ERR_EOF) {
            printf("\n");
            break;
        }
        if (st != ST_OK || parse_uint(line, &choice) != ST_OK) {
            printf("Введите номер пункта меню\n");
            continue;
        }
        switch (choice) {
        case 1: handle_all(*root); break;
        case 2: handle_count_word(*root, seps); break;
        case 3: handle_top(*root); break;
        case 4: handle_extremes(*root); break;
        case 5: handle_depth(*root); break;
        case 6: handle_save(*root); break;
        case 7: handle_load(root); break;
        case 8: handle_check(*root); break;
        case 0: running = 0; break;
        default: printf("Нет такого пункта меню\n"); break;
        }
    }
}

int main(int argc, char *argv[])
{
    Node *root = NULL;
    char *seps = NULL;
    Status st;

    if (argc < 3) {
        printf("Использование: %s <файл> <сепаратор> [<сепаратор> ...]\n",
               (argc > 0 && argv != NULL && argv[0] != NULL) ? argv[0] : "task9");
        printf("Каждый сепаратор - ровно один символ.\n");
        return 1;
    }
    st = parse_separators(argc, argv, &seps);
    if (st != ST_OK) {
        printf("Ошибка: %s (каждый сепаратор должен быть одним символом)\n",
               status_message(st));
        return 1;
    }
    st = build_tree_from_file(argv[1], seps, &root);
    if (st != ST_OK) {
        printf("Ошибка: %s\n", status_message(st));
        free(seps);
        return 1;
    }

    run_menu(&root, seps);

    tree_free(root);
    free(seps);
    return 0;
}