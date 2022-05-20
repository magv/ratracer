/* Rational Tracer records a trace of rational operations performed
 * on a set of variables (represented modulo a 63-bit prime),
 * so that the trace could be analyzed, optimized, re-evaluated
 * multiple times, and eventually reconstructed as a rational
 * expression by the Rational Toolbox.
 */

#ifndef RATRACER_H
#define RATRACER_H

#include <flint/fmpz.h>
#include <flint/nmod_vec.h>
#include <stdint.h>
#include <string.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#define API __attribute__((unused)) static
#define noreturn __attribute__((noreturn))
#define packed __attribute__((packed))
#define restrict __restrict__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* Name table
 */

struct NameTable {
    size_t nnames;
    size_t nalloc;
    char *names;
    unsigned exp;
};

static void
_nt_resize(NameTable &nt, size_t nnames, size_t nbytes)
{
    unsigned exp = nt.exp;
    size_t nalloc = nt.nalloc >= 4 ? nt.nalloc : 4;
    while (((size_t)1 << exp) < nbytes) exp++;
    while (nalloc < nnames) nalloc *= 2;
    char *names = (char*)calloc(nalloc, (size_t)1 << exp);
    if (unlikely(names == NULL)) { fprintf(stderr, "_nt_resize(): failed to allocate %zu bytes\n", (size_t)1 << exp); exit(1); };
    if (nt.names != NULL) {
        for (size_t i = 0; i < nt.nnames; i++) {
            memcpy(&names[i << exp], &nt.names[i << nt.exp], (size_t)1 << nt.exp);
        }
        free(nt.names);
    }
    nt = NameTable { nt.nnames, nalloc, names, exp };
}

API void
nt_resize(NameTable &nt, size_t nnames)
{
    if (nnames >= nt.nalloc) _nt_resize(nt, nnames, (size_t)1 << nt.exp);
    nt.nnames = nnames;
}

API ssize_t
nt_lookup(const NameTable &nt, const char *name, size_t size)
{
    if (size >= ((size_t)1 << nt.exp)) return -1;
    for (size_t i = 0; i < nt.nnames; i++) {
        size_t offs = i << nt.exp;
        if ((memcmp(&nt.names[offs], name, size) == 0) && (nt.names[offs + size] == 0)) return (ssize_t)i;
    }
    return -1;
}

API const char *
nt_get(const NameTable &nt, size_t index)
{
    if (unlikely(index >= nt.nnames)) { fprintf(stderr, "nt_get(): index %zu >= %zu\n", index, nt.nnames); abort(); };
    return &nt.names[index << nt.exp];
}

API size_t
nt_append(NameTable &nt, const char *name, size_t size)
{
    if ((nt.nnames >= nt.nalloc) || (size >= ((size_t)1 << nt.exp))) {
        _nt_resize(nt, nt.nnames+1, size+1);
    }
    size_t index = nt.nnames++;
    memset(&nt.names[index << nt.exp], 0, (size_t)1 << nt.exp);
    memcpy(&nt.names[index << nt.exp], name, size);
    return index;
}

API void
nt_set(NameTable &nt, size_t index, const char *name, size_t size)
{
    if (unlikely(index >= nt.nnames)) { fprintf(stderr, "nt_set(): index %zu >= %zu\n", index, nt.nnames); abort(); };
    if (size >= ((size_t)1 << nt.exp)) {
        _nt_resize(nt, nt.nnames, size+1);
    }
    memset(&nt.names[index << nt.exp], 0, (size_t)1 << nt.exp);
    memcpy(&nt.names[index << nt.exp], name, size);
}

API void
nt_clear(NameTable &nt)
{
    free(nt.names);
    nt = NameTable{};
}

/* Rational tracer
 */

typedef uint64_t nloc_t;
typedef mp_limb_t ncoef_t;
struct Value { nloc_t loc; ncoef_t n; };

