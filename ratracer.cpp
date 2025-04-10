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
    Cm{load-trace} Ar{filename}
        Load the given trace.

        Note that in this and all other commands files are
        automatically compressed and decompressed based on their
        filename. If the filename ends with Ql{.gz}, Ql{.bz2},
        Ql{.xz}, or Ql{.zst}, then Ql{gzip}, Ql{bzip2}, Ql{xz},
        or Ql{zstd} commands will be used to read or write them.

        The recommended compression format for trace files is
        Ql{.zst}, because it is the fastest while still providing
        considerable compression. Please install the Ql{zstd}
        tool to use it.

    Cm{save-trace} Ar{filename}
        Save the current trace to a file.

    Cm{show}
        Show a short summary of the current trace.

    Cm{list-inputs} [Fl{--to}=Ar{filename}]
        Print the full list of inputs of the current trace.

    Cm{list-outputs} [Fl{--to}=Ar{filename}]
        Print the full list of outputs of the current trace, one
        line per output of the form Ql{n name}, where Ql{n} is
        the output's sequential number (starting from 0).

    Cm{rename-outputs} Ar{filename}
        Read a list of output names from a file in the same
        format as in Cm{list-outputs}, and rename each listed
        output to the corresponding name.

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

    Cm{trace-expression} Ar{filename}
        Load a rational expression from a file and trace its
        evaluation.

    Cm{keep-outputs} Ar{filename}
        Read a list of output name patterns from a file, one
        pattern per line; keep all the outputs that match any
        of these patterns, and erase all the others.

        The pattern syntax is simple: Ql{*} stands for any
        sequence of any characters, all other characters stand
        for themselves.

    Cm{drop-outputs} Ar{filename}
        Read a list of output names patterns from a file, one
        pattern per line; erase all outputs contained in the list.
        The pattern syntax is the same as in Cm{keep-outputs}.

    Cm{optimize}
        Optimize the current trace by propagating constants,
        merging duplicate expressions, and erasing dead code.

    Cm{finalize}
        Convert the (not yet finalized) code into a final low-level
        representation that is smaller, and has drastically
        lower memory usage. Automatically eliminate the dead
        code while finalizing.

    Cm{unfinalize}
        The reverse of Cm{finalize} (i.e. convert low-level code
        into high-level code), except that the eliminated code
        is not brought back.

    Cm{divide-by} Ar{filename}
        Load factors from a file in the same format as in
        Cm{reconstruct}, and divide corresponding outputs by
        them.

    Cm{reconstruct} \
            [Fl{--to}=Ar{filename}] [Fl{--multiply-by}=Ar{filename}] \
            [Fl{--threads}=Ar{n}] [Fl{--inmem}] \
            [Fl{--factor-scan}] [Fl{--shift-scan}] [Fl{--bunches}=Ar{n}]
        Reconstruct the rational form of the current trace using
        the FireFly library.

        If the Fl{--multiply-by} option is provided, load the
        factors from the given file, and multiply the outputs by
        then after the reconstruction is over. Note that pure
        textual substitution is assumed here, so syntax is not
        checked, and substitutions established via Cm{set} are
        not applied.

        If the Fl{--inmem} flag is set, load the whole code
        into memory during reconstruction; this increases the
        performance especially with many threads, but comes at
        the price of higher memory usage.

        This command uses the FireFly library for the reconstruction.
        Flags Fl{--factor-scan} and Fl{--shift-scan} enable
        enable FireFly's factor scan and/or shift scan (which are
        normally recommended); Fl{--bunches} sets its maximal
        bunch size.

    Cm{reconstruct0} \
            [Fl{--to}=Ar{filename}] [Fl{--multiply-by}=Ar{filename}] \
            [Fl{--threads}=Ar{n}]
        Same as Cm{reconstruct}, but assumes that there are 0
        input variables needed, and is therefore faster.

        This command does not use the FireFly library. The code
        is always loaded into memory (as with the Fl{--inmem}
        option of Cm{reconstruct}).

    Cm{evaluate}
        Evaluate the trace in terms of rational numbers.

        Note that all the variables must have been previously
        substituted, e.g. using the Cm{set} command.

    Cm{define-family} Ar{name} [Fl{--indices}=Ar{n}]
        Predefine an indexed family with the given number of
        indices used in the equation parsing. This is only needed
        to guarantee the ordering of the families, otherwise
        they are auto-detected from the equation files.

        Up to 256 different families are currently supported,
        each with up to 16 indices, and the total sum of the
        absolute index values of at most 20.

    Cm{load-equations} Ar{file.eqns}
        Load linear equations from the given file in Kira format,
        tracing the expressions.

    Cm{drop-equations}
        Forget all current equations and families.

    Cm{sort-integrals}
        Sort the integrals in the elimination order.

        This will be called automatically by Cm{solve-equations}.

    Cm{list-integrals} [Fl{--to}=Ar{filename}]
        Print the full list of integrals in the current equation
        set.

    Cm{solve-equations}
        Solve all the currently loaded equations by Gaussian
        elimination, tracing the process.

        Do not forget to Cm{choose-equation-outputs} after this.

    Cm{choose-equation-outputs} \
            [Fl{--family}=Ar{name}] \
            [Fl{--maxr}=Ar{n}] [Fl{--maxs}=Ar{n}] [Fl{--maxd}=Ar{n}]
        Mark the equations defining the specified integrals
        as the outputs, so they could be later reconstructed.

        That is, for each selected equation of the form
        Ma{- I_0 + \sum_i I_i C_i = 0}, add each of the coefficients
        Ma{C_i} as an output with the name Ma{CO[I_0,I_i]}.

        This command will fail if the equations are not in the
        fully reduced form (i.e. after Cm{solve-equations}).

        The equations are filtered by the family name, maximal
        sum of integral's positive powers (Fl{--maxr}), maximal
        sum of negative powers (Fl{--maxs}), and/or maximal sum
        of powers above one (Fl{--maxd}).

    Cm{show-equation-masters} \
            [Fl{--family}=Ar{name}] \
            [Fl{--maxr}=Ar{n}] [Fl{--maxs}=Ar{n}] [Fl{--maxd}=Ar{n}]
        List the unreduced items of the equations filtered the
        same as in Cm{choose-equation-outputs}.

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
        Show a help message and quit.

