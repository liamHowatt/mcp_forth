#include "mcp_forth.h"
#include <stdlib.h>

#define UNKNOWN_ERROR                                          -1
#define NOT_ALLOWED_INSIDE_WORD_ERROR                          -2
#define EARLY_END_OF_SOURCE_ERROR                              -3
#define WORD_REDEFINED_ERROR                                   -4
#define WORD_USED_BEFORE_DEFINED_ERROR                         -5
#define UNEXPECTED_SEMICOLON_ERROR                             -6
#define NOT_ALLOWED_OUTSIDE_WORD_ERROR                         -7
#define VARIABLE_OR_CONSTANT_REDEFINED_ERROR                   -8
#define WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR           -9
#define UNTERMINATED_CONTROL_FLOW_ERROR                       -10
#define UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR              -11
#define LOOP_INDEX_USED_OUTSIDE_LOOP                          -12
#define TICK_OPERATOR_COULD_NOT_FIND_DEFINED_WORD_ERROR       -13
#define INVALID_DEFINED_WORD_PARENTHESIS_FOR_C_CALLBACK_ERROR -14

#define NSTRING_LITERAL(lit) {.str = (lit), .len = sizeof(lit) - 1}
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*(arr)))
#define EQUAL_STRING_LITERAL(lit, charstar, charstar_len, case_sensitive) ((sizeof(lit) - 1) == (charstar_len) && equal_string((lit), (charstar), sizeof(lit) - 1, case_sensitive))
#define FIND_NSTRING_LITERAL(nstrings, nstrings_len, lit, case_sensitive) find_nstring(nstrings, nstrings_len, lit, sizeof(lit) - 1, case_sensitive)
#define RASSERT(expr, ecode) do { if(!(expr)) { ret=(ecode); goto end; } } while(0)

typedef struct {
    const char * cur;
    const char * end;
    const char * word;
    int word_len;
} source_scanner_t;

typedef struct {
    int len;
    int capacity;
    int memb_size;
    max_align_t data[];
} grow_array_t;

typedef struct {
    const char * str;
    int len;
} nstring_t;

typedef struct {
    enum {
        CONTROL_FLOW_TYPE_IF,
        CONTROL_FLOW_TYPE_ELSE,
        CONTROL_FLOW_TYPE_DO,
        CONTROL_FLOW_TYPE_BEGIN,
        CONTROL_FLOW_TYPE_WHILE
    } type;
    int fragment;
} control_flow_t;

static const nstring_t builtins[] = {
    NSTRING_LITERAL("dup"),
    NSTRING_LITERAL("-"),
    NSTRING_LITERAL(">"),
    NSTRING_LITERAL("+"),
    NSTRING_LITERAL("="),
    NSTRING_LITERAL("over"),
    NSTRING_LITERAL("mod"),
    NSTRING_LITERAL("drop"),
    NSTRING_LITERAL("swap"),
    NSTRING_LITERAL("*"),
    NSTRING_LITERAL("allot"),
    NSTRING_LITERAL("1+"),
    NSTRING_LITERAL("invert"),
    NSTRING_LITERAL("0="),
    NSTRING_LITERAL("i"),
    NSTRING_LITERAL("j"),
    NSTRING_LITERAL("1-"),
    NSTRING_LITERAL("rot"),
    NSTRING_LITERAL("pick"),
    NSTRING_LITERAL("xor"),
    NSTRING_LITERAL("lshift"),
    NSTRING_LITERAL("rshift"),
    NSTRING_LITERAL("<"),
    NSTRING_LITERAL(">="),
    NSTRING_LITERAL("<="),
    NSTRING_LITERAL("or"),
    NSTRING_LITERAL("and"),
    NSTRING_LITERAL("depth"),
    NSTRING_LITERAL("c!"),
    NSTRING_LITERAL("c@"),
    NSTRING_LITERAL("0<"),
    NSTRING_LITERAL("!"),
    NSTRING_LITERAL("@"),
    NSTRING_LITERAL("c,"),
    NSTRING_LITERAL("here"),
    NSTRING_LITERAL("max"),
    NSTRING_LITERAL("fill"),
    NSTRING_LITERAL("2drop"),
    NSTRING_LITERAL("move"),
    NSTRING_LITERAL("execute"),
    NSTRING_LITERAL("align"),
    NSTRING_LITERAL(","),
    NSTRING_LITERAL("compare"),
    NSTRING_LITERAL("<>"),
    NSTRING_LITERAL(">r"),
    NSTRING_LITERAL("r>"),
    NSTRING_LITERAL("2>r"),
    NSTRING_LITERAL("2r>"),
    NSTRING_LITERAL("2/"),
    NSTRING_LITERAL("2dup"),
    NSTRING_LITERAL("0<>"),
    NSTRING_LITERAL("0>"),
    NSTRING_LITERAL("w@"),
    NSTRING_LITERAL("w!"),
    NSTRING_LITERAL("+!"),
    NSTRING_LITERAL("/"),
    NSTRING_LITERAL("cells"),
    NSTRING_LITERAL("nip"),
};

