/* Rational Toolbox is a collection of functions to generate and
 * work with the traces (produced by Rational Tracer).
 */

#ifndef RATBOX_H
#define RATBOX_H

#include <algorithm>
#include <map>
#include <set>
#include <math.h>

/* Trace optimization
 */

API nloc_t
maybe_replace(const nloc_t key, const std::unordered_map<nloc_t, nloc_t> &map)
{
    const auto it = map.find(key);
    if (it == map.end()) {
        return key;
    } else {
        return it->second;
    }
}

static Instruction
instr_imm(uint64_t dst, int32_t value)
{
    if (value >= 0) return Instruction{OP_OF_INT, dst, (uint64_t)value, 0};
    else return Instruction{OP_OF_NEGINT, dst, (uint64_t)-value, 0};
}

API void
tr_opt_propagate_constants(Trace &tr)
{
    std::unordered_map<nloc_t, int32_t> values;
    std::unordered_map<nloc_t, nloc_t> repl;
    for (Instruction &i : tr.code) {
        switch(i.op) {
        case OP_OF_VAR: break;
        case OP_OF_INT: values[i.dst] = (int32_t)i.a; break;
        case OP_OF_NEGINT: values[i.dst] = -(int32_t)i.a; break;
        case OP_OF_LONGINT: break;
        case OP_INV: {
                i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), 0};
                const auto &va = values.find(i.a);
                if (va != values.end()) {
                    if (va->second == 1) {
                        values[i.dst] = 1;
                        i = instr_imm(i.dst, 1);
                    } else if (va->second == -1) {
                        values[i.dst] = -1;
                        i = instr_imm(i.dst, -1);
                    }
                }
            }
            break;
        case OP_NEGINV: {
                i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), 0};
                const auto &va = values.find(i.a);
                if (va != values.end()) {
                    if (va->second == 1) {
                        values[i.dst] = -1;
                        i = instr_imm(i.dst, -1);
                    } else if (va->second == -1) {
                        values[i.dst] = 1;
                        i = instr_imm(i.dst, 1);
                    }
                }
            }
            break;
        case OP_NEG: {
                i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), 0};
                const auto &va = values.find(i.a);
                if (va != values.end()) {
                    values[i.dst] = -va->second;
                    i = instr_imm(i.dst, -va->second);
                }
            }
            break;
        case OP_POW: {
                i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), i.b};
                const auto &va = values.find(i.a);
                if (va != values.end()) {
                    int64_t a = va->second, r = 1;
                    for (uint64_t n = 0; n < i.b; n++) {
                        if (abs(r) <= IMM_MAX / abs(a)) { r *= a; } else { r = IMM_MAX+1; break; }
                    }
                    if (abs(r) <= IMM_MAX) {
                        values[i.dst] = r;
                        i = instr_imm(i.dst, r);
                    }
                }
            }
            break;
        case OP_ADD: {
                i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), maybe_replace(i.b, repl)};
                const auto &va = values.find(i.a);
                const auto &vb = values.find(i.b);
                if ((va != values.end()) && (vb != values.end())) {
                    int64_t r = va->second + vb->second;
                    if (abs(r) <= IMM_MAX) {
                        values[i.dst] = r;
                        i = instr_imm(i.dst, r);
                    }
                } else if (va != values.end()) {
                    if (va->second == 0) {
                        repl[i.dst] = i.b;
                        i = Instruction{OP_NOP, 0, 0, 0};
                    }
                } else if (vb != values.end()) {
                    if (vb->second == 0) {
                        repl[i.dst] = i.a;
                        i = Instruction{OP_NOP, 0, 0, 0};
                    }
                }
            }
            break;
        case OP_SUB: {
                i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), maybe_replace(i.b, repl)};
                const auto &va = values.find(i.a);
                const auto &vb = values.find(i.b);
                if ((va != values.end()) && (vb != values.end())) {
                    int64_t r = va->second - vb->second;
                    if (abs(r) <= IMM_MAX) {
                        values[i.dst] = r;
                        i = instr_imm(i.dst, r);
                    }
                } else if (va != values.end()) {
                    if (va->second == 0) {
                        i = Instruction{OP_NEG, i.dst, i.b, 0};
                    }
                } else if (vb != values.end()) {
                    if (vb->second == 0) {
                        repl[i.dst] = i.a;
                        i = Instruction{OP_NOP, 0, 0, 0};
                    }
                }
            }
            break;
        case OP_MUL: {
                i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), maybe_replace(i.b, repl)};
                const auto &va = values.find(i.a);
                const auto &vb = values.find(i.b);
                if ((va != values.end()) && (vb != values.end())) {
                    int64_t a = va->second, b = vb->second;
                    if (abs(a) <= IMM_MAX/abs(b)) {
                        values[i.dst] = a*b;
                        i = instr_imm(i.dst, a*b);
                    }
                } else if ((va != values.end())) {
                    switch (va->second) {
                    case 0:
                        values[i.dst] = 0;
                        i = Instruction{OP_OF_INT, i.dst, 0, 0};
                        break;
                    case 1:
                        repl[i.dst] = i.b;
                        i = Instruction{OP_NOP, 0, 0, 0};
                        break;
                    case -1:
                        i = Instruction{OP_NEG, i.dst, i.b, 0};
                        break;
                    }
                } else if ((vb != values.end())) {
                    switch (vb->second) {
                    case 0:
                        values[i.dst] = 0;
                        i = Instruction{OP_OF_INT, i.dst, 0, 0};
                        break;
                    case 1:
                        repl[i.dst] = i.a;
                        i = Instruction{OP_NOP, 0, 0, 0};
                        break;
                    case -1:
                        i = Instruction{OP_NEG, i.dst, i.a, 0};
                        break;
                    }
                }
            }
            break;
        case OP_TO_INT: {
                i = Instruction{i.op, 0, maybe_replace(i.a, repl), i.b};
                const auto &va = values.find(i.a);
                if ((va != values.end()) && (va->second == (int64_t)i.b)) {
                    i = Instruction{OP_NOP, 0, 0, 0};
                }
            }
            break;
        case OP_TO_NEGINT: {
                i = Instruction{i.op, 0, maybe_replace(i.a, repl), i.b};
                const auto &va = values.find(i.a);
                if ((va != values.end()) && (va->second == -(int64_t)i.b)) {
                    i = Instruction{OP_NOP, 0, 0, 0};
                }
            }
            break;
        case OP_TO_RESULT:
            i = Instruction{i.op, 0, maybe_replace(i.a, repl), i.b};
            break;
        case OP_NOP: break;
        }
    }
}


