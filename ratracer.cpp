static const char usagetext[] = R"(
Ss{NAME}
    Rational Tracer Toolbox (Nm{ratracer}) -- a tool for reconstructing
    rational expressions via modular arithmetics.

Ss{SYNOPSYS}
    Nm{ratracer} Cm{command} Ar{args} ... Cm{command} Ar{args} ...

Ss{DESCRIPTION}
    Nm{ratracer} contains tools to simplify complicated rational
    expressions into a normal form. It can simplify
    - arithmetic expressions provided as text files;
    - arbitrary computations provided as trace files;
    - solutions of linear equation systems.

    Nm{ratracer} works by tracing the evaluation of a given expression,
    and replaying the trace using modular arithmetics inside
    a rational reconstruction algorithm. It contains tools to
    record, save, inspect, and optimize the traces.

Ss{EXAMPLE}

    To simplify a single expression:

    |   echo '2*y/(x^2-y^2) + 1/(x+y) + 1/(x-y)' >expression.txt
    |   Nm{ratracer} Cm{trace-expression} expression.txt Cm{reconstruct}
    |   [...]
    |   expression.txt =
    |     (1)/(1/2*x+(-1/2)*y);

    To solve a linear system of equations:

    |   Nm{ratracer} \
    |       Cm{load-equations} equations.list \
    |       Cm{solve-equations} \
    |       Cm{choose-equation-outputs} Fl{--maxr}=7 Fl{--maxs}=1 \
    |       Cm{optimize} \
    |       Cm{finalize} \
    |       Cm{reconstruct}

Ss{COMMANDS}
    Cm{load-trace} Ar{file.trace}
        Load the given trace. Automatically decompress the file
        if the filename ends with '.gz', '.bz2', '.xz', or '.zst'.

    Cm{save-trace} Ar{file.trace}
        Save the current trace to a file. Automatically compress
        the file if the filename ends with '.gz', '.bz2', '.xz',
        or '.zst'.

    Cm{show}
        Show a short summary of the current trace.

    Cm{list-outputs} [Fl{--to}=Ar{filename}]
        Print the full list of outputs of the current trace.

    Cm{stat}
        Collect and show the current code statistics.

    Cm{disasm} [Fl{--to}=Ar{filename}]
        Print a disassembly of the current trace.

    Cm{measure}
        Measure the evaluation speed of the current trace.

    Cm{set} Ar{name} Ar{expression}
        Set the given variable to the given expression in
        the further traces created by Cm{trace-expression},
        Cm{load-equations}, or loaded via Cm{load-trace}.

    Cm{unset} Ar{name}
        Remove the mapping specified by Cm{set}.

    Cm{load-trace} Ar{file.trace}
        Load the given trace.

    Cm{trace-expression} Ar{filename}
        Load a rational expression from a file and trace its
        evaluation.

    Cm{select-output} Ar{index}
        Erase all the outputs aside from the one indicated by
        index. (Numbering starts at 0 here).

    Cm{drop-output} Ar{name} [Fl{--and} Ar{name}] ...
        Erase the given output (or outputs).

    Cm{optimize}
        Optimize the current trace by propagating constants,
        merging duplicate expressions, and erasing dead code.

    Cm{finalize}
        Convert the (not yet finalized) code into a final low-level
        representation that is smaller, and has drastically
        lower memory usage. Automatically eliminate the dead
        code while finalizing.

    Cm{unfinalize}
        The reverse of Cm{finalize}, except that the eliminated
        code is not brought back.

    Cm{reconstruct} [Fl{--to}=Ar{filename}] [Fl{--threads}=Ar{n}] [Fl{--factor-scan}] [Fl{--shift-scan}] [Fl{--inmem}]
        Reconstruct the rational form of the current trace using
        the FireFly library. Optionally enable FireFly's factor
        scan and/or shift scan.

        If the Fl{--inmem} flag is set, load the whole code
        into memory during reconstruction; this increases the
        performance especially with many threads, but comes at
        the price of higher memory usage.

    Cm{reconstruct0} [Fl{--to}=Ar{filename}] [Fl{--inmem}]
        Same as {reconstruct}, but assumes that there are 0 input
        variables needed, and is therefore faster.

    Cm{evaluate}
        Evaluate the trace in terms of rational numbers.

        Note that all the variables must have been previously
        substitited, e.g. using the Cm{set} command.

    Cm{define-family} Ar{name} [Fl{--indices}=Ar{n}]
        Predefine an indexed family with the given number of
        indices used in the equation parsing. This is only needed
        to guarantee the ordering of the families, otherwise
        they are auto-detected from the equation files.

    Cm{load-equations} Ar{file.eqns}
        Load the equations from the given file, tracing the
        expressions.

    Cm{solve-equations}
        Solve all the currently loaded equations by gaussian
        elimination, tracing the process.

        Don't forget to Cm{choose-equation-outputs} after this.

    Cm{choose-equation-outputs} [Fl{--family}=Ar{name}] [Fl{--maxr}=Ar{n}] [Fl{--maxs}=Ar{n}] [Fl{--maxd}=Ar{n}]
        Mark the equations containing the specified integrals
        as the outputs, so they could be later reconstructed.

        This command will fail if the equations are not in the
        fully reduced form (i.e. after Cm{solve-equations}).

    Cm{show-equation-masters} [Fl{--family}=Ar{name}] [Fl{--maxr}=Ar{n}] [Fl{--maxs}=Ar{n}] [Fl{--maxd}=Ar{n}]
        List the unreduced items of the equations filtered by
        the given family/max-r/max-s/max-d values.

    Cm{dump-equations} [Fl{--to}=Ar{filename}]
        Dump the current list of equations with numeric coefficients.
        This should only be needed for debugging.

    Cm{to-series} Ar{varname} Ar{maxorder}
        Re-run the current trace treating each value as a series
        in the given variable, and splitting each output into
        separate outputs per term in the series.

        The given variable is eliminated from the trace as a
        result. The variable mapping is also reset.

    Cm{sh} Ar{command}
        Run the given shell command.

    Cm{help}
        Show this help message and quit.

Ss{AUTHORS}
    Vitaly Magerya <vitaly.magerya@tx97.net>
)";

#include "ratracer.h"
#include "ratbox.h"

#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

static Tracer tr;
static EquationSet the_eqset;
static std::map<size_t, Value> the_varmap;

/* Logging
 */

static int log_depth = 0;
static double log_first_timestamp = timestamp();
static double log_last_timestamp = log_first_timestamp;

typedef struct {
    const char *name;
    double time;
} log_func_info;

