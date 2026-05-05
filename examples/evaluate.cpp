/* This example evaluates a trace in a modular field using the
 * ratbox.h API.
 *
 * Compile with:
 *   g++ -o evaluate evaluate.cpp \
 *       -O3 -std=c++17 -fopenmp -Ibuild/include -Lbuild/lib \
 *       -lfirefly -lflint -lmpfr -lgmp -lz -ljemalloc
 */

#include "ratracer.h"
#include "ratbox.h"

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s tracefile\n", argv[0]);
        return 1;
    }
    // Load the trace.
    Trace t = tr_init();
    if (tr_mergeimport(t, argv[1]) != 0) return 1;
    // Prepare memory buffers for inputs, outpus, and temporary
    // data. Ratracer will not allocate during evaluation.
    ncoef_t *in = (ncoef_t *)safe_malloc(t.ninputs * sizeof(ncoef_t));
    ncoef_t *out = (ncoef_t *)safe_malloc(t.noutputs * sizeof(ncoef_t));
    ncoef_t *tmp_data = (ncoef_t *)safe_malloc(t.nextloc * sizeof(ncoef_t));
    uint8_t *tmp_pagebuf =
        (uint8_t *)safe_memalign(CODE_BUFALIGN, CODE_PAGESIZE + CODE_PAGELUFT);
    // Setup the modular field.
    nmod_t mod;
    nmod_init(&mod, 0x7FFFFFFFFFFFFFE7ull);
    printf("Modulus: %llu\n", (unsigned long long)mod.n);
    // Set the input values.
    printf("Inputs:\n");
    for (size_t i = 0; i < t.ninputs; i++) {
        in[i] = ncoef_hash(i, mod.n);
        printf("%zu) '%s' = %llu\n",
               i,
               t.input_names[i].c_str(),
               (unsigned long long)in[i]);
    }
    // Evaluate.
    //
    // One can call this function from multiple threads, if
    // multiple evaluations are needed, but each threads will
    // need an independent tmp_data and tmp_pagebuf.
    //
    // Note that this evaluation keeps the trace on disk, only
    // loading one code page at a time. This has overhead, but
    // keeps the memory usage independent of the trace size.
    if (tr_evaluate(t, in, out, tmp_data, mod, tmp_pagebuf) != 0) return 1;
    // Report the results.
    printf("Outputs:\n");
    for (size_t i = 0; i < t.noutputs; i++) {
        printf("%zu) '%s' = %llu\n",
               i,
               t.output_names[i].c_str(),
               (unsigned long long)out[i]);
    }
    // Cleanup the allocated memory.
    free(in);
    free(out);
    free(tmp_data);
    free(tmp_pagebuf);
    // Close and cleanup the opened trace. This is important,
    // because opened traces are copied to the temporary storage
    // (for e.g. uncompressing). This will remove the temporary
    // files.
    tr_clear(t);
    return 0;
}