API bool
operator <(const Instruction &a, const Instruction &b)
{
    if (a.op < b.op) return true;
    if (a.op > b.op) return false;
    if (a.a < b.a) return true;
    if (a.a > b.a) return false;
    if (a.b < b.b) return true;
    if (a.b > b.b) return false;
    return false;
}

API void
tr_opt_deduplicate(Trace &tr)
{
    std::map<Instruction, nloc_t> i2loc;
    std::unordered_map<nloc_t, nloc_t> repl;
    for (Instruction &i : tr.code) {
        switch(i.op) {
        case OP_OF_VAR:
            //i = Instruction{i.op, i.dst, i.a, 0};
            break;
        case OP_OF_INT:
            //i = Instruction{i.op, i.dst, i.a, 0};
            break;
        case OP_OF_NEGINT:
            //i = Instruction{i.op, i.dst, i.a, 0};
            break;
        case OP_OF_LONGINT:
            //i = Instruction{i.op, i.dst, i.a, 0};
            break;
        case OP_INV:
            i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), 0};
            break;
        case OP_NEGINV:
            i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), 0};
            break;
        case OP_NEG:
            i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), 0};
            break;
        case OP_POW:
            i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), i.b};
            break;
        case OP_ADD:
            i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), maybe_replace(i.b, repl)};
            if (i.a > i.b) { nloc_t t = i.a; i.a = i.b; i.b = t; }
            break;
        case OP_SUB:
            i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), maybe_replace(i.b, repl)};
            if (i.a == i.b) { i = Instruction{OP_OF_INT, i.dst, 0, 0}; }
            break;
        case OP_MUL:
            i = Instruction{i.op, i.dst, maybe_replace(i.a, repl), maybe_replace(i.b, repl)};
            if (i.a > i.b) { nloc_t t = i.a; i.a = i.b; i.b = t; }
            break;
        case OP_TO_INT:
            i = Instruction{i.op, 0, maybe_replace(i.a, repl), i.b};
            break;
        case OP_TO_NEGINT:
            i = Instruction{i.op, 0, maybe_replace(i.a, repl), i.b};
            break;
        case OP_TO_RESULT:
            i = Instruction{i.op, 0, maybe_replace(i.a, repl), i.b};
            break;
        case OP_NOP:
            break;
        }
        Instruction i0 = {i.op, 0, i.a, i.b};
        const auto it = i2loc.find(i0);
        if (it != i2loc.end()) {
            switch(i.op) {
            case OP_INV:
            case OP_NEGINV:
            case OP_MUL:
            case OP_NEG:
            case OP_ADD:
            case OP_SUB:
            case OP_POW:
            case OP_OF_VAR:
            case OP_OF_INT:
            case OP_OF_NEGINT:
            case OP_OF_LONGINT:
                repl[i.dst] = it->second;
                i = Instruction{OP_NOP, 0, 0, 0};
                break;
            case OP_TO_INT:
            case OP_TO_NEGINT:
            case OP_TO_RESULT:
            case OP_NOP:
                i = Instruction{OP_NOP, 0, 0, 0};
                break;
            }
        } else {
            i2loc[i0] = i.dst;
        }
    }
}

API void
tr_opt_compact_nops(Trace &tr)
{
    size_t n = 0;
    for (size_t i = 0; i < tr.code.size(); i++) {
        if (tr.code[i].op == OP_NOP) continue;
        if (i > n) tr.code[n] = tr.code[i];
        n++;
    }
    tr.code.resize(n);
}

API void
tr_opt_remove_asserts(Trace &tr)
{
    size_t n = 0;
    for (size_t i = 0; i < tr.code.size(); i++) {
        if (tr.code[i].op == OP_TO_INT) continue;
        if (tr.code[i].op == OP_TO_NEGINT) continue;
        if (i > n) tr.code[n] = tr.code[i];
        n++;
    }
    tr.code.resize(n);
}