static void
logd(const char *fmt, ...)
{
    double now = timestamp();
    fprintf(stderr, "\033[2m%.4f +%.4f%*s \033[0m", now - log_first_timestamp, now - log_last_timestamp, log_depth*2, "");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\033[0m\n");
    log_last_timestamp = now;
}

static void
log_func_start(const log_func_info *i)
{
    logd("\033[1m* %s", i->name);
    log_depth++;
}

static void
log_func_end(const log_func_info *i)
{
    (void)i;
    log_depth--;
}

#define LOGBLOCK(name) \
    __attribute__((cleanup(log_func_end))) log_func_info _log_func_info = {name, timestamp()}; \
    log_func_start(&_log_func_info);

#define LOGME LOGBLOCK(__func__)

/* Commands
 */

static bool
startswith(const char *string, const char *prefix)
{
    for (;;) {
        if (*prefix == 0) return true;
        if (*string == 0) return false;
        if (*string != *prefix) return false;
        string++;
        prefix++;
    }
}

static char *
fmt_bytes(char *buf, size_t bufsize, size_t n)
{
    if (n < 2000) { snprintf(buf, bufsize, "%dB", (int)n); return buf; }
    if (n < 9999) { snprintf(buf, bufsize, "%d.%02dkB", (int)n/1024, (int)(n%1024)*100/1024); return buf; }
    n >>= 10;
    if (n < 2000) { snprintf(buf, bufsize, "%dkB", (int)n); return buf; }
    if (n < 9999) { snprintf(buf, bufsize, "%d.%02dMB", (int)n/1024, (int)(n%1024)*100/1024); return buf; }
    n >>= 10;
    if (n < 2000) { snprintf(buf, bufsize, "%dMB", (int)n); return buf; }
    if (n < 9999) { snprintf(buf, bufsize, "%d.%02dGB", (int)n/1024, (int)(n%1024)*100/1024); return buf; }
    n >>= 10;
    if (n < 2000) { snprintf(buf, bufsize, "%dGB", (int)n); return buf; }
    if (n < 9999) { snprintf(buf, bufsize, "%d.%02dTB", (int)n/1024, (int)(n%1024)*100/1024); return buf; }
    n >>= 10;
    snprintf(buf, bufsize, "%zuTB", n);
    return buf;
}

static int
cmd_show(int argc, char *argv[])
{
    LOGBLOCK("show");
    (void)argc; (void)argv;
    logd("Current trace:");
    logd("- inputs: %zu", tr.t.ninputs);
    for (size_t i = 0; i < tr.t.ninputs; i++) {
        if (i < tr.t.input_names.size()) {
            logd("  %zu) %s", i, tr.t.input_names[i].c_str());
        }
        if (i >= 9) {
            logd("  ...");
            break;
        }
    }
    logd("- outputs: %zu", tr.t.noutputs);
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        if (i < tr.t.output_names.size()) {
            logd("  %zu) %s", i, tr.t.output_names[i].c_str());
        }
        if (i >= 9) {
            logd("  ...");
            break;
        }
    }
    logd("- big integers: %zu", tr.t.constants.size());
    char buf1[16], buf2[16];
    logd("- instructions: %s final, %s temp",
            fmt_bytes(buf1, 16, code_size(tr.t.fincode)),
            fmt_bytes(buf2, 16, code_size(tr.t.code)));
    logd("- locations: %zu final (%s), %zu temp (%s)",
            tr.t.nfinlocations,
            fmt_bytes(buf1, 16, tr.t.nfinlocations*sizeof(ncoef_t)),
            code_size(tr.t.code)/sizeof(HiOp),
            fmt_bytes(buf2, 16, code_size(tr.t.code)/sizeof(HiOp)*sizeof(ncoef_t)));
    logd("- next free location: %zu", tr.t.nextloc);
    logd("Current equation set:");
    logd("- families: %zu", the_eqset.families.size());
    for (size_t i = 0; i < the_eqset.families.size(); i++) {
        logd("  %zu) '%s' with %d indices", i, the_eqset.families[i].name.c_str(), the_eqset.families[i].nindices);
        if (i >= 9) {
            logd("  ...");
            break;
        }
    }
    logd("- equations: %zu", the_eqset.equations.size());
    if (the_varmap.empty()) {
        logd("Active variable replacements: none");
    } else {
        logd("Active variable replacements:");
        for (const auto &kv : the_varmap) {
            logd("- %s", nt_get(tr.var_names, kv.first));
        }
    }
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        char buf[16];
        logd("Runtime: %.3fs user time, %.3fs system time, %s maximum RSS",
                usage.ru_utime.tv_sec + usage.ru_utime.tv_usec*1e-6,
                usage.ru_stime.tv_sec + usage.ru_stime.tv_usec*1e-6,
                fmt_bytes(buf, 16, usage.ru_maxrss*1024));
    }
    return 0;
}

static int
cmd_list_outputs(int argc, char *argv[])
{
    LOGBLOCK("list-outputs");
    (void)argc; (void)argv;
    int na = 0;
    const char *filename = NULL;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else break;
    }
    FILE *f = stdout;
    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) crash("list-outputs: failed to open %s\n", filename);
    }
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        if (i < tr.t.output_names.size()) {
            fprintf(f, "%zu %s\n", i, tr.t.output_names[i].c_str());
        }
    }
    fflush(f);
    if (filename != NULL) {
        fclose(f);
        logd("Saved the list of outputs into '%s'", filename);
    }
    return na;
}

static int
cmd_stat(int argc, char *argv[])
{
    LOGBLOCK("stat");
    (void)argc; (void)argv;
    tr_flush(tr.t);
    if (code_size(tr.t.fincode) > 0) {
        logd("Finalized code statistics:");
        size_t size = code_size(tr.t.fincode);
        size_t opcount[LOP_COUNT] = {};
        size_t nops = 0;
        char buf[16];
        CODE_PAGEITER_BEGIN(tr.t.fincode, 0)
        LOOP_ITER_BEGIN(PAGE, PAGEEND)
            opcount[OP]++;
            nops++;
        LOOP_ITER_END(PAGE, PAGEEND)
        CODE_PAGEITER_END()
        for (int op = 0; op < LOP_COUNT; op++) {
            if (opcount[op] == 0) continue;
            logd("- %13s %12zu %6s %5.2f%%", LoOpName[op], opcount[op], fmt_bytes(buf, 16, opcount[op]*LoOpSize[op]), opcount[op]*LoOpSize[op]*100./size);
        }
        logd("- %13s %12zu %6s", "total:", nops, fmt_bytes(buf, 16, size));
    }
    if (code_size(tr.t.code) > 0) {
        logd("Code statistics:");
        size_t size = code_size(tr.t.code);
        size_t opcount[LOP_COUNT] = {};
        size_t nops = 0;
        char buf[16];
        CODE_PAGEITER_BEGIN(tr.t.code, 0)
        HIOP_ITER_BEGIN(PAGE, PAGEEND)
            opcount[OP]++;
            nops++;
        HIOP_ITER_END(PAGE, PAGEEND)
        CODE_PAGEITER_END()
        for (int op = 0; op < LOP_COUNT; op++) {
            if (opcount[op] == 0) continue;
            logd("- %13s %12zu %6s %5.2f%%", LoOpName[op], opcount[op], fmt_bytes(buf, 16, opcount[op]*sizeof(HiOp)), opcount[op]*sizeof(HiOp)*100./size);
        }
        logd("- %13s %12zu %6s", "total:", nops, fmt_bytes(buf, 16, size));
    }
    return 0;
}

static int
cmd_disasm(int argc, char *argv[])
{
    LOGBLOCK("disasm");
    int na = 0;
    const char *filename = NULL;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else break;
    }
    FILE *f = stdout;
    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) crash("disasm: failed to open %s\n", filename);
    }
    fprintf(f, "# ninputs = %zu\n", tr.t.ninputs);
    fprintf(f, "# noutputs = %zu \n", tr.t.noutputs);
    fprintf(f, "# nconstants = %zu \n", tr.t.constants.size());
    fprintf(f, "# nfinlocations = %zu\n", tr.t.nfinlocations);
    fprintf(f, "# nlocations = %zu\n", tr.t.nextloc);
    tr_print_disasm(f, tr.t);
    fflush(f);
    if (filename != NULL) {
        fclose(f);
        logd("Saved the disassembly into '%s'", filename);
    }
    return na;
}

static char *
fgetall(FILE *f)
{
    ssize_t num = 0;
    ssize_t alloc = 1024;
    char *text = (char*)safe_malloc(alloc);
    for (;;) {
        ssize_t n = fread(text + num, 1, alloc - num, f);
        if (n < alloc - num) {
            num += n;
            break;
        } else {
            num = alloc;
            alloc *= 2;
            text = (char*)safe_realloc(text, alloc);
        }
    }
    text = (char*)safe_realloc(text, num+1);
    text[num] = 0;
    return text;
}

#define TRACE_MOD_BEGIN() \
{ \
    tr_flush(tr.t); \
    size_t _idx1 = code_size(tr.t.fincode); \
    size_t _idx2 = code_size(tr.t.code);

#define TRACE_MOD_END() \
    size_t _idx3 = code_size(tr.t.fincode); \
    size_t _idx4 = code_size(tr.t.code); \
    if (the_varmap.size() != 0) { \
        size_t _nr = tr_replace_variables(tr.t, _idx1, _idx3, _idx2, _idx4, the_varmap); \
        logd("Replaced %zu instructions using the variable map", _nr); \
    } \
}

static int
cmd_set(int argc, char *argv[])
{
    LOGBLOCK("set");
    if (argc < 2) crash("ratracer: set varname expression\n");
    size_t idx = tr.input(argv[0], strlen(argv[0]));
    logd("Variable '%s' will now mean '%s'", argv[0], argv[1]);
    Parser p = {tr, argv[1], argv[1], {}};
    Value v;
    TRACE_MOD_BEGIN()
    v = parse_complete_expr(p);
    TRACE_MOD_END()
    the_varmap[idx] = v;
    auto it = tr.var_cache.find(idx);
    if (it != tr.var_cache.end()) tr.var_cache.erase(it);
    return 2;
}

