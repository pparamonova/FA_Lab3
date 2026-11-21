#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_CAP 64

typedef enum {
    ST_OK = 0,
    ST_ERR_ARGS,
    ST_ERR_NULL,
    ST_ERR_MEMORY,
    ST_ERR_FILE_OPEN,
    ST_ERR_FILE_IO,
    ST_ERR_SYNTAX,
    ST_ERR_EOF
} Status;

typedef struct Node {
    char *name;
    struct Node *parent;
    struct Node **children;
    size_t count;
    size_t cap;
    size_t index;
} Node;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Prefix;

static const char *status_message(Status st)
{
    switch (st) {
    case ST_OK:           return "Успешно";
    case ST_ERR_ARGS:     return "Неверные аргументы";
    case ST_ERR_NULL:     return "Передан нулевой указатель";
    case ST_ERR_MEMORY:   return "Не удалось выделить память";
    case ST_ERR_FILE_OPEN:return "Не удалось открыть файл";
    case ST_ERR_FILE_IO:  return "Ошибка чтения или записи файла";
    case ST_ERR_SYNTAX:   return "Ошибка синтаксиса";
    case ST_ERR_EOF:      return "Конец файла";
    }
    return "Неизвестная ошибка";
}

static Node *node_create(const char *name, size_t len)
{
    Node *node = (Node *)malloc(sizeof(Node));

    if (node == NULL) {
        return NULL;
    }
    node->name = (char *)malloc(len + 1);
    if (node->name == NULL) {
        free(node);
        return NULL;
    }
    memcpy(node->name, name, len);
    node->name[len] = '\0';
    node->parent = NULL;
    node->children = NULL;
    node->count = 0;
    node->cap = 0;
    node->index = 0;
    return node;
}

static void node_free_single(Node *node)
{
    if (node != NULL) {
        free(node->children);
        free(node->name);
        free(node);
    }
}

static Status node_add_child(Node *parent, Node *child)
{
    Node **bigger;
    size_t new_cap;

    if (parent == NULL || child == NULL) {
        return ST_ERR_NULL;
    }
    if (parent->count == parent->cap) {
        new_cap = (parent->cap == 0) ? 2 : parent->cap * 2;
        bigger = (Node **)realloc(parent->children, new_cap * sizeof(Node *));
        if (bigger == NULL) {
            return ST_ERR_MEMORY;
        }
        parent->children = bigger;
        parent->cap = new_cap;
    }
    child->parent = parent;
    child->index = parent->count;
    parent->children[parent->count] = child;
    parent->count++;
    return ST_OK;
}

static void tree_free(Node *root)
{
    Node *node = root;
    Node *up;

    while (node != NULL) {
        if (node->count > 0) {
            node->count--;
            node = node->children[node->count];
        } else {
            up = node->parent;
            node_free_single(node);
            node = up;
        }
    }
}

static int is_name_char(char c)
{
    return c != '\0' && c != '(' && c != ')' && c != ',' &&
           !isspace((unsigned char)c);
}

