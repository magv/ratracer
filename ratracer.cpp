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
    |   Nm{ratracer} Cm{trace-expression} expression.txt Cm{optimize} Cm{reconstruct}
    |   [...]
    |   expression.txt =
    |     (1)/(1/2*x+(-1/2)*y);

    To solve a linear system of equations:

    |   Nm{ratracer} \
    |       Cm{load-equations} equations.list \
    |       Cm{solve-equations} \
    |       Cm{choose-equation-outputs} Fl{--maxr}=7 Fl{--maxs}=1 \
    |       Cm{optimize} \
    |       Cm{reconstruct}

Ss{COMMANDS}
    Cm{load-trace} Ar{file.trace}
        Load the given trace.

    Cm{save-trace} Ar{file.trace}
        Save the current trace to a file.

    Cm{show}
        Print a short summary of the current trace.

    Cm{disasm}
        Print a disassembly of the current trace.

    Cm{toC}
        Print a C++ source file of an evaluation library
        corresponding to the current trace. The library can then
        be compiled with e.g.

        |   c++ -shared -fPIC -Os -o file.so file.cpp

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

    Cm{optimize}
        Optimize the current trace.

    Cm{reconstruct} [Fl{--to}=Ar{filename}] [Fl{--threads}=Ar{n}] [Fl{--factor-scan}] [Fl{--shift-scan}]
        Reconstruct the rational form of the current trace using
        the FireFly library. Optionally enable FireFly's factor
        scan and/or shift scan.

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

        Don't foget to Cm{choose-equation-outputs} after this.

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
#include <time.h>
#include <dlfcn.h>

#include <firefly/Reconstructor.hpp>

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
    fprintf(stderr, "\033[2m%.4f +%.4f%*s \033[0m", now - log_first_timestamp, now - log_last_timestamp, log_depth*2, ""); \
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
    logd("- long integers: %zu", tr.t.constants.size());
    logd("- instructions: %zu (%.1fMB)", tr.t.code.size(), tr.t.code.size()*sizeof(Instruction)*1./1024/1024);
    logd("- memory locations: %zu (%.1fMB)", tr.t.nlocations, tr.t.nlocations*sizeof(ncoef_t)*1./1024/1024);
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
        logd("Active variable replacements: (none)");
    } else {
        logd("Active variable replacements:");
        for (const auto &kv : the_varmap) {
            logd("- %s", nt_get(tr.var_names, kv.first));
        }
    }
    return 0;
}

static int
cmd_disasm(int argc, char *argv[])
{
    LOGBLOCK("disasm");
    (void)argc; (void)argv;
    printf("# ninputs = %zu\n", tr.t.ninputs);
    printf("# noutputs = %zu \n", tr.t.noutputs);
    printf("# nconstants = %zu \n", tr.t.constants.size());
    printf("# nlocations = %zu\n", tr.t.nlocations);
    printf("# ninstructions = %zu\n", tr.t.code.size());
    if (tr_print_disasm(stdout, tr.t) != 0) crash("disasm: failed to print the disassembly\n");
    return 0;
}

static char *
fgetall(FILE *f)
{
    ssize_t num = 0;
    ssize_t alloc = 1024;
    char *text = (char*)malloc(alloc);
    for (;;) {
        ssize_t n = fread(text + num, 1, alloc - num, f);
        if (n < alloc - num) {
            num += n;
            break;
        } else {
            num = alloc;
            alloc *= 2;
            text = (char*)realloc(text, alloc);
        }
    }
    text = (char*)realloc(text, num+1);
    text[num] = 0;
    return text;
}