static int
cmd_unset(int argc, char *argv[])
{
    LOGBLOCK("unset");
    if (argc < 1) crash("ratracer: unset varname\n");
    size_t len = strlen(argv[0]);
    ssize_t idx = nt_lookup(tr.var_names, argv[0], len);
    if (idx < 0) crash("unset: no such variable '%s'\n", argv[0]);
    auto it = the_varmap.find(idx);
    if (it == the_varmap.end()) crash("unset: variable '%s' is not set\n", argv[0]);
    logd("Variable '%s' will now just mean itself", argv[0]);
    the_varmap.erase(it);
    auto itc = tr.var_cache.find(idx);
    if (itc != tr.var_cache.end()) tr.var_cache.erase(itc);
    return 1;
}

static int cmd_finalize(int argc, char *argv[]);

static int
cmd_load_trace(int argc, char *argv[])
{
    LOGBLOCK("load-trace");
    if (argc < 1) crash("ratracer: load-trace file.trace\n");
    if (code_size(tr.t.code) != 0) {
        logd("Looks like the current trace is not yet finalized; lets do it now");
        cmd_finalize(0, NULL);
    }
    logd("Importing '%s'", argv[0]);
    TRACE_MOD_BEGIN()
    if (tr_mergeimport(tr.t, argv[0]) != 0)
        crash("load-trace: failed to load '%s'\n", argv[0]);
    nt_clear(tr.var_names);
    for (size_t i = 0; i < tr.t.ninputs; i++) {
        nt_append(tr.var_names, tr.t.input_names[i].data(), tr.t.input_names[i].size());
    }
    TRACE_MOD_END()
    return 1;
}

static int
cmd_trace_expression(int argc, char *argv[])
{
    LOGBLOCK("trace-expression");
    if (argc < 1) crash("ratracer: trace-expression filename\n");
    FILE *f = fopen(argv[0], "r");
    if (f == NULL) crash("trace-expression failed to open %s\n", argv[0]);
    char *text = fgetall(f);
    fclose(f);
    char buf[16];
    logd("Read %s from '%s'", fmt_bytes(buf, 16, strlen(text)), argv[0]);
    Parser p = {tr, text, text, {}};
    TRACE_MOD_BEGIN()
    tr.add_output(parse_complete_expr(p), argv[0]);
    TRACE_MOD_END()
    free(text);
    return 1;
}

static int
cmd_select_output(int argc, char *argv[])
{
    LOGBLOCK("select-output");
    if (argc < 1) crash("ratracer: select-output number\n");
    size_t keep = atol(argv[0]);
    if (keep > tr.t.noutputs) crash("can't select output %zu, no such thing\n", keep);
    tr_flush(tr.t);
    size_t nerased = tr_erase_outputs(tr.t, keep);
    logd("Erased %zu outputs, kept #%zu: %s", nerased, keep, tr.t.output_names[0].c_str());
    return 1;
}

static int
cmd_drop_output(int argc, char *argv[])
{
    LOGBLOCK("drop-output");
    std::vector<ssize_t> outmap;
    outmap.resize(tr.t.noutputs, 0);
    if (argc < 1) crash("ratracer: drop-output name [--and name] ...\n");
    int na = 0;
    for (;;) {
        for (size_t i = 0; i < tr.t.noutputs; i++) {
            if (strcmp(argv[na], tr.t.output_names[i].c_str()) == 0) {
                outmap[i] = -1;
                logd("Will remove '%s'", argv[na]);
                goto found;
            }
        }
        logd("Did not find '%s'", argv[na]);
    found:;
        na++;
        if (strcmp(argv[na], "--and") != 0) break;
        na++;
    }
    for (size_t i = 0, j = 0; i < tr.t.noutputs; i++) {
        if (outmap[i] != -1) {
            outmap[i] = j++;
        }
    }
    tr_flush(tr.t);
    size_t nerased = tr_map_outputs(tr.t, &outmap[0]);
    logd("Erased %zu outputs", nerased);
    return na;
}

static int
cmd_save_trace(int argc, char *argv[])
{
    LOGBLOCK("save-trace");
    if (argc < 1) crash("ratracer: save-trace file.trace\n");
    if (tr_export(tr.t, argv[0]) != 0)
        crash("save-trace: failed to save '%s'\n", argv[0]);
    logd("Saved the trace into '%s'", argv[0]);
    return 1;
}

int
cmd_measure(int argc, char *argv[])
{
    LOGBLOCK("measure");
    (void)argc; (void)argv;
    tr_flush(tr.t);
    std::vector<ncoef_t> inputs;
    std::vector<ncoef_t> outputs;
    std::vector<ncoef_t> data;
    inputs.resize(tr.t.ninputs);
    outputs.resize(tr.t.noutputs);
    data.resize(tr.t.nextloc);
    nmod_t mod;
    nmod_init(&mod, 0x7FFFFFFFFFFFFFE7ull); // 2^63-25
    logd("Raw read time: %.4gs + %.4gs", code_readtime(tr.t.fincode), code_readtime(tr.t.code));
    logd("Prime: 0x%016zx", mod.n);
    logd("Inputs:");
    for (size_t i = 0; (i < inputs.size()) && (i < 10); i++) {
        inputs[i] = ncoef_hash(i, mod.n);
        logd("%zu) 0x%016zx", i, inputs[i]);
    }
    long n = 0;
    double t1 = timestamp(), t2;
    for (long k = 1; k < 1000000000; k *= 2) {
        for (int i = 0; i < k; i++) {
            int r = tr_evaluate(tr.t, &inputs[0], &outputs[0], &data[0], mod, NULL);
            if (r != 0) crash("measure: evaluation failed with code %d\n", r);
        }
        n += k;
        t2 = timestamp();
        if (t2 >= t1 + 0.5) break;
    }
    logd("Outputs:");
    for (size_t i = 0; (i < outputs.size()) && (i < 10); i++) {
        logd("%zu) 0x%016zx", i, outputs[i]);
    }
    logd("Average time: %.4gs after %ld evals", (t2-t1)/n, n);
    logd("Raw read time: %.4gs + %.4gs", code_readtime(tr.t.fincode), code_readtime(tr.t.code));
    return 0;
}

int
cmd_check(int argc, char *argv[])
{
    LOGBLOCK("check");
    (void)argc; (void)argv;
    tr_flush(tr.t);
    std::vector<ncoef_t> inputs;
    std::vector<ncoef_t> outputs;
    std::vector<ncoef_t> data;
    inputs.resize(tr.t.ninputs);
    outputs.resize(tr.t.noutputs);
    data.resize(tr.t.nextloc);
    nmod_t mod;
    nmod_init(&mod, 0x7FFFFFFFFFFFFFE7ull); // 2^63-25
    for (size_t i = 0; i < tr.t.ninputs; i++) {
        inputs[i] = ncoef_hash(i, mod.n);
    }
    logd("Values:");
    for (size_t i = 0; (i < inputs.size()) && (i < 10); i++) {
        logd("- input[%zu]  = 0x%016zx", i, inputs[i]);
    }
    if (inputs.size() > 10) logd("- ...");
    int r = tr_evaluate(tr.t, &inputs[0], &outputs[0], &data[0], mod, NULL);
    if (r != 0) crash("check: evaluation failed with code %d\n", r);
    for (size_t i = 0; (i < data.size()) && (i < 10); i++) {
        logd("- data[%zu]   = 0x%016zx", i, data[i]);
    }
    if (data.size() > 10) logd("- ...");
    for (size_t i = 0; (i < outputs.size()) && (i < 10); i++) {
        logd("- output[%zu] = 0x%016zx", i, outputs[i]);
    }
    if (outputs.size() > 10) logd("- ...");
    uint64_t h = 0;
    for (size_t i = 0; i < outputs.size(); i++) {
        h += outputs[i]*0x9E3779B185EBCA87ull; // XXH_PRIME64_1
        h ^= h >> 33;
        h *= 0xC2B2AE3D27D4EB4Full; // XXH_PRIME64_2;
        h ^= h >> 29;
        h *= 0x165667B19E3779F9ull; // XXH_PRIME64_3;
    }
    logd("Output hash = 0x%016zx", h);
    return 0;
}