API void
tr_opt_compact_unused_locations(Trace &tr)
{
    std::vector<bool> is_used(tr.nlocations, false);
    for (size_t idx = tr.code.size(); idx > 0; idx--) {
        Instruction &i = tr.code[idx-1];
        switch(i.op) {
        case OP_OF_VAR:
        case OP_OF_INT:
        case OP_OF_NEGINT:
        case OP_OF_LONGINT:
        case OP_INV:
        case OP_NEGINV:
        case OP_NEG:
        case OP_POW:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
            if (!is_used[i.dst]) {
                i = Instruction{OP_NOP, 0, 0, 0};
            }
            break;
        case OP_TO_INT:
        case OP_TO_NEGINT:
        case OP_TO_RESULT:
        case OP_NOP:
            break;
        }
        switch(i.op) {
        case OP_OF_VAR:
        case OP_OF_INT:
        case OP_OF_NEGINT:
        case OP_OF_LONGINT:
            break;
        case OP_INV:
        case OP_NEGINV:
        case OP_NEG:
        case OP_POW:
            is_used[i.a] = true;
            break;
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
            is_used[i.a] = true;
            is_used[i.b] = true;
            break;
        case OP_TO_INT:
        case OP_TO_NEGINT:
        case OP_TO_RESULT:
            is_used[i.a] = true;
            break;
        case OP_NOP:
            break;
        }
    }
    std::vector<nloc_t> map(tr.nlocations, 0);
    size_t idx = 0;
    for (size_t i = 0; i < is_used.size(); i++) {
        if (is_used[i]) map[i] = idx++;
    }
    for (Instruction &i : tr.code) {
        switch(i.op) {
        case OP_OF_VAR:
        case OP_OF_INT:
        case OP_OF_NEGINT:
        case OP_OF_LONGINT:
            i.dst = map[i.dst];
            break;
        case OP_INV:
        case OP_NEGINV:
        case OP_NEG:
        case OP_POW:
            i.dst = map[i.dst];
            i.a = map[i.a];
            break;
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
            i.dst = map[i.dst];
            i.a = map[i.a];
            i.b = map[i.b];
            break;
        case OP_TO_INT:
        case OP_TO_NEGINT:
        case OP_TO_RESULT:
            i.a = map[i.a];
            break;
        case OP_NOP:
            break;
        }
    }
    tr.nlocations = idx;
}

API void
tr_optimize(Trace &tr)
{
    tr_opt_propagate_constants(tr);
    tr_opt_deduplicate(tr);
    tr_opt_compact_unused_locations(tr);
    tr_opt_compact_nops(tr);
}

API void
tr_unsafe_optimize(Trace &tr)
{
    tr_opt_remove_asserts(tr);
    tr_optimize(tr);
}

/* Trace import
 */

API void
tr_recount(Trace &tr)
{
#define max(a, b) ((a) >= (b)) ? (a) : (b)
    nloc_t maxloc = 0;
    nloc_t maxout = 0;
    nloc_t maxvar = 0;
    for (Instruction &i : tr.code) {
        switch(i.op) {
        case OP_INV:
        case OP_NEGINV:
        case OP_MUL:
        case OP_NEG:
        case OP_ADD:
        case OP_SUB:
        case OP_POW:
            maxloc = max(maxloc, i.dst);
            maxloc = max(maxloc, i.a);
            maxloc = max(maxloc, i.b);
            break;
        case OP_OF_VAR:
        case OP_OF_INT:
        case OP_OF_NEGINT:
        case OP_OF_LONGINT:
            maxloc = max(maxloc, i.dst);
            break;
        case OP_TO_INT:
        case OP_TO_NEGINT:
        case OP_TO_RESULT:
            maxloc = max(maxloc, i.a);
            break;
        case OP_NOP:
            break;
        }
        switch(i.op) {
        case OP_OF_VAR:
            maxvar = max(maxvar, i.a);
            break;
        case OP_TO_RESULT:
            maxout = max(maxout, i.b);
            break;
        }
    }
    tr.ninputs = maxvar + 1;
    tr.noutputs = maxout + 1;
    tr.nlocations = maxloc + 1;
#undef max
}

API int
tr_import(Trace &tr, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) return 1;
    TraceFileHeader h;
    if (fread(&h, sizeof(TraceFileHeader), 1, f) != 1) goto fail;
    if (h.magic != RATRACER_MAGIC) goto fail;
    tr.ninputs = h.ninputs;
    tr.noutputs = h.noutputs;
    tr.nlocations = h.nlocations;
    tr.code.resize(h.ninstructions);
    if (fread(&tr.code[0], sizeof(Instruction), h.ninstructions, f) != h.ninstructions) goto fail;
    for (size_t i = 0; i < h.ninputs; i++) {
        uint16_t len = 0;
        if (fread(&len, sizeof(len), 1, f) != 1) goto fail;
        std::string s(len, 0);
        if (len > 0) {
            if (fread(&s[0], len, 1, f) != 1) goto fail;
        }
        tr.input_names.push_back(std::move(s));
    }
    for (size_t i = 0; i < h.noutputs; i++) {
        uint16_t len = 0;
        if (fread(&len, sizeof(len), 1, f) != 1) goto fail;
        std::string s(len, 0);
        if (len > 0) {
            if (fread(&s[0], len, 1, f) != 1) goto fail;
        }
        tr.output_names.push_back(std::move(s));
    }
    for (size_t i = 0; i < h.nconstants; i++) {
        fmpz x;
        fmpz_init(&x);
        fmpz_inp_raw(&x, f);
        tr.constants.push_back(x);
    }
    fclose(f);
    return 0;
fail:
    fclose(f);
    return 1;
}

/* Trace output
 */

API int
tr_print_text(FILE *f, const Trace &tr)
{
    for (const Instruction &i : tr.code) {
        const char *op = "???";
        switch (i.op) {
            case OP_OF_VAR: op = "of_var"; break;
            case OP_OF_INT: op = "of_int"; break;
            case OP_OF_NEGINT: op = "of_negint"; break;
            case OP_OF_LONGINT: op = "of_longint"; break;
            case OP_INV: op = "inv"; break;
            case OP_NEGINV: op = "neginv"; break;
            case OP_NEG: op = "neg"; break;
            case OP_POW: op = "pow"; break;
            case OP_ADD: op = "add"; break;
            case OP_SUB: op = "sub"; break;
            case OP_MUL: op = "mul"; break;
            case OP_TO_INT: op = "to_int"; break;
            case OP_TO_NEGINT: op = "to_negint"; break;
            case OP_TO_RESULT: op = "to_result"; break;
            case OP_NOP: op = "nop"; break;
        }
        fprintf(f, "%zu = %s %zu %zu\n", i.dst, op, i.a, i.b);
    }
    return 0;
}

