/* Rational Toolbox is a collection of functions to generate and
 * work with the traces (produced by Rational Tracer).
 */

#ifndef RATBOX_H
#define RATBOX_H

#include <algorithm>
#include <map>
#include <queue>
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
instr_imm(uint64_t dst, int64_t value)
{
    if (value >= 0) return Instruction{OP_OF_INT, dst, (uint64_t)value, 0};
    else return Instruction{OP_OF_NEGINT, dst, (uint64_t)-value, 0};
}

API void
tr_opt_propagate_constants(Trace &tr)
{
    std::unordered_map<nloc_t, int64_t> values;
    std::unordered_map<nloc_t, nloc_t> repl;
    for (Instruction &i : tr.code) {
        switch(i.op) {
        case OP_OF_VAR: break;
        case OP_OF_INT: values[i.dst] = (int64_t)i.a; break;
        case OP_OF_NEGINT: values[i.dst] = -(int64_t)i.a; break;
        case OP_OF_LONGINT: break;
        case OP_COPY:
            repl[i.dst] = i.a;
            i = Instruction{OP_NOP, 0, 0, 0};
            break;
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

struct InstructionSourceHash {
    const std::vector<Instruction> &code;
    inline size_t operator()(const nloc_t idx) const {
        const Instruction &i = code[idx];
        size_t h = (i.op + 1)*0x9E3779B185EBCA87ull; // XXH_PRIME64_1
        h += i.a;
        h ^= h >> 33;
        h *= 0xC2B2AE3D27D4EB4Full; // XXH_PRIME64_2;
        h += i.b;
        h ^= h >> 33;
        h *= 0xC2B2AE3D27D4EB4Full; // XXH_PRIME64_2;
        h ^= h >> 29;
        h *= 0x165667B19E3779F9ull; // XXH_PRIME64_3;
        h ^= h >> 32;
        return h;
    }
};

struct InstructionSourceEq {
    const std::vector<Instruction> &code;
    inline bool operator()(const nloc_t aidx, const nloc_t bidx) const {
        const Instruction &a = code[aidx];
        const Instruction &b = code[bidx];
        return (a.op == b.op) && (a.a == b.a) && (a.b == b.b);
    }
};

API void
tr_opt_deduplicate(Trace &tr)
{
    std::unordered_set<nloc_t, InstructionSourceHash, InstructionSourceEq>
        locs(0, InstructionSourceHash{tr.code}, InstructionSourceEq{tr.code});
    std::unordered_map<nloc_t, nloc_t> repl;
    for (size_t idx = 0; idx < tr.code.size(); idx++) {
        Instruction &i = tr.code[idx];
        switch(i.op) {
        case OP_OF_VAR: case OP_OF_INT: case OP_OF_NEGINT: case OP_OF_LONGINT:
            break;
        case OP_COPY: case OP_INV: case OP_NEGINV: case OP_NEG:
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
        case OP_TO_INT: case OP_TO_NEGINT: case OP_TO_RESULT:
            i = Instruction{i.op, 0, maybe_replace(i.a, repl), i.b};
            break;
        case OP_NOP:
            break;
        }
        const auto it = locs.find(idx);
        if (it != locs.end()) {
            switch(i.op) {
            case OP_COPY:
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
                repl[i.dst] = tr.code[*it].dst;
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
            locs.insert(idx);
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
        case OP_COPY:
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
        case OP_COPY:
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
        case OP_COPY:
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
tr_opt_overlap_locations(Trace &tr)
{
    std::vector<nloc_t> lastuse(tr.nlocations, 0);
    for (size_t idx = 0; idx < tr.code.size(); idx++) {
        Instruction &i = tr.code[idx];
        switch (i.op) {
        case OP_OF_VAR: case OP_OF_INT: case OP_OF_NEGINT: case OP_OF_LONGINT:
            break;
        case OP_COPY: case OP_INV: case OP_NEGINV: case OP_NEG: case OP_POW:
            lastuse[i.a] = idx;
            break;
        case OP_ADD: case OP_SUB: case OP_MUL:
            lastuse[i.a] = idx;
            lastuse[i.b] = idx;
            break;
        case OP_TO_INT: case OP_TO_NEGINT: case OP_TO_RESULT:
            lastuse[i.a] = idx;
            break;
        case OP_NOP:
            break;
        }
    }
    std::priority_queue<nloc_t> free = {};
    nloc_t freeceiling = 0;
    std::vector<nloc_t> repl(tr.nlocations, 0);
    for (size_t idx = 0; idx < tr.code.size(); idx++) {
        Instruction &i = tr.code[idx];
        switch (i.op) {
        case OP_OF_VAR: case OP_OF_INT: case OP_OF_NEGINT: case OP_OF_LONGINT:
        case OP_COPY: case OP_INV: case OP_NEGINV: case OP_NEG: case OP_POW:
        case OP_ADD: case OP_SUB: case OP_MUL:
            if (free.size() > 0) {
                i.dst = repl[i.dst] = free.top();
                free.pop();
            } else {
                i.dst = repl[i.dst] = freeceiling++;
            }
            break;
        case OP_TO_INT: case OP_TO_NEGINT: case OP_TO_RESULT:
        case OP_NOP:
            break;
        }
        switch (i.op) {
        case OP_OF_VAR: case OP_OF_INT: case OP_OF_NEGINT: case OP_OF_LONGINT:
            break;
        case OP_COPY: case OP_INV: case OP_NEGINV: case OP_NEG: case OP_POW:
            if (idx == lastuse[i.a]) free.push(repl[i.a]);
            i.a = repl[i.a];
            break;
        case OP_ADD: case OP_SUB: case OP_MUL:
            if (idx == lastuse[i.a]) free.push(repl[i.a]);
            if (idx == lastuse[i.b]) free.push(repl[i.b]);
            i.a = repl[i.a];
            i.b = repl[i.b];
            break;
        case OP_TO_INT: case OP_TO_NEGINT: case OP_TO_RESULT:
            if (idx == lastuse[i.a]) free.push(repl[i.a]);
            i.a = repl[i.a];
            break;
        case OP_NOP:
            break;
        }
    }
    tr.nlocations = freeceiling;
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
    //tr_opt_overlap_locations(tr);
}

/* Trace import
 */

API void
tr_import_fixup(Trace &tr, size_t i1, size_t i2, size_t *inputs, nloc_t out0, nloc_t loc0)
{
    for (size_t idx = i1; idx < i2; idx++) {
        Instruction &i = tr.code[idx];
        switch(i.op) {
        case OP_OF_VAR:
            i = Instruction{i.op, i.dst + loc0, inputs[i.a], 0};
            break;
        case OP_OF_INT: case OP_OF_NEGINT: case OP_OF_LONGINT:
            i = Instruction{i.op, i.dst + loc0, i.a, 0};
            break;
        case OP_COPY: case OP_INV: case OP_NEGINV: case OP_NEG:
            i = Instruction{i.op, i.dst + loc0, i.a + loc0, 0};
            break;
        case OP_POW:
            i = Instruction{i.op, i.dst + loc0, i.a + loc0, i.b};
            break;
        case OP_ADD: case OP_SUB: case OP_MUL:
            i = Instruction{i.op, i.dst + loc0, i.a + loc0, i.b + loc0};
            break;
        case OP_TO_INT: case OP_TO_NEGINT:
            i = Instruction{i.op, 0, i.a + loc0, i.b};
            break;
        case OP_TO_RESULT:
            i = Instruction{i.op, 0, i.a + loc0, i.b + out0};
            break;
        case OP_NOP:
            break;
        }
    }
}

API int
tr_import(Trace &tr, const char *filename)
{
    size_t ninputs0 = tr.ninputs;
    size_t noutputs0 = tr.noutputs;
    size_t nlocations0 = tr.nlocations;
    size_t ninstructions0 = tr.code.size();
    std::vector<size_t> inputs;
    // Read the header
    FILE *f = fopen(filename, "rb");
    if (f == NULL) return 1;
    TraceFileHeader h;
    if (fread(&h, sizeof(TraceFileHeader), 1, f) != 1) goto fail;
    if (h.magic != RATRACER_MAGIC) goto fail;
    // Append instructions
    tr.code.resize(ninstructions0 + h.ninstructions);
    if (fread(&tr.code[ninstructions0], sizeof(Instruction), h.ninstructions, f) != h.ninstructions) goto fail;
    tr.nlocations += h.nlocations;
    // Merge inputs
    inputs.reserve(h.ninputs);
    for (size_t i = 0; i < h.ninputs; i++) {
        uint16_t len = 0;
        if (fread(&len, sizeof(len), 1, f) != 1) goto fail;
        std::string name(len, 0);
        if (len > 0) {
            if (fread(&name[0], len, 1, f) != 1) goto fail;
            for (size_t k = 0; k < tr.input_names.size(); k++) {
                if (tr.input_names[k] == name) {
                    inputs.push_back(k);
                    goto found;
                }
            }
        }
        tr.input_names.push_back(std::move(name));
        inputs.push_back(tr.ninputs++);
    found:;
    }
    // Append outputs
    for (size_t i = 0; i < h.noutputs; i++) {
        uint16_t len = 0;
        if (fread(&len, sizeof(len), 1, f) != 1) goto fail;
        std::string name(len, 0);
        if (len > 0) {
            if (fread(&name[0], len, 1, f) != 1) goto fail;
        }
        tr.output_names.push_back(std::move(name));
        tr.noutputs++;
    }
    // Append constants
    for (size_t i = 0; i < h.nconstants; i++) {
        fmpz x;
        fmpz_init(&x);
        fmpz_inp_raw(&x, f);
        tr.constants.push_back(x);
    }
    fclose(f);
    // Fixup inputs, outputs, and location if needed
    if ((ninstructions0 != 0) || (ninputs0 != 0) || (noutputs0 != 0) || (nlocations0 != 0)) {
        tr_import_fixup(tr, ninstructions0, tr.code.size(), &inputs[0], noutputs0, nlocations0);
    }
    return 0;
fail:;
    fclose(f);
    return 1;
}

API void
tr_replace_variables(Trace &tr, std::map<size_t, Value> varmap, size_t idx1, size_t idx2)
{
    for (size_t idx = idx1; idx < idx2; idx++) {
        Instruction &i = tr.code[idx];
        if (i.op == OP_OF_VAR) {
            auto it = varmap.find(i.a);
            if (it != varmap.end()) {
                i = Instruction{OP_COPY, i.dst, it->second.loc, 0};
            }
        }
    }
}

API void
tr_list_used_inputs(Trace &tr, int *inputs)
{
    for (size_t i = 0; i < tr.ninputs; i++) {
        inputs[i] = 0;
    }
    for (const Instruction &i : tr.code) {
        if (i.op == OP_OF_VAR) {
            inputs[i.a] = 1;
        }
    }
}

/* Trace output
 */

API int
tr_print_disasm(FILE *f, const Trace &tr)
{
    for (const Instruction &i : tr.code) {
        switch (i.op) {
        case OP_OF_VAR: fprintf(f, "%zu = of_var #%zu\n", i.dst, i.a); break;
        case OP_OF_INT: fprintf(f, "%zu = of_int #%zu\n", i.dst, i.a); break;
        case OP_OF_NEGINT: fprintf(f, "%zu = of_negint #%zu\n", i.dst, i.a); break;
        case OP_OF_LONGINT: fprintf(f, "%zu = of_longint #%zu\n", i.dst, i.a); break;
        case OP_COPY: fprintf(f, "%zu = copy %zu\n", i.dst, i.a); break;
        case OP_INV: fprintf(f, "%zu = inv %zu\n", i.dst, i.a); break;
        case OP_NEGINV: fprintf(f, "%zu = neginv %zu\n", i.dst, i.a); break;
        case OP_NEG: fprintf(f, "%zu = neg %zu\n", i.dst, i.a); break;
        case OP_POW: fprintf(f, "%zu = pow %zu #%zu\n", i.dst, i.a, i.b); break;
        case OP_ADD: fprintf(f, "%zu = add %zu %zu\n", i.dst, i.a, i.b); break;
        case OP_SUB: fprintf(f, "%zu = sub %zu %zu\n", i.dst, i.a, i.b); break;
        case OP_MUL: fprintf(f, "%zu = mul %zu %zu\n", i.dst, i.a, i.b); break;
        case OP_TO_INT: fprintf(f, "to_int %zu #%zu\n", i.a, i.b); break;
        case OP_TO_NEGINT: fprintf(f, "to_negint %zu #%zu\n", i.a, i.b); break;
        case OP_TO_RESULT: fprintf(f, "to_result %zu #%zu\n", i.a, i.b); break;
        case OP_NOP: fprintf(f, "nop\n"); break;
        default: fprintf(f, "%zu = op_%d %zu %zu\n", i.dst, i.op, i.a, i.b); break;
        }
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
    fprintf(f, "evaluate(const Trace &restrict tr, const ncoef_t *restrict input, ncoef_t *restrict output, ncoef_t *restrict data, nmod_t mod)\n");
    fprintf(f, "{\n");
    for (const Instruction &i : tr.code) {
        const char *op = "???";
        switch (i.op) {
            case OP_COPY: op = "copy"; break;
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

#define INSTR_OF_VAR(dst, a, b) data[dst] = input[a];
#define INSTR_OF_INT(dst, a, b) data[dst] = a;
#define INSTR_OF_NEGINT(dst, a, b) data[dst] = nmod_neg(a, mod);
#define INSTR_OF_LONGINT(dst, a, b) data[dst] = _fmpz_get_nmod(&constants[a], mod);
#define INSTR_COPY(dst, a, b) data[dst] = data[a];
#define INSTR_INV(dst, a, b) if (unlikely(data[a] == 0)) return 1; data[dst] = nmod_inv(data[a], mod);
#define INSTR_NEGINV(dst, a, b) if (unlikely(data[a] == 0)) return 2; data[dst] = nmod_neg(nmod_inv(data[a], mod), mod);
#define INSTR_NEG(dst, a, b) data[dst] = nmod_neg(data[a], mod);
#define INSTR_POW(dst, a, b) data[dst] = nmod_pow_ui(data[a], b, mod);
#define INSTR_ADD(dst, a, b) data[dst] = _nmod_add(data[a], data[b], mod);
#define INSTR_SUB(dst, a, b) data[dst] = _nmod_sub(data[a], data[b], mod);
#define INSTR_MUL(dst, a, b) data[dst] = nmod_mul(data[a], data[b], mod);
#define INSTR_TO_INT(dst, a, b) if (unlikely(data[a] != b)) return 3;
#define INSTR_TO_NEGINT(dst, a, b) if (unlikely(data[a] != nmod_neg(b, mod))) return 4;
#define INSTR_TO_RESULT(dst, a, b) output[b] = data[a];
#define INSTR_NOP(dst, a, b)
#define INSTR_HALT(dst, a, b) return 0;

API int
tr_evaluate(const Trace &restrict tr, const ncoef_t *restrict input, ncoef_t *restrict output, ncoef_t *restrict data, nmod_t mod)
{
    const auto &constants = tr.constants;
    for (const Instruction i : tr.code) {
        switch(i.op) {
        case OP_OF_VAR: INSTR_OF_VAR(i.dst, i.a, i.b); break;
        case OP_OF_INT: INSTR_OF_INT(i.dst, i.a, i.b); break;
        case OP_OF_NEGINT: INSTR_OF_NEGINT(i.dst, i.a, i.b); break;
        case OP_OF_LONGINT: INSTR_OF_LONGINT(i.dst, i.a, i.b); break;
        case OP_COPY: INSTR_COPY(i.dst, i.a, i.b); break;
        case OP_INV: INSTR_INV(i.dst, i.a, i.b); break;
        case OP_NEGINV: INSTR_NEGINV(i.dst, i.a, i.b); break;
        case OP_NEG: INSTR_NEG(i.dst, i.a, i.b); break;
        case OP_POW: INSTR_POW(i.dst, i.a, i.b); break;
        case OP_ADD: INSTR_ADD(i.dst, i.a, i.b); break;
        case OP_SUB: INSTR_SUB(i.dst, i.a, i.b); break;
        case OP_MUL: INSTR_MUL(i.dst, i.a, i.b); break;
        case OP_TO_INT: INSTR_TO_INT(i.dst, i.a, i.b); break;
        case OP_TO_NEGINT: INSTR_TO_NEGINT(i.dst, i.a, i.b); break;
        case OP_TO_RESULT: INSTR_TO_RESULT(i.dst, i.a, i.b); break;
        case OP_NOP: INSTR_NOP(i.dst, i.a, i.b); break;
        case OP_HALT: INSTR_HALT(i.dst, i.a, i.b); break;
        }
    }
    return 0;
}

API int
tr_evaluate_faster(const Trace &restrict tr, const ncoef_t *restrict input, ncoef_t *restrict output, ncoef_t *restrict data, nmod_t mod)
{
    const auto &constants = tr.constants;
    static void* jumptable[] = {
        &&do_OF_VAR,
        &&do_OF_INT,
        &&do_OF_NEGINT,
        &&do_OF_LONGINT,
        &&do_COPY,
        &&do_INV,
        &&do_NEGINV,
        &&do_NEG,
        &&do_POW,
        &&do_ADD,
        &&do_SUB,
        &&do_MUL,
        &&do_TO_INT,
        &&do_TO_NEGINT,
        &&do_TO_RESULT,
        &&do_NOP,
        &&do_HALT
    };
    const Instruction *pi = &tr.code[0];
    Instruction i = *pi++;
    goto *jumptable[i.op];
#define INSTR(opname) \
        do_ ## opname:; { \
            INSTR_ ## opname(i.dst, i.a, i.b); \
            i = *pi++; \
            goto *jumptable[i.op]; \
        }
    for (;;) {
        INSTR(OF_VAR);
        INSTR(OF_INT);
        INSTR(OF_NEGINT);
        INSTR(OF_LONGINT);
        INSTR(COPY);
        INSTR(INV);
        INSTR(NEGINV);
        INSTR(NEG);
        INSTR(POW);
        INSTR(ADD);
        INSTR(SUB);
        INSTR(MUL);
        INSTR(TO_INT);
        INSTR(TO_NEGINT);
        INSTR(TO_RESULT);
        INSTR(NOP);
        INSTR(HALT);
    }
#undef INSTR
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

#define MAX_FAMILIES 8
#define MAX_INDICES 11
#define MIN_INDEX -11
#define MAX_INDEX 11
// (1+MAX_INDEX-MIN_INDEX)^MAX_INDICES*max(1+MAX_INDEX,1-MIN_INDEX)*MAX_INDICES*MAX_INDICES-1
#define MAX_NAME_NUMBER 1383479768491022003ull

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
            eqn.id = eqs.equations.size();
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
    bool paranoid = false;
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
                if (paranoid) tr_to_int(r, 0);
            }
            i2++;
        } else {
            Value r = tr_addmul(a.terms[i1].coef, b.terms[i2].coef, bfactor);
            if (r.n != 0) {
                res.terms.push_back(Term{a.terms[i1].integral, r});
                res.len++;
            } else {
                if (paranoid) tr_to_int(r, 0);
            }
            i1++;
            i2++;
        }
    }
    for (; i1 < a.len; i1++) {
        if (i1 == idx) { continue; }
        res.terms.push_back(a.terms[i1]);
        res.len++;
    }
    for (; i2 < b.len; i2++) {
        Value r = tr_mul(b.terms[i2].coef, bfactor);
        if (r.n != 0) {
            res.terms.push_back(Term{b.terms[i2].integral, r});
            res.len++;
        } else {
            if (paranoid) tr_to_int(r, 0);
        }
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
    bool paranoid = false;
    Value minus1 = tr_of_int(-1);
    Equation res = {};
    std::make_heap(neqns.begin(), neqns.end(), neqn_is_better);
    size_t n = neqns.size();
    while (n > 0) {
        std::pop_heap(neqns.begin(), neqns.begin() + n--, neqn_is_better);
        Equation &neqnx = neqns[n];
        if (neqnx.len == 0) { continue; }
        if (neqnx.terms[0].coef.n != minus1.n) {
            Value nic = tr_neginv(neqnx.terms[0].coef);
            neqnx.terms[0].coef = minus1;
            for (size_t i = 1; i < neqnx.len; i++) {
                neqnx.terms[i].coef = tr_mul(neqnx.terms[i].coef, nic);
            }
        } else {
            if (paranoid) tr_to_int(neqnx.terms[0].coef, -1);
        }
        while (n > 0) {
            Equation &neqn = neqns[0];
            if (neqn.len == 0) {
                std::pop_heap(neqns.begin(), neqns.begin() + n--, neqn_is_better);
            } else if (neqn.terms[0].integral == neqnx.terms[0].integral) {
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
        if (neqn.len == 0) continue;
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