static int
cmd_optimize(int argc, char *argv[])
{
    LOGBLOCK("optimize");
    (void)argc; (void)argv;
    char buf1[16], buf2[16], buf3[16], buf4[16];
    logd("Starting with %s+%s instructions and the memory requirement of %s+%s",
            fmt_bytes(buf1, 16, code_size(tr.t.fincode)),
            fmt_bytes(buf2, 16, code_size(tr.t.code)),
            fmt_bytes(buf3, 16, tr.t.nfinlocations*sizeof(ncoef_t)),
            fmt_bytes(buf4, 16, code_size(tr.t.code)/sizeof(HiOp)*sizeof(ncoef_t)));
    tr_flush(tr.t);
    std::vector<Value> roots;
    for (auto &&kv : the_varmap) roots.push_back(kv.second);
    { size_t n = tr_opt_erase_dead_code(tr.t, roots.size(), &roots[0]); logd("Erased %zu dead instruction", n); }
    { size_t n = tr_opt_propagate_constants(tr.t); logd("Propagated %zu constants", n); }
    { size_t n = tr_opt_deduplicate(tr.t); logd("Identified %zu duplicated instructions", n); }
    { size_t n = tr_opt_erase_dead_code(tr.t, roots.size(), &roots[0]); logd("Erased %zu dead instruction", n); }
    return 0;
}

static int
cmd_finalize(int argc, char *argv[])
{
    LOGBLOCK("finalize");
    (void)argc; (void)argv;
    char buf1[16], buf2[16], buf3[16], buf4[16];
    logd("Starting with %s+%s instructions and the memory requirement of %s+%s",
            fmt_bytes(buf1, 16, code_size(tr.t.fincode)),
            fmt_bytes(buf2, 16, code_size(tr.t.code)),
            fmt_bytes(buf3, 16, tr.t.nfinlocations*sizeof(ncoef_t)),
            fmt_bytes(buf4, 16, code_size(tr.t.code)/sizeof(HiOp)*sizeof(ncoef_t)));
    std::vector<Value*> roots;
    for (auto &&kv : the_varmap) roots.push_back(&kv.second);
    tr_finalize(tr.t, roots.size(), &roots[0]);
    tr.var_cache.clear();
    tr.const_cache.clear();
    logd("Ended with %s+%s instructions and the memory requirement of %s+%s",
            fmt_bytes(buf1, 16, code_size(tr.t.fincode)),
            fmt_bytes(buf2, 16, code_size(tr.t.code)),
            fmt_bytes(buf3, 16, tr.t.nfinlocations*sizeof(ncoef_t)),
            fmt_bytes(buf4, 16, code_size(tr.t.code)/sizeof(HiOp)*sizeof(ncoef_t)));
    return 0;
}

static int
cmd_unfinalize(int argc, char *argv[])
{
    LOGBLOCK("unfinalize");
    (void)argc; (void)argv;
    if (code_size(tr.t.code) != 0) {
        logd("Looks like the trace wasn't finalized yet; lets do it now");
        cmd_finalize(0, NULL);
    }
    char buf1[16], buf2[16], buf3[16], buf4[16];
    logd("Starting with %s+%s instructions and the memory requirement of %s+%s",
            fmt_bytes(buf1, 16, code_size(tr.t.fincode)),
            fmt_bytes(buf2, 16, code_size(tr.t.code)),
            fmt_bytes(buf3, 16, tr.t.nfinlocations*sizeof(ncoef_t)),
            fmt_bytes(buf4, 16, code_size(tr.t.code)/sizeof(HiOp)*sizeof(ncoef_t)));
    std::vector<Value*> roots;
    for (auto &&kv : the_varmap) roots.push_back(&kv.second);
    tr_unfinalize(tr.t, roots.size(), &roots[0]);
    tr.var_cache.clear();
    tr.const_cache.clear();
    logd("Ended with %s+%s instructions and the memory requirement of %s+%s",
            fmt_bytes(buf1, 16, code_size(tr.t.fincode)),
            fmt_bytes(buf2, 16, code_size(tr.t.code)),
            fmt_bytes(buf3, 16, tr.t.nfinlocations*sizeof(ncoef_t)),
            fmt_bytes(buf4, 16, code_size(tr.t.code)/sizeof(HiOp)*sizeof(ncoef_t)));
    return 0;
}

#include <firefly/Reconstructor.hpp>

#define TR_EVAL_BEGIN(tr, codeptr, inmem) \
    if (inmem) { \
        assert(code_size(tr.code) == 0); \
        ftruncate(tr.fincode.fd, tr.fincode.filesize + CODE_PAGELUFT); \
        codeptr = (uint8_t*)mmap(NULL, tr.fincode.filesize + CODE_PAGELUFT, PROT_READ, MAP_PRIVATE, tr.fincode.fd, 0); \
        if ((codeptr) == NULL) { \
            crash("failed to mmap() the code file: %s", strerror(errno)); \
        } \
    } else { \
        codeptr = NULL; \
    }

#define TR_EVAL(tr, inputs, outputs, data, mod, codeptr, buf) \
    (codeptr == NULL) ? \
        tr_evaluate(tr, inputs, outputs, data, mod, buf) : \
        code_evaluate_lo_mem(codeptr, tr.fincode.filesize, &(inputs)[0], &(outputs)[0], &(tr).constants[0], &(data)[0], mod)

#define TR_EVAL_END(tr, codeptr) \
    if (codeptr != NULL) { \
        munmap(codeptr, tr.fincode.filesize + CODE_PAGELUFT); \
        ftruncate(tr.fincode.fd, tr.fincode.filesize); \
    }

namespace firefly {
    class TraceBB : public BlackBoxBase<TraceBB> {
        const Trace &tr;
        const int *inputmap;
        std::vector<ncoef_t*> datas;
        std::vector<uint8_t*> bufs;
        nmod_t mod;
        uint8_t *code;
    public:
        TraceBB(const Trace &tr, const int *inputmap, size_t nthreads, bool inmem)
        : tr(tr), inputmap(inputmap)
        {
            datas.resize(nthreads);
            bufs.resize(nthreads);
            for (size_t i = 0; i < nthreads; i++) {
                datas[i] = (ncoef_t*)safe_memalign(sizeof(ncoef_t),
                        (tr.ninputs + tr.nextloc)*sizeof(ncoef_t));
                bufs[i] = (uint8_t*)safe_memalign(CODE_BUFALIGN,
                        CODE_PAGESIZE + CODE_PAGELUFT);
            }
            TR_EVAL_BEGIN(this->tr, this->code, inmem)
        }
        ~TraceBB()
        {
            TR_EVAL_END(this->tr, this->code)
            for (size_t i = 0; i < datas.size(); i++) free(datas[i]);
            for (size_t i = 0; i < bufs.size(); i++) free(bufs[i]);
        }
        inline void prime_changed() {
            this->mod.n = FFInt::p;
            this->mod.ninv = FFInt::p_inv;
            count_leading_zeros(this->mod.norm, this->mod.n);
        }
        std::vector<FFInt>
        operator()(const std::vector<FFInt> &ffinputs, uint32_t threadidx) {
            assert(threadidx <= this->datas.size());
            auto data = this->datas[threadidx];
            auto buf = this->bufs[threadidx];
            for (size_t i = 0; i < ffinputs.size(); i++) {
                static_assert(sizeof(FFInt) == sizeof(ncoef_t));
                data[this->inputmap[i]] = *(ncoef_t*)&ffinputs[i];
            }
            std::vector<FFInt> outputs(tr.noutputs, 0);
            int r = TR_EVAL(this->tr, &data[0], (ncoef_t*)&outputs[0], &data[tr.ninputs], this->mod, this->code, buf);
            if (unlikely(r != 0)) crash("reconstruct: evaluation failed with code %d\n", r);
            return outputs;
        }
        template <int N> std::vector<FFIntVec<N>>
        operator()(const std::vector<FFIntVec<N>> &inputs, uint32_t threadidx) {
            (void)inputs; (void)threadidx;
            crash("reconstruct: FireFly bunches are not supported yet\n");
        }
    };
}

