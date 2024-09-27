#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <endian.h>

#if !defined(__BYTE_ORDER) || __BYTE_ORDER != __LITTLE_ENDIAN
#error unsupported endian
#endif

#define NSTRING_LITERAL(s) {.str = (s), .len = sizeof(s) - 1}
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*(arr)))
#define NSTRING_CMP_LITERAL(lit, charstar, charstar_len) ((sizeof(lit) - 1) == (charstar_len) && 0 == memcmp((lit), (charstar), sizeof(lit) - 1))

typedef int32_t i32;

typedef enum {
    OPCODE_BUILTIN_WORD = 0,
    OPCODE_DEFINED_WORD,
    OPCODE_RUNTIME_WORD,
    OPCODE_LITERAL,
    OPCODE_PRINT_CONST_STRING,
    OPCODE_EXIT_WORD
} opcode_t;

typedef struct {
    i32 len;
    i32 capacity;
    i32 memb_size;
    max_align_t data[];
} grow_array_t;

typedef struct {
    const char * str;
    i32 len;
} nstring_t;

static const nstring_t builtins[] = {
    NSTRING_LITERAL("dup"),
    NSTRING_LITERAL("-"),
    NSTRING_LITERAL(">"),
    NSTRING_LITERAL("+"),
    NSTRING_LITERAL("="),
    NSTRING_LITERAL("over"),
    NSTRING_LITERAL("mod"),
    NSTRING_LITERAL("drop"),
    NSTRING_LITERAL("."),
    NSTRING_LITERAL("cr"),
    NSTRING_LITERAL("swap"),
    NSTRING_LITERAL("*")
};

static void * grow_array_create(i32 memb_size)
{
    grow_array_t * arr = malloc(sizeof(grow_array_t) + 4 * memb_size);
    assert(arr);
    arr->len = 0;
    arr->capacity = 4;
    arr->memb_size = memb_size;
    return arr->data;
}

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

static i32 grow_array_get_len(const void * v_arr)
{
    const grow_array_t * arr = v_arr - offsetof(grow_array_t, data);
    return arr->len;
}

static void grow_array_destroy(void * v_arr)
{
    grow_array_t * arr = v_arr - offsetof(grow_array_t, data);
    free(arr);
}


static bool next_word(char ** cur, const char * end, char ** w_res, i32 * len_res)
{
    char * p = *cur;

    while(1) {
        if(p == end) {
            return false;
        }
        char c = *p;
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p++;
            continue;
        }
        break;
    }

    *w_res = p;
    i32 len = 1;

    p++;
    while(1) {
        if(p == end) {
            break;
        }
        char c = *p;
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            break;
        }
        p++;
        len++;
    }
    *cur = p;
    *len_res = len;
    return true;
}


static i32 find_nstring(const nstring_t * nstrings, i32 nstrings_len, const char * match, i32 match_len)
{
    for(i32 i=0; i<nstrings_len; i++) {
        if(nstrings[i].len == match_len && 0 == memcmp(nstrings[i].str, match, match_len)) {
            return i;
        }
    }
    return -1;
}


static bool parse_literal(i32 * literal_res, const char * word, i32 word_len)
{
    i32 i = 0;
    bool negative = false;
    if(word_len && word[0] == '-') {
        negative = true;
        i ++;
    }
    int64_t num = 0;
    bool valid = false;
    for( ; i < word_len; i++) {
        if(word[i] < '0' || word[i] > '9') {
            return false;
        }
        num *= 10ll;
        int64_t new_digit = word[i] - '0';
        num += new_digit;
        if(num > ((int64_t) UINT32_MAX)) {
            return false;
        }
        valid = true;
    }
    if(!valid) {
        return false;
    }
    if(negative) {
        num = -num;
        if(num < ((int64_t) INT32_MIN)) {
            return false;
        }
    }
    else {
        if(num > ((int64_t) INT32_MAX)) {
            union {i32 i; uint32_t u;} iu;
            iu.u = num;
            *literal_res = iu.i;
            return true;
        }
    }
    *literal_res = num;
    return true;
}