Ss{ENVIRONMENT}
    Ev{TMP}
        Nm{ratracer} always keeps the current trace in one or more
        temporary files in this directory. The files themselves
        have no names, so they will not be visible to Ql{ls},
        but they will take disk space.

Ss{AUTHORS}
    Vitaly Magerya <vitaly.magerya@tx97.net>
)";

#include "ratracer.h"
#include "ratbox.h"
#include "primes.h"

#include <omp.h>
#include <regex.h>
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
snprintf_integral(char *buf, size_t len, index_t intidx, const std::vector<Family> &families)
{
    int family;
    long long number;
    name_t name = the_eqset.integrals[intidx].name;
    undo_number_notation(&family, &number, name);
    const Family &fam = families[family];
    if (fam.nindices == 0) {
        return snprintf(buf, len, "%s@%lld", fam.name.c_str(), number);
    } else {
        int indices[MAX_INDICES];
        undo_index_notation(&family, &indices[0], name);
        char *out = buf;
        out += snprintf(out, len - (out - buf), "%s[", fam.name.c_str());
        //out += snprintf(out, len - (out - buf), "%s[%zu:", fam.name.c_str(), intidx);
        for (int i = 0; i < fam.nindices; i++) {
            if (i) out += snprintf(out, len - (out - buf), ",");
            out += snprintf(out, len - (out - buf), "%d", indices[i]);
        }
        out += snprintf(out, len - (out - buf), "]");
        return out - buf;
    }
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
    logd("- integrals: %zu", the_eqset.integrals.size());
    for (size_t i = 0; i < the_eqset.integrals.size(); i++) {
        char buf[1024];
        snprintf_integral(buf, sizeof(buf), i, the_eqset.families);
        logd("  %zu) %s", i, buf);
        if (i >= 9) {
            logd("  ...");
            break;
        }
    }
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
cmd_list_inputs(int argc, char *argv[])
{
    LOGBLOCK("list-inputs");
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
        if (f == NULL) crash("list-inputs: failed to open %s\n", filename);
    }
    for (size_t i = 0; i < tr.t.ninputs; i++) {
        if (i < tr.t.input_names.size()) {
            fprintf(f, "%zu %s\n", i, tr.t.input_names[i].c_str());
        }
    }
    fflush(f);
    if (filename != NULL) {
        fclose(f);
        logd("Saved the list of inputs into '%s'", filename);
    }
    return na;
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
    if (argc < 1) crash("ratracer: load-trace filename\n");
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
    OPEN_FILE_R(f, argv[0]);
    char *text = fgetall(f);
    CLOSE_FILE(f);
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
regcomp_filelist(regex_t *pregex, FILE *f)
{
    std::vector<char> buf;
    char *line = NULL;
    size_t linesize = 0;
    for (;;) {
        ssize_t len = getline(&line, &linesize, f);
        while ((len > 0) && (isspace(line[len-1]))) line[--len] = 0;
        if (len <= 0) break;
        if (buf.size() != 0) buf.push_back('|');
        buf.push_back('^');
        for (char *p = line; *p; p++) {
            switch(*p) {
                case '.':
                case '[': case ']':
                case '(': case ')':
                case '{': case '}':
                case '+': case '?':
                case '\\':
                case '|':
                case '^': case '$':
                    buf.push_back('\\');
                    break;
                case '*':
                    buf.push_back('.');
                    break;
            }
            buf.push_back(*p);
        }
        buf.push_back('$');
    }
    if (buf.size() == 0) {
        // The file is empty, add an unmatchable regexp.
        buf.push_back('^');
        buf.push_back('$');
        buf.push_back('_');
    }
    buf.push_back('\0');
    if (line != NULL) free(line);
    return regcomp(pregex, &buf[0], REG_EXTENDED | REG_NOSUB);
}

static int
cmd_keep_outputs(int argc, char *argv[])
{
    LOGBLOCK("keep-outputs");
    std::vector<ssize_t> outmap;
    outmap.resize(tr.t.noutputs, -1);
    if (argc < 1) crash("ratracer: keep-outputs filename\n");
    OPEN_FILE_R(f, argv[0]);
    regex_t reg;
    regcomp_filelist(&reg, f);
    CLOSE_FILE(f);
    size_t j = 0;
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        const char *name = tr.t.output_names[i].c_str();
        if (regexec(&reg, name, 0, NULL, 0) == 0) {
            outmap[i] = j++;
        }
    }
    regfree(&reg);
    logd("Will keep %zu outputs and delete %zu out of %zu", j, tr.t.noutputs-j, tr.t.noutputs);
    tr_flush(tr.t);
    tr_map_outputs(tr.t, &outmap[0]);
    return 1;
}