API int
tr_print_c(FILE *f, const Trace &tr)
{
    fprintf(f, "#include \"ratracer.h\"\n");
    fprintf(f, "#include \"ratbox.h\"\n");
    fprintf(f, "static const char *input_names[%zu] = {", tr.ninputs);
    for (size_t i = 0; i < tr.ninputs; i++) {
        fprintf(f, i == 0 ? "\n" : ",\n");
        if (i < tr.input_names.size()) {
            fprintf(f, "    \"%s\"", tr.input_names[i].c_str());
        } else {
            fprintf(f, "    \"\"");
        }
    }
    fprintf(f, "\n};\n");
    fprintf(f, "static const char *output_names[%zu] = {", tr.noutputs);
    for (size_t i = 0; i < tr.noutputs; i++) {
        fprintf(f, i == 0 ? "\n" : ",\n");
        if (i < tr.output_names.size()) {
            fprintf(f, "    \"%s\"", tr.output_names[i].c_str());
        } else {
            fprintf(f, "    \"\"");
        }
    }
    fprintf(f, "\n};\n");
    fprintf(f, "extern \"C\" int get_ninputs() { return %zu; }\n", tr.ninputs);
    fprintf(f, "extern \"C\" int get_noutputs() { return %zu; }\n", tr.noutputs);
    fprintf(f, "extern \"C\" int get_nlocations() { return %zu; }\n", tr.nlocations);
    fprintf(f, "extern \"C\" const char *get_input_name(uint32_t i) { return input_names[i]; }\n");
    fprintf(f, "extern \"C\" const char *get_output_name(uint32_t i) { return output_names[i]; }\n");
    fprintf(f, "extern \"C\" int\n");
    fprintf(f, "evaluate(const Trace &__restrict tr, const ncoef_t *__restrict input, ncoef_t *__restrict output, ncoef_t *__restrict data, nmod_t mod)\n");
    fprintf(f, "{\n");
    for (const Instruction &i : tr.code) {
        const char *op = "???";
        switch (i.op) {
            case OP_INV: op = "inv"; break;
            case OP_NEGINV: op = "neginv"; break;
            case OP_MUL: op = "mul"; break;
            case OP_NEG: op = "neg"; break;
            case OP_ADD: op = "add"; break;
            case OP_SUB: op = "sub"; break;
            case OP_POW: op = "pow"; break;
            case OP_OF_VAR: op = "of_var"; break;
            case OP_OF_INT: op = "of_int"; break;
            case OP_OF_NEGINT: op = "of_negint"; break;
            case OP_OF_LONGINT: op = "of_longint"; break;
            case OP_TO_INT: op = "to_int"; break;
            case OP_TO_NEGINT: op = "to_negint"; break;
            case OP_TO_RESULT: op = "to_result"; break;
            case OP_NOP: op = "nop"; break;
        }
        fprintf(f, "    INSTR_%s(%zu, %zu, %zu);\n", op, i.dst, i.a, i.b);
    }
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
    return 0;
}

/* Trace evaluation
 */

#define INSTR_inv(dst, a, b) if (unlikely(data[a] == 0)) return 1; data[dst] = nmod_inv(data[a], mod);
#define INSTR_neginv(dst, a, b) if (unlikely(data[a] == 0)) return 1; data[dst] = nmod_neg(nmod_inv(data[a], mod), mod);
#define INSTR_mul(dst, a, b) data[dst] = nmod_mul(data[a], data[b], mod);
#define INSTR_neg(dst, a, b) data[dst] = nmod_neg(data[a], mod);
#define INSTR_add(dst, a, b) data[dst] = _nmod_add(data[a], data[b], mod);
#define INSTR_sub(dst, a, b) data[dst] = _nmod_sub(data[a], data[b], mod);
#define INSTR_pow(dst, a, b) data[dst] = nmod_pow_ui(data[a], b, mod);
#define INSTR_of_var(dst, a, b) data[dst] = input[a];
#define INSTR_of_int(dst, a, b) data[dst] = a;
#define INSTR_of_longint(dst, a, b) data[dst] = _fmpz_get_nmod(&constants[a], mod);
#define INSTR_of_negint(dst, a, b) data[dst] = nmod_neg(a, mod);
#define INSTR_to_int(dst, a, b) if (unlikely(data[a] != b)) return 2;
#define INSTR_to_negint(dst, a, b) if (unlikely(data[a] != nmod_neg(b, mod))) return 2;
#define INSTR_to_result(dst, a, b) output[b] = data[a];
#define INSTR_nop(dst, a, b)

API int
tr_evaluate(const Trace &restrict tr, const ncoef_t *restrict input, ncoef_t *restrict output, ncoef_t *restrict data, nmod_t mod)
{
    const auto &constants = tr.constants;
    for (const Instruction &i : tr.code) {
        switch(i.op) {
        case OP_INV: INSTR_inv(i.dst, i.a, i.b); break;
        case OP_NEGINV: INSTR_neginv(i.dst, i.a, i.b); break;
        case OP_MUL: INSTR_mul(i.dst, i.a, i.b); break;
        case OP_NEG: INSTR_neg(i.dst, i.a, i.b); break;
        case OP_ADD: INSTR_add(i.dst, i.a, i.b); break;
        case OP_SUB: INSTR_sub(i.dst, i.a, i.b); break;
        case OP_POW: INSTR_pow(i.dst, i.a, i.b); break;
        case OP_OF_VAR: INSTR_of_var(i.dst, i.a, i.b); break;
        case OP_OF_INT: INSTR_of_int(i.dst, i.a, i.b); break;
        case OP_OF_NEGINT: INSTR_of_negint(i.dst, i.a, i.b); break;
        case OP_OF_LONGINT: INSTR_of_longint(i.dst, i.a, i.b); break;
        case OP_TO_INT: INSTR_to_int(i.dst, i.a, i.b); break;
        case OP_TO_NEGINT: INSTR_to_negint(i.dst, i.a, i.b); break;
        case OP_TO_RESULT: INSTR_to_result(i.dst, i.a, i.b); break;
        case OP_NOP: INSTR_nop(i.dst, i.a, i.b); break;
        }
    }
    return 0;
}