static bool next_word(source_scanner_t * ss)
{
    const char * p = ss->cur;

    while(1) {
        if(p == ss->end) {
            return false;
        }
        char c = *p;
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p++;
            continue;
        }
        break;
    }

    ss->word = p;
    int len = 1;

    p++;
    while(1) {
        if(p == ss->end) {
            break;
        }
        char c = *p;
        p++;
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            break;
        }
        len++;
    }
    ss->cur = p;
    ss->word_len = len;
    return true;
}

static bool next_line(source_scanner_t * ss)
{
    while(ss->cur != ss->end && *ss->cur != '\n') ss->cur++;
    if(ss->cur == ss->end) return false;
    ss->cur++;
    return true;
}

static void * grow_array_create(int memb_size)
{
    grow_array_t * arr = malloc(sizeof(grow_array_t) + 4 * memb_size);
    assert(arr);
    arr->len = 0;
    arr->capacity = 4;
    arr->memb_size = memb_size;
    return arr->data;
}

// static void grow_array_reserve(void * v_arr_p, int extra)
// {
//     void ** v_arr_p2 = v_arr_p;
//     grow_array_t * arr = *v_arr_p2 - offsetof(grow_array_t, data);

//     int new_len = arr->len + extra;
//     if(new_len < arr->capacity) {
//         do {
//             arr->capacity += arr->capacity / 2;
//         } while(new_len < arr->capacity);

//         arr = realloc(arr, sizeof(grow_array_t) + arr->capacity * arr->memb_size);
//         assert(arr);
//         *v_arr_p2 = arr->data;
//     }
// }

// static void grow_array_add(void * v_arr_p, const void * to_add)
// {
//     grow_array_reserve(v_arr_p, 1);

//     void ** v_arr_p2 = v_arr_p;
//     grow_array_t * arr = *v_arr_p2 - offsetof(grow_array_t, data);

//     void * data = arr->data;
//     memcpy(data + arr->len * arr->memb_size, to_add, arr->memb_size);

//     arr->len ++;
// }

static void grow_array_add(void * v_arr_p, const void * to_add)
{
    void ** v_arr_p2 = v_arr_p;

    grow_array_t * arr = *v_arr_p2 - offsetof(grow_array_t, data);

    if(arr->len == arr->capacity) {
        arr->capacity += arr->capacity / 2;
        arr = realloc(arr, sizeof(grow_array_t) + arr->capacity * arr->memb_size);
        assert(arr);
        *v_arr_p2 = arr->data;
    }

    void * data = arr->data;
    memcpy(data + arr->len * arr->memb_size, to_add, arr->memb_size);

    arr->len ++;
}

static int grow_array_get_len(const void * v_arr)
{
    const grow_array_t * arr = v_arr - offsetof(grow_array_t, data);
    return arr->len;
}

// static void grow_array_unsafe_set_len(void * v_arr, int len)
// {
//     grow_array_t * arr = v_arr - offsetof(grow_array_t, data);
//     arr->len = len;
// }

static void grow_array_drop_end(void * v_arr)
{
    grow_array_t * arr = v_arr - offsetof(grow_array_t, data);
    arr->len--;
}

static void grow_array_destroy(void * v_arr)
{
    grow_array_t * arr = v_arr - offsetof(grow_array_t, data);
    free(arr);
}