static int
cmd_drop_outputs(int argc, char *argv[])
{
    LOGBLOCK("drop-outputs");
    std::vector<ssize_t> outmap;
    outmap.resize(tr.t.noutputs, 0);
    if (argc < 1) crash("ratracer: drop-outputs filename\n");
    OPEN_FILE_R(f, argv[0]);
    regex_t reg;
    regcomp_filelist(&reg, f);
    CLOSE_FILE(f);
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        const char *name = tr.t.output_names[i].c_str();
        if (regexec(&reg, name, 0, NULL, 0) == 0) {
            outmap[i] = -1;
        }
    }
    size_t j = 0;
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        if (outmap[i] != -1) {
            outmap[i] = j++;
        }
    }
    logd("Will keep %zu outputs and delete %zu out of %zu", j, tr.t.noutputs-j, tr.t.noutputs);
    tr_flush(tr.t);
    tr_map_outputs(tr.t, &outmap[0]);
    return 1;
}

static int
cmd_rename_outputs(int argc, char *argv[])
{
    LOGBLOCK("rename-outputs");
    if (argc < 1) crash("ratracer: rename-outputs filename\n");
    OPEN_FILE_R(f, argv[0]);
    char *text = fgetall(f);
    CLOSE_FILE(f);
    char *ptr = text;
    size_t renamed = 0;
    size_t lineno = 1;
    while (*ptr == '\n') { ptr++; lineno++; }
    while (*ptr != 0) {
        char *name = ptr;
        long i = strtol(ptr, &name, 10);
        if (name == ptr)
            crash("rename-outputs: output id is missing at line %zu\n", lineno);
        if (i < 0)
            crash("rename-outputs: output id '%ld' at line %zu is negative\n", i, lineno);
        if ((nloc_t)i >= tr.t.noutputs)
            crash("rename-outputs: output id '%ld' at line %zu is too big\n", i, lineno);
        while ((*name == ' ') || (*name == '\t')) name++;
        char *eol = name;
        while (*eol != '\n') eol++;
        char *nameend = eol;
        while ((nameend > name) && is_whitespace(nameend[-1])) nameend--;
        if (nameend == name)
            crash("rename-outputs: output name is missing at line %zu\n", lineno);
        tr.t.output_names[i] = std::string(name, nameend);
        renamed++;
        ptr = eol;
        while (*ptr == '\n') { ptr++; lineno++; }
    }
    free(text);
    logd("Renamed %zu outputs out of %zu", renamed, tr.t.noutputs);
    return 1;
}