API int
tr_evaluate_double(const Trace &restrict tr, const double *restrict input, double *restrict output, double *restrict data)
{
    for (const Instruction &i : tr.code) {
        switch(i.op) {
        case OP_NEGINV: data[i.dst] = -1/data[i.a]; break;
        case OP_MUL: data[i.dst] = data[i.a]*data[i.b]; break;
        case OP_NEG: data[i.dst] = -data[i.a]; break;
        case OP_ADD: data[i.dst] = data[i.a]+data[i.b]; break;
        case OP_SUB: data[i.dst] = data[i.a]-data[i.b]; break;
        case OP_POW: data[i.dst] = pow(data[i.a], i.b); break;
        case OP_OF_VAR: data[i.dst] = input[i.a]; break;
        case OP_OF_INT: data[i.dst] = i.a; break;
        case OP_OF_NEGINT: data[i.dst] = -i.a; break;
        case OP_OF_LONGINT: data[i.dst] = fmpz_get_d(&tr.constants[i.a]); break;
        case OP_TO_INT: if (data[i.a] != i.b) return 1; break;
        case OP_TO_NEGINT: if (data[i.a] != -i.b) return 1; break;
        case OP_TO_RESULT: output[i.b] = data[i.a]; break;
        case OP_NOP: break;
        }
    }
    return 0;
}

API double
timestamp()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec*1e-9;
}

#define crash(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while(0)

/* Expression parsing
 */

struct Parser {
    const char *input;
    const char *ptr;
    std::vector<char> tmp;
};

static noreturn void
parse_fail(const Parser &p, const char *reason)
{
    size_t line = 1;
    for (const char *ptr = p.input; ptr < p.ptr; ptr++) {
        if (*ptr == '\n') line++;
    }
    const char *bol = p.ptr;
    while ((bol >= p.input) && (*bol != '\n')) bol--;
    bol++;
    const char *eol = p.ptr;
    while ((*eol != 0) && (*eol != '\n')) eol++;
    fprintf(stderr, "parsing failed: %s at line %zu column %zu:\n| ", reason, line, (p.ptr - bol) + 1);
    fwrite(bol, eol-bol, 1, stderr);
    fprintf(stderr, "\n| ");
    for (const char *q = bol; q < p.ptr; q++) {
        fputc('-', stderr);
    }
    fprintf(stderr, "^\n");
    exit(1);
}

static long long
parse_integer(Parser &p, long long min, long long max)
{
    char *end;
    long long x = strtoll(p.ptr, &end, 10);
    if (unlikely(end == p.ptr)) parse_fail(p, "integer expected");
    if (unlikely((x < min) || (x > max))) parse_fail(p, "integer out of range");
    p.ptr = end;
    return x;
}

/* expr ::= term [+ -] term [+ -] ...
 * term ::= factor [/ *] factor [/ *] ...
 * factor ::= [+ -] factor | atom
 * atom ::= number | symbol | ( expr ) | atom ^ exponent
 * exponent ::= [+ -] ? number | ( [+ -] ? number ) | exponent ^ exponent
 * number ::= [0-9]+
 * symbol ::= [a-z_] [a-z_0-9]*
 */

static Value parse_expr(Parser &p);

static void
skip_whitespace(Parser &p)
{
    while ((*p.ptr == '\t') || (*p.ptr == '\n') || (*p.ptr == '\r') || (*p.ptr == ' ')) p.ptr++;
}

static Value
parse_symbol(Parser &p)
{
    const char *end = p.ptr;
    while ((('a' <= *end) && (*end <= 'z')) || (('0' <= *end) && (*end <= '9')) || (*end == '_')) end++;
    ssize_t i = nt_lookup(tr.var_names, p.ptr, end - p.ptr);
    if (unlikely(i < 0)) {
        size_t n = tr.t.ninputs;
        tr_set_var_name(n, p.ptr, end-p.ptr);
        p.ptr = end;
        return tr_of_var(n);
    } else {
        p.ptr = end;
        return tr_of_var(i);
    }
}

static Value
parse_number(Parser &p)
{
    skip_whitespace(p);
    const char *end = p.ptr;
    while (('0' <= *end) && (*end <= '9')) end++;
    if (end-p.ptr <= 12) {
        long x = strtol(p.ptr, (char**)&end, 10);
        if (unlikely(end == p.ptr)) parse_fail(p, "integer expected");
        p.ptr = end;
        return tr_of_int(x);
    } else {
        // TODO: get rid of this tmp business.
        if (p.tmp.size() < (size_t)(end+1-p.ptr)) p.tmp.resize(end+1-p.ptr);
        memcpy(&p.tmp[0], p.ptr, end-p.ptr);
        p.tmp[end-p.ptr] = 0;
        fmpz_t num;
        fmpz_init(num);
        if (unlikely(fmpz_set_str(num, &p.tmp[0], 10) != 0)) parse_fail(p, "long integer expected");
        p.ptr = end;
        Value r = tr_of_fmpz(num);
        fmpz_clear(num);
        return r;
    }
}

