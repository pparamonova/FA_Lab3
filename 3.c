#include <stdio.h>
#include <stdlib.h>

#define ALPHABET 256
#define BUFFER_SIZE 4096

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NO_MEMORY,
    STATUS_OPEN_ERROR,
    STATUS_READ_ERROR
} status_t;

typedef struct Node {
    int value;
    unsigned long long weight;
    struct Node *left;
    struct Node *right;
} Node;

typedef struct {
    Node **items;
    size_t size;
} Heap;

typedef struct {
    char bits[ALPHABET];
    size_t length;
} Code;

static Node *create_node(int value, unsigned long long weight, Node *left, Node *right)
{
    Node *node = (Node *)malloc(sizeof(Node));

    if (node == NULL) {
        return NULL;
    }
    node->value = value;
    node->weight = weight;
    node->left = left;
    node->right = right;
    return node;
}

static void free_tree(Node *root)
{
    if (root == NULL) {
        return;
    }
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

static void heap_push(Heap *heap, Node *node)
{
    size_t i = heap->size;
    size_t parent;

    heap->size++;
    while (i > 0) {
        parent = (i - 1) / 2;
        if (heap->items[parent]->weight <= node->weight) {
            break;
        }
        heap->items[i] = heap->items[parent];
        i = parent;
    }
    heap->items[i] = node;
}

static Node *heap_pop(Heap *heap)
{
    Node *top = heap->items[0];
    Node *last;
    size_t i = 0;
    size_t child;

    heap->size--;
    if (heap->size == 0) {
        return top;
    }

    last = heap->items[heap->size];
    while (2 * i + 1 < heap->size) {
        child = 2 * i + 1;
        if (child + 1 < heap->size &&
            heap->items[child + 1]->weight < heap->items[child]->weight) {
            child++;
        }
        if (last->weight <= heap->items[child]->weight) {
            break;
        }
        heap->items[i] = heap->items[child];
        i = child;
    }
    heap->items[i] = last;
    return top;
}

static void free_heap(Heap *heap)
{
    size_t i;

    for (i = 0; i < heap->size; i++) {
        free_tree(heap->items[i]);
    }
    free(heap->items);
    heap->items = NULL;
    heap->size = 0;
}

status_t count_frequencies(const char *path, unsigned long long *freq,
                           unsigned long long *size)
{
    FILE *file;
    unsigned char buffer[BUFFER_SIZE];
    size_t read_count;
    size_t i;

    if (path == NULL || freq == NULL || size == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < ALPHABET; i++) {
        freq[i] = 0;
    }
    *size = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        return STATUS_OPEN_ERROR;
    }

    while ((read_count = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        for (i = 0; i < read_count; i++) {
            freq[buffer[i]]++;
        }
        *size += read_count;
    }

    if (ferror(file)) {
        fclose(file);
        return STATUS_READ_ERROR;
    }

    fclose(file);
    return STATUS_OK;
}

status_t build_tree(const unsigned long long *freq, Node **root)
{
    Heap heap;
    Node *node;
    Node *first;
    Node *second;
    Node *parent;
    int i;

    if (freq == NULL || root == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    *root = NULL;

    heap.size = 0;
    heap.items = (Node **)malloc(ALPHABET * sizeof(Node *));
    if (heap.items == NULL) {
        return STATUS_NO_MEMORY;
    }

    for (i = 0; i < ALPHABET; i++) {
        if (freq[i] > 0) {
            node = create_node(i, freq[i], NULL, NULL);
            if (node == NULL) {
                free_heap(&heap);
                return STATUS_NO_MEMORY;
            }
            heap_push(&heap, node);
        }
    }

    while (heap.size > 1) {
        first = heap_pop(&heap);
        second = heap_pop(&heap);
        parent = create_node(0, first->weight + second->weight, first, second);
        if (parent == NULL) {
            free_tree(first);
            free_tree(second);
            free_heap(&heap);
            return STATUS_NO_MEMORY;
        }
        heap_push(&heap, parent);
    }

    if (heap.size == 1) {
        *root = heap_pop(&heap);
    }
    free(heap.items);

    return STATUS_OK;
}

static void fill_codes(const Node *node, char *path, size_t depth, Code *codes)
{
    size_t i;

    if (node->left == NULL && node->right == NULL) {
        for (i = 0; i < depth; i++) {
            codes[node->value].bits[i] = path[i];
        }
        codes[node->value].bits[depth] = '\0';
        codes[node->value].length = depth;
        return;
    }

    path[depth] = '0';
    fill_codes(node->left, path, depth + 1, codes);
    path[depth] = '1';
    fill_codes(node->right, path, depth + 1, codes);
}

status_t build_codes(const Node *root, Code *codes)
{
    char path[ALPHABET];
    int i;

    if (codes == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < ALPHABET; i++) {
        codes[i].bits[0] = '\0';
        codes[i].length = 0;
    }

    if (root == NULL) {
        return STATUS_OK;
    }

    if (root->left == NULL && root->right == NULL) {
        codes[root->value].bits[0] = '0';
        codes[root->value].bits[1] = '\0';
        codes[root->value].length = 1;
        return STATUS_OK;
    }

    fill_codes(root, path, 0, codes);
    return STATUS_OK;
}

status_t compute_stats(const unsigned long long *freq, const Code *codes,
                       unsigned long long size, unsigned long long *bits,
                       unsigned long long *bytes, double *saving)
{
    int i;

    if (freq == NULL || codes == NULL || bits == NULL || bytes == NULL ||
        saving == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    *bits = 0;
    for (i = 0; i < ALPHABET; i++) {
        *bits += freq[i] * (unsigned long long)codes[i].length;
    }
    *bytes = (*bits + 7) / 8;

    if (size == 0) {
        *saving = 0.0;
    } else {
        *saving = 100.0 * (1.0 - (double)*bytes / (double)size);
    }

    return STATUS_OK;
}

void print_report(const unsigned long long *freq, const Code *codes,
                  unsigned long long size, unsigned long long bits,
                  unsigned long long bytes, double saving)
{
    int i;

    printf("Байт\tЧастота\tКод\n");
    for (i = 0; i < ALPHABET; i++) {
        if (freq[i] > 0) {
            printf("%d\t%llu\t%s\n", i, freq[i], codes[i].bits);
        }
    }
    printf("Размер исходного файла: %llu байт\n", size);
    printf("Размер закодированных данных: %llu бит (%llu байт)\n", bits, bytes);
    printf("Экономия: %.2f%%\n", saving);
}

int main(int argc, char *argv[])
{
    unsigned long long freq[ALPHABET];
    Code codes[ALPHABET];
    unsigned long long size;
    unsigned long long bits;
    unsigned long long bytes;
    double saving;
    Node *root = NULL;
    status_t status;

    if (argc != 2) {
        printf("Использование: %s <путь к файлу>\n", argv[0]);
        return 1;
    }

    status = count_frequencies(argv[1], freq, &size);
    if (status == STATUS_OK) {
        status = build_tree(freq, &root);
    }
    if (status == STATUS_OK) {
        status = build_codes(root, codes);
    }
    if (status == STATUS_OK) {
        status = compute_stats(freq, codes, size, &bits, &bytes, &saving);
    }

    switch (status) {
        case STATUS_OK:
            print_report(freq, codes, size, bits, bytes, saving);
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
    }

    free_tree(root);

    return status == STATUS_OK ? 0 : 1;
}