static int
cmd_save_trace(int argc, char *argv[])
{
    LOGBLOCK("save-trace");
    if (argc < 1) crash("ratracer: save-trace filename\n");
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
        assert(code_size((tr).code) == 0); \
        ftruncate((tr).fincode.fd, (tr).fincode.filesize + CODE_PAGELUFT); \
        codeptr = (uint8_t*)mmap(NULL, (tr).fincode.filesize + CODE_PAGELUFT, PROT_READ, MAP_PRIVATE, (tr).fincode.fd, 0); \
        if ((codeptr) == NULL) { \
            crash("failed to mmap() the code file: %s", strerror(errno)); \
        } \
    } else { \
        codeptr = NULL; \
    }

#define TR_EVAL(res, tr, input, output, data, mod, codeptr, buf) \
    if (codeptr == NULL) { \
        res = tr_evaluate(tr, input, output, data, mod, buf); \
    } else { \
        res = code_evaluate_lo_mem(codeptr, (tr).fincode.filesize, &(input)[0], &(tr).constants[0], &(data)[0], mod); \
        for (size_t i = 0; i < (tr).noutputs; i++) { (output)[i] = (data)[(tr).outputs[i]]; } \
    }

#define TR_EVAL_END(tr, codeptr) \
    if (codeptr != NULL) { \
        munmap(codeptr, (tr).fincode.filesize + CODE_PAGELUFT); \
        ftruncate((tr).fincode.fd, (tr).fincode.filesize); \
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
            this->mod.norm = flint_clz(this->mod.n);
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
            int r;
            TR_EVAL(r, this->tr, &data[0], (ncoef_t*)&outputs[0], &data[tr.ninputs], this->mod, this->code, buf);
            if (unlikely(r != 0)) crash("reconstruct: evaluation failed with code %d\n", r);
            return outputs;
        }
        template <int N> std::vector<FFIntVec<N>>
        operator()(const std::vector<FFIntVec<N>> &ffinputs, uint32_t threadidx) {
            assert(threadidx <= this->datas.size());
            auto data = this->datas[threadidx];
            auto buf = this->bufs[threadidx];
            std::vector<FFInt> outputs(tr.noutputs, 0);
            std::vector<FFIntVec<N>> vecoutputs(tr.noutputs);
            for (int idx = 0; idx < N; idx++) {
                for (size_t i = 0; i < ffinputs.size(); i++) {
                    static_assert(sizeof(FFInt) == sizeof(ncoef_t));
                    data[this->inputmap[i]] = *(ncoef_t*)&ffinputs[i].vec[idx];
                }
                int r;
                TR_EVAL(r, this->tr, &data[0], (ncoef_t*)&outputs[0], &data[tr.ninputs], this->mod, this->code, buf);
                if (unlikely(r != 0)) crash("reconstruct: evaluation failed with code %d\n", r);
                for (size_t i = 0; i < tr.noutputs; i++) {
                    vecoutputs[i].vec[idx] = outputs[i];
                }
            }
            return vecoutputs;
        }
    };
}

