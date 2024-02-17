#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct Stack {
    int stack[100];
    int strcnt;
    std::map<int, std::string> stringtable;
    int sp;

    void push(int operand) {
        stack[sp++] = operand;
    }

    void pushs(const std::string &s) {
        int cnt = strcnt++;
        stringtable[cnt] = s;
        push(cnt);
    }

    int pop() {
        return stack[--sp];
    }

    std::string pops() {
        return stringtable[stack[--sp]];
    }
};

enum class OPType {
    NOP = 0,
    PUSHI,
    PLUSI,
    PUSHS,
    INTRINSIC
};

enum class IntrinsicType {
    EXIT = 0,
    PUTS
};

struct OP {
    OPType type;
    int operand;
    std::string soperand;
};

#define ENUMI(enum) static_cast<int>(enum)

#define OP_PUSH_I(op) \
    OP { OPType::PUSHI, op }
#define OP_PLUS_I \
    OP { OPType::PLUSI }
#define OP_PUSH_S(sop) \
    OP { OPType::PUSHS, 0, sop }
#define OP_INTRINSIC(intrinsic) \
    OP { OPType::INTRINSIC, ENUMI(intrinsic) }

#define INTRINSIC_EXIT OP_INTRINSIC(IntrinsicType::EXIT)
#define INTRINSIC_PUTS OP_INTRINSIC(IntrinsicType::PUTS)

std::string intrinsicToName(const IntrinsicType &type) {
    switch (type) {
    case IntrinsicType::EXIT:
        return "IntrinsicType::EXIT";
    case IntrinsicType::PUTS:
        return "IntrinsicType::PUTS";
    default:
        return "Unknown intrinsic.";
    }
}

std::string opTypeToName(const OP &op) {
    switch (op.type) {
    case OPType::PUSHI:
        return "OPType::PUSHI";
    case OPType::PLUSI:
        return "OPType::PLUSI";
    case OPType::PUSHS:
        return "OPType::PUSHS";
    case OPType::INTRINSIC:
        return intrinsicToName(static_cast<IntrinsicType>(op.operand));
    default:
        return "";
    }
}

std::vector<OP> program = {};

void parseProgram(const std::string &source) {
    OP current = {};
    int cursor = 0;

    auto parseStr = [&]() {
        int start = ++cursor;
        int end = start;
        std::string str = {};
        while (source[end] != '"') {
            if (source[end] != '\\') {
                str += source[end];
                end++;
            } else {
                switch (source[end + 1]) {
                case 'n':
                    str += 0xA;
                    break;
                case 't':
                    str += 0x09;
                    break;
                default:
                    break;
                }
                end += 2;
            }
        }
        return make_pair(str, end + 1);
    };

    auto tryParseIdent = [&]() {
        int start = cursor;
        int end = start;
        while (std::isalpha(source[end]))
            end++;
        auto str = source.substr(start, end - start);
        return std::make_pair(str, end);
    };

    auto tryParseNumber = [&]() {
        int start = cursor;
        int end = start;
        while (std::isalnum(source[end]))
            end++;
        auto str = source.substr(start, end - start);
        return std::make_pair(str, end);
    };

    auto push_op = [&]() {
        program.emplace_back(current);
        printf("> Pushing op:\n");
        printf("    - Type: %s\n", opTypeToName(current).c_str());
        printf("    - Operand: %d\n", current.operand);
        printf("    - Str Operand: %s\n", current.soperand.c_str());
        current = {};
        cursor++;
    };

    while (cursor < source.length()) {
        switch (source[cursor]) {
        case ';':
            push_op();
            break;
        case '\n':
        case ' ':
            cursor++;
            break;
        case '"': {
            auto a = parseStr();
            current.soperand = a.first;
            cursor = a.second;
        } break;
        default:
            if (isalpha(source[cursor])) {
                auto res = tryParseIdent();

                if (res.first == "pushs") {
                    current.type = OPType::PUSHS;
                } else if (res.first == "puts") {
                    current = INTRINSIC_PUTS;
                } else if (res.first == "pushi") {
                    current.type = OPType::PUSHI;
                } else if (res.first == "plusi") {
                    current.type = OPType::PLUSI;
                } else if (res.first == "exit") {
                    current = INTRINSIC_EXIT;
                }

                cursor = res.second;
            } else if (isalnum(source[cursor])) {
                auto res = tryParseNumber();
                current.operand = std::stoi(res.first);
                cursor = res.second;
            }
            break;
        }
    }
}

void genQBEIR() {
    std::string data;
    std::string main;

    main.append("export function w $main() {\n");
    main.append("@start\n");

    Stack stack = {};

    int strCnt = 0;

    for (auto op : program) {
        switch (op.type) {
        case OPType::PUSHI:
            stack.push(op.operand);
            break;
        case OPType::PLUSI: {
            auto a = stack.pop();
            auto b = stack.pop();
            stack.push(a + b);
        } break;
        case OPType::PUSHS:
            stack.pushs(op.soperand);
            break;
        case OPType::INTRINSIC: {
            switch (op.operand) {
            case ENUMI(IntrinsicType::PUTS): {
                auto s = stack.pops();
                data.append("data $str").append(std::to_string(strCnt)).append(" = { b  \"").append(s).append("\", b 0}\n");
                main.append("   %r =w call $puts(l $str").append(std::to_string(strCnt++)).append(")\n");
            } break;
            case ENUMI(IntrinsicType::EXIT): {
                auto e = stack.pop();
                main.append("   %r =w call $exit(l ").append(std::to_string(e)).append(")\n");
            } break;
            default:
                printf("Not implemented yet: %s\n", opTypeToName(op).c_str());
                exit(1);
                break;
            }
        } break;
        default:
            printf("Not implemented yet: %s\n", opTypeToName(op).c_str());
            exit(1);
            break;
        }
    }

    main.append("   ret 0\n");
    main.append("}\n");

    data.append("\n").append(main);

    std::ofstream out("./build/output.ssa");
    out << data;
    out.close();
}

int main(int argc, char **argv) {

    (void)*argv++;
    if (argc > 1) {
        auto path = *argv++;

        std::ifstream source;
        source.open(path);
        std::stringstream sourcestream;
        sourcestream << source.rdbuf();
        source.close();
        parseProgram(sourcestream.str());
        genQBEIR();
    }

    return 0;
}