static Status parse_expression(const char *s, size_t len, Node **out_root, size_t *err_pos)
{
    Node *root = NULL;
    Node *cur = NULL;
    Node *open = NULL;
    Node *node;
    int expect_name = 1;
    int can_open = 0;
    size_t i = 0;
    size_t start;
    Status st;

    if (s == NULL || out_root == NULL || err_pos == NULL) {
        return ST_ERR_NULL;
    }
    *out_root = NULL;
    *err_pos = 0;

    for (;;) {
        while (i < len && s[i] != '\0' && isspace((unsigned char)s[i])) {
            i++;
        }
        if (i >= len) {
            break;
        }
        if (expect_name) {
            start = i;
            while (i < len && is_name_char(s[i])) {
                i++;
            }
            if (i == start) {
                *err_pos = i + 1;
                tree_free(root);
                return ST_ERR_SYNTAX;
            }
            node = node_create(s + start, i - start);
            if (node == NULL) {
                tree_free(root);
                return ST_ERR_MEMORY;
            }
            if (open != NULL) {
                st = node_add_child(open, node);
                if (st != ST_OK) {
                    node_free_single(node);
                    tree_free(root);
                    return st;
                }
            } else if (root == NULL) {
                root = node;
            } else {
                node_free_single(node);
                *err_pos = start + 1;
                tree_free(root);
                return ST_ERR_SYNTAX;
            }
            cur = node;
            expect_name = 0;
            can_open = 1;
        } else if (s[i] == '(' && can_open) {
            open = cur;
            expect_name = 1;
            can_open = 0;
            i++;
        } else if (s[i] == ',' && open != NULL) {
            expect_name = 1;
            can_open = 0;
            i++;
        } else if (s[i] == ')' && open != NULL) {
            cur = open;
            open = open->parent;
            can_open = 0;
            i++;
        } else {
            *err_pos = i + 1;
            tree_free(root);
            return ST_ERR_SYNTAX;
        }
    }

    if (root == NULL || expect_name || open != NULL) {
        *err_pos = len + 1;
        tree_free(root);
        return ST_ERR_SYNTAX;
    }
    *out_root = root;
    return ST_OK;
}

static Status prefix_init(Prefix *p)
{
    p->cap = INITIAL_CAP;
    p->len = 0;
    p->data = (char *)malloc(p->cap);
    if (p->data == NULL) {
        return ST_ERR_MEMORY;
    }
    p->data[0] = '\0';
    return ST_OK;
}

static Status prefix_append(Prefix *p, const char *piece)
{
    size_t add = strlen(piece);
    char *bigger;
    size_t new_cap;

    if (p->len + add + 1 > p->cap) {
        new_cap = p->cap * 2;
        while (new_cap < p->len + add + 1) {
            new_cap *= 2;
        }
        bigger = (char *)realloc(p->data, new_cap);
        if (bigger == NULL) {
            return ST_ERR_MEMORY;
        }
        p->data = bigger;
        p->cap = new_cap;
    }
    memcpy(p->data + p->len, piece, add + 1);
    p->len += add;
    return ST_OK;
}

static void prefix_pop(Prefix *p, size_t n)
{
    p->len -= (n <= p->len) ? n : p->len;
    p->data[p->len] = '\0';
}

static Status read_line(FILE *f, char **line, size_t *len_out)
{
    char *buf;
    char *bigger;
    size_t cap = INITIAL_CAP;
    size_t len = 0;
    int c;

    if (f == NULL || line == NULL || len_out == NULL) {
        return ST_ERR_NULL;
    }
    *line = NULL;
    buf = (char *)malloc(cap);
    if (buf == NULL) {
        return ST_ERR_MEMORY;
    }
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            bigger = (char *)realloc(buf, cap * 2);
            if (bigger == NULL) {
                free(buf);
                return ST_ERR_MEMORY;
            }
            buf = bigger;
            cap *= 2;
        }
        buf[len] = (char)c;
        len++;
    }
    if (ferror(f)) {
        free(buf);
        return ST_ERR_FILE_IO;
    }
    if (c == EOF && len == 0) {
        free(buf);
        return ST_ERR_EOF;
    }
    if (len > 0 && buf[len - 1] == '\r') {
        len--;
    }
    buf[len] = '\0';
    *line = buf;
    *len_out = len;
    return ST_OK;
}

static int is_blank(const char *s, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (s[i] == '\0' || !isspace((unsigned char)s[i])) {
            return 0;
        }
    }
    return 1;
}