static std::unordered_map<std::string, std::string>
load_factor_file(const char *text)
{
    std::unordered_map<std::string, std::string> factors;
    Parser p = {tr, text, text, {}};
    for (;;) {
        skip_whitespace(p);
        if (*p.ptr == 0) break;
        const char *name_start = p.ptr;
        skip_nonwhitespace(p);
        const char *name_end = p.ptr;
        skip_whitespace_expect(p, '=');
        skip_whitespace(p);
        const char *coef_start = p.ptr;
        skip_until(p, ';');
        const char *coef_end = p.ptr;
        while ((coef_end-1 >= coef_start) && is_whitespace(*(coef_end-1))) coef_end--;
        if (unlikely(*p.ptr != ';')) parse_fail(p, "expected ';'");
        p.ptr++;
        std::string key = std::string(name_start, name_end-name_start);
        std::string val = std::string(coef_start, coef_end-coef_start);
        factors[key] = val;
    }
    return factors;
}

static void
parse_factor_file(Parser &p, std::unordered_map<size_t, Value> &invfactors, Tracer &tr)
{
    for (;;) {
        skip_whitespace(p);
        if (*p.ptr == 0) break;
        const char *name_start = p.ptr;
        skip_nonwhitespace(p);
        const char *name_end = p.ptr;
        ssize_t idx = -1;
        for (size_t i = 0; i < tr.t.noutputs; i++) {
            if (tr.t.output_names[i].size() == name_end - name_start) {
                if (memcmp(tr.t.output_names[i].data(), name_start, name_end - name_start) == 0) {
                    idx = i;
                    break;
                }
            }
        }
        skip_whitespace_expect(p, '=');
        if (idx >= 0) {
            invfactors[idx] = parse_term_inverted(p);
        } else {
            skip_until(p, ';');
        }
        skip_whitespace_expect(p, ';');
    }
}

static int
cmd_divide_by(int argc, char *argv[])
{
    LOGBLOCK("divide-by");
    if (argc < 1) crash("ratracer: divide-by filename\n");
    std::unordered_map<size_t, Value> invfactors;
    logd("Loading the factors from '%s'", argv[0]);
    OPEN_FILE_R(f, argv[0]);
    char *text = fgetall(f);
    CLOSE_FILE(f);
    Parser p = {tr, text, text, {}};
    TRACE_MOD_BEGIN()
    parse_factor_file(p, invfactors, tr);
    TRACE_MOD_END()
    free(text);
    for (const auto &it : invfactors) {
        // Fake value just to be able to call tr.mul().
        Value v = { tr.t.outputs[it.first], 123456789 };
        tr.t.outputs[it.first] = tr.mul(v, it.second).loc;
    }
    return 1;
}

