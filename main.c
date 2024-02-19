#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "strb.h"
#include "vector.h"

#define MAX_STACK 100

typedef struct {
    char *data;
    int len;
} str;

#define STR(cstr) \
    (str) { .data = cstr, .len = strlen(cstr) }

#define STREQ(str, cstr) strcmp(str.data, cstr) == 0

typedef struct {
    str *data;
    int cnt;
    int cap;
} StringTable;

typedef struct {
    int stack[MAX_STACK];
    StringTable stable;
    int sp;
} Stack;

void stack_push(Stack *s, int operand) {
    s->stack[s->sp++] = operand;
}

int stack_pop(Stack *s) {
    return s->stack[s->sp--];
}

void stack_pushs(Stack *s, str string) {
    VEC_ADD(&s->stable, string);
}

str stack_pops(Stack *s) {
    StringTable *ptr = &s->stable;
    str val = ptr->data[ptr->cnt - 1];
    ptr->cnt--;
    return val;
}

typedef enum {
    NOP = 0,
    PUSHI,
    PLUSI,
    PUSHS,
    INTRINSIC
} OPType;

typedef enum {
    EXIT = 0,
    PUTS
} IntrinsicType;

typedef struct {
    OPType type;
    int operand;
    str soperand;
} OP;

#define OP_PUSH_I(op) \
    (OP) { PUSHI, op, STR("") }
#define OP_PLUS_I \
    (OP) { PLUSI, 0, STR("") }
#define OP_PUSH_S(sop) \
    (OP) { PUSHS, 0, sop }
#define OP_INTRINSIC(intrinsic) \
    (OP) { INTRINSIC, intrinsic, STR("") }

#define INTRINSIC_EXIT OP_INTRINSIC(EXIT)
#define INTRINSIC_PUTS OP_INTRINSIC(PUTS)

char *intrinsicToName(IntrinsicType type) {
    switch (type) {
    case EXIT:
        return "IntrinsicType::EXIT";
    case PUTS:
        return "IntrinsicType::PUTS";
    default:
        return "Unknown intrinsic.";
    }
}

char *opTypeToName(OP op) {
    switch (op.type) {
    case PUSHI:
        return "OPType::PUSHI";
    case PLUSI:
        return "OPType::PLUSI";
    case PUSHS:
        return "OPType::PUSHS";
    case INTRINSIC:
        return intrinsicToName(op.operand);
    default: {
        printf("Unknown %d", op.type);
        return "";
    }
    }
}

typedef struct {
    OP *data;
    int cnt;
    int cap;
} Program;

Program program = {0};

static size_t memTmpPos = 0;
static char *memTmp[10 * 1024] = {0};

void *memTmpAlloc(size_t amt) {
    if (memTmpPos + amt > (10 * 1024))
        return NULL;

    void *memAddr = &memTmp[memTmpPos];
    memTmpPos += amt;
    return memAddr;
}

int parseStr(const char *source, int cursor, str *s) {
    int start = ++cursor;
    int end = start;
    int buflen = 0;
    char buf[1024];
    while (source[end] != '"') {
        if (source[end] != '\\') {
            buf[buflen++] = source[end];
            end++;
        } else {
            switch (source[end + 1]) {
            case 'n':
                buf[buflen++] = 0xA;
                break;
            case 't':
                buf[buflen++] = 0x09;
                break;
            default:
                break;
            }
            end += 2;
        }
    }
    buf[buflen] = 0;

    s->data = memTmpAlloc(sizeof(char) * buflen);
    memcpy(s->data, buf, buflen);
    s->len = strlen(buf);

    return end + 1;
}

int tryParseIdent(const char *source, int cursor, str *s) {
    int start = cursor;
    int end = start;
    while (isalpha(source[end]))
        end++;

    s->data = memTmpAlloc(sizeof(char) * end - start);
    memcpy(s->data, source + start, end - start);
    s->len = end - start;
    return end;
}

int tryParseNumber(const char *source, int cursor, str *s) {
    int start = cursor;
    int end = start;
    while (isalnum(source[end]))
        end++;

    s->data = memTmpAlloc(sizeof(char) * end - start);
    memcpy(s->data, source + start, end - start);
    s->len = end - start;
    return end;
};