int main(int argc, char ** argv)
{
    assert(argc == 2);

    int res;

    int fd = open(argv[1], O_RDONLY);
    assert(fd != -1);
    int len = lseek(fd, 0, SEEK_END);
    assert(len != -1);
    fprintf(stderr, "source code is %d bytes long\n", len);
    res = lseek(fd, 0, SEEK_SET);
    assert(res != -1);

    char * program = malloc(len);
    assert(program);

    res = read(fd, program, len);
    assert(res == len);

    res = close(fd);
    assert(res != -1);


    char * cur = program;
    char * end = program + len;
    char * word;
    i32 word_len;

    i32 * ga_main_output_program = grow_array_create(sizeof(i32));
    i32 * ga_defined_word_output_programs = grow_array_create(sizeof(i32));
    i32 ** gap_current_output_program = &ga_main_output_program;
    i32 * ga_defined_word_output_program_offsets = grow_array_create(sizeof(i32));

    i32 defining_word_i = -1;
    nstring_t * ga_defined_words = grow_array_create(sizeof(nstring_t));

    nstring_t * ga_runtime_words = grow_array_create(sizeof(nstring_t));
    i32 runtime_words_serialized_len = 0;

    nstring_t * ga_const_strings = grow_array_create(sizeof(nstring_t));
    i32 const_string_offset = 0;

    while(1) {
        if(!next_word(&cur, end, &word, &word_len)) {
            break;
        }

        /* builtin words */
        i32 builtin_i = find_nstring(builtins, ARRAY_LEN(builtins), word, word_len);
        if(builtin_i != -1) {
            i32 opcode = OPCODE_BUILTIN_WORD;
            grow_array_add(gap_current_output_program, &opcode);
            grow_array_add(gap_current_output_program, &builtin_i);
            continue;
        }

        /* defined words */
        i32 defined_word_i = find_nstring(ga_defined_words, grow_array_get_len(ga_defined_words), word, word_len);
        if(defined_word_i != -1) {
            i32 opcode = OPCODE_DEFINED_WORD;
            grow_array_add(gap_current_output_program, &opcode);
            grow_array_add(gap_current_output_program, &defined_word_i);
            continue;
        }

        /* intrinsic words */
        bool intrinsic_found = true;
        if(NSTRING_CMP_LITERAL(":", word, word_len)) {
            assert(defining_word_i == -1);
            char * defining_word;
            i32 defining_word_len;
            assert(next_word(&cur, end, &defining_word, &defining_word_len));
            assert(-1 == find_nstring(ga_defined_words, grow_array_get_len(ga_defined_words), defining_word, defining_word_len));
            assert(-1 == find_nstring(ga_runtime_words, grow_array_get_len(ga_runtime_words), defining_word, defining_word_len));
            defining_word_i = grow_array_get_len(ga_defined_words);
            nstring_t new_defined_word = {.str=defining_word, .len=defining_word_len};
            grow_array_add(&ga_defined_words, &new_defined_word);
            gap_current_output_program = &ga_defined_word_output_programs;
            i32 program_offset = grow_array_get_len(ga_defined_word_output_programs);
            grow_array_add(&ga_defined_word_output_program_offsets, &program_offset);
        }
        else if(NSTRING_CMP_LITERAL(";", word, word_len)) {
            assert(defining_word_i != -1);
            defining_word_i = -1;
            i32 opcode = OPCODE_EXIT_WORD;
            grow_array_add(gap_current_output_program, &opcode);
            gap_current_output_program = &ga_main_output_program;
        }
        else if(NSTRING_CMP_LITERAL("(", word, word_len)) {
            do {
                assert(next_word(&cur, end, &word, &word_len));
            } while(word[word_len - 1] != ')');
        }
        else if(NSTRING_CMP_LITERAL(".\"", word, word_len)) {
            i32 opcode = OPCODE_PRINT_CONST_STRING;
            grow_array_add(gap_current_output_program, &opcode);
            grow_array_add(gap_current_output_program, &const_string_offset);
            char * string_part;
            i32 string_part_len;
            do {
                assert(next_word(&cur, end, &string_part, &string_part_len));
            } while(string_part[string_part_len - 1] != '"');
            char * string_start = word + 3; /* skip the '." ' */
            i32 string_len = string_part - string_start;
            string_len += string_part_len - 1; /* Exclude the end '"' */
            const_string_offset += string_len + 1; /* Include room for a '\0' */
            nstring_t new_const_string = {.str=string_start, .len=string_len};
            grow_array_add(&ga_const_strings, &new_const_string);
        }
        else if(NSTRING_CMP_LITERAL("recurse", word, word_len)) {
            assert(defining_word_i != -1);
            i32 opcode = OPCODE_DEFINED_WORD;
            grow_array_add(gap_current_output_program, &opcode);
            grow_array_add(gap_current_output_program, &defining_word_i);
        }
        else {
            i32 literal;
            if(parse_literal(&literal, word, word_len)) {
                i32 opcode = OPCODE_LITERAL;
                grow_array_add(gap_current_output_program, &opcode);
                grow_array_add(gap_current_output_program, &literal);
            }
            else {
                intrinsic_found = false;
            }
        }
        if(intrinsic_found) {
            continue;
        }

        /* runtime words */
        i32 runtime_word_i = find_nstring(ga_runtime_words, grow_array_get_len(ga_runtime_words), word, word_len);
        if(runtime_word_i == -1) {
            runtime_word_i = grow_array_get_len(ga_runtime_words);
            nstring_t new_runtime_word = {.str=word, .len=word_len};
            grow_array_add(&ga_runtime_words, &new_runtime_word);
            runtime_words_serialized_len += word_len + 1; /* for the '\0' in the serialized output */
        }
        i32 opcode = OPCODE_RUNTIME_WORD;
        grow_array_add(gap_current_output_program, &opcode);
        grow_array_add(gap_current_output_program, &runtime_word_i);
    }
    assert(defining_word_i == -1);

    fwrite("<\0\0\0", 1, 4, stdout);

    i32 dump_len = grow_array_get_len(ga_main_output_program);
    fwrite(&dump_len, sizeof(i32), 1, stdout);
    fwrite(ga_main_output_program, sizeof(i32), grow_array_get_len(ga_main_output_program), stdout);

    dump_len = grow_array_get_len(ga_defined_word_output_programs);
    fwrite(&dump_len, sizeof(i32), 1, stdout);
    fwrite(ga_defined_word_output_programs, sizeof(i32), grow_array_get_len(ga_defined_word_output_programs), stdout);

    dump_len = grow_array_get_len(ga_defined_word_output_program_offsets);
    fwrite(&dump_len, sizeof(i32), 1, stdout);
    fwrite(ga_defined_word_output_program_offsets, sizeof(i32), grow_array_get_len(ga_defined_word_output_program_offsets), stdout);
    
    i32 arr_len = grow_array_get_len(ga_const_strings);
    fwrite(&const_string_offset, sizeof(i32), 1, stdout);
    fwrite(&runtime_words_serialized_len, sizeof(i32), 1, stdout);
    for(i32 i = 0; i < arr_len; i++) {
        fwrite(ga_const_strings[i].str, 1, ga_const_strings[i].len, stdout);
        fwrite("\0", 1, 1, stdout);
    }
    arr_len = grow_array_get_len(ga_runtime_words);
    for(i32 i = 0; i < arr_len; i++) {
        fwrite(ga_runtime_words[i].str, 1, ga_runtime_words[i].len, stdout);
        fwrite("\0", 1, 1, stdout);
    }

    fprintf(stderr, "defined words:\n");
    arr_len = grow_array_get_len(ga_defined_words);
    for(i32 i = 0; i < arr_len; i++) {
        fprintf(stderr, "  %.*s\n", ga_defined_words[i].len, ga_defined_words[i].str);
    }

    fprintf(stderr, "runtime words:\n");
    arr_len = grow_array_get_len(ga_runtime_words);
    for(i32 i = 0; i < arr_len; i++) {
        fprintf(stderr, "  %.*s\n", ga_runtime_words[i].len, ga_runtime_words[i].str);
    }

    fprintf(stderr, "const strings:\n");
    arr_len = grow_array_get_len(ga_const_strings);
    for(i32 i = 0; i < arr_len; i++) {
        fprintf(stderr, "  %.*s\n", ga_const_strings[i].len, ga_const_strings[i].str);
    }

    grow_array_destroy(ga_main_output_program);
    grow_array_destroy(ga_defined_word_output_programs);
    grow_array_destroy(ga_defined_word_output_program_offsets);
    grow_array_destroy(ga_defined_words);
    grow_array_destroy(ga_runtime_words);
    grow_array_destroy(ga_const_strings);

    free(program);
}