static int
cmd_reconstruct(int argc, char *argv[])
{
    LOGBLOCK("reconstruct");
    int nthreads = 1, factor_scan = 0, shift_scan = 0, inmem = 0;
    const char *filename = NULL;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--threads=")) { nthreads = atoi(argv[na] + 10); }
        else if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else if (strcmp(argv[na], "--factor-scan") == 0) { factor_scan = 1; }
        else if (strcmp(argv[na], "--shift-scan") == 0) { shift_scan = 1; }
        else if (strcmp(argv[na], "--inmem") == 0) { inmem = 1; }
        else break;
    }
    tr_flush(tr.t);
    if (inmem && (code_size(tr.t.code) != 0)) {
        logd("The --inmem options need the trace to be finalized; lets do it now");
        cmd_finalize(0, NULL);
    }
    char buf1[16], buf2[16];
    logd("Will use %d*%s=%s for the probe data", nthreads,
            fmt_bytes(buf1, 16, tr.t.nextloc*sizeof(ncoef_t)),
            fmt_bytes(buf2, 16, nthreads*tr.t.nextloc*sizeof(ncoef_t)));
    if (inmem) {
        logd("Will also use %s for the code", fmt_bytes(buf1, 16, code_size(tr.t.fincode)));
    }
    std::vector<int> usedvarmap(tr.t.ninputs, 0);
    std::vector<std::string> usedvarnames;
    tr_list_used_inputs(tr.t, &usedvarmap[0]);
    int nusedinputs = 0;
    for (size_t i = 0; i < tr.t.ninputs; i++) {
        if (usedvarmap[i]) {
            usedvarmap[nusedinputs++] = i;
            usedvarnames.push_back(tr.t.input_names[i]);
        }
    }
    logd("Reconstructing in %d (out of %d) variables:", nusedinputs, tr.t.ninputs);
    for (auto &&name : usedvarnames) {
        logd("- %s", name.c_str());
    }
    firefly::TraceBB ffbb(tr.t, &usedvarmap[0], nthreads, inmem);
    firefly::Reconstructor<firefly::TraceBB> re(
            nusedinputs, nthreads, 1, ffbb, firefly::Reconstructor<firefly::TraceBB>::IMPORTANT);
    if (factor_scan) re.enable_factor_scan();
    if (shift_scan) re.enable_shift_scan();
    re.reconstruct();
    std::vector<firefly::RationalFunction> results = re.get_result();
    FILE *f = stdout;
    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) crash("reconstruct: failed to open %s\n", filename);
    }
    for (size_t i = 0; i < results.size(); i++) {
        std::string fn = results[i].to_string(usedvarnames);
        fprintf(f, "%s =\n  %s;\n", tr.t.output_names[i].c_str(), fn.c_str());
    }
    fflush(f);
    if (filename != NULL) {
        fclose(f);
        logd("Saved the result into '%s'", filename);
    }
    return na;
}

static int
cmd_evaluate(int argc, char *argv[])
{
    (void)argc; (void)argv;
    LOGBLOCK("evaluate");
    if (code_size(tr.t.code) != 0) {
        logd("Looks like the current trace is not yet finalized; lets do it now");
        cmd_finalize(0, NULL);
    }
    fmpq *outputs = (fmpq*)safe_memalign(sizeof(fmpq), sizeof(fmpq)*tr.t.noutputs);
    fmpq *data = (fmpq*)safe_memalign(sizeof(fmpq), sizeof(fmpq)*tr.t.nfinlocations);
    int r = tr_evaluate_fmpq(tr.t, outputs, data);
    if (r != 0) crash("evaluate: evaluation failed with code %d\n", r);
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        char *buf = fmpq_get_str(NULL, 10, &outputs[i]);
        printf("%s =\n  %s;\n", tr.t.output_names[i].c_str(), buf);
        free(buf);
        fmpq_clear(&outputs[i]);
    }
    free(data);
    free(outputs);
    return 0;
}

#define countof(array) (sizeof(array)/sizeof(*(array)))