static int
cmd_reconstruct(int argc, char *argv[])
{
    LOGBLOCK("reconstruct");
    int nthreads = 1, nbunches = 4, factor_scan = 0, shift_scan = 0, inmem = 0;
    const char *filename = NULL;
    const char *factorfile = NULL;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--threads=")) { nthreads = atoi(argv[na] + 10); }
        else if (startswith(argv[na], "--bunches=")) { nbunches = atoi(argv[na] + 10); }
        else if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else if (startswith(argv[na], "--multiply-by=")) { factorfile = argv[na] + 14; }
        else if (strcmp(argv[na], "--factor-scan") == 0) { factor_scan = 1; }
        else if (strcmp(argv[na], "--shift-scan") == 0) { shift_scan = 1; }
        else if (strcmp(argv[na], "--inmem") == 0) { inmem = 1; }
        else break;
    }
    std::unordered_map<std::string, std::string> factors;
    if (factorfile) {
        logd("Loading factors from '%s'", factorfile);
        OPEN_FILE_R(f, factorfile);
        char *text = fgetall(f);
        CLOSE_FILE(f);
        factors = load_factor_file(text);
        free(text);
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
            nusedinputs, nthreads, nbunches, ffbb, firefly::Reconstructor<firefly::TraceBB>::IMPORTANT);
    if (factor_scan) re.enable_factor_scan();
    if (shift_scan) re.enable_shift_scan();
    re.reconstruct();
    std::vector<firefly::RationalFunction> results = re.get_result();
    OPEN_FILE_W(f, filename);
    for (size_t i = 0; i < results.size(); i++) {
        std::string fn = results[i].to_string(usedvarnames);
        const auto it = factors.find(tr.t.output_names[i]);
        if (it == factors.end()) {
            fprintf(f, "%s =\n  %s;\n", tr.t.output_names[i].c_str(), fn.c_str());
        } else {
            fprintf(f, "%s =\n  (%s)*(%s);\n", tr.t.output_names[i].c_str(), fn.c_str(), it->second.c_str());
        }
    }
    CLOSE_FILE(f);
    if (filename != NULL) {
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

static int
cmd_reconstruct0(int argc, char *argv[])
{
    LOGBLOCK("reconstruct0");
    const char *filename = NULL;
    const char *factorfile = NULL;
    int nthreads = 1;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--threads=")) { nthreads = atoi(argv[na] + 10); }
        else if (startswith(argv[na], "--multiply-by=")) { factorfile = argv[na] + 14; }
        else if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else break;
    }
    std::unordered_map<std::string, std::string> factors;
    if (factorfile) {
        logd("Loading factors from '%s'", factorfile);
        OPEN_FILE_R(f, factorfile);
        char *text = fgetall(f);
        CLOSE_FILE(f);
        factors = load_factor_file(text);
        free(text);
    }
    tr_flush(tr.t);
    if (code_size(tr.t.code) != 0) {
        logd("The the trace needs to be finalized; lets do it now");
        cmd_finalize(0, NULL);
    }
    char buf1[16], buf2[16];
    logd("Will use %d*%s=%s for the probe data", nthreads,
            fmt_bytes(buf1, 16, tr.t.nextloc*sizeof(ncoef_t)),
            fmt_bytes(buf2, 16, nthreads*tr.t.nextloc*sizeof(ncoef_t)));
    uint8_t *code = NULL;
    logd("Will also use %s for the code", fmt_bytes(buf1, 16, code_size(tr.t.fincode)));
    TR_EVAL_BEGIN(tr.t, code, true)
    double t1 = timestamp();
    std::vector<ncoef_t> inputs;
    inputs.resize(tr.t.ninputs, 0);
    struct PerOutput {
        fmpz_t r;
        fmpq_t q;
        bool done;
    };
    struct PerThread {
        ncoef_t *outputs;
        nmod_t mod;
        double eval_t;
    };
    PerOutput *os = (PerOutput*)safe_malloc(sizeof(PerOutput)*tr.t.noutputs);
    PerThread *ts = (PerThread*)safe_malloc(sizeof(PerThread)*nthreads);
    size_t nprobes = 0;
    #pragma omp parallel num_threads(nthreads)
    {
        int tid = omp_get_thread_num();
        PerThread &t = ts[tid];
        t.outputs = (ncoef_t*)safe_malloc(sizeof(ncoef_t)*tr.t.noutputs);
        ncoef_t *data = (ncoef_t*)safe_malloc(sizeof(ncoef_t)*tr.t.nextloc);
        for (size_t oid = tid; oid < tr.t.noutputs; oid += nthreads) {
            PerOutput &o = os[oid];
            fmpz_init2(o.r, 16);
            fmpq_init(o.q);
            o.done = false;
        }
        int primeid = 0;
        fmpz_t current_m, next_m, rneg;
        fmpq_t q;
        fmpz_init(current_m);
        fmpz_init(next_m);
        fmpz_init(rneg);
        fmpq_init(q);
        size_t ndone = 0;
        for (;;) {
            if (primeid + nthreads > (int)countof(primes)) {
                crash("reconstruct0: don't know enough primes to continue\n");
            }
            if (tid == 0) {
                logd("Reconstructing in primes %zu .. %zu; %zu done, %zu todo", primeid, primeid + nthreads - 1, ndone, tr.t.noutputs-ndone);
            }
            nmod_init(&t.mod, primes[primeid + tid].n);
            double t1 = timestamp();
            int r;
            TR_EVAL(r, tr.t, &inputs[0], &t.outputs[0], &data[0], t.mod, code, NULL);
            double t2 = timestamp();
            t.eval_t += t2-t1;
            if (r != 0) crash("reconstrunct0: evaluation failed with code %d\n", r);
            #pragma omp barrier
            // For each new prime field evaluated before the barrier.
            for (int p = 0; p < nthreads; p++, primeid++) {
                PerThread &t = ts[p];
                if (primeid == 0) {
                    fmpz_set_ui(next_m, primes[primeid].n);
                } else {
                    fmpz_mul_ui(next_m, current_m, primes[primeid].n);
                }
                mp_limb_t current_m_inv_mod = primes[primeid].invm_mod_n;
                mp_limb_t current_m_inv_mod_shoup = primes[primeid].invm_mod_n_shoup;
                for (size_t oid = tid; oid < tr.t.noutputs; oid += nthreads) {
                    PerOutput &o = os[oid];
                    if (o.done) continue;
                    // Chinese remaindering
                    if (primeid == 0) {
                        fmpz_set_ui(o.r, t.outputs[oid]);
                    } else {
                        mp_limb_t rmod = fmpz_get_nmod(o.r, t.mod);
                        if (rmod == t.outputs[oid]) {
                            // Fast path for positive integers.
                            o.done = true;
                            fmpq_set_fmpz(o.q, o.r);
                            continue;
                        }
                        assert(fmpz_sgn(o.r) >= 0);
                        mp_limb_t s;
                        s = _nmod_sub(t.outputs[oid], rmod, t.mod);
                        s = n_mulmod_shoup(current_m_inv_mod, s, current_m_inv_mod_shoup, t.mod.n);
                        fmpz_addmul_ui(o.r, current_m, s);
                    }
                    // Rational number reconstruction
                    fmpz_sub(rneg, next_m, o.r); 
                    if (fmpz_cmpabs(rneg, o.r) > 0) {
                        if (fmpq_reconstruct_fmpz(q, o.r, next_m) == 0) continue;
                    } else {
                        assert(fmpz_sgn(rneg) >= 0);
                        if (fmpq_reconstruct_fmpz(q, rneg, next_m) == 0) continue;
                        fmpq_neg(q, q);
                    }
                    if (fmpz_bits(fmpq_numref(q)) + fmpz_bits(fmpq_denref(q)) + 63 < fmpz_bits(next_m)) {
                        o.done = true;
                        fmpq_swap(o.q, q);
                    }
                }
                fmpz_swap(current_m, next_m);
            }
            #pragma omp barrier
            ndone = 0;
            for (size_t i = 0; i < tr.t.noutputs; i++) {
                if (os[i].done) ndone++;
            }
            if (ndone >= tr.t.noutputs) break;
        }
        fmpq_clear(q);
        fmpz_clear(next_m);
        fmpz_clear(current_m);
        fmpz_clear(rneg);
        free(t.outputs);
        free(data);
        nprobes = primeid;
    }
    double t2 = timestamp();
    double eval_t = 0;
    for (int i = 0; i < nthreads; i++) {
        eval_t += ts[i].eval_t;
    }
    logd("Evaluated %ld probes in %.3gs, %.3gs/probe; %.3g%% efficiency", nprobes, t2-t1, eval_t/nprobes, eval_t/(t2-t1)/nthreads*100);
    OPEN_FILE_W(f, filename);
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        char *buf = fmpq_get_str(NULL, 10, os[i].q);
        const auto it = factors.find(tr.t.output_names[i]);
        if (it == factors.end()) {
            fprintf(f, "%s =\n  %s;\n", tr.t.output_names[i].c_str(), buf);
        } else {
            fprintf(f, "%s =\n  (%s)*(%s);\n", tr.t.output_names[i].c_str(), buf, it->second.c_str());
        }
        free(buf);
    }
    CLOSE_FILE(f);
    if (filename != NULL) {
        logd("Saved the result into '%s'", filename);
    }
    for (size_t i = 0; i < tr.t.noutputs; i++) {
        fmpz_clear(os[i].r);
        fmpq_clear(os[i].q);
    }
    free(os);
    free(ts);
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
    if (nindices > MAX_INDICES) { crash("define-family: the number of indices is > %d\n", MAX_INDICES); }
    int fam = nt_append(the_eqset.family_names, name, strlen(name));
    if (fam >= MAX_FAMILIES) { crash("define-family: too many families\n"); }
    the_eqset.families.push_back(Family{std::string(name), fam, nindices});
    logd("Family #%d: '%s' with %d indices", fam, name, nindices);
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
cmd_drop_equations(int argc, char *argv[])
{
    LOGBLOCK("drop-equations");
    (void)argc; (void)argv;
    the_eqset.families.clear();
    the_eqset.equations.clear();
    nt_clear(the_eqset.family_names);
    return 0;
}