// Opcodes:
// - dst/-/-:
//   - dst = of_var #a
//   - dst = of_int #a
//   - dst = of_negint #a
//   - dst = of_longint #a
// - dst/a/-:
//   - dst = copy a
//   - dst = inv a
//   - dst = neginv a
//   - dst = neg a
//   - dst = pow a #b
// - dst/a/b:
//   - dst = add a b
//   - dst = sub a b
//   - dst = mul a b
// - -/a/-:
//   - ___ = to_int a #b
//   - ___ = to_negint a #b
//   - ___ = to_result a #b
// - -/-/-:
//   - ___ = nop
// Format:
// - op:1 dst:5 a:5 b:5

enum {
    OP_OF_VAR,
    OP_OF_INT,
    OP_OF_NEGINT,
    OP_OF_LONGINT,
    OP_COPY,
    OP_INV,
    OP_NEGINV,
    OP_NEG,
    OP_POW,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_TO_INT,
    OP_TO_NEGINT,
    OP_TO_RESULT,
    OP_NOP
};

struct packed Instruction {
    uint8_t op:8;
    uint64_t dst:40;
    uint64_t a:40;
    uint64_t b:40;
};

#define IMM_MAX 0xFFFFFFFFFFll
#define LOC_MAX IMM_MAX

struct Trace {
    nloc_t ninputs;
    nloc_t noutputs;
    nloc_t nlocations;
    std::vector<Instruction> code;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<fmpz> constants;
};

struct Tracer {
    nmod_t mod;
    Trace t;
    std::unordered_map<int64_t, Value> const_cache;
    std::unordered_map<size_t, Value> var_cache;
    NameTable var_names;
};

// TODO: drop this global
static Tracer tr;

API void
tr_init()
{
    nmod_init(&tr.mod, 0x7FFFFFFFFFFFFFE7ull); // 2^63-25
}

API ncoef_t
ncoef_hash(size_t idx, ncoef_t mod)
{
    uint64_t h = (uint64_t)(idx + 1)*0x9E3779B185EBCA87ull; // XXH_PRIME64_1
    ncoef_t val;
    do {
        h ^= h >> 33;
        h *= 0xC2B2AE3D27D4EB4Full; // XXH_PRIME64_2;
        h ^= h >> 29;
        h *= 0x165667B19E3779F9ull; // XXH_PRIME64_3;
        h ^= h >> 32;
        val = h & 0x7FFFFFFFFFFFFFFFull;
    } while (val >= mod);
    return val;
}

API void
tr_append(const Instruction &i)
{
    tr.t.code.push_back(i);
}

API Value
tr_of_var(size_t idx)
{
    auto it = tr.var_cache.find(idx);
    if (it == tr.var_cache.end()) {
        ncoef_t val = ncoef_hash(idx, tr.mod.n);
        tr_append(Instruction{OP_OF_VAR, tr.t.nlocations, (nloc_t)idx, 0});
        if (idx + 1 > tr.t.ninputs) tr.t.ninputs = idx + 1;
        Value v = Value{tr.t.nlocations++, val};
        tr.var_cache[idx] = v;
        return v;
    } else {
        return it->second;
    }
}

API Value
tr_of_int(int64_t x)
{
    auto it = tr.const_cache.find(x);
    if (it == tr.const_cache.end()) {
        ncoef_t c;
        if (x >= 0) {
            tr_append(Instruction{OP_OF_INT, tr.t.nlocations, (nloc_t)x, 0});
            NMOD_RED(c, x, tr.mod);
        } else {
            tr_append(Instruction{OP_OF_NEGINT, tr.t.nlocations, (nloc_t)-x, 0});
            NMOD_RED(c, -x, tr.mod);
            c = nmod_neg(c, tr.mod);
        }
        Value v = Value{tr.t.nlocations++, c};
        tr.const_cache[x] = v;
        return v;
    } else {
        return it->second;
    }
}