static uint64_t primes[] = {
    UINT64_C(9223372036854775783), // 2^63-25
    UINT64_C(9223372036854775643), // 2^63-165
    UINT64_C(9223372036854775549), // 2^63-259
    UINT64_C(9223372036854775507), // 2^63-301
    UINT64_C(9223372036854775433), // 2^63-375
    UINT64_C(9223372036854775421), // 2^63-387
    UINT64_C(9223372036854775417), // 2^63-391
    UINT64_C(9223372036854775399), // 2^63-409
    UINT64_C(9223372036854775351), // 2^63-457
    UINT64_C(9223372036854775337), // 2^63-471
    UINT64_C(9223372036854775291), // 2^63-517
    UINT64_C(9223372036854775279), // 2^63-529
    UINT64_C(9223372036854775259), // 2^63-549
    UINT64_C(9223372036854775181), // 2^63-627
    UINT64_C(9223372036854775159), // 2^63-649
    UINT64_C(9223372036854775139), // 2^63-669
    UINT64_C(9223372036854775097), // 2^63-711
    UINT64_C(9223372036854775073), // 2^63-735
    UINT64_C(9223372036854775057), // 2^63-751
    UINT64_C(9223372036854774959), // 2^63-849
    UINT64_C(9223372036854774937), // 2^63-871
    UINT64_C(9223372036854774917), // 2^63-891
    UINT64_C(9223372036854774893), // 2^63-915
    UINT64_C(9223372036854774797), // 2^63-1011
    UINT64_C(9223372036854774739), // 2^63-1069
    UINT64_C(9223372036854774713), // 2^63-1095
    UINT64_C(9223372036854774679), // 2^63-1129
    UINT64_C(9223372036854774629), // 2^63-1179
    UINT64_C(9223372036854774587), // 2^63-1221
    UINT64_C(9223372036854774571), // 2^63-1237
    UINT64_C(9223372036854774559), // 2^63-1249
    UINT64_C(9223372036854774511), // 2^63-1297
    UINT64_C(9223372036854774509), // 2^63-1299
    UINT64_C(9223372036854774499), // 2^63-1309
    UINT64_C(9223372036854774451), // 2^63-1357
    UINT64_C(9223372036854774413), // 2^63-1395
    UINT64_C(9223372036854774341), // 2^63-1467
    UINT64_C(9223372036854774319), // 2^63-1489
    UINT64_C(9223372036854774307), // 2^63-1501
    UINT64_C(9223372036854774277), // 2^63-1531
    UINT64_C(9223372036854774257), // 2^63-1551
    UINT64_C(9223372036854774247), // 2^63-1561
    UINT64_C(9223372036854774233), // 2^63-1575
    UINT64_C(9223372036854774199), // 2^63-1609
    UINT64_C(9223372036854774179), // 2^63-1629
    UINT64_C(9223372036854774173), // 2^63-1635
    UINT64_C(9223372036854774053), // 2^63-1755
    UINT64_C(9223372036854773999), // 2^63-1809
    UINT64_C(9223372036854773977), // 2^63-1831
    UINT64_C(9223372036854773953), // 2^63-1855
    UINT64_C(9223372036854773899), // 2^63-1909
    UINT64_C(9223372036854773867), // 2^63-1941
    UINT64_C(9223372036854773783), // 2^63-2025
    UINT64_C(9223372036854773639), // 2^63-2169
    UINT64_C(9223372036854773561), // 2^63-2247
    UINT64_C(9223372036854773557), // 2^63-2251
    UINT64_C(9223372036854773519), // 2^63-2289
    UINT64_C(9223372036854773507), // 2^63-2301
    UINT64_C(9223372036854773489), // 2^63-2319
    UINT64_C(9223372036854773477), // 2^63-2331
    UINT64_C(9223372036854773443), // 2^63-2365
    UINT64_C(9223372036854773429), // 2^63-2379
    UINT64_C(9223372036854773407), // 2^63-2401
    UINT64_C(9223372036854773353), // 2^63-2455
    UINT64_C(9223372036854773293), // 2^63-2515
    UINT64_C(9223372036854773173), // 2^63-2635
    UINT64_C(9223372036854773069), // 2^63-2739
    UINT64_C(9223372036854773047), // 2^63-2761
    UINT64_C(9223372036854772961), // 2^63-2847
    UINT64_C(9223372036854772957), // 2^63-2851
    UINT64_C(9223372036854772949), // 2^63-2859
    UINT64_C(9223372036854772903), // 2^63-2905
    UINT64_C(9223372036854772847), // 2^63-2961
    UINT64_C(9223372036854772801), // 2^63-3007
    UINT64_C(9223372036854772733), // 2^63-3075
    UINT64_C(9223372036854772681), // 2^63-3127
    UINT64_C(9223372036854772547), // 2^63-3261
    UINT64_C(9223372036854772469), // 2^63-3339
    UINT64_C(9223372036854772429), // 2^63-3379
    UINT64_C(9223372036854772367), // 2^63-3441
    UINT64_C(9223372036854772289), // 2^63-3519
    UINT64_C(9223372036854772241), // 2^63-3567
    UINT64_C(9223372036854772169), // 2^63-3639
    UINT64_C(9223372036854772141), // 2^63-3667
    UINT64_C(9223372036854772061), // 2^63-3747
    UINT64_C(9223372036854772051), // 2^63-3757
    UINT64_C(9223372036854772039), // 2^63-3769
    UINT64_C(9223372036854771989), // 2^63-3819
    UINT64_C(9223372036854771977), // 2^63-3831
    UINT64_C(9223372036854771973), // 2^63-3835
    UINT64_C(9223372036854771953), // 2^63-3855
    UINT64_C(9223372036854771869), // 2^63-3939
    UINT64_C(9223372036854771841), // 2^63-3967
    UINT64_C(9223372036854771833), // 2^63-3975
    UINT64_C(9223372036854771797), // 2^63-4011
    UINT64_C(9223372036854771749), // 2^63-4059
    UINT64_C(9223372036854771737), // 2^63-4071
    UINT64_C(9223372036854771727), // 2^63-4081
    UINT64_C(9223372036854771703), // 2^63-4105
    UINT64_C(9223372036854771689), // 2^63-4119
    UINT64_C(9223372036854771673), // 2^63-4135
    UINT64_C(9223372036854771613), // 2^63-4195
    UINT64_C(9223372036854771571), // 2^63-4237
    UINT64_C(9223372036854771569), // 2^63-4239
    UINT64_C(9223372036854771563), // 2^63-4245
    UINT64_C(9223372036854771559), // 2^63-4249
    UINT64_C(9223372036854771541), // 2^63-4267
    UINT64_C(9223372036854771487), // 2^63-4321
    UINT64_C(9223372036854771457), // 2^63-4351
    UINT64_C(9223372036854771451), // 2^63-4357
    UINT64_C(9223372036854771239), // 2^63-4569
    UINT64_C(9223372036854771227), // 2^63-4581
    UINT64_C(9223372036854771149), // 2^63-4659
    UINT64_C(9223372036854771109), // 2^63-4699
    UINT64_C(9223372036854771071), // 2^63-4737
    UINT64_C(9223372036854771023), // 2^63-4785
    UINT64_C(9223372036854771017), // 2^63-4791
    UINT64_C(9223372036854770939), // 2^63-4869
    UINT64_C(9223372036854770911), // 2^63-4897
    UINT64_C(9223372036854770813), // 2^63-4995
    UINT64_C(9223372036854770749), // 2^63-5059
    UINT64_C(9223372036854770723), // 2^63-5085
    UINT64_C(9223372036854770591), // 2^63-5217
    UINT64_C(9223372036854770569), // 2^63-5239
    UINT64_C(9223372036854770351), // 2^63-5457
    UINT64_C(9223372036854770321), // 2^63-5487
    UINT64_C(9223372036854770309), // 2^63-5499
    UINT64_C(9223372036854770287), // 2^63-5521
    UINT64_C(9223372036854770203), // 2^63-5605
    UINT64_C(9223372036854770153), // 2^63-5655
    UINT64_C(9223372036854770129), // 2^63-5679
    UINT64_C(9223372036854770027), // 2^63-5781
    UINT64_C(9223372036854769939), // 2^63-5869
    UINT64_C(9223372036854769921), // 2^63-5887
    UINT64_C(9223372036854769823), // 2^63-5985
    UINT64_C(9223372036854769799), // 2^63-6009
    UINT64_C(9223372036854769763), // 2^63-6045
    UINT64_C(9223372036854769721), // 2^63-6087
    UINT64_C(9223372036854769459), // 2^63-6349
    UINT64_C(9223372036854769421), // 2^63-6387
    UINT64_C(9223372036854769369), // 2^63-6439
    UINT64_C(9223372036854769357), // 2^63-6451
    UINT64_C(9223372036854769331), // 2^63-6477
    UINT64_C(9223372036854769303), // 2^63-6505
    UINT64_C(9223372036854769289), // 2^63-6519
    UINT64_C(9223372036854769249), // 2^63-6559
    UINT64_C(9223372036854769243), // 2^63-6565
    UINT64_C(9223372036854769231), // 2^63-6577
    UINT64_C(9223372036854769163), // 2^63-6645
    UINT64_C(9223372036854769141), // 2^63-6667
    UINT64_C(9223372036854769103), // 2^63-6705
    UINT64_C(9223372036854769099), // 2^63-6709
    UINT64_C(9223372036854769063), // 2^63-6745
    UINT64_C(9223372036854769061), // 2^63-6747
    UINT64_C(9223372036854769009), // 2^63-6799
    UINT64_C(9223372036854768973), // 2^63-6835
    UINT64_C(9223372036854768967), // 2^63-6841
    UINT64_C(9223372036854768841), // 2^63-6967
    UINT64_C(9223372036854768823), // 2^63-6985
    UINT64_C(9223372036854768773), // 2^63-7035
    UINT64_C(9223372036854768743), // 2^63-7065
    UINT64_C(9223372036854768679), // 2^63-7129
    UINT64_C(9223372036854768539), // 2^63-7269
    UINT64_C(9223372036854768509), // 2^63-7299
    UINT64_C(9223372036854768497), // 2^63-7311
    UINT64_C(9223372036854768467), // 2^63-7341
    UINT64_C(9223372036854768451), // 2^63-7357
    UINT64_C(9223372036854768427), // 2^63-7381
    UINT64_C(9223372036854768347), // 2^63-7461
    UINT64_C(9223372036854768337), // 2^63-7471
    UINT64_C(9223372036854768269), // 2^63-7539
    UINT64_C(9223372036854768157), // 2^63-7651
    UINT64_C(9223372036854768101), // 2^63-7707
    UINT64_C(9223372036854768083), // 2^63-7725
    UINT64_C(9223372036854767971), // 2^63-7837
    UINT64_C(9223372036854767881), // 2^63-7927
    UINT64_C(9223372036854767839), // 2^63-7969
    UINT64_C(9223372036854767819), // 2^63-7989
    UINT64_C(9223372036854767713), // 2^63-8095
    UINT64_C(9223372036854767633), // 2^63-8175
    UINT64_C(9223372036854767609), // 2^63-8199
    UINT64_C(9223372036854767509), // 2^63-8299
    UINT64_C(9223372036854767483), // 2^63-8325
    UINT64_C(9223372036854767383), // 2^63-8425
    UINT64_C(9223372036854767371), // 2^63-8437
    UINT64_C(9223372036854767293), // 2^63-8515
    UINT64_C(9223372036854767237), // 2^63-8571
    UINT64_C(9223372036854767161), // 2^63-8647
    UINT64_C(9223372036854767131), // 2^63-8677
    UINT64_C(9223372036854767087), // 2^63-8721
    UINT64_C(9223372036854767083), // 2^63-8725
    UINT64_C(9223372036854767021), // 2^63-8787
    UINT64_C(9223372036854766969), // 2^63-8839
    UINT64_C(9223372036854766963), // 2^63-8845
    UINT64_C(9223372036854766943), // 2^63-8865
    UINT64_C(9223372036854766859), // 2^63-8949
    UINT64_C(9223372036854766787), // 2^63-9021
    UINT64_C(9223372036854766771), // 2^63-9037
    UINT64_C(9223372036854766751), // 2^63-9057
    UINT64_C(9223372036854766541), // 2^63-9267
    UINT64_C(9223372036854766387), // 2^63-9421
    UINT64_C(9223372036854766379), // 2^63-9429
    UINT64_C(9223372036854766321), // 2^63-9487
    UINT64_C(9223372036854766261), // 2^63-9547
    UINT64_C(9223372036854766243), // 2^63-9565
    UINT64_C(9223372036854766199), // 2^63-9609
    UINT64_C(9223372036854766169), // 2^63-9639
    UINT64_C(9223372036854766129), // 2^63-9679
    UINT64_C(9223372036854766061), // 2^63-9747
    UINT64_C(9223372036854766033), // 2^63-9775
    UINT64_C(9223372036854766031), // 2^63-9777
    UINT64_C(9223372036854766013), // 2^63-9795
    UINT64_C(9223372036854766009), // 2^63-9799
    UINT64_C(9223372036854765941), // 2^63-9867
    UINT64_C(9223372036854765827), // 2^63-9981
    UINT64_C(9223372036854765809)  // 2^63-9999
};