static int
cmd_sort_integrals(int argc, char *argv[])
{
    LOGBLOCK("sort-integrals");
    (void)argc; (void)argv;
    sort_integrals(the_eqset);
    logd("Sorted the integrals");
    return 0;
}

static int
cmd_list_integrals(int argc, char *argv[])
{
    LOGBLOCK("list-integrals");
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
        if (f == NULL) crash("list-integrals: failed to open %s\n", filename);
    }
    char buf[1024];
    for (size_t i = 0; i < the_eqset.integrals.size(); i++) {
        snprintf_integral(buf, sizeof(buf), i, the_eqset.families);
        fprintf(f, "%zu %s\n", i, buf);
    }
    fflush(f);
    if (filename != NULL) {
        fclose(f);
        logd("Saved the list of integrals into '%s'", filename);
    }
    return na;
}

static int
cmd_solve_equations(int argc, char *argv[])
{
    LOGBLOCK("solve-equations");
    (void)argc; (void)argv;
    sort_integrals(the_eqset);
    logd("Sorted the integrals");
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
    std::set<index_t> masters = {};
    for (const Equation &eqn : the_eqset.equations) {
        if (eqn.len <= 0) continue;
        if (!tr.is_minus1(eqn.terms[0].coef)) {
            crash("show-equation-masters: the equations are not in the back-reduced form yet\n");
        }
        int fam, indices[MAX_INDICES];
        undo_index_notation(&fam, &indices[0], the_eqset.integrals[eqn.terms[0].integral].name);
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
    for (index_t idx : masters) {
        char buf[512];
        snprintf_integral(buf, sizeof(buf), idx, the_eqset.families);
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
        undo_index_notation(&family0, &indices0[0], the_eqset.integrals[eqn.terms[0].integral].name);
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
            out += snprintf_integral(out, sizeof(buf) - (out - buf), eqn.terms[0].integral, the_eqset.families);
            out += snprintf(out, sizeof(buf) - (out - buf), ",");
            out += snprintf_integral(out, sizeof(buf) - (out - buf), eqn.terms[i].integral, the_eqset.families);
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
            snprintf_integral(buf, sizeof(buf), term.integral, the_eqset.families);
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
        if (l[-2] == 'M' && l[-1] == 'a') { a = "\033[35m"; goto found; }
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
        CMD("list-inputs", cmd_list_inputs)
        CMD("list-outputs", cmd_list_outputs)
        CMD("stat", cmd_stat)
        CMD("disasm", cmd_disasm)
        CMD("set", cmd_set)
        CMD("unset", cmd_unset)
        CMD("load-trace", cmd_load_trace)
        CMD("save-trace", cmd_save_trace)
        CMD("trace-expression", cmd_trace_expression)
        CMD("keep-outputs", cmd_keep_outputs)
        CMD("drop-outputs", cmd_drop_outputs)
        CMD("rename-outputs", cmd_rename_outputs)
        CMD("measure", cmd_measure)
        CMD("check", cmd_check)
        CMD("optimize", cmd_optimize)
        CMD("finalize", cmd_finalize)
        CMD("unfinalize", cmd_unfinalize)
        CMD("divide-by", cmd_divide_by)
        CMD("reconstruct", cmd_reconstruct)
        CMD("evaluate", cmd_evaluate)
        CMD("reconstruct0", cmd_reconstruct0)
        CMD("define-family", cmd_define_family)
        CMD("load-equations", cmd_load_equations)
        CMD("drop-equations", cmd_drop_equations)
        CMD("sort-integrals", cmd_sort_integrals)
        CMD("list-integrals", cmd_list_integrals)
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