static long
parse_exponent(Parser &p)
{
    skip_whitespace(p);
    if (*p.ptr != '(') {
        long e = parse_integer(p, -IMM_MAX, IMM_MAX);
        skip_whitespace(p);
        if (unlikely(*p.ptr == '^')) parse_fail(p, "nested exponents are forbidden");
        return e;
    } else {
        p.ptr++;
        long e = parse_integer(p, -IMM_MAX, IMM_MAX);
        skip_whitespace(p);
        if (unlikely(*p.ptr != ')')) parse_fail(p, "expected ')'");
        p.ptr++;
        skip_whitespace(p);
        if (unlikely(*p.ptr == '^')) parse_fail(p, "nested exponents are forbidden");
        return e;
    }
}

static Value
parse_factor(Parser &p)
{
    int sign = 1;
    for (;;) {
        skip_whitespace(p);
        if (*p.ptr == '+') { sign = +sign; }
        else if (*p.ptr == '-') { sign = -sign; }
        else break;
        p.ptr++;
    }
    char c = *p.ptr;
    Value x;
    if (('0' <= c) && (c <= '9')) { x = parse_number(p); }
    else if (('a' <= c) && (c <= 'z')) { x = parse_symbol(p); }
    else if (c == '(') {
        p.ptr++;
        x = parse_expr(p);
        skip_whitespace(p);
        if (unlikely(*p.ptr != ')')) parse_fail(p, "expected ')'");
        p.ptr++;
    }
    else parse_fail(p, "unexpected character in a factor");
    skip_whitespace(p);
    if (*p.ptr == '^') {
        p.ptr++;
        x = tr_pow(x, parse_exponent(p));
    }
    return (sign == 1) ? x : tr_neg(x);
}

static Value
parse_term(Parser &p)
{
    Value num, den;
    bool have_num = false, have_den = false;
    bool inverted = false;
    for (;;) {
        Value f = parse_factor(p);
        if (inverted) {
            den = have_den ? tr_mul(den, f) : f;
            have_den = true;
        } else {
            num = have_num ? tr_mul(num, f) : f;
            have_num = true;
        }
        skip_whitespace(p);
        if (*p.ptr == '*') {
            inverted = false;
            p.ptr++;
            continue;
        }
        if (*p.ptr == '/') {
            inverted = true;
            p.ptr++;
            continue;
        }
        break;
    }
    return have_num ? (have_den ? tr_div(num, den) : num) : (have_den ? tr_inv(den) : tr_of_int(1));
}

API Value
parse_expr(Parser &p)
{
    Value sum;
    bool have_sum = false;
    bool inverted = false;
    for (;;) {
        Value t = parse_term(p);
        if (inverted) t = tr_neg(t);
        sum = have_sum ? tr_add(sum, t) : t;
        have_sum = true;
        skip_whitespace(p);
        if (*p.ptr == 0) break;
        if (*p.ptr == '+') {
            inverted = false;
            p.ptr++;
            continue;
        }
        if (*p.ptr == '-') {
            inverted = true;
            p.ptr++;
            continue;
        }
        break;
    }
    return sum;
}

API Value
parse_complete_expr(Parser &p)
{
    Value x = parse_expr(p);
    skip_whitespace(p);
    if (unlikely(*p.ptr != 0)) {
        parse_fail(p, "unrecognized trailing charactes");
    }
    return x;
}

/* Linear system solving
 */

typedef uint64_t name_t;

struct Term {
    name_t integral;
    Value coef;
};

struct Equation {
    size_t id;
    size_t len;
    std::vector<Term> terms;
};

struct Family {
    std::string name;
    int index;
    int nindices;
};

struct EquationSet {
    std::vector<Family> families;
    std::vector<Equation> equations;
    NameTable family_names;
};

#define MAX_FAMILIES 4
#define MAX_INDICES 11
#define MIN_INDEX -12
#define MAX_INDEX 12
#define MAX_NAME_NUMBER 3750324249267578124ull

static name_t
index_notation(int fam, const int *indices)
{
    int t = 0, rs = 0;
    for (int i = 0; i < MAX_INDICES; i++) {
        t += (indices[i] > 0) ? 1 : 0;
        rs += (indices[i] > 0) ? indices[i] : -indices[i];
    }
    name_t w = 0;
    w = w*MAX_FAMILIES + fam;
    w = w*MAX_INDICES + t;
    w = w*(MAX_INDEX > -MIN_INDEX ? 1+MAX_INDEX : 1-MIN_INDEX)*MAX_INDICES + rs;
    for (int i = MAX_INDICES - 1; i >= 0; i--) {
        w = w*(1 + MAX_INDEX - MIN_INDEX) + (indices[i] - MIN_INDEX);
    }
    return w;
}

static name_t
number_notation(int fam, long long n)
{
    name_t w = 0;
    w = w*MAX_FAMILIES + fam;
    w = w*MAX_INDICES;
    w = w*(MAX_INDEX > -MIN_INDEX ? 1+MAX_INDEX : 1-MIN_INDEX)*MAX_INDICES;
    for (int i = MAX_INDICES - 1; i >= 0; i--) {
        w = w*(1 + MAX_INDEX - MIN_INDEX);
    }
    return w + (name_t)n;
}

API void
undo_index_notation(int *fam, int *indices, name_t name)
{
    name_t w = name;
    for (int i = 0; i < MAX_INDICES; i++) {
        indices[i] = (w % (1 + MAX_INDEX - MIN_INDEX)) + MIN_INDEX;
        w /= (1 + MAX_INDEX - MIN_INDEX);
    }
    w /= (MAX_INDEX > -MIN_INDEX ? 1+MAX_INDEX : 1-MIN_INDEX)*MAX_INDICES;
    w /= MAX_INDICES;
    *fam = w % MAX_FAMILIES;
}