/* This function is missing in FLINT 2.8.2 */
mp_limb_t _fmpz_get_nmod(const fmpz_t aa, nmod_t mod)
{
    fmpz A = *aa;
    mp_limb_t r, SA, UA;
    if (!COEFF_IS_MPZ(A))
    {
        SA = FLINT_SIGN_EXT(A);
        UA = FLINT_ABS(A);
        NMOD_RED(r, UA, mod);
    }
    else
    {
        mpz_srcptr a = COEFF_TO_PTR(A);
        mp_srcptr ad = a->_mp_d;
        slong an = a->_mp_size;
        if (an < 0)
        {
            SA = -UWORD(1);
            an = -an;
        }
        else
        {
            SA = 0;
        }
        if (an < 5)
        {
            r = 0;
            while (an > 0)
            {
                NMOD_RED2(r, r, ad[an - 1], mod);
                an--;
            }
        }
        else
        {
            r = mpn_mod_1(ad, an, mod.n);
        }
    }
    return (SA == 0 || r == 0) ? r : (mod.n - r);
}

API Value
tr_of_fmpz(const fmpz_t x)
{
    if (fmpz_fits_si(x) && (fmpz_bits(x) < 40)) {
        return tr_of_int(fmpz_get_si(x));
    } else {
        tr_append(Instruction{OP_OF_LONGINT, tr.t.nlocations, (nloc_t)tr.t.constants.size(), 0});
        fmpz xx;
        fmpz_init_set(&xx, x);
        tr.t.constants.push_back(xx);
        return Value{tr.t.nlocations++, _fmpz_get_nmod(x, tr.mod)};
    }
}

API Value
tr_mul(const Value &a, const Value &b)
{
    tr_append(Instruction{OP_MUL, tr.t.nlocations, a.loc, b.loc});
    return Value{tr.t.nlocations++, nmod_mul(a.n, b.n, tr.mod)};
}

API Value
tr_pow(const Value &base, unsigned exp)
{
    switch (exp) {
    case 0: return tr_of_int(1);
    case 1: return base;
    case 2: return tr_mul(base, base);
    default:
        tr_append(Instruction{OP_POW, tr.t.nlocations, base.loc, exp});
        return Value{tr.t.nlocations++, nmod_pow_ui(base.n, exp, tr.mod)};
    }
}

API Value
tr_add(const Value &a, const Value &b)
{
    tr_append(Instruction{OP_ADD, tr.t.nlocations, a.loc, b.loc});
    return Value{tr.t.nlocations++, _nmod_add(a.n, b.n, tr.mod)};
}

API Value
tr_sub(const Value &a, const Value &b)
{
    tr_append(Instruction{OP_SUB, tr.t.nlocations, a.loc, b.loc});
    return Value{tr.t.nlocations++, _nmod_sub(a.n, b.n, tr.mod)};
}

API Value
tr_mulpow(const Value &src, const Value &base, unsigned exp)
{
    if (exp == 0) return src;
    return tr_mul(src, tr_pow(base, exp));
}

API Value
tr_addmul(const Value &a, const Value &b, const Value &bfactor)
{
    return tr_add(a, tr_mul(b, bfactor));
}

API Value
tr_inv(const Value &a)
{
    tr_append(Instruction{OP_INV, tr.t.nlocations, a.loc, 0});
    return Value{tr.t.nlocations++, nmod_inv(a.n, tr.mod)};
}

API Value
tr_neginv(const Value &a)
{
    tr_append(Instruction{OP_NEGINV, tr.t.nlocations, a.loc, 0});
    return Value{tr.t.nlocations++, nmod_neg(nmod_inv(a.n, tr.mod), tr.mod)};
}

API Value
tr_neg(const Value &a)
{
    tr_append(Instruction{OP_NEG, tr.t.nlocations, a.loc, 0});
    return Value{tr.t.nlocations++, nmod_neg(a.n, tr.mod)};
}

API Value
tr_div(const Value &a, const Value &b)
{
    return tr_mul(a, tr_inv(b));
}