static bool equal_string(const char * s1, const char * s2, int len, bool case_sensitive) {
    if(case_sensitive) {
        return 0 == memcmp(s1, s2, len);
    } else {
        for(int i=0; i<len; i++) {
            char c1 = s1[i];
            if(c1 >= 'A' && c1 <= 'Z') c1 += 32;
            char c2 = s2[i];
            if(c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if(c1 != c2) return false;
        }
        return true;
    }
}

static int find_nstring(const nstring_t * nstrings, int nstrings_len, const char * match, int match_len, bool case_sensitive)
{
    for(int i=0; i<nstrings_len; i++) {
        if(nstrings[i].len == match_len && equal_string(nstrings[i].str, match, match_len, case_sensitive)) {
            return i;
        }
    }
    return -1;
}

static int create_fragment(m4_fragment_t ** all_fragments, m4_opcode_t op, int param, bool is_word_fragment)
{
    m4_fragment_t fragment = {
        .param = param,
        .position = 0, /* initial guess (bad) */
        .op = op,
        .is_word_fragment = is_word_fragment
    };
    grow_array_add(all_fragments, &fragment);
    return grow_array_get_len(*all_fragments) - 1;
}

static int sequence_helper(m4_fragment_t ** all_fragments, int ** sequence, m4_opcode_t op, int param, bool is_word_fragment)
{
    int fragment = create_fragment(all_fragments, op, param, is_word_fragment);
    grow_array_add(sequence, &fragment);
    return fragment;
}

static bool parse_literal(int * literal_out, const char * word, int word_len)
{
    int i = 0;
    bool negative = false;
    if(word_len && word[0] == '-') {
        negative = true;
        i ++;
    }
    if(i >= word_len) {
        return false;
    }
    long long num = 0;
    /* hex */
    if(word[i] == '0' && i + 1 < word_len && word[i + 1] == 'x') {
        i += 2;
        if(i >= word_len) {
            return false;
        }
        for( ; i < word_len; i++) {
            long long new_digit;
            if(word[i] >= '0' && word[i] <= '9') {
                new_digit = word[i] - '0';
            }
            else if(word[i] >= 'a' && word[i] <= 'f') {
                new_digit = word[i] - 'a' + 10;
            }
            else if(word[i] >= 'A' && word[i] <= 'F') {
                new_digit = word[i] - 'A' + 10;
            }
            else {
                return false;
            }
            num *= 16ll;
            num += new_digit;
            if(num > ((long long) UINT_MAX)) {
                return false;
            }
        }
    }
    /* decimal */
    else {
        for( ; i < word_len; i++) {
            if(word[i] < '0' || word[i] > '9') {
                return false;
            }
            num *= 10ll;
            long long new_digit = word[i] - '0';
            num += new_digit;
            if(num > ((long long) UINT_MAX)) {
                return false;
            }
        }
    }
    if(negative) {
        num = -num;
        if(num < ((long long) INT_MIN)) {
            return false;
        }
        *literal_out = num;
    }
    else {
        if(num > ((long long) INT_MAX)) {
            union {int i; unsigned u;} iu;
            iu.u = num;
            *literal_out = iu.i;
        } else {
            *literal_out = num;
        }
    }
    return true;
}

int m4_compile(
    const char * source,
    int source_len,
    uint8_t ** bin_out,
    const m4_backend_t * backend,
    int * error_near
) {
    int arr_len;

    int ret = UNKNOWN_ERROR;
    m4_fragment_t * all_fragments = grow_array_create(sizeof(m4_fragment_t));
    int * sequence = grow_array_create(sizeof(int));
    nstring_t * defined_word_nstrings = grow_array_create(sizeof(nstring_t));
    int * defined_word_fragments = grow_array_create(sizeof(int));
    nstring_t * runtime_word_nstrings = grow_array_create(sizeof(nstring_t));
    nstring_t * data_nstrings = grow_array_create(sizeof(nstring_t));
    nstring_t * variable_nstrings = grow_array_create(sizeof(nstring_t));
    control_flow_t * control_flow_stack = grow_array_create(sizeof(control_flow_t));
    int * defined_words_that_need_callbacks = grow_array_create(sizeof(int));

    source_scanner_t ss = {.cur=source, .end=source+source_len};

    bool defining_word = false;

    /* process source */
    while(next_word(&ss)) {
        int word_i;

        /* builtin words */
        word_i = find_nstring(builtins, ARRAY_LEN(builtins), ss.word, ss.word_len, false);
        if(word_i != -1) {
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_BUILTIN_WORD, word_i, defining_word);

            /* special check for i and j */
            if(EQUAL_STRING_LITERAL("i", builtins[word_i].str, builtins[word_i].len, false)
               || EQUAL_STRING_LITERAL("j", builtins[word_i].str, builtins[word_i].len, false)) {
                int required_depth = builtins[word_i].str[0] == 'i' || builtins[word_i].str[0] == 'I'
                                     ? 1 : 2;
                arr_len = grow_array_get_len(control_flow_stack);
                for(int i=0; i<arr_len; i++) {
                    if(control_flow_stack[i].type == CONTROL_FLOW_TYPE_DO) required_depth --;
                    if(!required_depth) break;
                }
                if(required_depth) {
                    ret = LOOP_INDEX_USED_OUTSIDE_LOOP;
                    goto end;
                }
            }

            continue;
        }

        /* defined words */
        word_i = find_nstring(defined_word_nstrings, grow_array_get_len(defined_word_nstrings), ss.word, ss.word_len, false);
        if(word_i != -1) {
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_DEFINED_WORD, defined_word_fragments[word_i], defining_word);
            continue;
        }

        /* variables and constants */
        word_i = find_nstring(variable_nstrings, grow_array_get_len(variable_nstrings), ss.word, ss.word_len, false);
        if(word_i != -1) {
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_USE_VARIABLE_OR_CONSTANT, word_i, defining_word);
            continue;
        }

        /* intrinsic words */
        bool intrinsic_found = true;
        if(EQUAL_STRING_LITERAL(":", ss.word, ss.word_len, false)) {
            RASSERT(!defining_word, NOT_ALLOWED_INSIDE_WORD_ERROR);
            RASSERT(next_word(&ss), EARLY_END_OF_SOURCE_ERROR);
            RASSERT(-1 == find_nstring(builtins, ARRAY_LEN(builtins), ss.word, ss.word_len, false), WORD_REDEFINED_ERROR);
            RASSERT(-1 == find_nstring(defined_word_nstrings, grow_array_get_len(defined_word_nstrings), ss.word, ss.word_len, false), WORD_REDEFINED_ERROR);
            RASSERT(-1 == find_nstring(runtime_word_nstrings, grow_array_get_len(runtime_word_nstrings), ss.word, ss.word_len, false), WORD_USED_BEFORE_DEFINED_ERROR);
            RASSERT(-1 == find_nstring(variable_nstrings, grow_array_get_len(variable_nstrings), ss.word, ss.word_len, false), VARIABLE_OR_CONSTANT_REDEFINED_ERROR);
            RASSERT(0 == grow_array_get_len(control_flow_stack), UNTERMINATED_CONTROL_FLOW_ERROR);
            defining_word = true;
            nstring_t new_defined_word = {.str=ss.word, .len=ss.word_len};
            grow_array_add(&defined_word_nstrings, &new_defined_word);
            int fragment = sequence_helper(&all_fragments, &sequence, M4_OPCODE_DEFINED_WORD_LOCATION, 0, defining_word);
            grow_array_add(&defined_word_fragments, &fragment);
        }
        else if(EQUAL_STRING_LITERAL(";", ss.word, ss.word_len, false)) {
            RASSERT(defining_word, UNEXPECTED_SEMICOLON_ERROR);
            RASSERT(0 == grow_array_get_len(control_flow_stack), UNTERMINATED_CONTROL_FLOW_ERROR);
            int defining_word_fragment_i = defined_word_fragments[grow_array_get_len(defined_word_fragments) - 1];
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_EXIT_WORD, defining_word_fragment_i, defining_word);
            defining_word = false;
        }
        else if(EQUAL_STRING_LITERAL("exit", ss.word, ss.word_len, false)) {
            RASSERT(defining_word, NOT_ALLOWED_OUTSIDE_WORD_ERROR);
            int defining_word_fragment_i = defined_word_fragments[grow_array_get_len(defined_word_fragments) - 1];
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_EXIT_WORD, defining_word_fragment_i, defining_word);
            arr_len = grow_array_get_len(control_flow_stack);
            for(int i=0; i<arr_len; i++) {
                if(control_flow_stack[i].type == CONTROL_FLOW_TYPE_DO) {
                    all_fragments[defining_word_fragment_i].param = 1;
                    break;
                }
            }
        }
        else if(EQUAL_STRING_LITERAL("(", ss.word, ss.word_len, false)) {
            if(defining_word && all_fragments[sequence[grow_array_get_len(sequence) - 1]].op == M4_OPCODE_DEFINED_WORD_LOCATION) {
                int param_count = 0;
                int return_count = 0;
                bool divider_found = false;
                do {
                    RASSERT(next_word(&ss), EARLY_END_OF_SOURCE_ERROR);
                    if (EQUAL_STRING_LITERAL("--", ss.word, ss.word_len, false)) {
                        divider_found = true;
                    }
                    else if (ss.word_len > 1 || ss.word[0] != ')') {
                        if(!divider_found) {
                            param_count++;
                        } else {
                            return_count++;
                        }
                    }
                } while(ss.word[ss.word_len - 1] != ')');
                int ext_param = 1 << 2;
                ext_param |= (return_count > 0x03 ? 0x03 : return_count) & 0x03;
                ext_param <<= 5;
                ext_param |= (param_count > 0x1f ? 0x1f : param_count) & 0x1f;
                m4_fragment_t * fragment = &all_fragments[defined_word_fragments[grow_array_get_len(defined_word_fragments) - 1]];
                fragment->ext_param = ext_param;
            }
            else {
                do {
                    RASSERT(next_word(&ss), EARLY_END_OF_SOURCE_ERROR);
                } while(ss.word[ss.word_len - 1] != ')');
            }
        }
        else if(EQUAL_STRING_LITERAL(".\"", ss.word, ss.word_len, false) || EQUAL_STRING_LITERAL("s\"", ss.word, ss.word_len, false)) {
            bool is_dot_quote = ss.word[0] == '.';

            const char * string_start = ss.word + 3; /* skip the '." ' */
            do {
                RASSERT(next_word(&ss), EARLY_END_OF_SOURCE_ERROR);
            } while(ss.word[ss.word_len - 1] != '"');
            int string_len = ss.word - string_start;
            string_len += ss.word_len - 1; /* Exclude the end '"' */

            int data_i = find_nstring(data_nstrings, grow_array_get_len(data_nstrings), string_start, string_len, true);
            if(data_i == -1) {
                data_i = grow_array_get_len(data_nstrings);
                nstring_t data_nstring = {.str=string_start, .len=string_len};
                grow_array_add(&data_nstrings, &data_nstring);
            }
            int data_offset = 0;
            for(int i=0; i<data_i; i++) {
                data_offset += data_nstrings[i].len + 1;
            }

            sequence_helper(&all_fragments, &sequence, M4_OPCODE_PUSH_DATA_ADDRESS, data_offset, defining_word);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_PUSH_LITERAL, string_len, defining_word);

            if(is_dot_quote) {
                int type_i = FIND_NSTRING_LITERAL(runtime_word_nstrings, grow_array_get_len(runtime_word_nstrings), "type", false);
                if(type_i == -1) {
                    type_i = grow_array_get_len(runtime_word_nstrings);
                    nstring_t type_nstring = NSTRING_LITERAL("type");
                    grow_array_add(&runtime_word_nstrings, &type_nstring);
                }

                sequence_helper(&all_fragments, &sequence, M4_OPCODE_RUNTIME_WORD, type_i, defining_word);
            }
        }
        else if(EQUAL_STRING_LITERAL("recurse", ss.word, ss.word_len, false)) {
            RASSERT(defining_word, NOT_ALLOWED_OUTSIDE_WORD_ERROR);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_DEFINED_WORD, defined_word_fragments[grow_array_get_len(defined_word_fragments) - 1], defining_word);
        }
        else if(EQUAL_STRING_LITERAL("variable", ss.word, ss.word_len, false) || EQUAL_STRING_LITERAL("constant", ss.word, ss.word_len, false)) {
            bool is_constant = ss.word[0] == 'c' || ss.word[0] == 'C';
            RASSERT(!defining_word, NOT_ALLOWED_INSIDE_WORD_ERROR);
            RASSERT(next_word(&ss), EARLY_END_OF_SOURCE_ERROR);
            RASSERT(-1 == find_nstring(builtins, ARRAY_LEN(builtins), ss.word, ss.word_len, false), WORD_REDEFINED_ERROR);
            RASSERT(-1 == find_nstring(defined_word_nstrings, grow_array_get_len(defined_word_nstrings), ss.word, ss.word_len, false), WORD_REDEFINED_ERROR);
            RASSERT(-1 == find_nstring(runtime_word_nstrings, grow_array_get_len(runtime_word_nstrings), ss.word, ss.word_len, false), WORD_USED_BEFORE_DEFINED_ERROR);
            RASSERT(-1 == find_nstring(variable_nstrings, grow_array_get_len(variable_nstrings), ss.word, ss.word_len, false), VARIABLE_OR_CONSTANT_REDEFINED_ERROR);
            int variable_i = grow_array_get_len(variable_nstrings);
            nstring_t new_variable = {.str=ss.word, .len=ss.word_len};
            grow_array_add(&variable_nstrings, &new_variable);
            sequence_helper(&all_fragments, &sequence, is_constant ? M4_OPCODE_DECLARE_CONSTANT : M4_OPCODE_DECLARE_VARIABLE,
                            variable_i, defining_word);
        }
        else if(EQUAL_STRING_LITERAL("if", ss.word, ss.word_len, false)) {
            int branch_location_fragment = create_fragment(&all_fragments, M4_OPCODE_BRANCH_LOCATION, -1, defining_word);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH_IF_ZERO, branch_location_fragment, defining_word);
            control_flow_t cf = {.type=CONTROL_FLOW_TYPE_IF, .fragment=branch_location_fragment};
            grow_array_add(&control_flow_stack, &cf);
        }
        else if(EQUAL_STRING_LITERAL("then", ss.word, ss.word_len, false)) {
            arr_len = grow_array_get_len(control_flow_stack);
            RASSERT(arr_len != 0, UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR);
            control_flow_t * top_cf = &control_flow_stack[arr_len - 1];
            RASSERT(top_cf->type == CONTROL_FLOW_TYPE_IF || top_cf->type == CONTROL_FLOW_TYPE_ELSE, WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR);
            grow_array_add(&sequence, &top_cf->fragment);
            grow_array_drop_end(control_flow_stack);
        }
        else if(EQUAL_STRING_LITERAL("else", ss.word, ss.word_len, false)) {
            arr_len = grow_array_get_len(control_flow_stack);
            RASSERT(arr_len != 0, UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR);
            control_flow_t * top_cf = &control_flow_stack[arr_len - 1];
            RASSERT(top_cf->type == CONTROL_FLOW_TYPE_IF, WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR);

            int branch_location_fragment = create_fragment(&all_fragments, M4_OPCODE_BRANCH_LOCATION, -1, defining_word);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH, branch_location_fragment, defining_word);

            grow_array_add(&sequence, &top_cf->fragment);

            top_cf->fragment = branch_location_fragment;
            top_cf->type = CONTROL_FLOW_TYPE_ELSE;
        }
        else if(EQUAL_STRING_LITERAL("do", ss.word, ss.word_len, false)) {
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_DO, -1, defining_word);
            int branch_location_fragment = sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH_LOCATION, -1, defining_word);
            control_flow_t cf = {.type=CONTROL_FLOW_TYPE_DO, .fragment=branch_location_fragment};
            grow_array_add(&control_flow_stack, &cf);
        }
        else if(EQUAL_STRING_LITERAL("loop", ss.word, ss.word_len, false)) {
            arr_len = grow_array_get_len(control_flow_stack);
            RASSERT(arr_len != 0, UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR);
            control_flow_t * top_cf = &control_flow_stack[arr_len - 1];
            RASSERT(top_cf->type == CONTROL_FLOW_TYPE_DO, WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_LOOP, top_cf->fragment, defining_word);
            grow_array_drop_end(control_flow_stack);
        }
        else if(EQUAL_STRING_LITERAL("begin", ss.word, ss.word_len, false)) {
            int branch_location_fragment = sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH_LOCATION, -1, defining_word);
            control_flow_t cf = {.type=CONTROL_FLOW_TYPE_BEGIN, .fragment=branch_location_fragment};
            grow_array_add(&control_flow_stack, &cf);
        }
        else if(EQUAL_STRING_LITERAL("until", ss.word, ss.word_len, false)) {
            arr_len = grow_array_get_len(control_flow_stack);
            RASSERT(arr_len != 0, UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR);
            control_flow_t * top_cf = &control_flow_stack[arr_len - 1];
            RASSERT(top_cf->type == CONTROL_FLOW_TYPE_BEGIN, WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH_IF_ZERO, top_cf->fragment, defining_word);
            grow_array_drop_end(control_flow_stack);
        }
        else if(EQUAL_STRING_LITERAL("while", ss.word, ss.word_len, false)) {
            arr_len = grow_array_get_len(control_flow_stack);
            RASSERT(arr_len != 0, UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR);
            control_flow_t * top_cf = &control_flow_stack[arr_len - 1];
            RASSERT(top_cf->type == CONTROL_FLOW_TYPE_BEGIN, WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR);
            int branch_location_fragment = create_fragment(&all_fragments, M4_OPCODE_BRANCH_LOCATION, -1, defining_word);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH_IF_ZERO, branch_location_fragment, defining_word);
            control_flow_t cf = {.type=CONTROL_FLOW_TYPE_WHILE, .fragment=branch_location_fragment};
            grow_array_add(&control_flow_stack, &cf);
        }
        else if(EQUAL_STRING_LITERAL("repeat", ss.word, ss.word_len, false)) {
            arr_len = grow_array_get_len(control_flow_stack);
            RASSERT(arr_len != 0, UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR);
            assert(arr_len >= 2);
            control_flow_t * while_cf = &control_flow_stack[arr_len - 1];
            RASSERT(while_cf->type == CONTROL_FLOW_TYPE_WHILE, WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR);
            control_flow_t * begin_cf = &control_flow_stack[arr_len - 2];
            assert(begin_cf->type == CONTROL_FLOW_TYPE_BEGIN);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH, begin_cf->fragment, defining_word);
            grow_array_add(&sequence, &while_cf->fragment);
            grow_array_drop_end(control_flow_stack);
            grow_array_drop_end(control_flow_stack);
        }
        else if(EQUAL_STRING_LITERAL("again", ss.word, ss.word_len, false)) {
            arr_len = grow_array_get_len(control_flow_stack);
            RASSERT(arr_len != 0, UNEXPECTED_CONTROL_FLOW_TERMINATOR_ERROR);
            control_flow_t * top_cf = &control_flow_stack[arr_len - 1];
            RASSERT(top_cf->type == CONTROL_FLOW_TYPE_BEGIN, WRONG_TERMINATOR_FOR_THIS_CONTROL_FLOW_ERROR);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_BRANCH, top_cf->fragment, defining_word);
            grow_array_drop_end(control_flow_stack);
        }
        else if(EQUAL_STRING_LITERAL("\\", ss.word, ss.word_len, false)) {
            next_line(&ss);
        }
        else if(EQUAL_STRING_LITERAL("'", ss.word, ss.word_len, false)) {
            RASSERT(next_word(&ss), EARLY_END_OF_SOURCE_ERROR);
            int defined_i = find_nstring(defined_word_nstrings, grow_array_get_len(defined_word_nstrings), ss.word, ss.word_len, false);
            RASSERT(defined_i != -1, TICK_OPERATOR_COULD_NOT_FIND_DEFINED_WORD_ERROR);
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_PUSH_OFFSET_ADDRESS, defined_word_fragments[defined_i], defining_word);
        }
        else if(EQUAL_STRING_LITERAL("c'", ss.word, ss.word_len, false)) {
            /* non-standard "create C callback from defined word" */
            RASSERT(next_word(&ss), EARLY_END_OF_SOURCE_ERROR);
            int defined_word_i = find_nstring(defined_word_nstrings, grow_array_get_len(defined_word_nstrings), ss.word, ss.word_len, false);
            RASSERT(defined_word_i != -1, TICK_OPERATOR_COULD_NOT_FIND_DEFINED_WORD_ERROR);
            int callback_i = -1;
            arr_len = grow_array_get_len(defined_words_that_need_callbacks);
            for (int i = 0; i < arr_len; i++) {
                if(defined_word_i == defined_words_that_need_callbacks[i]) {
                    callback_i = i;
                    break;
                }
            }
            if(callback_i == -1) {
                m4_fragment_t * fragment = &all_fragments[defined_word_fragments[defined_word_i]];
                int ext_param = fragment->ext_param;
                RASSERT(ext_param, INVALID_DEFINED_WORD_PARENTHESIS_FOR_C_CALLBACK_ERROR);
                int n_parameters = ext_param & 0x1f;
                ext_param >>= 5;
                int n_return_vals = ext_param & 0x03;
                RASSERT(n_parameters >= 1 && n_parameters <= 8 && n_return_vals <= 1, INVALID_DEFINED_WORD_PARENTHESIS_FOR_C_CALLBACK_ERROR);
                callback_i = grow_array_get_len(defined_words_that_need_callbacks);
                grow_array_add(&defined_words_that_need_callbacks, &defined_word_i);
            }
            sequence_helper(&all_fragments, &sequence, M4_OPCODE_PUSH_CALLBACK, callback_i, defining_word);
        }
        else if(EQUAL_STRING_LITERAL("unloop", ss.word, ss.word_len, false)) {
        }
        else {
            int literal;
            if(parse_literal(&literal, ss.word, ss.word_len)) {
                sequence_helper(&all_fragments, &sequence, M4_OPCODE_PUSH_LITERAL, literal, defining_word);
            }
            else {
                intrinsic_found = false;
            }
        }
        if(intrinsic_found) {
            continue;
        }

        /* runtime words */
        word_i = find_nstring(runtime_word_nstrings, grow_array_get_len(runtime_word_nstrings), ss.word, ss.word_len, false);
        if(word_i == -1) {
            word_i = grow_array_get_len(runtime_word_nstrings);
            nstring_t runtime_word_nstring = {.str=ss.word, .len=ss.word_len};
            grow_array_add(&runtime_word_nstrings, &runtime_word_nstring);
        }
        sequence_helper(&all_fragments, &sequence, M4_OPCODE_RUNTIME_WORD, word_i, defining_word);
    }

    /* halt */
    RASSERT(!defining_word && 0 == grow_array_get_len(control_flow_stack), EARLY_END_OF_SOURCE_ERROR);
    sequence_helper(&all_fragments, &sequence, M4_OPCODE_HALT, -1, defining_word);

    /* move main program fragments to front */
    arr_len = grow_array_get_len(sequence);
    int main_program_i = 0;
    for(int i=0; i<arr_len; i++) {
        if(!all_fragments[sequence[i]].is_word_fragment) {
            int fragment_from_main_program = sequence[i];
            memmove(&sequence[main_program_i + 1], &sequence[main_program_i], (i - main_program_i) * sizeof(int));
            sequence[main_program_i++] = fragment_from_main_program;
        }
    }

    /* solve fragment lengths and positions */
    bool solved = false;
    while(!solved) {
        solved = true;
        int position = 0;
        arr_len = grow_array_get_len(sequence);
        for(int i=0; i<arr_len; i++) {
            if(all_fragments[sequence[i]].position != position) {
                all_fragments[sequence[i]].position = position;
                solved = false;
            }
            position += backend->fragment_bin_size(all_fragments, sequence, arr_len, i);
        }
    }

    /* dump bin */
    int bin_len = 0;

    bin_len += m4_num_encoded_size_from_int(grow_array_get_len(variable_nstrings));

    arr_len = grow_array_get_len(runtime_word_nstrings);
    bin_len += m4_num_encoded_size_from_int(arr_len);
    int runtime_words_len = 0;
    for(int i=0; i<arr_len; i++) {
        runtime_words_len += runtime_word_nstrings[i].len + 1;
    }
    bin_len += runtime_words_len;

    int data_len = 0;
    arr_len = grow_array_get_len(data_nstrings);
    for(int i=0; i<arr_len; i++) {
        data_len += data_nstrings[i].len + 1;
    }
    bin_len += m4_num_encoded_size_from_int(data_len);
    bin_len += data_len;

    arr_len = grow_array_get_len(defined_words_that_need_callbacks);
    bin_len += m4_num_encoded_size_from_int(arr_len);
    int callback_info_len = 0;
    for(int i=0; i<arr_len; i++) {
        callback_info_len += 1 + m4_num_encoded_size_from_int(all_fragments[defined_word_fragments[defined_words_that_need_callbacks[i]]].position);
    }
    bin_len += callback_info_len;

    arr_len = grow_array_get_len(sequence);
    for(int i=0; i<arr_len; i++) {
        bin_len += backend->fragment_bin_size(all_fragments, sequence, arr_len, i);
    }

    uint8_t * bin = malloc(bin_len);
    assert(bin);
    uint8_t * bin_p = bin;

    arr_len = grow_array_get_len(variable_nstrings);
    m4_num_encode(arr_len, bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);

    arr_len = grow_array_get_len(runtime_word_nstrings);
    m4_num_encode(arr_len, bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);
    for(int i=0; i<arr_len; i++) {
        const char * str = runtime_word_nstrings[i].str;
        int str_len = runtime_word_nstrings[i].len;
        for(int j=0; j<str_len; j++) {
            char c = str[j];
            if(c >= 'A' && c <= 'Z') c += 32;
            *(bin_p++) = c;
        }
        *(bin_p++) = '\0';
    }

    m4_num_encode(data_len, bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);
    arr_len = grow_array_get_len(data_nstrings);
    for(int i=0; i<arr_len; i++) {
        memcpy(bin_p, data_nstrings[i].str, data_nstrings[i].len);
        bin_p += data_nstrings[i].len;
        *(bin_p++) = '\0';
    }

    arr_len = grow_array_get_len(defined_words_that_need_callbacks);
    m4_num_encode(arr_len, bin_p);
    bin_p += m4_num_encoded_size_from_encoded(bin_p);
    for(int i=0; i<arr_len; i++) {
        m4_fragment_t * fragment = &all_fragments[defined_word_fragments[defined_words_that_need_callbacks[i]]];
        int ext_param = fragment->ext_param;
        int params_info = ext_param & 0x1f;
        params_info <<= 1;
        if(ext_param & 0x60) params_info |= 1;
        *(bin_p++) = params_info;
        m4_num_encode(fragment->position, bin_p);
        bin_p += m4_num_encoded_size_from_encoded(bin_p);
    }

    arr_len = grow_array_get_len(sequence);
    for(int i=0; i<arr_len; i++) {
        backend->fragment_bin_dump(all_fragments, sequence, arr_len, i, bin_p);
        bin_p += backend->fragment_bin_size(all_fragments, sequence, arr_len, i);
    }

    *bin_out = bin;
    ret = bin_len;
end:
    if(error_near) *error_near = ss.word - source;

    grow_array_destroy(defined_words_that_need_callbacks);
    grow_array_destroy(control_flow_stack);
    grow_array_destroy(variable_nstrings);
    grow_array_destroy(data_nstrings);
    grow_array_destroy(runtime_word_nstrings);
    grow_array_destroy(defined_word_fragments);
    grow_array_destroy(defined_word_nstrings);
    grow_array_destroy(sequence);
    grow_array_destroy(all_fragments);
    return ret;
}