static int
cmd_reconstruct0(int argc, char *argv[])
{
    LOGBLOCK("reconstruct0");
    const char *filename = NULL;
    int nthreads = 1, inmem = 0;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else if (strcmp(argv[na], "--inmem") == 0) { inmem = 1; }
        else break;
    }
    tr_flush(tr.t);
    if (inmem && (code_size(tr.t.code) != 0)) {
        logd("The --inmem options need the trace to be finalized; lets do it now");
        cmd_finalize(0, NULL);
    }
    char buf1[16], buf2[16];
    logd("Will use %d*%s=%s for the probe data", nthreads,
            fmt_bytes(buf1, 16, tr.t.nextloc*sizeof(ncoef_t)),
            fmt_bytes(buf2, 16, nthreads*tr.t.nextloc*sizeof(ncoef_t)));
    uint8_t *code = NULL;
    if (inmem) {
        logd("Will also use %s for the code", fmt_bytes(buf1, 16, code_size(tr.t.fincode)));
    }
    TR_EVAL_BEGIN(tr.t, code, inmem)
    std::vector<ncoef_t> inputs;
    std::vector<ncoef_t> outputs;
    std::vector<ncoef_t> data;
    inputs.resize(tr.t.ninputs, 0);
    outputs.resize(tr.t.noutputs, 0);
    data.resize(tr.t.nextloc);
    std::vector<fmpz> current_r;
    std::vector<fmpq> current_q;
    std::vector<bool> done;
    int ndone = 0;
    for (int i = 0; i < tr.t.noutputs; i++) {
        fmpz_t zero;
        fmpz_init_set_ui(zero, 1);
        current_r.push_back(*zero);
        fmpq_t q;
        fmpq_init(q);
        current_q.push_back(*q);
        done.push_back(false);
    }
    fmpz_t current_m;
    fmpz_t next_m;
    fmpz_init(current_m);
    fmpz_init(next_m);
    for (size_t primei = 0;; primei++) {
        if (primei >= countof(primes)) {
            crash("reconstruct0: don't know enough primes to continue\n");
        }
        logd("Reconstructing in prime %zu: %zu", primei, primes[primei]);
        nmod_t mod;
        nmod_init(&mod, primes[primei]);
        double t1 = timestamp();
        int r = TR_EVAL(tr.t, &inputs[0], &outputs[0], &data[0], mod, code, NULL);
        double t2 = timestamp();
        if (r != 0) crash("reconstrunct0: evaluation failed with code %d\n", r);
        if (primei == 0) {
            fmpz_set_ui(next_m, mod.n);
        } else {
            fmpz_mul_ui(next_m, current_m, mod.n);
        }
        bool alldone = true;
        fmpq_t q;
        fmpq_init(q);
        for (int i = 0; i < tr.t.noutputs; i++) {
            if (done[i]) continue;
            if (primei == 0) {
                fmpz_set_ui(&current_r[i], outputs[i]);
            } else {
                fmpz_CRT_ui(&current_r[i], &current_r[i], current_m, outputs[i], mod.n, 1);
            }
            if (fmpz_sgn(&current_r[i]) >= 0) {
                fmpq_reconstruct_fmpz(q, &current_r[i], next_m);
            } else {
                fmpz_neg(&current_r[i], &current_r[i]);
                fmpq_reconstruct_fmpz(q, &current_r[i], next_m);
                fmpq_neg(q, q);
                fmpz_neg(&current_r[i], &current_r[i]);
            }
            if (fmpq_equal(&current_q[i], q)) {
                done[i] = true;
                ndone++;
            } else {
                alldone = false;
            }
            fmpq_swap(&current_q[i], q);
        }
        fmpq_clear(q);
        fmpz_swap(current_m, next_m);
        double t3 = timestamp();
        logd("Times: %.4fs evaluation, %.4fs CRT+RR; finished: %d/%zu", t2-t1, t3-t2, ndone, tr.t.noutputs);
        if (alldone) break;
    }
    FILE *f = stdout;
    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) crash("reconstruct0: failed to open %s\n", filename);
    }
    for (int i = 0; i < tr.t.noutputs; i++) {
        char *buf = fmpq_get_str(NULL, 10, &current_q[i]);
        fprintf(f, "%s =\n  %s;\n", tr.t.output_names[i].c_str(), buf);
        free(buf);
    }
    fflush(f);
    if (filename != NULL) {
        fclose(f);
        logd("Saved the result into '%s'", filename);
    }
    TR_EVAL_END(tr.t, code)
    return na;
}

static int
cmd_define_family(int argc, char *argv[])
{
    LOGBLOCK("define-family");
    if (argc < 1) crash("ratracer: define-family name [--indices=n]\n");
    char *name = argv[0];
    int nindices = 0;
    int na = 1;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--indices=")) { nindices = atoi(argv[na] + 10); }
        else break;
    }
    int fam = nt_append(the_eqset.family_names, name, strlen(name));
    if (fam >= MAX_FAMILIES) { crash("define-family: too many families\n"); }
    the_eqset.families.push_back(Family{std::string(name), fam, nindices});
    return na;
}

static int
cmd_load_equations(int argc, char *argv[])
{
    LOGBLOCK("load-equations");
    if (argc < 1) crash("ratracer: load-equations file.eqns\n");
    size_t n0 = the_eqset.equations.size();
    TRACE_MOD_BEGIN()
    load_equations(the_eqset, argv[0], tr);
    logd("Loaded %zu equations", the_eqset.equations.size() - n0);
    TRACE_MOD_END()
    return 1;
}

static int
cmd_solve_equations(int argc, char *argv[])
{
    LOGBLOCK("solve-equations");
    (void)argc; (void)argv;
    nreduce(the_eqset.equations, tr);
    logd("Traced the forward reduction");
    if (!is_reduced(the_eqset.equations, tr)) crash("solve-equations: forward reduction failed\n");
    nbackreduce(the_eqset.equations, tr);
    logd("Traced the backward reduction");
    if (!is_backreduced(the_eqset.equations, tr)) crash("solve-equations: back reduction failed\n");
    return 0;
}

static int
cmd_to_series(int argc, char *argv[])
{
    LOGBLOCK("to-series");
    if (argc < 2) crash("ratracer: to-series varname maxorder\n");
    ssize_t varidx = nt_lookup(tr.var_names, argv[0], strlen(argv[0]));
    if (varidx < 0) {
        crash("could not find variable '%s'\n", argv[0]);
    }
    int maxorder = atoi(argv[1]);
    if (code_size(tr.t.code) != 0) {
        logd("Looks like the current trace is not yet finalized; lets do it now");
        cmd_finalize(0, NULL);
    }
    char buf[16];
    logd("Will use %s memory during the transformation",
            fmt_bytes(buf, 16, tr.t.nfinlocations*sizeof(SValue)));
    Trace t = tr_to_series(tr.t, varidx, maxorder);
    tr = tracer_of_trace(t);
    the_varmap.clear();
    return 2;
}

static int
snprintf_name(char *buf, size_t len, name_t name, const std::vector<Family> &families)
{
    int family = name_family(name);
    const Family &fam = families[family];
    if (fam.nindices == 0) {
        return snprintf(buf, len, "%s#%lld", fam.name.c_str(), name_number(name));
    } else {
        int indices[MAX_INDICES];
        undo_index_notation(&family, &indices[0], name);
        char *out = buf;
        out += snprintf(out, len - (out - buf), "%s[", fam.name.c_str());
        for (int i = 0; i < fam.nindices; i++) {
            if (i) out += snprintf(out, len - (out - buf), ",");
            out += snprintf(out, len - (out - buf), "%d", indices[i]);
        }
        out += snprintf(out, len - (out - buf), "]");
        return out - buf;
    }
}

static int
cmd_show_equation_masters(int argc, char *argv[])
{
    LOGBLOCK("show-equation-masters");
    (void)argc; (void)argv;
    const char *name = NULL;
    int maxr = INT_MAX;
    int maxs = INT_MAX;
    int maxd = INT_MAX;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--family=")) { name = argv[na] + 9; }
        else if (startswith(argv[na], "--maxr=")) { maxr = atoi(argv[na] + 7); }
        else if (startswith(argv[na], "--maxs=")) { maxs = atoi(argv[na] + 7); }
        else if (startswith(argv[na], "--maxd=")) { maxd = atoi(argv[na] + 7); }
        else break;
    }
    std::set<name_t> masters = {};
    for (const Equation &eqn : the_eqset.equations) {
        if (eqn.len <= 0) continue;
        if (!tr.is_minus1(eqn.terms[0].coef)) {
            crash("show-equation-masters: the equations are not in the back-reduced form yet\n");
        }
        int fam, indices[MAX_INDICES];
        undo_index_notation(&fam, &indices[0], eqn.terms[0].integral);
        const Family &fam0 = the_eqset.families[fam];
        if ((name != NULL) && (strcmp(name, fam0.name.c_str()) != 0)) continue;
        int r = 0, s = 0, d = 0;
        for (int i = 0; i < MAX_INDICES; i++) {
            r += indices[i] > 0 ? indices[i] : 0;
            s += indices[i] < 0 ? -indices[i] : 0;
            d += indices[i] > 1 ? indices[i]-1 : 0;
        }
        if (r > maxr) continue;
        if (s > maxs) continue;
        if (d > maxd) continue;
        for (size_t i = 1; i < eqn.len; i++) {
            masters.insert(eqn.terms[i].integral);
        }
    }
    size_t i = 0;
    for (name_t name : masters) {
        char buf[512];
        snprintf_name(buf, sizeof(buf), name, the_eqset.families);
        printf("%zu) %s\n", i++, buf);
    }
    return na;
}