API int name_family(name_t name) { return name / (MAX_NAME_NUMBER + 1); }
API long long name_number(name_t name) { return name % (MAX_NAME_NUMBER + 1); }

static Term
parse_equation_term(Parser &p, EquationSet &eqs)
{
    skip_whitespace(p);
    const char *start = p.ptr, *end = p.ptr;
    while ((('a' <= *end) && (*end <= 'z')) || (('0' <= *end) && (*end <= '9')) || (*end == '_')) end++;
    p.ptr = end;
    int indices[MAX_INDICES] = {};
    if (*p.ptr == '#') {
        p.ptr++;
        int fam = nt_lookup(eqs.family_names, start, end-start);
        name_t n = parse_integer(p, 0, MAX_NAME_NUMBER);
        if (unlikely(*p.ptr != '*')) { parse_fail(p, "'*' expected"); };
        p.ptr++;
        Value c = parse_complete_expr(p);
        if (unlikely(fam < 0)) {
            fam = nt_append(eqs.family_names, start, end-start);
            if (unlikely(fam >= MAX_FAMILIES)) { p.ptr = start; parse_fail(p, "too many families already"); }
            eqs.families.push_back(Family{std::string(start, end-start), fam, 0});
        }
        return Term{number_notation(fam, n), c};
    } else if (*p.ptr == '[') {
        p.ptr++;
        int nindices = 0;
        for (;;) {
            indices[nindices++] = (int)parse_integer(p, MIN_INDEX, MAX_INDEX);
            if (*p.ptr != ',') break;
            if (unlikely(nindices >= MAX_INDICES)) break;
            p.ptr++;
        }
        if (unlikely(*p.ptr != ']')) { parse_fail(p, "']' expected"); };
        p.ptr++;
        if (unlikely(*p.ptr != '*')) { parse_fail(p, "'*' expected"); };
        p.ptr++;
        Value c = parse_complete_expr(p);
        int fam = nt_lookup(eqs.family_names, start, end-start);
        if (unlikely(fam < 0)) {
            fam = nt_append(eqs.family_names, start, end-start);
            if (unlikely(fam >= MAX_FAMILIES)) { p.ptr = start; parse_fail(p, "too many families already"); }
            eqs.families.push_back(Family{std::string(start, end-start), fam, nindices});
        }
        return Term{index_notation(fam, indices), c};
    } else {
        parse_fail(p, "'[' or '#' expected");
    }
}

#define WORSE >

static void
neqn_sort(Equation &eqn)
{
    std::sort(eqn.terms.begin(), eqn.terms.end(), [](const Term &a, const Term &b) -> bool {
        return a.integral WORSE b.integral;
    });
}

API void
load_equations(EquationSet &eqs, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (f == NULL) crash("can't open %s\n", filename);
    char *line = NULL;
    size_t size = 0;
    for (bool done = false; !done;) {
        Equation eqn = {};
        for (;;) {
            ssize_t len = getline(&line, &size, f);
            if (len <= 0) { done = true; break; }
            while ((len > 0) && ((line[len-1] == '\t') || (line[len-1] == '\n') || (line[len-1] == '\r') || (line[len-1] == ' '))) line[--len] = 0;
            if (len <= 0) break;
            Parser p = {line, line, {}};
            Term t = parse_equation_term(p, eqs);
            eqn.terms.push_back(t);
            eqn.len++;
        }
        if (eqn.len > 0) {
            neqn_sort(eqn);
            eqs.equations.push_back(std::move(eqn));
        }
    }
    fclose(f);
}

static void
neqn_clear(Equation &neqn)
{
    neqn.terms.clear();
    neqn.len = 0;
}

static void
neqn_eliminate(Equation &res, const Equation &a, size_t idx, const Equation &b)
{
    size_t i1 = 0, i2 = 1;
    // assert(b.coefs[0] == -1);
    const Value &bfactor = a.terms[idx].coef;
    while ((i1 < a.len) && (i2 < b.len)) {
        if (i1 == idx) { i1++; continue; }
        if (a.terms[i1].integral WORSE b.terms[i2].integral) {
            res.terms.push_back(a.terms[i1]);
            res.len++;
            i1++;
        } else if (b.terms[i2].integral WORSE a.terms[i1].integral) {
            Value r = tr_mul(b.terms[i2].coef, bfactor);
            if (r.n != 0) {
                res.terms.push_back(Term{b.terms[i2].integral, r});
                res.len++;
            } else {
                tr_to_int(r, 0);
            }
            i2++;
        } else {
            Value r = tr_addmul(a.terms[i1].coef, b.terms[i2].coef, bfactor);
            if (r.n != 0) {
                res.terms.push_back(Term{a.terms[i1].integral, r});
                res.len++;
            } else {
                tr_to_int(r, 0);
            }
            i1++;
            i2++;
        }
    }
    for (; i1 < a.len; i1++) {
        if (i1 == idx) { i1++; continue; }
        res.terms.push_back(a.terms[i1]);
        res.len++;
    }
    for (; i2 < b.len; i2++) {
        Value r = tr_mul(b.terms[i2].coef, bfactor);
        if (r.n != 0) {
            res.terms.push_back(Term{b.terms[i2].integral, r});
            res.len++;
        } else {
            tr_to_int(r, 0);
        }
    }
    if (res.len == 0) {
        fprintf(stderr, "ZERO EQ IN neqn_eliminate(%zu:%zu + %zu)\n", a.id, idx, b.id);
    }
}