static int
cmd_set(int argc, char *argv[])
{
    LOGBLOCK("set");
    if (argc < 2) crash("ratracer: set varname expression\n");
    size_t len = strlen(argv[0]);
    ssize_t idx = nt_lookup(tr.var_names, argv[0], len);
    if (idx < 0) {
        idx = tr.t.ninputs;
        tr_set_var_name(idx, argv[0], len);
        logd("New variable '%s' will mean '%s'", argv[0], argv[1]);
    } else {
        logd("Variable '%s' will now mean '%s'", argv[0], argv[1]);
    }
    Parser p = {argv[1], argv[1], {}};
    size_t idx1 = tr.t.code.size();
    Value v = parse_complete_expr(p);
    size_t idx2 = tr.t.code.size();
    tr_replace_variables(tr.t, the_varmap, idx1, idx2);
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

static int
cmd_load_trace(int argc, char *argv[])
{
    LOGBLOCK("load-trace");
    if (argc < 1) crash("ratracer: load-trace file.trace\n");
    logd("Importing '%s'", argv[0]);
    size_t idx1 = tr.t.code.size();
    if (tr_import(tr.t, argv[0]) != 0) crash("load-trace: failed to load '%s'\n", argv[0]);
    nt_clear(tr.var_names);
    for (size_t i = 0; i < tr.t.ninputs; i++) {
        nt_append(tr.var_names, tr.t.input_names[i].data(), tr.t.input_names[i].size());
    }
    size_t idx2 = tr.t.code.size();
    tr_replace_variables(tr.t, the_varmap, idx1, idx2);
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
    logd("Read %zu bytes from '%s'", strlen(text), argv[0]);
    Parser p = {text, text, {}};
    size_t n = tr.t.noutputs;
    tr_set_result_name(n, argv[0]);
    size_t idx1 = tr.t.code.size();
    tr_to_result(n, parse_complete_expr(p));
    size_t idx2 = tr.t.code.size();
    tr_replace_variables(tr.t, the_varmap, idx1, idx2);
    free(text);
    return 1;
}

static int
cmd_save_trace(int argc, char *argv[])
{
    LOGBLOCK("save-trace");
    if (argc < 1) crash("ratracer: save-trace file.trace\n");
    if (tr_export(argv[0], tr.t) != 0) crash("save-trace: failed to save '%s'\n", argv[0]);
    logd("Saved the trace into '%s'", argv[0]);
    return 1;
}

static int
cmd_toC(int argc, char *argv[])
{
    LOGBLOCK("toC");
    (void)argc; (void)argv;
    if (tr_print_c(stdout, tr.t) != 0) crash("toC: failed to print the C++ source");
    return 0;
}

static int
cmd_measure(int argc, char *argv[])
{
    LOGBLOCK("measure");
    (void)argc; (void)argv;
    std::vector<ncoef_t> inputs;
    std::vector<ncoef_t> outputs;
    std::vector<ncoef_t> data;
    inputs.resize(tr.t.ninputs);
    outputs.resize(tr.t.noutputs);
    data.resize(tr.t.nlocations);
    nmod_t mod;
    nmod_init(&mod, 0x7FFFFFFFFFFFFFE7ull); // 2^63-25
    for (size_t i = 0; i < tr.t.ninputs; i++) {
        inputs[i] = ncoef_hash(i, mod.n);
    }
    logd("Prime: 0x%016zx", mod.n);
    logd("Inputs:");
    for (size_t i = 0; i < inputs.size(); i++) {
        logd("%zu) 0x%016zx", i, inputs[i]);
    }
    long n = 0;
    double t1 = timestamp(), t2;
    for (long k = 1; k < 1000000000; k *= 2) {
        for (int i = 0; i < k; i++) {
            int r = tr_evaluate(tr.t, &inputs[0], &outputs[0], &data[0], mod);
            if (r != 0) crash("measure: evaluation failed with code %d\n", r);
        }
        n += k;
        t2 = timestamp();
        if (t2 >= t1 + 0.5) break;
    }
    logd("Outputs:");
    for (size_t i = 0; i < outputs.size(); i++) {
        logd("%zu) 0x%016zx", i, outputs[i]);
    }
    logd("Average time: %.4gs after %ld evals", (t2-t1)/n, n);
    return 0;
}

static int
cmd_optimize(int argc, char *argv[])
{
    LOGBLOCK("optimize");
    (void)argc; (void)argv;
    logd("Initial: %zu instructions (%.1fMB), %zu locations (%.1fMB)",
            tr.t.code.size(), tr.t.code.size()*sizeof(Instruction)*1./1024/1024,
            tr.t.nlocations, tr.t.nlocations*sizeof(ncoef_t)*1./1024/1024);
    tr_optimize(tr.t);
    logd("Optimized: %zu instructions (%.1fMB), %zu locations (%.1fMB)",
            tr.t.code.size(), tr.t.code.size()*sizeof(Instruction)*1./1024/1024,
            tr.t.nlocations, tr.t.nlocations*sizeof(ncoef_t)*1./1024/1024);
    return 0;
}

static int
cmd_unsafe_optimize(int argc, char *argv[])
{
    LOGBLOCK("unsafe-optimize");
    (void)argc; (void)argv;
    logd("Initial: %zu instructions (%.1fMB), %zu locations (%.1fMB)",
            tr.t.code.size(), tr.t.code.size()*sizeof(Instruction)*1./1024/1024,
            tr.t.nlocations, tr.t.nlocations*sizeof(ncoef_t)*1./1024/1024);
    tr_unsafe_optimize(tr.t);
    logd("Optimized: %zu instructions (%.1fMB), %zu locations (%.1fMB)",
            tr.t.code.size(), tr.t.code.size()*sizeof(Instruction)*1./1024/1024,
            tr.t.nlocations, tr.t.nlocations*sizeof(ncoef_t)*1./1024/1024);
    return 0;
}

namespace firefly {
    class TraceBB : public BlackBoxBase<TraceBB> {
        const Trace &tr;
        const int *inputmap;
        std::vector<std::vector<ncoef_t>> data;
    public:
        TraceBB(const Trace &tr, const int *inputmap, size_t nthreads) : tr(tr), inputmap(inputmap) {
            data.resize(nthreads);
            for (size_t i = 0; i < nthreads; i++) {
                data[i].resize(tr.ninputs + tr.nlocations);
            }
        }
        std::vector<FFInt>
        operator()(const std::vector<FFInt> &ffinputs, uint32_t threadidx) {
            if (sizeof(FFInt) != sizeof(ncoef_t)) crash("reconstruct: FireFly::FFInt is not a machine word\n");
            if (unlikely(threadidx >= data.size())) crash("reconstruct: called from thread %zu of %zu\n", (size_t)threadidx+1, data.size());
            nmod_t mod;
            mod.n = FFInt::p;
            mod.ninv = FFInt::p_inv;
            count_leading_zeros(mod.norm, mod.n);
            auto data = this->data[threadidx];
            for (size_t i = 0; i < ffinputs.size(); i++) {
                data[this->inputmap[i]] = *(ncoef_t*)&ffinputs[i];
            }
            std::vector<FFInt> outputs(tr.noutputs, 0);
            int r = tr_evaluate(tr, (ncoef_t*)&data[0], (ncoef_t*)&outputs[0], &data[tr.ninputs], mod);
            if (unlikely(r != 0)) crash("reconstruct: evaluation failed with code %d\n", r);
            return outputs;
        }
        template <int N> std::vector<FFIntVec<N>>
        operator()(const std::vector<FFIntVec<N>> &inputs, size_t threadidx) {
            (void)inputs; (void)threadidx;
            crash("reconstruct: FireFly bunches are not supported yet\n");
        }
        inline void prime_changed() { }
    };
}

static int
cmd_reconstruct(int argc, char *argv[])
{
    LOGBLOCK("reconstruct");
    int nthreads = 1, factor_scan = 0, shift_scan = 0;
    char *filename = NULL;
    int na = 0;
    for (; na < argc; na++) {
        if (startswith(argv[na], "--threads=")) { nthreads = atoi(argv[na] + 10); }
        else if (startswith(argv[na], "--to=")) { filename = argv[na] + 5; }
        else if (strcmp(argv[na], "--factor-scan") == 0) { factor_scan = 1; }
        else if (strcmp(argv[na], "--shift-scan") == 0) { shift_scan = 1; }
        else break;
    }
    logd("Will use %.1fMB for the probe data", nthreads*tr.t.nlocations*sizeof(ncoef_t)/1024./1024.);
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
    logd("Reconstructing in %d (out of %d) variables", nusedinputs, tr.t.ninputs);
    firefly::TraceBB ffbb(tr.t, &usedvarmap[0], nthreads);
    firefly::Reconstructor<firefly::TraceBB> re(nusedinputs, nthreads, 1, ffbb, firefly::Reconstructor<firefly::TraceBB>::IMPORTANT);
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
cmd_measure_compiled(int argc, char *argv[])
{
    LOGBLOCK("measure-compiled");
    if (argc < 1) crash("ratracer: measure-compiled some-trace.so\n");
    Trace tr;
    void *lib = dlopen(argv[0], RTLD_NOW);
    if (lib == NULL) {
        crash("measure-compiled: failed to dlopen '%s': %s\n", argv[0], dlerror());
    }
    std::vector<ncoef_t> inputs;
    std::vector<ncoef_t> outputs;
    std::vector<ncoef_t> data;
    inputs.resize(((int (*)())dlsym(lib, "get_ninputs"))());
    outputs.resize(((int (*)())dlsym(lib, "get_noutputs"))());
    data.resize(((int (*)())dlsym(lib, "get_nlocations"))());
    nmod_t mod;
    nmod_init(&mod, 0x7FFFFFFFFFFFFFE7ull); // 2^63-25
    for (size_t i = 0; i < inputs.size(); i++) {
        inputs[i] = ncoef_hash(i, mod.n);
    }
    int (*eval)(const Trace*, const ncoef_t*, ncoef_t*, ncoef_t*, nmod_t) = (int (*)(const Trace*, const ncoef_t*, ncoef_t*, ncoef_t*, nmod_t))dlsym(lib, "evaluate");
    logd("Prime: 0x%016zx", mod.n);
    logd("Inputs:");
    for (size_t i = 0; i < inputs.size(); i++) {
        logd("%zu) 0x%016zx", i, inputs[i]);
    }
    long n = 0;
    double t1 = timestamp(), t2;
    for (long k = 1; k < 1000000000; k *= 2) {
        for (int i = 0; i < k; i++) {
            int r = eval(&tr, &inputs[0], &outputs[0], &data[0], mod);
            if (r != 0) crash("measure-compiled: evaluation failed with code %d\n", r);
        }
        n += k;
        t2 = timestamp();
        if (t2 >= t1 + 0.5) break;
    }
    logd("Outputs:");
    for (size_t i = 0; i < outputs.size(); i++) {
        logd("%zu) 0x%016zx", i, outputs[i]);
    }
    logd("Average time: %.4gs after %ld evals", (t2-t1)/n, n);
    return 1;
}

static int
cmd_compile(int argc, char *argv[])
{
    LOGBLOCK("compile");
    if (argc < 1) crash("ratracer: compile some.so\n");
    const char *tmp = getenv("TMP");
    if (tmp == NULL) tmp = "/tmp";
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/ratracer.XXXXXX.cpp", tmp);
    int fd = mkstemps(buf, 4);
    FILE *f = fdopen(fd, "w");
    tr_print_c(f, tr.t);
    fclose(f);
    char buf2[1024];
    snprintf(buf2, sizeof(buf2), "c++ -shared -fPIC -O1 -o '%s' -I. '%s' -lflint", argv[0], buf);
    logd("%s\n", buf2);
    system(buf2);
    snprintf(buf2, sizeof(buf2), "rm -f '%s'", buf);
    logd("%s\n", buf2);
    system(buf2);
    return 1;
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
    if (fam >= MAX_FAMILIES) { crash("define-family: too many families"); }
    the_eqset.families.push_back(Family{std::string(name), fam, nindices});
    return na;
}

static int
cmd_load_equations(int argc, char *argv[])
{
    LOGBLOCK("load-equations");
    if (argc < 1) crash("ratracer: load-equations file.eqns\n");
    size_t n0 = the_eqset.equations.size();
    size_t idx1 = tr.t.code.size();
    load_equations(the_eqset, argv[0]);
    size_t idx2 = tr.t.code.size();
    tr_replace_variables(tr.t, the_varmap, idx1, idx2);
    logd("Loaded %zu equations", the_eqset.equations.size() - n0);
    return 1;
}

static int
cmd_solve_equations(int argc, char *argv[])
{
    LOGBLOCK("solve-equations");
    (void)argc; (void)argv;
    nreduce(the_eqset.equations);
    logd("Traced the forward reduction");
    if (!is_reduced(the_eqset.equations)) crash("solve-equations: forward reduction failed\n");
    nbackreduce(the_eqset.equations);
    logd("Traced the backward reduction");
    if (!is_backreduced(the_eqset.equations)) crash("solve-equations: back reduction failed\n");
    return 0;
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
    ncoef_t minus1 = nmod_neg(1, tr.mod);
    for (const Equation &eqn : the_eqset.equations) {
        if (eqn.len <= 0) continue;
        if (eqn.terms[0].coef.n != minus1) {
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
    size_t idx = tr.t.noutputs;
    char buf[1024];
    ncoef_t minus1 = nmod_neg(1, tr.mod);
    for (const Equation &eqn : the_eqset.equations) {
        if (eqn.len <= 0) continue;
        if (eqn.terms[0].coef.n != minus1) {
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
            out += snprintf(out, sizeof(buf) - (out - buf), ", ");
            out += snprintf_name(out, sizeof(buf) - (out - buf), eqn.terms[i].integral, the_eqset.families);
            out += snprintf(out, sizeof(buf) - (out - buf), "]");
            tr_set_result_name(idx, buf);
            tr_to_result(idx++, eqn.terms[i].coef);
        }
    }
    logd("Chosen %zu outputs", idx - idx0);
    return na;
}

static int
cmd_dump_equations(int argc, char *argv[])
{
    LOGBLOCK("dump-equations");
    char *filename = NULL;
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
    tr_init();
    for (int i = 1; i < argc;) {
#define CMD(name, cmd_fun) \
        else if (strcasecmp(argv[i], name) == 0) \
        {  i += cmd_fun(argc - i - 1, argv + i + 1) + 1;  }
        if (strcasecmp(argv[i], "help") == 0) {
            usage(stdout);
            exit(0);
        }
        CMD("show", cmd_show)
        CMD("disasm", cmd_disasm)
        CMD("set", cmd_set)
        CMD("unset", cmd_unset)
        CMD("load-trace", cmd_load_trace)
        CMD("save-trace", cmd_save_trace)
        CMD("toC", cmd_toC)
        CMD("trace-expression", cmd_trace_expression)
        CMD("measure", cmd_measure)
        CMD("optimize", cmd_optimize)
        CMD("unsafe-optimize", cmd_unsafe_optimize)
        CMD("reconstruct", cmd_reconstruct)
        CMD("compile", cmd_compile)
        CMD("measure-compiled", cmd_measure_compiled)
        CMD("define-family", cmd_define_family)
        CMD("load-equations", cmd_load_equations)
        CMD("solve-equations", cmd_solve_equations)
        CMD("show-equation-masters", cmd_show_equation_masters)
        CMD("choose-equation-outputs", cmd_choose_equation_outputs)
        CMD("dump-equations", cmd_dump_equations)
        CMD("sh", cmd_sh)
        else {
            fprintf(stderr, "ratracer: unrecognized command '%s' (use 'ratracer help' to see usage)\n", argv[i]);
            return 1;
        }
    }
    { LOGBLOCK("done"); }
}