static int
cmd_choose_equation_outputs(int argc, char *argv[])
{
    LOGBLOCK("choose-equation-outputs");
    const char *name = NULL;
    int maxr = INT_MAX;
    int maxs = INT_MAX;
    int maxd = INT_MAX;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--family=")) { name = argv[na] + 9; }
        else if (startswith(argv[na], "--maxr=")) { maxr = atoi(argv[na] + 7); }
        else if (startswith(argv[na], "--maxs=")) { maxs = atoi(argv[na] + 7); }
        else if (startswith(argv[na], "--maxd=")) { maxd = atoi(argv[na] + 7); }
        else break;
    }
    size_t idx0 = tr.t.noutputs;
    char buf[1024];
    for (const Equation &eqn : the_eqset.equations) {
        if (eqn.len <= 0) continue;
        if (!tr.is_minus1(eqn.terms[0].coef)) {
            crash("choose-equation-outputs: the equations are not in the back-reduced form yet\n");
        }
        int family0, indices0[MAX_INDICES];
        undo_index_notation(&family0, &indices0[0], eqn.terms[0].integral);
        const Family &fam0 = the_eqset.families[family0];
        if ((name != NULL) && (strcmp(name, fam0.name.c_str()) != 0)) continue;
        int r = 0, s = 0, d = 0;
        for (int i = 0; i < MAX_INDICES; i++) {
            r += indices0[i] > 0 ? indices0[i] : 0;
            s += indices0[i] < 0 ? -indices0[i] : 0;
            d += indices0[i] > 1 ? indices0[i]-1 : 0;
        }
        if (r > maxr) continue;
        if (s > maxs) continue;
        if (d > maxd) continue;
        for (size_t i = 1; i < eqn.len; i++) {
            char *out = buf;
            out += snprintf(out, sizeof(buf) - (out - buf), "CO[");
            out += snprintf_name(out, sizeof(buf) - (out - buf), eqn.terms[0].integral, the_eqset.families);
            out += snprintf(out, sizeof(buf) - (out - buf), ",");
            out += snprintf_name(out, sizeof(buf) - (out - buf), eqn.terms[i].integral, the_eqset.families);
            out += snprintf(out, sizeof(buf) - (out - buf), "]");
            tr.add_output(eqn.terms[i].coef, buf);
        }
    }
    logd("Chosen %zu outputs", tr.t.noutputs - idx0);
    return na;
}

static int
cmd_dump_equations(int argc, char *argv[])
{
    LOGBLOCK("dump-equations");
    const char *filename = NULL;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else break;
    }
    FILE *f = stdout;
    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) crash("reconstruct: failed to open %s\n", filename);
    }
    char buf[1024];
    for (const Equation &eqn : the_eqset.equations) {
        if (eqn.len == 0) continue;
        for (const Term &term : eqn.terms) {
            snprintf_name(buf, sizeof(buf), term.integral, the_eqset.families);
            fprintf(f, "%s*0x%zx\n", buf, term.coef.n);
        }
        fprintf(f, "\n");
    }
    fflush(f);
    if (filename != NULL) {
        fclose(f);
        logd("Saved the equations into '%s'", filename);
    }
    return na;
}

static int
cmd_sh(int argc, char *argv[])
{
    LOGBLOCK("sh");
    if (argc < 1) crash("ratracer: sh command\n");
    logd("sh: running '%s'", argv[0]);
    int r = system(argv[0]);
    if (r != 0) crash("sh: command exited with code %d\n", r);
    return 1;
}

static void
usage(FILE *f)
{
    const char *p = strchr(usagetext, '\n') + 1;
    for (;;) {
        const char *l1 = strchr(p + 2, '{');
        const char *l2 = strchr(p + 2, '[');
        if ((l1 == NULL) && (l2 == NULL)) break;
        const char *l = l1 == NULL ? l2 : l2 == NULL ? l1 : l1 < l2 ? l1 : l2;
        const char *r = strchr(l, (*l == '{') ? '}' : ']');
        if (r == NULL) break;
        const char *a = "", *b = "\033[0m";
        if (l[-2] == 'S' && l[-1] == 's') { a = "\033[1m"; goto found; }
        if (l[-2] == 'N' && l[-1] == 'm') { a = "\033[1;35m"; goto found; }
        if (l[-2] == 'F' && l[-1] == 'l') { a = "\033[33m"; goto found; }
        if (l[-2] == 'C' && l[-1] == 'm') { a = "\033[1;34m"; goto found; }
        if (l[-2] == 'A' && l[-1] == 'r') { a = "\033[32m"; goto found; }
        if (l[-2] == 'E' && l[-1] == 'v') { a = "\033[34m"; goto found; }
        if (l[-2] == 'Q' && l[-1] == 'l') { a = "\033[35m"; goto found; }
        fwrite(p, l + 1 - p, 1, f);
        p = l + 1;
        continue;
    found:
        fwrite(p, l - p - 2, 1, f);
        fputs(a, f);
        fwrite(l + 1, r - l - 1, 1, f);
        fputs(b, f);
        p = r + 1;
    }
    fputs(p, f);
}

int
main(int argc, char *argv[])
{
    logd("\033[0;1;31m    ___");
    logd("\033[0;1;31m(\\/)   \\");
    logd("\033[0;1;31m\\''/_(__~~~~");
    logd("\033[0;1;31m `` \033[0;2mratracer ");
    tr = tracer_init();
    for (int i = 1; i < argc;) {
#define CMD(name, cmd_fun) \
        else if (strcasecmp(argv[i], name) == 0) \
        {  i += cmd_fun(argc - i - 1, argv + i + 1) + 1;  }
        if (strcasecmp(argv[i], "help") == 0) {
            usage(stdout);
            exit(0);
        }
        CMD("show", cmd_show)
        CMD("list-outputs", cmd_list_outputs)
        CMD("stat", cmd_stat)
        CMD("disasm", cmd_disasm)
        CMD("set", cmd_set)
        CMD("unset", cmd_unset)
        CMD("load-trace", cmd_load_trace)
        CMD("save-trace", cmd_save_trace)
        CMD("trace-expression", cmd_trace_expression)
        CMD("select-output", cmd_select_output)
        CMD("drop-output", cmd_drop_output)
        CMD("measure", cmd_measure)
        CMD("check", cmd_check)
        CMD("optimize", cmd_optimize)
        CMD("finalize", cmd_finalize)
        CMD("unfinalize", cmd_unfinalize)
        CMD("reconstruct", cmd_reconstruct)
        CMD("evaluate", cmd_evaluate)
        CMD("reconstruct0", cmd_reconstruct0)
        CMD("define-family", cmd_define_family)
        CMD("load-equations", cmd_load_equations)
        CMD("solve-equations", cmd_solve_equations)
        CMD("show-equation-masters", cmd_show_equation_masters)
        CMD("choose-equation-outputs", cmd_choose_equation_outputs)
        CMD("dump-equations", cmd_dump_equations)
        CMD("to-series", cmd_to_series)
        CMD("sh", cmd_sh)
        else {
            fprintf(stderr, "ratracer: unrecognized command '%s' (use 'ratracer help' to see usage)\n", argv[i]);
            return 1;
        }
    }
    {
        LOGBLOCK("done");
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            char buf[16];
            logd("Runtime: %.3fs user time, %.3fs system time, %s maximum RSS",
                    usage.ru_utime.tv_sec + usage.ru_utime.tv_usec*1e-6,
                    usage.ru_stime.tv_sec + usage.ru_stime.tv_usec*1e-6,
                    fmt_bytes(buf, 16, usage.ru_maxrss*1024));
        }
    }
}