API void
tr_to_int(const Value &a, int64_t n)
{
    if (n >= 0) {
        tr_append(Instruction{OP_TO_INT, 0, a.loc, (uint64_t)n});
    } else {
        tr_append(Instruction{OP_TO_NEGINT, 0, a.loc, (uint64_t)-n});
    }
}

API void
tr_to_result(size_t outidx, const Value &src)
{
    if (outidx + 1 > tr.t.noutputs) tr.t.noutputs = outidx + 1;
    tr_append(Instruction{OP_TO_RESULT, 0, src.loc, outidx});
}

API void
tr_set_var_name(size_t idx, const char *name, size_t len)
{
    if (idx >= tr.t.input_names.size()) tr.t.input_names.resize(idx + 1);
    tr.t.input_names[idx] = std::string(name, len);
    if (idx + 1 > tr.t.ninputs) {
        tr.t.ninputs = idx + 1;
        nt_resize(tr.var_names, idx + 1);
    }
    nt_set(tr.var_names, idx, name, len);
}

API void
tr_set_var_name(size_t idx, const char *name)
{
    tr_set_var_name(idx, name, strlen(name));
}

API void
tr_set_result_name(size_t idx, const char *name)
{
    if (idx >= tr.t.output_names.size()) tr.t.output_names.resize(idx + 1);
    tr.t.output_names[idx] = std::string(name);
    if (idx + 1 > tr.t.noutputs) tr.t.noutputs = idx + 1;
}

/* Trace export to file
 *
 * The file format is:
 * - TraceFileHeader{...}
 * - Instruction{...} for each instruction
 * - { u16 len; u8 name[len]; } for each input
 * - { u16 len; u8 name[len]; } for each output
 * - { u32 len; u8 value[len]; } for each big constant (GMP format)
 */

struct packed TraceFileHeader {
    uint64_t magic;
    uint32_t ninputs;
    uint32_t noutputs;
    uint32_t nconstants;
    uint64_t nlocations;
    uint64_t ninstructions;
};

static const uint64_t RATRACER_MAGIC = 0x3230303043524052ull;

API int
tr_export(const char *filename, const Trace &t)
{
    FILE *f = fopen(filename, "wb");
    if (f == NULL) return 1;
    TraceFileHeader h = {
        RATRACER_MAGIC,
        (uint32_t)t.ninputs,
        (uint32_t)t.noutputs,
        (uint32_t)t.constants.size(),
        t.nlocations,
        t.code.size()
    };
    if (fwrite(&h, sizeof(TraceFileHeader), 1, f) != 1) goto fail;
    if (fwrite(&t.code[0], sizeof(Instruction), t.code.size(), f) != t.code.size()) goto fail;
    for (size_t i = 0; i < t.ninputs; i++) {
        if (i < t.input_names.size()) {
            const auto &n = t.input_names[i];
            uint16_t len = n.size();
            if (fwrite(&len, sizeof(len), 1, f) != 1) goto fail;
            if (len > 0) {
                if (fwrite(&n[0], len, 1, f) != 1) goto fail;
            }
        } else {
            uint16_t len = 0;
            if (fwrite(&len, sizeof(len), 1, f) != 1) goto fail;
        }
    }
    for (size_t i = 0; i < t.noutputs; i++) {
        if (i < t.output_names.size()) {
            const auto &n = t.output_names[i];
            uint16_t len = n.size();
            if (fwrite(&len, sizeof(len), 1, f) != 1) goto fail;
            if (len > 0) {
                if (fwrite(&n[0], len, 1, f) != 1) goto fail;
            }
        } else {
            uint16_t len = 0;
            if (fwrite(&len, sizeof(len), 1, f) != 1) goto fail;
        }
    }
    for (size_t i = 0; i < t.constants.size(); i++) {
        fmpz_out_raw(f, &t.constants[i]);
    }
    return fclose(f);
fail:;
    fclose(f);
    return 1;
}

#endif // RATRACER_H
