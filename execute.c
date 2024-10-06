#include "mcp_forth.h"

static_assert(sizeof(int) == sizeof(void *), "expected a 32 bit system");

int mcp_forth_execute(
    const uint8_t * bin,
    int bin_len,
    const runtime_t * runtime,
    void * runtime_ctx,
    const mcp_forth_engine_t * engine,
    const char ** missing_word
) {
    int ret = 0;
    runtime_cb * runtime_cbs = NULL;

    const uint8_t * bin_p = bin;

    int bin_runtime_words_len = num_decode(bin_p);
    bin_p += num_encoded_size_from_encoded(bin_p);

    const char * bin_runtime_words = (const char *) bin_p;
    bin_p += bin_runtime_words_len;
    const char * bin_runtime_words_end = (const char *) bin_p;

    int runtime_words_count = 0;
    const char * bin_runtime_words_p = bin_runtime_words;
    while(bin_runtime_words_p != bin_runtime_words_end) {
        runtime_words_count ++;
        bin_runtime_words_p += strlen(bin_runtime_words_p) + 1;
    }

    runtime_cbs = malloc(sizeof(runtime_cb) * runtime_words_count);
    assert(runtime_cbs);
    bin_runtime_words_p = bin_runtime_words;
    for(int i = 0; i < runtime_words_count; i++) {
        bool found = false;
        for(int j = 0; runtime->runtime_cb_array[j].name; j++) {
            if(0 == strcmp(bin_runtime_words_p, runtime->runtime_cb_array[j].name)) {
                runtime_cbs[i] = runtime->runtime_cb_array[j].cb;
                found = true;
                break;
            }
        }
        if(!found) {
            if(missing_word) *missing_word = bin_runtime_words_p;
            ret = RUNTIME_WORD_MISSING_ERROR;
            goto end_label;
        }
        bin_runtime_words_p += strlen(bin_runtime_words_p) + 1;
    }

    int bin_data_len = num_decode(bin_p);
    bin_p += num_encoded_size_from_encoded(bin_p);

    const uint8_t * bin_data = bin_p;
    bin_p += bin_data_len;

    int variable_count = num_decode(bin_p);
    bin_p += num_encoded_size_from_encoded(bin_p);

    const uint8_t * bin_program_start = bin_p;

    ret = engine->run(bin_program_start, bin_data, runtime_cbs, runtime_ctx, variable_count);

end_label:
    free(runtime_cbs);
    return ret;
}