static Status tree_print(FILE *out, const Node *root)
{
    Prefix prefix;
    const Node *cur;
    int last;
    int done = 0;
    Status st;

    if (out == NULL || root == NULL) {
        return ST_ERR_NULL;
    }
    if (fputs(root->name, out) == EOF || fputc('\n', out) == EOF) {
        return ST_ERR_FILE_IO;
    }
    if (root->count == 0) {
        return ST_OK;
    }
    st = prefix_init(&prefix);
    if (st != ST_OK) {
        return st;
    }

    cur = root->children[0];
    while (!done && st == ST_OK) {
        last = (cur->index + 1 == cur->parent->count);
        if (fputs(prefix.data, out) == EOF
            || fputs(last ? "`-- " : "+-- ", out) == EOF
            || fputs(cur->name, out) == EOF
            || fputc('\n', out) == EOF) {
            st = ST_ERR_FILE_IO;
        } else if (cur->count > 0) {
            st = prefix_append(&prefix, last ? "    " : "|   ");
            if (st == ST_OK) {
                cur = cur->children[0];
            }
        } else {
            for (;;) {
                if (cur->index + 1 < cur->parent->count) {
                    cur = cur->parent->children[cur->index + 1];
                    break;
                }
                cur = cur->parent;
                if (cur == root) {
                    done = 1;
                    break;
                }
                prefix_pop(&prefix, 4);
            }
        }
    }
    free(prefix.data);
    return st;
}

static Status process_file(FILE *in, FILE *out, size_t *ok_count, size_t *bad_count)
{
    char *line;
    size_t len;
    size_t line_no = 0;
    size_t err_pos;
    Node *root;
    Status st;
    Status rd;

    if (in == NULL || out == NULL || ok_count == NULL || bad_count == NULL) {
        return ST_ERR_NULL;
    }
    *ok_count = 0;
    *bad_count = 0;

    for (;;) {
        rd = read_line(in, &line, &len);
        if (rd == ST_ERR_EOF) {
            return ST_OK;
        }
        if (rd != ST_OK) {
            return rd;
        }
        line_no++;
        if (is_blank(line, len)) {
            free(line);
            continue;
        }

        st = parse_expression(line, len, &root, &err_pos);
        if (st == ST_OK) {
            if (fprintf(out, "Строка %lu: %s\n", (unsigned long)line_no, line) < 0) {
                st = ST_ERR_FILE_IO;
            } else {
                st = tree_print(out, root);
            }
            if (st == ST_OK && fputc('\n', out) == EOF) {
                st = ST_ERR_FILE_IO;
            }
            tree_free(root);
            if (st != ST_OK) {
                free(line);
                return st;
            }
            (*ok_count)++;
        } else if (st == ST_ERR_SYNTAX) {
            if (fprintf(out, "Строка %lu: %s\nОшибка синтаксиса в позиции %lu\n\n",
                        (unsigned long)line_no, line, (unsigned long)err_pos) < 0) {
                free(line);
                return ST_ERR_FILE_IO;
            }
            (*bad_count)++;
        } else {
            free(line);
            return st;
        }
        free(line);
    }
}

int main(int argc, char *argv[])
{
    FILE *in;
    FILE *out;
    size_t ok_count = 0;
    size_t bad_count = 0;
    Status st;

    if (argc != 3) {
        printf("Использование: %s <входной файл> <выходной файл>\n",
               (argc > 0 && argv != NULL && argv[0] != NULL) ? argv[0] : "task10");
        return 1;
    }
    if (strcmp(argv[1], argv[2]) == 0) {
        printf("Ошибка: входной и выходной файлы должны различаться\n");
        return 1;
    }

    in = fopen(argv[1], "r");
    if (in == NULL) {
        printf("Ошибка: %s (%s)\n", status_message(ST_ERR_FILE_OPEN), argv[1]);
        return 1;
    }
    out = fopen(argv[2], "w");
    if (out == NULL) {
        printf("Ошибка: %s (%s)\n", status_message(ST_ERR_FILE_OPEN), argv[2]);
        fclose(in);
        return 1;
    }

    st = process_file(in, out, &ok_count, &bad_count);

    fclose(in);
    if (fclose(out) != 0 && st == ST_OK) {
        st = ST_ERR_FILE_IO;
    }
    if (st != ST_OK) {
        printf("Ошибка: %s\n", status_message(st));
        return 1;
    }
    printf("Готово. Построено деревьев: %lu, строк с ошибками: %lu\n",
           (unsigned long)ok_count, (unsigned long)bad_count);
    return 0;
}