static bool
neqn_is_worse(const Equation &a, const Equation &b)
{
    if ((a.len == 0) || (b.len == 0)) return a.len < b.len;
    if (a.terms[0].integral WORSE b.terms[0].integral) return true;
    if (b.terms[0].integral WORSE a.terms[0].integral) return false;
    /*
    for (size_t i = 0; (i < a.len) && (i < b.len); i++) {
        if (a.integrals[i] WORSE b.integrals[i]) return true;
        if (b.integrals[i] WORSE a.integrals[i]) return false;
    }
    */
    return a.len < b.len;
}

static bool
neqn_is_better(const Equation &a, const Equation &b)
{
    return neqn_is_worse(b, a);
}

template<typename Iterator, typename Compare>
static void
adjust_heap_top(Iterator first, Iterator last, Compare less)
{
    auto value = std::move(*first);
    size_t item = 0;
    size_t len = last - first;
    for (;;) {
        size_t left = 2*item + 1;
        size_t right = 2*item + 2;
        if (left >= len) break;
        size_t child = (right < len) ?
            (less(*(first + left), *(first + right)) ? right : left) :
            left;
        if (less(value, *(first + child))) {
            *(first + item) = std::move(*(first + child));
            item = child;
        } else break;
    }
    *(first + item) = std::move(value);
}

API void
nreduce(std::vector<Equation> &neqns)
{
    ncoef_t minus1 = nmod_neg(1, tr.mod);
    Equation res = {};
    size_t totsub = 0;
    std::make_heap(neqns.begin(), neqns.end(), neqn_is_better);
    size_t n = neqns.size();
    while (n > 0) {
        std::pop_heap(neqns.begin(), neqns.begin() + n--, neqn_is_better);
        Equation &neqnx = neqns[n];
        if (neqnx.len == 0) {
        }
        if (neqnx.terms[0].coef.n != minus1) {
            Value nic = tr_neginv(neqnx.terms[0].coef);
            neqnx.terms[0].coef = tr_of_int(-1);
            for (size_t i = 1; i < neqnx.len; i++) {
                neqnx.terms[i].coef = tr_mul(neqnx.terms[i].coef, nic);
            }
        } else {
            tr_to_int(neqnx.terms[0].coef, -1);
        }
        while (n > 0) {
            Equation &neqn = neqns[0];
            if (neqn.terms[0].integral == neqnx.terms[0].integral) {
                totsub++;
                res.id = neqn.id;
                neqn_eliminate(res, neqn, 0, neqnx);
                std::swap(res, neqn);
                neqn_clear(res);
                if (n > 1) {
                    adjust_heap_top(neqns.begin(), neqns.begin() + n, neqn_is_better);
                } else break;
            } else break;
        }
    }
    std::reverse(neqns.begin(), neqns.end());
}

API bool
is_reduced(const std::vector<Equation> &neqns)
{
    if (neqns.size() == 0) return true;
    ncoef_t minus1 = nmod_neg(1, tr.mod);
    ssize_t last = -1;
    for (size_t i = 0; i < neqns.size(); i++) {
        if (neqns[i].len == 0) continue;
        if (neqns[i].terms[0].coef.n != minus1) {
            fprintf(stderr, "eq %zu starts with i%zx, c=%zu\n", i, neqns[i].terms[0].integral, neqns[i].terms[0].coef.n);
            return false;
        }
        if (last >= 0) {
            if (!(neqns[last].terms[0].integral WORSE neqns[i].terms[0].integral)) {
                fprintf(stderr, "eq %zd starts with i%zx, while eq %zu with i%zx\n", last, neqns[last].terms[0].integral, i, neqns[i].terms[0].integral);
                return false;
            }
        }
        last = i;
    }
    return true;
}

API int
list_masters(std::set<name_t> &masters, const std::vector<Equation> &neqns)
{
    if (neqns.size() == 0) return 0;
    ncoef_t minus1 = nmod_neg(1, tr.mod);
    for (const Equation &neqn : neqns) {
        if (neqn.len <= 1) continue;
        if (neqn.terms[0].coef.n != minus1) return 1;
        for (size_t i = 1; i < neqn.len; i++) {
            masters.insert(neqn.terms[i].integral);
        }
    }
    for (const Equation &neqn : neqns) {
        if (neqn.len == 0) continue;
        if (masters.count(neqn.terms[0].integral) != 0) {
            return 1;
        }
    }
    return 0;
}

API void
nbackreduce(std::vector<Equation> &neqns)
{
    std::unordered_map<name_t, size_t> int2idx;
    Equation res = {};
    for (ssize_t i = neqns.size() - 1; i >= 0; i--) {
        Equation &neqn = neqns[i];
        if (neqn.len <= 1) continue;
        int2idx[neqn.terms[0].integral] = i;
        for (size_t j = 1; j < neqn.len;) {
            auto it = int2idx.find(neqn.terms[j].integral);
            if (it == int2idx.end()) {
                j++;
            } else {
                res.id = neqn.id;
                neqn_eliminate(res, neqn, j, neqns[it->second]);
                std::swap(res, neqn);
                neqn_clear(res);
            }
        }
    }
}

API bool
is_backreduced(const std::vector<Equation> &neqns)
{
    if (neqns.size() == 0) return true;
    std::set<name_t> masters;
    ncoef_t minus1 = nmod_neg(1, tr.mod);
    for (const Equation &neqn : neqns) {
        if (neqn.len <= 1) continue;
        if (neqn.terms[0].coef.n != minus1) return false;
        for (size_t i = 1; i < neqn.len; i++) {
            masters.insert(neqn.terms[i].integral);
        }
    }
    for (const Equation &neqn : neqns) {
        if (neqn.len == 0) continue;
        if (masters.count(neqn.terms[0].integral) != 0) {
            fprintf(stderr, "eq #%zu defines a master i%zx\n", neqn.id, neqn.terms[0].integral);
            return false;
        }
    }
    return true;
}

#endif // RATBOX_H