void push_op(OP current) {
    VEC_ADD(&program, current);
    current = (OP){0};
}

void parseProgram(str source) {
    OP current = {0};
    int cursor = 0;
    str tmp = {};

    while (cursor < source.len) {
        switch (source.data[cursor]) {
        case ';': {
            push_op(current);
            cursor++;
        } break;
        case '\n':
        case ' ':
            cursor++;
            break;
        case '"': {
            cursor = parseStr(source.data, cursor, &tmp);
            current.soperand = tmp;
        } break;
        default:
            if (isalpha(source.data[cursor])) {
                cursor = tryParseIdent(source.data, cursor, &tmp);

                if (STREQ(tmp, "pushs")) {
                    current.type = PUSHS;
                } else if (STREQ(tmp, "puts")) {
                    current = INTRINSIC_PUTS;
                } else if (STREQ(tmp, "pushi")) {
                    current.type = PUSHI;
                } else if (STREQ(tmp, "plusi")) {
                    current.type = PLUSI;
                } else if (STREQ(tmp, "exit")) {
                    current = INTRINSIC_EXIT;
                } else {
                    printf("Not a valid instruction: %s\n", tmp.data);
                }

            } else if (isalnum(source.data[cursor])) {
                cursor = tryParseNumber(source.data, cursor, &tmp);
                current.operand = atoi(tmp.data);
            }
            break;
        }
    }
}

void genQBEIR() {
    strb data = strb_init(16);
    strb main = strb_init(16);

    strb_append(&main, "export function w $main() {\n");
    strb_append(&main, "@start\n");

    Stack stack = {0};

    char buf[32];
    int strCnt = 0;

    for (int i = 0; i < program.cnt; i++) {
        OP op = program.data[i];
        switch (op.type) {
        case PUSHI:
            stack_push(&stack, op.operand);
            break;
        case PLUSI: {
            int a = stack_pop(&stack);
            int b = stack_pop(&stack);
            stack_push(&stack, a + b);
        } break;
        case PUSHS:
            stack_pushs(&stack, op.soperand);
            break;
        case INTRINSIC: {
            switch (op.operand) {
            case PUTS: {
                str s = stack_pops(&stack);
                strb_append(&data, "data $str");
                snprintf(buf, 32, "%d", strCnt);
                strb_append(&data, buf);
                strb_append(&data, " = { b  \"");
                strb_append(&data, s.data);
                strb_append(&data, "\", b 0}\n");
                strb_append(&main, "   %r =w call $puts(l $str");
                snprintf(buf, 32, "%d", strCnt++);
                strb_append(&main, buf);
                strb_append(&main, ")\n");
            } break;
            case EXIT: {
                int e = stack_pop(&stack);
                strb_append(&main, "   %r =w call $exit(l ");
                snprintf(buf, 32, "%d", e);
                strb_append(&main, buf);
                strb_append(&main, ")\n");
            } break;
            default:
                printf("Not implemented yet: %s\n", opTypeToName(op));
                exit(1);
                break;
            }
        } break;
        default:
            printf("Not implemented yet: %s\n", opTypeToName(op));
            exit(1);
            break;
        }
    }

    strb_append(&main, "   ret 0\n");
    strb_append(&main, "}\n");

    strb_append(&data, "\n");
    strb_append(&data, main.str);

    FILE *out = fopen("./build/output.ssa", "w");
    fwrite(data.str, sizeof(char), data.cnt, out);
    fclose(out);

    strb_free(data);
    strb_free(main);
}

void readFile(const char *path, str *buf) {
    FILE *fd = fopen(path, "r");

    fseek(fd, 0, SEEK_END);
    int n = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    buf->data = memTmpAlloc(sizeof(char) * n);
    fread(buf->data, sizeof(char), n, fd);
    buf->len = n;

    fclose(fd);
}

int main(int argc, char **argv) {

    (void)*argv++;
    if (argc > 1) {
        const char* path = *argv++;

        str source = {0};
        readFile(path, &source);

        parseProgram(source);

        genQBEIR();

        VEC_FREE(program);
    }
    return 0;
}