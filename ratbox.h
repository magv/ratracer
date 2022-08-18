/* Rational Toolbox is a collection of functions to generate and
 * work with the traces (produced by Rational Tracer).
 */

#ifndef RATBOX_H
#define RATBOX_H

#include <algorithm>
#include <inttypes.h>
#include <map>
#include <math.h>
#include <queue>
#include <set>
#include <sys/uio.h>
#include <flint/fmpq.h>

/* Trace optimization
 */

API size_t
tr_erase_outputs(Trace &tr, size_t keep)
{
    assert(keep < tr.noutputs);
    size_t nerased = 0;
    CODE_PAGEITER_BEGIN(tr.fincode, 1)
        PAGEWRITE = 0;
        LOOP_ITER_BEGIN(PAGE, PAGEEND)
            if (OP == LOP_OUTPUT) {
                PAGEWRITE = 1;
                if (B != keep) {
                    ((LoOp0*)INSTR)[0] = LoOp0{LOP_NOP};
                    ((LoOp0*)INSTR)[1] = LoOp0{LOP_NOP};
                    ((LoOp0*)INSTR)[2] = LoOp0{LOP_NOP};
                    nerased++;
                } else {
                    *(LoOp2*)INSTR = LoOp2{LOP_OUTPUT, A, 0};
                }
            }
        LOOP_ITER_END(PAGE, PAGEEND)
    CODE_PAGEITER_END()
    CODE_PAGEITER_BEGIN(tr.code, 1)
        PAGEWRITE = 0;
        HIOP_ITER_BEGIN(PAGE, PAGEEND)
            if (OP == HOP_OUTPUT) {
                PAGEWRITE = 1;
                if (B != keep) {
                    *INSTR = HiOp{HOP_NOP, 0, 0, 0};
                    nerased++;
                } else {
                    *INSTR = HiOp{HOP_OUTPUT, A, 0, 0};
                }
            }
        HIOP_ITER_END(PAGE, PAGEEND)
    CODE_PAGEITER_END()
    if (keep != 0) std::swap(tr.output_names[0], tr.output_names[keep]);
    tr.noutputs = 1;
    tr.output_names.resize(1);
    return nerased;
}

API size_t
tr_map_outputs(Trace &tr, ssize_t *outmap)
{
    size_t nerased = 0;
    CODE_PAGEITER_BEGIN(tr.fincode, 1)
        PAGEWRITE = 0;
        LOOP_ITER_BEGIN(PAGE, PAGEEND)
            if (OP == LOP_OUTPUT) {
                PAGEWRITE = 1;
                if (outmap[B] < 0) {
                    ((LoOp0*)INSTR)[0] = LoOp0{LOP_NOP};
                    ((LoOp0*)INSTR)[1] = LoOp0{LOP_NOP};
                    ((LoOp0*)INSTR)[2] = LoOp0{LOP_NOP};
                    nerased++;
                } else {
                    *(LoOp2*)INSTR = LoOp2{LOP_OUTPUT, A, (uint32_t)outmap[B]};
                }
            }
        LOOP_ITER_END(PAGE, PAGEEND)
    CODE_PAGEITER_END()
    CODE_PAGEITER_BEGIN(tr.code, 1)
        PAGEWRITE = 0;
        HIOP_ITER_BEGIN(PAGE, PAGEEND)
            if (OP == HOP_OUTPUT) {
                PAGEWRITE = 1;
                if (outmap[B] < 0) {
                    *INSTR = HiOp{HOP_NOP, 0, 0, 0};
                    nerased++;
                } else {
                    *INSTR = HiOp{HOP_OUTPUT, A, (uint64_t)outmap[B], 0};
                }
            }
        HIOP_ITER_END(PAGE, PAGEEND)
    CODE_PAGEITER_END()
    std::vector<std::string> output_names;
    output_names.resize(tr.noutputs);
    size_t noutputs = 0;
    for (size_t i = 0; i < tr.noutputs; i++) {
        if (outmap[i] >= 0) {
            std::swap(tr.output_names[i], output_names[outmap[i]]);
            noutputs++;
        }
    }
    std::swap(tr.output_names, output_names);
    tr.noutputs = noutputs;
    return nerased;
}


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

static HiOp
instr_imm(int64_t value)
{
    if (value >= 0) return HiOp{HOP_INT, (uint64_t)value, 0, 0};
    else return HiOp{HOP_NEGINT, (uint64_t)-value, 0, 0};
}

API size_t
tr_opt_propagate_constants(Trace &tr)
{
    size_t nreplaced = 0;
    std::unordered_map<nloc_t, int64_t> values;
    std::unordered_map<nloc_t, nloc_t> repl;
    nloc_t DST = tr.nfinlocations;
    CODE_PAGEITER_BEGIN(tr.code, 1)
    HIOP_ITER_BEGIN(PAGE, PAGEEND)
#define needA A = maybe_replace(A, repl); const auto &itA = values.find(A); bool knowA = itA != values.end(); (void)knowA;
#define needB B = maybe_replace(B, repl); const auto &itB = values.find(B); bool knowB = itB != values.end(); (void)knowB;
#define needC C = maybe_replace(C, repl); const auto &itC = values.find(C); bool knowC = itC != values.end(); (void)knowC;
#define valA (itA->second)
#define valB (itB->second)
#define valC (itC->second)
#define replace_instr(...) *(HiOp*)INSTR = (__VA_ARGS__); nreplaced++;
#define replace_imm(val) replace_instr(instr_imm(values[DST] = (val)))
#define update_instr() *(HiOp*)INSTR = HiOp{OP, A, B, C};
        switch(OP) {
        case HOP_VAR: break;
        case HOP_INT: values[DST] = (int64_t)A; break;
        case HOP_NEGINT: values[DST] = -(int64_t)A; break;
        case HOP_BIGINT: break;
        case HOP_COPY: {
                needA;
                repl[DST] = A;
                replace_instr(HiOp{HOP_NOP, 0, 0, 0})
            }
            break;
        case HOP_INV: {
                needA;
                if (knowA && (valA == 1)) { replace_imm(1); break; }
                if (knowA && (valA == -1)) { replace_imm(-1); break; }
                update_instr();
            }
            break;
        case HOP_NEGINV: {
                needA;
                if (knowA && (valA == 1)) { replace_imm(-1); break; }
                if (knowA && (valA == -1)) { replace_imm(1); break; }
                update_instr();
            }
            break;
        case HOP_NEG: {
                needA;
                if (knowA) { replace_imm(-valA); break; }
                update_instr();
            }
            break;
        case HOP_SHOUP_PRECOMP: {
                needA;
                update_instr();
            }
            break;
        case HOP_POW: {
                needA;
                if (knowA && (valA == 0)) { replace_imm(0); break; }
                if (knowA) {
                    int64_t a = valA, r = 1;
                    for (uint64_t n = 0; n < B; n++) {
                        if (abs(r) <= IMM_MAX / abs(a)) { r *= a; } else { r = IMM_MAX+1; break; }
                    }
                    if (abs(r) <= IMM_MAX) { replace_imm(r); break; }
                }
                update_instr();
            }
            break;
        case HOP_ADD: {
                needA;
                needB;
                if (knowA && knowB) {
                    int64_t r = valA + valB;
                    if (abs(r) <= IMM_MAX) { replace_imm(r); break; }
                }
                if (knowA && (valA == 0)) { repl[DST] = B; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                if (knowB && (valB == 0)) { repl[DST] = A; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                update_instr();
            }
            break;
        case HOP_SUB: {
                needA;
                needB;
                if (knowA && knowB) {
                    int64_t r = valA - valB;
                    if (abs(r) <= IMM_MAX) { replace_imm(r); break; }
                }
                if (knowA && (valA == 0)) { replace_instr(HiOp{HOP_NEG, B, 0, 0}); break; }
                if (knowB && (valB == 0)) { repl[DST] = A; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                update_instr();
            }
            break;
        case HOP_MUL: {
                needA;
                needB;
                if (knowA && knowB) {
                    if ((valB == 0) || (abs(valA) <= IMM_MAX/abs(valB))) { replace_imm(valA*valB); break; }
                }
                if (knowA && (valA == 0)) { replace_imm(0); break; }
                if (knowA && (valA == 1)) { repl[DST] = B; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                if (knowA && (valA == -1)) { replace_instr(HiOp{HOP_NEG, B, 0, 0}); break; }
                if (knowB && (valB == 0)) { replace_imm(0); break; }
                if (knowB && (valB == 1)) { repl[DST] = A; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                if (knowB && (valB == -1)) { replace_instr(HiOp{HOP_NEG, A, 0, 0}); break; }
                update_instr();
            }
            break;
        case HOP_SHOUP_MUL: {
                needA;
                needB;
                needC;
                if (knowA && knowC) {
                    if ((valC == 0) || (abs(valA) <= IMM_MAX/abs(valC))) { replace_imm(valA*valC); break; }
                }
                if (knowA && (valA == 0)) { replace_imm(0); break; }
                if (knowA && (valA == 1)) { repl[DST] = C; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                if (knowA && (valA == -1)) { replace_instr(HiOp{HOP_NEG, C, 0, 0}); break; }
                if (knowC && (valC == 0)) { replace_imm(0); break; }
                if (knowC && (valC == 1)) { repl[DST] = A; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                if (knowC && (valC == -1)) { replace_instr(HiOp{HOP_NEG, A, 0, 0}); break; }
                update_instr();
            }
            break;
        case HOP_ADDMUL: {
                needA;
                needB;
                needC;
                if (knowB && (valB == 0)) { repl[DST] = A; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                if (knowB && (valB == 1)) { replace_instr(HiOp{HOP_ADD, A, C, 0}); break; }
                if (knowB && (valB == -1)) { replace_instr(HiOp{HOP_SUB, A, C, 0}); break; }
                if (knowC && (valC == 0)) { repl[DST] = A; replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                if (knowC && (valC == 1)) { replace_instr(HiOp{HOP_ADD, A, B, 0}); break; }
                if (knowC && (valC == -1)) { replace_instr(HiOp{HOP_SUB, A, B, 0}); break; }
                if (knowA && (valA == 0)) { replace_instr(HiOp{HOP_MUL, B, C, 0}); break; }
                update_instr();
            }
            break;
        case HOP_ASSERT_INT: {
                needA;
                if (knowA && (valA == (int64_t)B)) { replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                update_instr();
            }
            break;
        case HOP_ASSERT_NEGINT: {
                needA;
                if (knowA && (valA == -(int64_t)B)) { replace_instr(HiOp{HOP_NOP, 0, 0, 0}); break; }
                update_instr();
            }
            break;
        case HOP_OUTPUT: {
                needA;
                update_instr();
            }
            break;
        case HOP_NOP:
            break;
        case HOP_HALT:
            DST += (HiOp*)PAGEEND - (HiOp*)INSTR;
            goto halt;
        }
        DST++;
    HIOP_ITER_END(PAGE, PAGEEND)
halt:;
    CODE_PAGEITER_END()
    return nreplaced;
}

struct InstructionHash {
    const uint8_t *code;
    inline size_t operator()(const nloc_t idx) const {
        uint64_t A = *(uint64_t*)ASSUME_ALIGNED(code + idx*sizeof(HiOp), 8);
        uint64_t B = *(uint64_t*)ASSUME_ALIGNED(code + idx*sizeof(HiOp) + 8, 8);
        size_t h = A*0x9E3779B185EBCA87ull; // XXH_PRIME64_1
        h ^= h >> 33;
        h *= 0xC2B2AE3D27D4EB4Full; // XXH_PRIME64_2;
        h += B;
        h ^= h >> 33;
        h *= 0xC2B2AE3D27D4EB4Full; // XXH_PRIME64_2;
        h ^= h >> 29;
        h *= 0x165667B19E3779F9ull; // XXH_PRIME64_3;
        h ^= h >> 32;
        return h;
    }
};

struct InstructionEq {
    const uint8_t *code;
    inline bool operator()(const nloc_t aidx, const nloc_t bidx) const {
        void *p1 = ASSUME_ALIGNED(code + aidx*sizeof(HiOp), sizeof(HiOp));
        void *p2 = ASSUME_ALIGNED(code + bidx*sizeof(HiOp), sizeof(HiOp));
        return memcmp(p1, p2, sizeof(HiOp)) == 0;
    }
};

API size_t
tr_opt_deduplicate(Trace &tr)
{
    size_t nreplaced = 0;
    nloc_t DST = tr.nfinlocations;
    CODE_PAGEITER_BEGIN(tr.code, 1)
    nloc_t DST0 = DST;
    std::unordered_set<nloc_t, InstructionHash, InstructionEq>
        locs(0, InstructionHash{PAGE - DST0*sizeof(HiOp)}, InstructionEq{PAGE - DST0*sizeof(HiOp)});
    std::unordered_map<nloc_t, nloc_t> repl;
#define lookup(loc) (((loc) < DST0) ? (loc) : maybe_replace(loc, repl))
    HIOP_ITER_BEGIN(PAGE, PAGEEND)
        uint64_t newA, newB, newC;
        switch(OP) {
        case HOP_VAR: case HOP_INT: case HOP_NEGINT: case HOP_BIGINT:
            break;
        case HOP_COPY: case HOP_INV: case HOP_NEGINV: case HOP_NEG: case HOP_SHOUP_PRECOMP:
            *INSTR = HiOp{OP, lookup(A), 0, 0};
            break;
        case HOP_POW:
            *INSTR = HiOp{OP, lookup(A), B, 0};
            break;
        case HOP_ADD:
            newA = lookup(A);
            newB = lookup(B);
            if (newA > newB) { auto t = newA; newA = newB; newB = t; }
            *INSTR = HiOp{OP, newA, newB, 0};
            break;
        case HOP_SUB:
            newA = lookup(A);
            newB = lookup(B);
            if (newA != newB) {
                *INSTR = HiOp{OP, newA, newB, 0};
            } else {
                *INSTR = HiOp{HOP_INT, 0, 0, 0};
            }
            break;
        case HOP_MUL:
            newA = lookup(A);
            newB = lookup(B);
            if (newA > newB) { auto t = newA; newA = newB; newB = t; }
            *INSTR = HiOp{OP, newA, newB, 0};
            break;
        case HOP_SHOUP_MUL:
            newA = lookup(A);
            newB = lookup(B);
            newC = lookup(C);
            *INSTR = HiOp{OP, newA, newB, newC};
            break;
        case HOP_ADDMUL:
            newA = lookup(A);
            newB = lookup(B);
            newC = lookup(C);
            if (newB > newC) { auto t = newB; newB = newC; newC = t; }
            *INSTR = HiOp{OP, newA, newB, newC};
            break;
        case HOP_ASSERT_INT: case HOP_ASSERT_NEGINT: case HOP_OUTPUT:
            *INSTR = HiOp{OP, lookup(A), B, 0};
            break;
        case HOP_NOP:
            break;
        case HOP_HALT:
            DST += (HiOp*)PAGEEND - (HiOp*)INSTR;
            goto halt;
        }
        if (OP != HOP_NOP) {
            const auto it = locs.find(DST);
            if (it != locs.end()) {
                repl[DST] = *it;
                //*INSTR = HiOp{HOP_NOP, 0, 0, 0};
                nreplaced++;
            } else {
                locs.insert(DST);
            }
        }
        DST++;
    HIOP_ITER_END(PAGE, PAGEEND)
halt:;
    CODE_PAGEITER_END()
    return nreplaced;
}

API size_t
tr_opt_erase_asserts(Trace &tr)
{
    size_t nerased = 0;
    CODE_ITER_BEGIN(tr.code, 1)
        if ((OP == HOP_ASSERT_INT) || (OP == HOP_ASSERT_NEGINT)) {
            *INSTR = HiOp{HOP_NOP, 0, 0, 0};
            nerased++;
        }
    CODE_ITER_END()
    return nerased;
}

API size_t
tr_opt_erase_dead_code(Trace &tr, size_t nroots, const Value *roots)
{
    std::unordered_set<nloc_t> live;
    for (size_t i = 0; i < nroots; i++) live.insert(roots[i].loc);
    size_t nerased = 0;
    nloc_t DST = tr.nextloc;
    CODE_REVPAGEITER_BEGIN(tr.code, 1)
    HIOP_REVITER_BEGIN(PAGE, PAGEEND)
        DST--;
        if ((OP == HOP_ASSERT_INT) || (OP == HOP_ASSERT_NEGINT) || (OP == HOP_OUTPUT)) {
            live.insert(A);
        } else if (OP == HOP_NOP) {
        } else if (OP == HOP_HALT) {
        } else {
            auto it = live.find(DST);
            if (it != live.end()) {
                live.erase(it);
                switch (OP) {
                case HOP_VAR: case HOP_INT: case HOP_NEGINT: case HOP_BIGINT:
                    break;
                case HOP_COPY: case HOP_INV: case HOP_NEGINV: case HOP_NEG: case HOP_SHOUP_PRECOMP: case HOP_POW:
                    live.insert(A);
                    break;
                case HOP_ADD: case HOP_SUB: case HOP_MUL:
                    live.insert(A);
                    live.insert(B);
                    break;
                case HOP_SHOUP_MUL: case HOP_ADDMUL:
                    live.insert(A);
                    live.insert(B);
                    live.insert(C);
                    break;
                case HOP_ASSERT_INT: case HOP_ASSERT_NEGINT: case HOP_OUTPUT:
                    live.insert(A);
                    break;
                case HOP_NOP:
                    break;
                }
            } else {
                *(HiOp*)INSTR = HiOp{HOP_NOP, 0, 0, 0};
                nerased++;
            }
        }
    HIOP_REVITER_END(PAGE, PAGEEND)
    CODE_REVPAGEITER_END()
    return nerased;
}

API void
tr_optimize(Trace &tr)
{
    tr_flush(tr);
    tr_opt_erase_asserts(tr);
    tr_opt_erase_dead_code(tr, 0, NULL);
    tr_opt_propagate_constants(tr);
    tr_opt_deduplicate(tr);
    tr_opt_erase_dead_code(tr, 0, NULL);
}

/* Trace finalization
 */

static void
revcode_flush(Code &code)
{
    if (code.buflen > 0) {
        size_t leftover = CODE_PAGESIZE - code.buflen;
        ssize_t n;
        if (leftover == 0) {
            SYSCALL(n = write(code.fd, code.buf, CODE_PAGESIZE));
        } else {
            memset(code.buf, 0, leftover);
            struct iovec iov[2] = {
                {code.buf + leftover, code.buflen},
                {code.buf, leftover}
            };
            SYSCALL(n = writev(code.fd, iov, 2));
        }
        if (unlikely(n != CODE_PAGESIZE)) {
            crash("revcode_flush(): failed to write all the data\n");
        }
        code.filesize += CODE_PAGESIZE;
        code.buflen = 0;
    }
}

static void
revcode_copy(Code &code, Code &dst)
{
    CODE_REVPAGEITER_BEGIN(code, 0)
        code_append_pages(dst, PAGE, CODE_PAGESIZE);
    CODE_REVPAGEITER_END()
}

#define revcode_pack(code, align, type, ...) \
    do { \
        if (unlikely((code).buflen > CODE_PAGESIZE - sizeof(type))) { \
            revcode_flush(code); \
        } \
        *(type*)ASSUME_ALIGNED((code).buf + CODE_PAGESIZE - sizeof(type) - (code).buflen, align) = type(__VA_ARGS__); \
        (code).buflen += sizeof(type); \
    } while(0)

#define revcode_pack_LoOp1(code, op, a) revcode_pack(code, 4, LoOp1, {op, a})
#define revcode_pack_LoOp2(code, op, a, b) revcode_pack(code, 4, LoOp2, {op, a, b})
#define revcode_pack_LoOp3(code, op, a, b, c) revcode_pack(code, 4, LoOp3, {op, a, b, c})
#define revcode_pack_LoOp4(code, op, a, b, c, d) revcode_pack(code, 4, LoOp4, {op, a, b, c, d})

API void
tr_finalize(Trace &tr, size_t nroots, Value **roots)
{
    tr_flush(tr);
    size_t maxused = tr.nfinlocations;
    std::vector<nloc_t> free;
    std::unordered_map<nloc_t, uint32_t> map;
#define allocate(newX, X) \
        if (likely(X >= tr.nfinlocations)) { \
            auto it = map.find(X); \
            if (likely(it != map.end())) { \
                newX = it->second; \
            } else { \
                if (free.empty()) { newX = maxused++; } \
                else { newX = free.back(); free.pop_back(); } \
                map[X] = newX; \
            } \
        } else { \
            newX = X; \
        }
    for (size_t i = 0; i < nroots; i++) {
        nloc_t newloc;
        allocate(newloc, roots[i]->loc);
        roots[i]->loc = newloc;
    }
    Code rc = code_init();
    nloc_t DST = tr.nextloc;
    CODE_REVPAGEITER_BEGIN(tr.code, 0)
    HIOP_REVITER_BEGIN(PAGE, PAGEEND)
        DST--;
        uint32_t newA, newB, newC;
        if ((OP == HOP_ASSERT_INT) || (OP == HOP_ASSERT_NEGINT) || (OP == HOP_OUTPUT)) {
            allocate(newA, A);
            revcode_pack_LoOp2(rc, OP, newA, (uint32_t)B);
        } else {
            auto itdst = map.find(DST);
            if (itdst != map.end()) {
                uint32_t newDST = itdst->second;
                map.erase(itdst);
                free.push_back(newDST);
                assert(free.back() == newDST);
                switch (OP) {
                case HOP_VAR: case HOP_BIGINT:
                    revcode_pack_LoOp2(rc, OP, newDST, (uint32_t)A);
                    break;
                case HOP_INT: case HOP_NEGINT:
                    revcode_pack_LoOp3(rc, OP, newDST, (uint32_t)A, (uint32_t)(A>>32));
                    break;
                case HOP_COPY: case HOP_INV: case HOP_NEGINV: case HOP_NEG: case HOP_SHOUP_PRECOMP:
                    allocate(newA, A);
                    revcode_pack_LoOp2(rc, OP, newDST, newA);
                    break;
                case HOP_POW:
                    allocate(newA, A);
                    revcode_pack_LoOp3(rc, OP, newDST, newA, (uint32_t)B);
                    break;
                case HOP_ADD: case HOP_SUB:
                    allocate(newA, A);
                    allocate(newB, B);
                    revcode_pack_LoOp3(rc, OP, newDST, newA, newB);
                    break;
                case HOP_MUL:
                    allocate(newA, A);
                    allocate(newB, B);
                    if (newDST == newA) {
                        revcode_pack_LoOp2(rc, LOP_SETMUL, newA, newB);
                    } else if (newDST == newB) {
                        revcode_pack_LoOp2(rc, LOP_SETMUL, newB, newA);
                    } else {
                        revcode_pack_LoOp3(rc, LOP_MUL, newDST, newA, newB);
                    }
                    break;
                case HOP_SHOUP_MUL:
                    allocate(newA, A);
                    allocate(newB, B);
                    allocate(newC, C);
                    revcode_pack_LoOp4(rc, OP, newDST, newA, newB, newC);
                    break;
                case HOP_ADDMUL:
                    allocate(newA, A);
                    allocate(newB, B);
                    allocate(newC, C);
                    if (newDST == newA) {
                        revcode_pack_LoOp3(rc, LOP_SETADDMUL, newA, newB, newC);
                    } else {
                        revcode_pack_LoOp4(rc, LOP_ADDMUL, newDST, newA, newB, newC);
                    }
                    break;
                case HOP_ASSERT_INT: case HOP_ASSERT_NEGINT: case HOP_OUTPUT:
                    allocate(newA, A);
                    revcode_pack_LoOp2(rc, OP, newA, (uint32_t)B);
                    break;
                case HOP_NOP: case HOP_HALT:
                    assert(!"this should never happen");
                    break;
                }
            }
        }
#undef allocate
    HIOP_REVITER_END(PAGE, PAGEEND)
    CODE_REVPAGEITER_END()
    code_reset(tr.code);
    revcode_flush(rc);
    revcode_copy(rc, tr.fincode);
    code_clear(rc);
    tr.nfinlocations = maxused;
    tr.nextloc = tr.nfinlocations + code_size(tr.code)/sizeof(HiOp);
}

API void
tr_unfinalize(Trace &tr, size_t nroots, Value **roots)
{
    assert(code_size(tr.code) == 0);
    std::vector<nloc_t> data;
    data.resize(tr.nfinlocations, 0);
    nloc_t DST = 0;
    CODE_PAGEITER_BEGIN(tr.fincode, 0)
    LOOP_ITER_BEGIN(PAGE, PAGEEND)
        switch(OP) {
        case LOP_VAR:
            code_pack_HiOp1(tr.code, OP, B);
            data[A] = DST;
            break;
        case LOP_INT: case LOP_NEGINT:
            code_pack_HiOp1(tr.code, OP, (uint64_t)B | ((uint64_t)C << 32));
            data[A] = DST;
            break;
        case LOP_BIGINT:
            code_pack_HiOp1(tr.code, OP, B);
            data[A] = DST;
            break;
        case HOP_COPY: case HOP_INV: case HOP_NEGINV: case HOP_NEG: case HOP_SHOUP_PRECOMP:
            code_pack_HiOp1(tr.code, OP, data[B]);
            data[A] = DST;
            break;
        case LOP_POW:
            code_pack_HiOp2(tr.code, OP, data[B], C);
            data[A] = DST;
            break;
        case LOP_ADD: case LOP_SUB: case LOP_MUL:
            code_pack_HiOp2(tr.code, OP, data[B], data[C]);
            data[A] = DST;
            break;
        case LOP_SHOUP_MUL: case LOP_ADDMUL:
            code_pack_HiOp3(tr.code, OP, data[B], data[C], data[D]);
            data[A] = DST;
            break;
        case LOP_ASSERT_INT: case LOP_ASSERT_NEGINT:
            code_pack_HiOp2(tr.code, OP, data[B], C);
            break;
        case LOP_OUTPUT:
            code_pack_HiOp2(tr.code, OP, data[A], B);
            break;
        case LOP_NOP:
            code_pack_HiOp1(tr.code, OP, 0);
            break;
        case LOP_SETMUL:
            code_pack_HiOp2(tr.code, HOP_MUL, data[A], data[B]);
            data[A] = DST;
            break;
        case LOP_SETADDMUL:
            code_pack_HiOp3(tr.code, HOP_ADDMUL, data[A], data[B], data[C]);
            data[A] = DST;
            break;
        case LOP_HALT:
            goto halt;
        }
        DST++;
    LOOP_ITER_END(PAGE, PAGEEND)
halt:;
    CODE_PAGEITER_END()
    code_reset(tr.fincode);
    tr.nfinlocations = 0;
    tr.nextloc = tr.nfinlocations + code_size(tr.code)/sizeof(HiOp);
    for (size_t i = 0; i < nroots; i++) {
        roots[i]->loc = data[roots[i]->loc];
    }
}

/* Trace import
 */

static uint8_t *
fixup_fincode(uint8_t *from, uint8_t *to, size_t *inmap, uint32_t outshift, uint32_t locshift)
{
    LOOP_ITER_BEGIN(from, to)
        switch(OP) {
        case LOP_VAR:
            *(LoOp2*)INSTR = LoOp2{OP, A + locshift, (uint32_t)inmap[B]};
            break;
        case LOP_INT: case LOP_NEGINT: case LOP_BIGINT:
            *(LoOp2*)INSTR = LoOp2{OP, A + locshift, B};
            break;
        case HOP_COPY: case HOP_INV: case HOP_NEGINV: case HOP_NEG: case HOP_SHOUP_PRECOMP:
            *(LoOp2*)INSTR = LoOp2{OP, A + locshift, B + locshift};
            break;
        case LOP_POW:
            *(LoOp3*)INSTR = LoOp3{OP, A + locshift, B + locshift, C};
            break;
        case LOP_ADD: case LOP_SUB: case LOP_MUL:
            *(LoOp3*)INSTR = LoOp3{OP, A + locshift, B + locshift, C + locshift};
            break;
        case LOP_SHOUP_MUL: case LOP_ADDMUL:
            *(LoOp4*)INSTR = LoOp4{OP, A + locshift, B + locshift, C + locshift, D + locshift};
            break;
        case LOP_ASSERT_INT: case LOP_ASSERT_NEGINT:
            *(LoOp3*)INSTR = LoOp3{OP, A + locshift, B + locshift, C};
            break;
        case LOP_OUTPUT:
            *(LoOp2*)INSTR = LoOp2{OP, A + locshift, B + outshift};
            break;
        case LOP_NOP:
            break;
        case LOP_SETMUL:
            *(LoOp2*)INSTR = LoOp2{OP, A + locshift, B + locshift};
            break;
        case LOP_SETADDMUL:
            *(LoOp3*)INSTR = LoOp3{OP, A + locshift, B + locshift, C + locshift};
            break;
        case LOP_HALT:
            break;
        }
    LOOP_ITER_END(from, to)
    return from;
}

static uint8_t *
fixup_code(uint8_t *from, uint8_t *to, size_t *inmap, nloc_t outshift, nloc_t locshift)
{
    HIOP_ITER_BEGIN(from, to)
        switch(OP) {
        case HOP_VAR:
            *INSTR = HiOp{OP, inmap[A], 0, 0};
            break;
        case HOP_INT: case HOP_NEGINT: case HOP_BIGINT:
            break;
        case HOP_COPY: case HOP_INV: case HOP_NEGINV: case HOP_NEG: case HOP_SHOUP_PRECOMP:
            *INSTR = HiOp{OP, A + locshift, 0, 0};
            break;
        case HOP_POW:
            *INSTR = HiOp{OP, A + locshift, B, 0};
            break;
        case HOP_ADD: case HOP_SUB: case HOP_MUL:
            *INSTR = HiOp{OP, A + locshift, B + locshift, 0};
            break;
        case HOP_SHOUP_MUL: case HOP_ADDMUL:
            *INSTR = HiOp{OP, A + locshift, B + locshift, C + locshift};
            break;
        case HOP_ASSERT_INT: case HOP_ASSERT_NEGINT:
            *INSTR = HiOp{OP, A + locshift, B, 0};
            break;
        case HOP_OUTPUT:
            *INSTR = HiOp{OP, A + locshift, B + outshift, 0};
            break;
        case HOP_NOP:
            break;
        }
    HIOP_ITER_END(from, to)
    return from;
}

API int
tr_mergeimport_FILE(Trace &tr, FILE *f)
{
    tr_flush(tr);
    size_t ninputs0 = tr.ninputs;
    size_t noutputs0 = tr.noutputs;
    size_t nextloc0 = tr.nextloc;
    bool fresh = ((ninputs0 == 0) && (noutputs0 == 0) && (nextloc0 == 0));
    std::vector<size_t> inputs;
    // Read the header
    TraceFileHeader h;
    if (fread(&h, sizeof(TraceFileHeader), 1, f) != 1) return 1;
    if (h.magic != RATRACER_MAGIC) return 1;
    if ((h.fincodesize % CODE_PAGESIZE) != 0) return 1;
    if ((h.codesize % CODE_PAGESIZE) != 0) return 1;
    // Merge inputs
    inputs.reserve(h.ninputs);
    for (size_t i = 0; i < h.ninputs; i++) {
        uint16_t len = 0;
        if (fread(&len, sizeof(len), 1, f) != 1) return 1;
        std::string name(len, 0);
        if (len > 0) {
            if (fread(&name[0], len, 1, f) != 1) return 1;
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
        if (fread(&len, sizeof(len), 1, f) != 1) return 1;
        std::string name(len, 0);
        if (len > 0) {
            if (fread(&name[0], len, 1, f) != 1) return 1;
        }
        tr.output_names.push_back(std::move(name));
        tr.noutputs++;
    }
    // Append big constants
    for (size_t i = 0; i < h.nconstants; i++) {
        fmpz x;
        fmpz_init(&x);
        fmpz_inp_raw(&x, f);
        tr.constants.push_back(x);
    }
    // Append the instructions
    {
        uint8_t *page = (uint8_t*)safe_memalign(CODE_BUFALIGN, CODE_PAGESIZE);
        for (size_t i = 0; i < h.fincodesize; i += CODE_PAGESIZE) {
            if (fread(page, CODE_PAGESIZE, 1, f) != 1) { free(page); return 1; }
            if (!fresh) {
                fixup_fincode(page, page + CODE_PAGESIZE, &inputs[0], noutputs0, nextloc0);
            }
            code_append_pages(tr.fincode, page, CODE_PAGESIZE);
        }
        for (size_t i = 0; i < h.codesize; i += CODE_PAGESIZE) {
            if (fread(page, CODE_PAGESIZE, 1, f) != 1) { free(page); return 1; }
            if (!fresh) {
                fixup_code(page, page + CODE_PAGESIZE, &inputs[0], noutputs0, nextloc0);
            }
            code_append_pages(tr.code, page, CODE_PAGESIZE);
        }
        free(page);
    }
    tr.nfinlocations += h.nfinlocations;
    tr.nextloc = tr.nfinlocations + code_size(tr.code)/sizeof(HiOp);
    return 0;
}

API int
tr_mergeimport(Trace &tr, const char *filename)
{
    size_t len = strlen(filename);
    const char *cmd = NULL;
    if (memsuffix(filename, len, ".bz2", 4)) {
        cmd = "bzip2 -c -d ";
    } else if (memsuffix(filename, len, ".gz", 3)) {
        cmd = "gzip -c -d ";
    } else if (memsuffix(filename, len, ".xz", 3)) {
        cmd = "xz -c -d ";
    } else if (memsuffix(filename, len, ".zst", 4)) {
        cmd = "zstd -c -d ";
    } else {
        FILE *f = fopen(filename, "rb");
        if (f == NULL) return 1;
        int r = tr_mergeimport_FILE(tr, f);
        fclose(f);
        return r;
    }
    char *buf = shell_escape(cmd, filename, "");
    FILE *f = popen(buf, "r");
    free(buf);
    if (f == NULL) return 1;
    int r = tr_mergeimport_FILE(tr, f);
    pclose(f);
    return r;
}

API size_t
tr_replace_variables(Trace &tr, size_t fi1, size_t fi2, size_t ti1, size_t ti2, std::map<size_t, Value> varmap)
{
    size_t nreplaced = 0;
    tr_flush(tr);
    {
        size_t page1 = fi1 & ~(CODE_PAGESIZE - 1);
        size_t page2 = (fi2 + CODE_PAGESIZE - 1) & ~(CODE_PAGESIZE - 1);
        size_t offset = page1;
        CODE_PAGESUBITER_BEGIN(tr.fincode.fd, tr.fincode.buf, page1, page2, 1)
        LOOP_ITER_BEGIN(PAGE, PAGEEND)
            if ((fi1 <= offset) && (offset < fi2)) {
                if (OP == LOP_VAR) {
                    auto it = varmap.find(B);
                    if (it != varmap.end()) {
                        *(LoOp2*)INSTR = LoOp2{LOP_COPY, A, (uint32_t)it->second.loc};
                        nreplaced++;
                    }
                }
            }
        LOOP_ITER_END(PAGE, PAGEEND)
        offset += PAGEEND - PAGE;
        CODE_PAGESUBITER_END();
    }
    {
        size_t page1 = ti1 & ~(CODE_PAGESIZE - 1);
        size_t page2 = (ti2 + CODE_PAGESIZE - 1) & ~(CODE_PAGESIZE - 1);
        size_t offset = page1;
        CODE_PAGESUBITER_BEGIN(tr.code.fd, tr.code.buf, page1, page2, 1)
        HIOP_ITER_BEGIN(PAGE, PAGEEND)
            if ((ti1 <= offset) && (offset < ti2)) {
                if (OP == HOP_VAR) {
                    auto it = varmap.find(A);
                    if (it != varmap.end()) {
                        *(HiOp*)INSTR = HiOp{HOP_COPY, it->second.loc, 0, 0};
                        nreplaced++;
                    }
                }
            }
        HIOP_ITER_END(PAGE, PAGEEND)
        offset += PAGEEND - PAGE;
        CODE_PAGESUBITER_END();
    }
    return nreplaced;
}

API void
tr_list_used_inputs(Trace &tr, int *inputs)
{
    for (size_t i = 0; i < tr.ninputs; i++) {
        inputs[i] = 0;
    }
    CODE_PAGEITER_BEGIN(tr.fincode, 0)
    LOOP_ITER_BEGIN(PAGE, PAGEEND)
        if (OP == LOP_VAR) inputs[B] = 1;
    LOOP_ITER_END(PAGE, PAGEEND)
    CODE_PAGEITER_END()
    CODE_PAGEITER_BEGIN(tr.code, 0)
    HIOP_ITER_BEGIN(PAGE, PAGEEND)
        if (OP == HOP_VAR) inputs[A] = 1;
    HIOP_ITER_END(PAGE, PAGEEND)
    CODE_PAGEITER_END()
}

/* Trace output
 */

static void
tr_print_disasm_tmp(FILE *f, Trace &tr)
{
    size_t DST = tr.nfinlocations;
    CODE_PAGEITER_BEGIN(tr.code, 0)
    HIOP_ITER_BEGIN(PAGE, PAGEEND)
        switch (OP) {
        case HOP_HALT: DST += (HiOp*)PAGEEND - (HiOp*)INSTR; goto halt;
        case HOP_VAR: fprintf(f, "%zu = var #%zu '%s'\n", DST, A, tr.input_names[A].c_str()); break;
        case HOP_INT: fprintf(f, "%zu = int #%zu\n", DST, A); break;
        case HOP_NEGINT: fprintf(f, "%zu = negint #%zu\n", DST, A); break;
        case HOP_BIGINT: fprintf(f, "%zu = bigint #%zu\n", DST, A); break;
        case HOP_COPY: fprintf(f, "%zu = copy %zu\n", DST, A); break;
        case HOP_INV: fprintf(f, "%zu = inv %zu\n", DST, A); break;
        case HOP_NEGINV: fprintf(f, "%zu = neginv %zu\n", DST, A); break;
        case HOP_NEG: fprintf(f, "%zu = neg %zu\n", DST, A); break;
        case HOP_SHOUP_PRECOMP: fprintf(f, "%zu = shoup_precomp %zu\n", DST, A); break;
        case HOP_POW: fprintf(f, "%zu = pow %zu #%zu\n", DST, A, B); break;
        case HOP_ADD: fprintf(f, "%zu = add %zu %zu\n", DST, A, B); break;
        case HOP_SUB: fprintf(f, "%zu = sub %zu %zu\n", DST, A, B); break;
        case HOP_MUL: fprintf(f, "%zu = mul %zu %zu\n", DST, A, B); break;
        case HOP_SHOUP_MUL: fprintf(f, "%zu = shoup_mul %zu %zu %zu\n", DST, A, B, C); break;
        case HOP_ADDMUL: fprintf(f, "%zu = addmul %zu %zu %zu\n", DST, A, B, C); break;
        case HOP_ASSERT_INT: fprintf(f, "%zu = assert_int %zu #%zu\n", DST, A, B); break;
        case HOP_ASSERT_NEGINT: fprintf(f, "%zu = assert_negint %zu #%zu\n", DST, A, B); break;
        case HOP_OUTPUT: fprintf(f, "%zu = output %zu #%zu '%s'\n", DST, A, B, tr.output_names[B].c_str()); break;
        case HOP_NOP: fprintf(f, "%zu = nop\n", DST); break;
        default: fprintf(f, "%zu = op_%d %zu %zu %zu\n", DST, OP, A, B, C); break;
        }
        DST++;
    HIOP_ITER_END(PAGE, PAGEEND)
halt:;
    CODE_PAGEITER_END()
}

static void
tr_print_disasm_fin(FILE *f, Trace &tr)
{
    CODE_PAGEITER_BEGIN(tr.fincode, 0)
    LOOP_ITER_BEGIN(PAGE, PAGEEND)
        switch (OP) {
        case LOP_HALT: goto halt;
        case LOP_VAR: fprintf(f, "%" PRIu32 " = var #%" PRIu32 "\n", A, B); break;
        case LOP_INT: fprintf(f, "%" PRIu32 " = int #%" PRIu64 "\n", A, (uint64_t)B | ((uint64_t)C << 32)); break;
        case LOP_NEGINT: fprintf(f, "%" PRIu32 " = negint #%" PRIu32 "\n", A, B); break;
        case LOP_BIGINT: fprintf(f, "%" PRIu32 " = bigint #%" PRIu32 "\n", A, B); break;
        case LOP_COPY: fprintf(f, "%" PRIu32 " = copy %" PRIu32 "\n", A, B); break;
        case LOP_INV: fprintf(f, "%" PRIu32 " = inv %" PRIu32 "\n", A, B); break;
        case LOP_NEGINV: fprintf(f, "%" PRIu32 " = neginv %" PRIu32 "\n", A, B); break;
        case LOP_NEG: fprintf(f, "%" PRIu32 " = neg %" PRIu32 "\n", A, B); break;
        case LOP_SHOUP_PRECOMP: fprintf(f, "%" PRIu32 " = shoup_precomp %" PRIu32 "\n", A, B); break;
        case LOP_POW: fprintf(f, "%" PRIu32 " = pow %" PRIu32 " #%" PRIu32 "\n", A, B, C); break;
        case LOP_ADD: fprintf(f, "%" PRIu32 " = add %" PRIu32 " %" PRIu32 "\n", A, B, C); break;
        case LOP_SUB: fprintf(f, "%" PRIu32 " = sub %" PRIu32 " %" PRIu32 "\n", A, B, C); break;
        case LOP_MUL: fprintf(f, "%" PRIu32 " = mul %" PRIu32 " %" PRIu32 "\n", A, B, C); break;
        case LOP_SHOUP_MUL: fprintf(f, "%" PRIu32 " = shoup_mul %" PRIu32 " %" PRIu32 " %" PRIu32 "\n", A, B, C, D); break;
        case LOP_ADDMUL: fprintf(f, "%" PRIu32 " = addmul %" PRIu32 " %" PRIu32 " %" PRIu32 "\n", A, B, C, D); break;
        case LOP_ASSERT_INT: fprintf(f, "assert_int %" PRIu32 " #%" PRIu32 "\n", A, B); break;
        case LOP_ASSERT_NEGINT: fprintf(f, "assert_negint %" PRIu32 " #%" PRIu32 "\n", A, B); break;
        case LOP_OUTPUT: fprintf(f, "output %" PRIu32 " #%" PRIu32 " '%s'\n", A, B, tr.output_names[B].c_str()); break;
        case LOP_NOP: fprintf(f, "nop\n"); break;
        case LOP_SETMUL: fprintf(f, "setmul %" PRIu32 " %" PRIu32 "\n", A, B); break;
        case LOP_SETADDMUL: fprintf(f, "setaddmul %" PRIu32 " %" PRIu32 " %" PRIu32 "\n", A, B, C); break;
        default: fprintf(f, "op_%d %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 "\n", OP, A, B, C, D); break;
        }
    LOOP_ITER_END(PAGE, PAGEEND)
halt:;
    CODE_PAGEITER_END()
}

API void
tr_print_disasm(FILE *f, Trace &tr)
{
    tr_flush(tr);
    fprintf(f, "# low-level code (%zuB)\n", code_size(tr.fincode));
    tr_print_disasm_fin(f, tr);
    fprintf(f, "# high-level code (%zuB)\n", code_size(tr.code));
    tr_print_disasm_tmp(f, tr);
}

/* Trace evaluation
 */

NMOD_VEC_INLINE
mp_limb_t _nmod_mul(mp_limb_t a, mp_limb_t b, nmod_t mod)
{
    mp_limb_t res, hi, lo;
    umul_ppmm(hi, lo, a, b);
    NMOD_RED2(res, hi, lo, mod);
    return res;
}

#define INSTR_VAR(dst, a, b, c) data[dst] = input[a];
#define INSTR_INT(dst, a, b, c) data[dst] = a;
#define INSTR_NEGINT(dst, a, b, c) data[dst] = nmod_neg(a, mod);
#define INSTR_BIGINT(dst, a, b, c) data[dst] = _fmpz_get_nmod(&constants[a], mod);
#define INSTR_COPY(dst, a, b, c) data[dst] = data[a];
#define INSTR_INV(dst, a, b, c) if (unlikely(n_gcdinv(&data[dst], data[a], mod.n) != 1)) return 2;
#define INSTR_NEGINV(dst, a, b, c) if (unlikely(n_gcdinv(&data[dst], nmod_neg(data[a], mod), mod.n) != 1)) return 3;
#define INSTR_NEG(dst, a, b, c) data[dst] = nmod_neg(data[a], mod);
#define INSTR_SHOUP_PRECOMP(dst, a, b, c) data[dst] = n_mulmod_precomp_shoup(data[a], mod.n);
#define INSTR_POW(dst, a, b, c) data[dst] = nmod_pow_ui(data[a], b, mod);
#define INSTR_ADD(dst, a, b, c) data[dst] = _nmod_add(data[a], data[b], mod);
#define INSTR_SUB(dst, a, b, c) data[dst] = _nmod_sub(data[a], data[b], mod);
#define INSTR_MUL(dst, a, b, c) data[dst] = _nmod_mul(data[a], data[b], mod);
#define INSTR_SHOUP_MUL(dst, a, b, c) data[dst] = n_mulmod_shoup(data[a], data[b], data[c], mod.n);
#define INSTR_ADDMUL(dst, a, b, c) data[dst] = _nmod_addmul(data[a], data[b], data[c], mod);
#define INSTR_ASSERT_INT(dst, a, b, c) if (unlikely(data[a] != b)) return 4;
#define INSTR_ASSERT_NEGINT(dst, a, b, c) if (unlikely(data[a] != nmod_neg(b, mod))) return 5;
#define INSTR_OUTPUT(dst, a, b, c) output[b] = data[a];
#define INSTR_NOP(dst, a, b, c)

API int
code_evaluate_hi(const Code &restrict code, uint64_t index0, const ncoef_t *restrict input, ncoef_t *restrict output, const fmpz *restrict constants, ncoef_t *restrict data, nmod_t mod)
{
    if (code_size(code) == 0) return 0;
    if (mod.norm <= 0) return -1;
    data = (ncoef_t*)ASSUME_ALIGNED(data, sizeof(ncoef_t));
    uint64_t DST = index0;
    CODE_ITER_BEGIN(code, 0)
        switch(OP) {
        case HOP_VAR: INSTR_VAR(DST, A, B, C); break;
        case HOP_INT: INSTR_INT(DST, A, B, C); break;
        case HOP_NEGINT: INSTR_NEGINT(DST, A, B, C); break;
        case HOP_BIGINT: INSTR_BIGINT(DST, A, B, C); break;
        case HOP_COPY: INSTR_COPY(DST, A, B, C); break;
        case HOP_INV: INSTR_INV(DST, A, B, C); break;
        case HOP_NEGINV: INSTR_NEGINV(DST, A, B, C); break;
        case HOP_NEG: INSTR_NEG(DST, A, B, C); break;
        case HOP_SHOUP_PRECOMP: INSTR_SHOUP_PRECOMP(DST, A, B, C); break;
        case HOP_POW: INSTR_POW(DST, A, B, C); break;
        case HOP_ADD: INSTR_ADD(DST, A, B, C); break;
        case HOP_SUB: INSTR_SUB(DST, A, B, C); break;
        case HOP_MUL: INSTR_MUL(DST, A, B, C); break;
        case HOP_SHOUP_MUL: INSTR_SHOUP_MUL(DST, A, B, C); break;
        case HOP_ADDMUL: INSTR_ADDMUL(DST, A, B, C); break;
        case HOP_ASSERT_INT: INSTR_ASSERT_INT(DST, A, B, C); break;
        case HOP_ASSERT_NEGINT: INSTR_ASSERT_NEGINT(DST, A, B, C); break;
        case HOP_OUTPUT: INSTR_OUTPUT(DST, A, B, C); break;
        case HOP_NOP: INSTR_NOP(DST, A, B, C); break;
        }
        DST++;
    CODE_ITER_END()
    return 0;
}

API int
code_evaluate_lo_mem(const uint8_t *restrict code, size_t size, const ncoef_t *restrict input, ncoef_t *restrict output, const fmpz *restrict constants, ncoef_t *restrict data, nmod_t mod)
{
    if (size == 0) return 0;
    if (mod.norm <= 0) return -1;
    static void *jumptable[LOP_COUNT] = {
        &&do_HALT,
        &&do_VAR,
        &&do_INT,
        &&do_NEGINT,
        &&do_BIGINT,
        &&do_COPY,
        &&do_INV,
        &&do_NEGINV,
        &&do_NEG,
        &&do_SHOUP_PRECOMP,
        &&do_POW,
        &&do_ADD,
        &&do_SUB,
        &&do_MUL,
        &&do_SHOUP_MUL,
        &&do_ADDMUL,
        &&do_ASSERT_INT,
        &&do_ASSERT_NEGINT,
        &&do_OUTPUT,
        &&do_NOP,
        &&do_SETMUL,
        &&do_SETADDMUL,
    };
    // Note that this implementation assumes that there is at
    // least a LoOp4-sized zero padding past the end of the page
    // buffer. This is why CODE_PAGELUFT exists. This padding
    // will be read as the LOP_HALT instruction, which is the
    // only way the cycle below terminates.
    data = (ncoef_t*)ASSUME_ALIGNED(data, sizeof(ncoef_t));
    const uint8_t *pi = (const uint8_t*)ASSUME_ALIGNED(code, 4);
    const uint8_t *pend = pi + size;
#define INSTR(opname, nargs, code) \
        do_ ## opname:; { \
            uint32_t A = ((LoOp4*)pi)->a; \
            uint32_t B = ((LoOp4*)pi)->b; \
            uint32_t C = ((LoOp4*)pi)->c; \
            uint32_t D = ((LoOp4*)pi)->d; \
            (void)A; (void)B; (void)C; (void)D; \
            pi += sizeof(LoOp ## nargs); \
            code; \
            goto *jumptable[((LoOp4*)pi)->op]; \
        }
    goto *jumptable[((LoOp4*)pi)->op];
    for (;;) {
        INSTR(HALT, 0, if (pi >= pend) break);
        INSTR(VAR, 2, INSTR_VAR(A, B, C, D))
        INSTR(INT, 3, INSTR_INT(A, (uint64_t)B | ((uint64_t)C << 32), 0, 0))
        INSTR(NEGINT, 3, INSTR_NEGINT(A, (uint64_t)B | ((uint64_t)C << 32), 0, 0))
        INSTR(BIGINT, 2, INSTR_BIGINT(A, B, C, D))
        INSTR(COPY, 2, INSTR_COPY(A, B, C, D))
        INSTR(INV, 2, INSTR_INV(A, B, C, D))
        INSTR(NEGINV, 2, INSTR_NEGINV(A, B, C, D))
        INSTR(NEG, 2, INSTR_NEG(A, B, C, D))
        INSTR(SHOUP_PRECOMP, 2, INSTR_SHOUP_PRECOMP(A, B, C, D))
        INSTR(POW, 3, INSTR_POW(A, B, C, D))
        INSTR(ADD, 3, INSTR_ADD(A, B, C, D))
        INSTR(SUB, 3, INSTR_SUB(A, B, C, D))
        INSTR(MUL, 3, INSTR_MUL(A, B, C, D))
        INSTR(SHOUP_MUL, 4, INSTR_SHOUP_MUL(A, B, C, D))
        INSTR(ADDMUL, 4, INSTR_ADDMUL(A, B, C, D))
        INSTR(ASSERT_INT, 2, INSTR_ASSERT_INT(0, A, B, C))
        INSTR(ASSERT_NEGINT, 2, INSTR_ASSERT_NEGINT(0, A, B, C))
        INSTR(OUTPUT, 2, INSTR_OUTPUT(0, A, B, C))
        INSTR(NOP, 0, )
        INSTR(SETMUL, 2, INSTR_MUL(A, A, B, C))
        INSTR(SETADDMUL, 3, INSTR_ADDMUL(A, A, B, C))
    }
#undef INSTR
    return 0;
}

API int
code_evaluate_lo(const Code &restrict code, const ncoef_t *restrict input, ncoef_t *restrict output, const fmpz *restrict constants, ncoef_t *restrict data, nmod_t mod)
{
    if (code_size(code) == 0) return 0;
    if (mod.norm <= 0) return -1;
    static void *jumptable[LOP_COUNT] = {
        &&do_HALT,
        &&do_VAR,
        &&do_INT,
        &&do_NEGINT,
        &&do_BIGINT,
        &&do_COPY,
        &&do_INV,
        &&do_NEGINV,
        &&do_NEG,
        &&do_SHOUP_PRECOMP,
        &&do_POW,
        &&do_ADD,
        &&do_SUB,
        &&do_MUL,
        &&do_SHOUP_MUL,
        &&do_ADDMUL,
        &&do_ASSERT_INT,
        &&do_ASSERT_NEGINT,
        &&do_OUTPUT,
        &&do_NOP,
        &&do_SETMUL,
        &&do_SETADDMUL,
    };
    CODE_PAGEITER_BEGIN(code, 0)
    // Note that this implementation assumes that there is at
    // least a LoOp4-sized zero padding past the end of the page
    // buffer. This is why CODE_PAGELUFT exists. This padding
    // will be read as the LOP_HALT instruction, which is the
    // only way the cycle below terminates.
    data = (ncoef_t*)ASSUME_ALIGNED(data, sizeof(ncoef_t));
    const uint8_t *pi = (const uint8_t*)ASSUME_ALIGNED(PAGE, 4);
#define INSTR(opname, nargs, code) \
        do_ ## opname:; { \
            uint32_t A = ((LoOp4*)pi)->a; \
            uint32_t B = ((LoOp4*)pi)->b; \
            uint32_t C = ((LoOp4*)pi)->c; \
            uint32_t D = ((LoOp4*)pi)->d; \
            (void)A; (void)B; (void)C; (void)D; \
            pi += sizeof(LoOp ## nargs); \
            code; \
            goto *jumptable[((LoOp4*)pi)->op]; \
        }
    goto *jumptable[((LoOp4*)pi)->op];
    for (;;) {
        INSTR(HALT, 0, break);
        INSTR(VAR, 2, INSTR_VAR(A, B, C, D))
        INSTR(INT, 3, INSTR_INT(A, (uint64_t)B | ((uint64_t)C << 32), 0, 0))
        INSTR(NEGINT, 3, INSTR_NEGINT(A, (uint64_t)B | ((uint64_t)C << 32), 0, 0))
        INSTR(BIGINT, 2, INSTR_BIGINT(A, B, C, D))
        INSTR(COPY, 2, INSTR_COPY(A, B, C, D))
        INSTR(INV, 2, INSTR_INV(A, B, C, D))
        INSTR(NEGINV, 2, INSTR_NEGINV(A, B, C, D))
        INSTR(NEG, 2, INSTR_NEG(A, B, C, D))
        INSTR(SHOUP_PRECOMP, 2, INSTR_SHOUP_PRECOMP(A, B, C, D))
        INSTR(POW, 3, INSTR_POW(A, B, C, D))
        INSTR(ADD, 3, INSTR_ADD(A, B, C, D))
        INSTR(SUB, 3, INSTR_SUB(A, B, C, D))
        INSTR(MUL, 3, INSTR_MUL(A, B, C, D))
        INSTR(SHOUP_MUL, 4, INSTR_SHOUP_MUL(A, B, C, D))
        INSTR(ADDMUL, 4, INSTR_ADDMUL(A, B, C, D))
        INSTR(ASSERT_INT, 2, INSTR_ASSERT_INT(0, A, B, C))
        INSTR(ASSERT_NEGINT, 2, INSTR_ASSERT_NEGINT(0, A, B, C))
        INSTR(OUTPUT, 2, INSTR_OUTPUT(0, A, B, C))
        INSTR(NOP, 0, )
        INSTR(SETMUL, 2, INSTR_MUL(A, A, B, C))
        INSTR(SETADDMUL, 3, INSTR_ADDMUL(A, A, B, C))
    }
#undef INSTR
    CODE_PAGEITER_END()
    return 0;
}

API int
tr_evaluate(const Trace &restrict tr, const ncoef_t *restrict input, ncoef_t *restrict output, ncoef_t *restrict data, nmod_t mod, void *pagebuf)
{
    Code fincode = tr.fincode;
    if (pagebuf != NULL) fincode.buf = (uint8_t*)pagebuf;
    int r1 = code_evaluate_lo(fincode, input, output, &tr.constants[0], data, mod);
    if (unlikely(r1 != 0)) return r1;
    Code code = tr.code;
    if (pagebuf != NULL) code.buf = (uint8_t*)pagebuf;
    return code_evaluate_hi(code, tr.nfinlocations, input, output, &tr.constants[0], data, mod);
}

API int
tr_evaluate_fmpq(const Trace &restrict tr, fmpq *restrict output, fmpq *restrict data)
{
    assert(code_size(tr.code) == 0);
    for (size_t i = 0; i < tr.nfinlocations; i++) {
        fmpq_init(&data[i]);
    }
    for (size_t i = 0; i < tr.noutputs; i++) {
        fmpq_init(&output[i]);
    }
    fmpz_t one;
    fmpz_init_set_ui(one, 1);
    CODE_PAGEITER_BEGIN(tr.fincode, 0)
    LOOP_ITER_BEGIN(PAGE, PAGEEND)
        switch(OP) {
        case LOP_VAR: crash("not all variables have been eliminated");
        case LOP_INT: fmpq_set_si(data+A, (int64_t)((uint64_t)B | ((uint64_t)C << 32)), 1); break;
        case LOP_NEGINT: fmpq_set_si(data+A, -(int64_t)((uint64_t)B | ((uint64_t)C << 32)), 1); break;
        case LOP_BIGINT: fmpq_set_fmpz_frac(data+A, &tr.constants[B], one); break;
        case LOP_COPY: fmpq_set(data+A, data+B); break;
        case LOP_INV: if (unlikely(fmpq_is_zero(data+B))) return 2; fmpq_inv(data+A, data+B); break;
        case LOP_NEGINV: if (unlikely(fmpq_is_zero(data+B))) return 3; fmpq_inv(data+A, data+B); fmpq_neg(data+A, data+A); break;
        case LOP_NEG: fmpq_neg(data+A, data+B); break;
        case LOP_SHOUP_PRECOMP: return 1;
        case LOP_POW: fmpq_pow_si(data+A, data+B, C); break;
        case LOP_ADD: fmpq_add(data+A, data+B, data+C); break;
        case LOP_SUB: fmpq_sub(data+A, data+B, data+C); break;
        case LOP_MUL: fmpq_mul(data+A, data+B, data+C); break;
        case LOP_SHOUP_MUL: return 1;
        case LOP_ADDMUL:
            if (A == B) {
                fmpq_addmul(data+A, data+C, data+D);
            } else {
                fmpq_t t;
                fmpq_init(t);
                fmpq_set(t, data+B);
                fmpq_addmul(t, data+D, data+C);
                fmpq_swap(data+A, t);
                fmpq_clear(t);
            }
            break;
        case LOP_ASSERT_INT: return 1;
        case LOP_ASSERT_NEGINT: return 1;
        case LOP_OUTPUT: fmpq_set(output+B, data+A); break;
        case LOP_NOP: break;
        case LOP_SETMUL: fmpq_mul(data+A, data+A, data+B); break;
        case LOP_SETADDMUL: fmpq_addmul(data+A, data+B, data+C); break;
        case LOP_HALT: goto halt;
        }
    LOOP_ITER_END(PAGE, PAGEEND)
    halt:;
    CODE_PAGEITER_END()
    fmpz_clear(one);
    for (size_t i = 0; i < tr.nfinlocations; i++) {
        fmpq_clear(&data[i]);
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

API double
code_readtime(const Code &code)
{
    double t1 = timestamp();
    CODE_PAGEITER_BEGIN(code, 0)
    CODE_PAGEITER_END()
    return timestamp() - t1;
}

/* Expression parsing
 */

struct Parser {
    Tracer &tr;
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
    size_t i = p.tr.input(p.ptr, end - p.ptr);
    p.ptr = end;
    return p.tr.var(i);
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
        return p.tr.of_int(x);
    } else {
        // TODO: get rid of this tmp business.
        if (p.tmp.size() < (size_t)(end+1-p.ptr)) p.tmp.resize(end+1-p.ptr);
        memcpy(&p.tmp[0], p.ptr, end-p.ptr);
        p.tmp[end-p.ptr] = 0;
        fmpz_t num;
        fmpz_init(num);
        if (unlikely(fmpz_set_str(num, &p.tmp[0], 10) != 0)) parse_fail(p, "long integer expected");
        p.ptr = end;
        Value r = p.tr.of_fmpz(num);
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
        x = p.tr.pow(x, parse_exponent(p));
    }
    return (sign == 1) ? x : p.tr.neg(x);
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
            den = have_den ? p.tr.mul(den, f) : f;
            have_den = true;
        } else {
            num = have_num ? p.tr.mul(num, f) : f;
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
    return have_num ? (have_den ? p.tr.div(num, den) : num) : (have_den ? p.tr.inv(den) : p.tr.of_int(1));
}

API Value
parse_expr(Parser &p)
{
    Value pos, neg;
    bool have_pos = false, have_neg = false;
    bool inverted = false;
    for (;;) {
        Value t = parse_term(p);
        if (inverted) {
            neg = have_neg ? p.tr.add(neg, t) : t;
            have_neg = true;
        } else {
            pos = have_pos ? p.tr.add(pos, t) : t;
            have_pos = true;
        }
        skip_whitespace(p);
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
    return have_pos ? (have_neg ? p.tr.sub(pos, neg) : pos) : (have_neg ? p.tr.neg(neg) : p.tr.of_int(0));
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
load_equations_FILE(EquationSet &eqs, FILE *f, Tracer &tr)
{
    char *line = NULL;
    size_t size = 0;
    for (bool done = false; !done;) {
        Equation eqn = {};
        for (;;) {
            ssize_t len = getline(&line, &size, f);
            if (len <= 0) { done = true; break; }
            while ((len > 0) && ((line[len-1] == '\t') || (line[len-1] == '\n') || (line[len-1] == '\r') || (line[len-1] == ' '))) line[--len] = 0;
            if (len <= 0) break;
            Parser p = {tr, line, line, {}};
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
}

API void
load_equations(EquationSet &eqs, const char *filename, Tracer &tr)
{
    size_t len = strlen(filename);
    const char *cmd = NULL;
    if (memsuffix(filename, len, ".bz2", 4)) {
        cmd = "bzip2 -c -d ";
    } else if (memsuffix(filename, len, ".gz", 3)) {
        cmd = "gzip -c -d ";
    } else if (memsuffix(filename, len, ".xz", 3)) {
        cmd = "xz -c -d ";
    } else if (memsuffix(filename, len, ".zst", 4)) {
        cmd = "zstd -c -d ";
    } else {
        FILE *f = fopen(filename, "rb");
        if (f == NULL) crash("failed to open %s\n", filename);
        load_equations_FILE(eqs, f, tr);
        if (fclose(f) != 0) crash("failed to close %s\n", filename);
        return;
    }
    char *buf = shell_escape(cmd, filename, "");
    FILE *f = popen(buf, "r");
    free(buf);
    if (f == NULL) crash("failed to open %s\n", filename);
    load_equations_FILE(eqs, f, tr);
    if (pclose(f) != 0) crash("failed to close %s\n", filename);
}

static void
neqn_clear(Equation &neqn)
{
    neqn.terms.clear();
    neqn.len = 0;
}

static void
neqn_eliminate(Equation &res, const Equation &a, size_t idx, const Equation &b, Tracer &tr)
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
            Value r = tr.mul(b.terms[i2].coef, bfactor);
            if (!tr.is_zero(r)) {
                res.terms.push_back(Term{b.terms[i2].integral, r});
                res.len++;
            } else {
                if (paranoid) tr.assert_int(r, 0);
            }
            i2++;
        } else {
            Value r = tr.addmul(a.terms[i1].coef, b.terms[i2].coef, bfactor);
            if (!tr.is_zero(r)) {
                res.terms.push_back(Term{a.terms[i1].integral, r});
                res.len++;
            } else {
                if (paranoid) tr.assert_int(r, 0);
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
        Value r = tr.mul(b.terms[i2].coef, bfactor);
        if (!tr.is_zero(r)) {
            res.terms.push_back(Term{b.terms[i2].integral, r});
            res.len++;
        } else {
            if (paranoid) tr.assert_int(r, 0);
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
nreduce(std::vector<Equation> &neqns, Tracer &tr)
{
    bool paranoid = false;
    Value minus1 = tr.of_int(-1);
    Equation res = {};
    std::make_heap(neqns.begin(), neqns.end(), neqn_is_better);
    size_t n = neqns.size();
    while (n > 0) {
        std::pop_heap(neqns.begin(), neqns.begin() + n--, neqn_is_better);
        Equation &neqnx = neqns[n];
        if (neqnx.len == 0) { continue; }
        if (!tr.is_minus1(neqnx.terms[0].coef)) {
            Value nic = tr.neginv(neqnx.terms[0].coef);
            neqnx.terms[0].coef = minus1;
            for (size_t i = 1; i < neqnx.len; i++) {
                neqnx.terms[i].coef = tr.mul(neqnx.terms[i].coef, nic);
            }
        } else {
            if (paranoid) tr.assert_int(neqnx.terms[0].coef, -1);
        }
        while (n > 0) {
            Equation &neqn = neqns[0];
            if (neqn.len == 0) {
                std::pop_heap(neqns.begin(), neqns.begin() + n--, neqn_is_better);
            } else if (neqn.terms[0].integral == neqnx.terms[0].integral) {
                res.id = neqn.id;
                neqn_eliminate(res, neqn, 0, neqnx, tr);
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
is_reduced(const std::vector<Equation> &neqns, Tracer &tr)
{
    if (neqns.size() == 0) return true;
    ssize_t last = -1;
    for (size_t i = 0; i < neqns.size(); i++) {
        if (neqns[i].len == 0) continue;
        if (!tr.is_minus1(neqns[i].terms[0].coef)) {
            fprintf(stderr, "eq %zu starts with i%zx\n", i, neqns[i].terms[0].integral);
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
list_masters(std::set<name_t> &masters, const std::vector<Equation> &neqns, Tracer &tr)
{
    if (neqns.size() == 0) return 0;
    for (const Equation &neqn : neqns) {
        if (neqn.len <= 1) continue;
        if (!tr.is_minus1(neqn.terms[0].coef)) return 1;
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
nbackreduce(std::vector<Equation> &neqns, Tracer &tr)
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
                neqn_eliminate(res, neqn, j, neqns[it->second], tr);
                std::swap(res, neqn);
                neqn_clear(res);
            }
        }
    }
}

API bool
is_backreduced(const std::vector<Equation> &neqns, Tracer &tr)
{
    if (neqns.size() == 0) return true;
    std::set<name_t> masters;
    for (const Equation &neqn : neqns) {
        if (neqn.len <= 1) continue;
        if (!tr.is_minus1(neqn.terms[0].coef)) return false;
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

/* Series Tracer
 */

#define STERMS 25
#define SORDERS 1000

typedef int16_t sorder_t;

struct SValue {
    sorder_t order;
    sorder_t norders;
    sorder_t nterms;
    Value terms[STERMS];
};

struct STracer {
    Tracer tr;
    size_t varidx;
    int maxorder;

    void clear() {
        tr.clear();
    }
    size_t checkpoint() {
        return tr.checkpoint();
    }
    void rollback(size_t checkpoint) {
        tr.rollback(checkpoint);
    }
    SValue var(size_t idx) {
        if (idx != varidx) {
            return SValue{0, SORDERS, 1, {tr.var(idx)}};
        } else {
            return SValue{1, SORDERS, 1, {tr.of_int(1)}};
        }
    }
    SValue of_int(int64_t x) {
        if (x == 0) {
            return SValue{SORDERS, 0, 0, {}};
        }
        return SValue{0, SORDERS, 1, {tr.of_int(x)}};
    }
    SValue of_fmpz(const fmpz_t x) {
        if (fmpz_is_zero(x)) {
            return SValue{SORDERS, 0, 0, {}};
        }
        return SValue{0, SORDERS, 1, {tr.of_fmpz(x)}};
    }
    bool is_zero(const SValue &a) {
        bool r = true;
        for (int i = 0; i < a.nterms; i++)
            r = r && tr.is_zero(a.terms[i]);
        return r;
    }
    bool is_minus1(const SValue &a) {
        if (a.order + a.nterms < 0) return false;
        for (int i = 0; i < a.nterms; i++) {
            if (((i + a.order) == 0) && !tr.is_minus1(a.terms[i])) return false;
            if (((i + a.order) != 0) && !tr.is_zero(a.terms[i])) return false;
        }
        return true;
    }
    void print(const SValue &a) {
        for (int i = 0; i < a.nterms; i++) {
            printf("%c eps^%d * 0x%zx\n", (i == 0) ? '=' : '+', a.order + i, a.terms[i].n);
        }
    }
    void _trim_front(SValue &a) {
        while ((a.nterms > 0) && tr.is_zero(a.terms[0])) {
            a.order++;
            a.norders--;
            a.nterms--;
            memmove(&a.terms[0], &a.terms[1], sizeof(a.terms[0])*a.nterms);
        }
        if (a.nterms == 0) {
            a.order = a.order + a.norders;
            a.norders = 0;
        }
    }
    void _trim_rear(SValue &a) {
        while ((a.nterms > 0) && tr.is_zero(a.terms[a.nterms-1])) {
            a.nterms--;
        }
        if (a.nterms == 0) {
            a.order = a.order + a.norders;
            a.norders = 0;
        }
    }
    void _check(const SValue &a) {
        assert(a.nterms <= STERMS);
        assert(a.nterms <= a.norders);
        if (a.nterms == 0) assert(a.norders == 0);
        if (a.nterms != 0) {
            assert(!tr.is_zero(a.terms[0]));
            assert(!tr.is_zero(a.terms[a.nterms-1]));
        }
    }
    SValue mul(const SValue &a, const SValue &b) {
        _check(a); _check(b);
        assert(a.nterms);
        assert(b.nterms);
        if (a.nterms == 0) return a;
        if (b.nterms == 0) return b;
        SValue r;
        r.order = a.order + b.order;
        r.norders = std::min(a.norders, b.norders);
        r.nterms = std::min(a.nterms + b.nterms - 1, (int)r.norders);
        r.nterms = std::min(r.nterms, (sorder_t)STERMS);
        bool nonzero[STERMS] = {};
        for (int i = 0; i < a.nterms; i++) {
            for (int j = 0; j < std::min((int)b.nterms, r.nterms - i); j++) {
                if (nonzero[i+j]) {
                    r.terms[i+j] = tr.addmul(r.terms[i+j], a.terms[i], b.terms[j]);
                } else {
                    r.terms[i+j] = tr.mul(a.terms[i], b.terms[j]);
                    nonzero[i+j] = !tr.is_zero(r.terms[i+j]);
                }
            }
        }
        _trim_rear(r);
        _check(r);
        return r;
    }
    SValue _mulone(const SValue &a, const Value &b) {
        SValue r = a;
        for (int i = 0; i < a.nterms; i++)
            r.terms[i] = tr.mul(r.terms[i], b);
        return r;
    }
    SValue mulint(const SValue &a, int64_t b) {
        return _mulone(a, tr.of_int(b));
    }
    SValue add(const SValue &a, const SValue &b) {
        _check(a); _check(b);
        SValue r;
        r.order = std::min(a.order, b.order);
        r.norders = std::min(a.order + a.norders, b.order + b.norders) - r.order;
        r.nterms = std::max(a.order + a.nterms, b.order + b.nterms) - r.order;
        r.nterms = std::min(r.nterms, (sorder_t)STERMS);
        r.nterms = std::min(r.nterms, r.norders);
        if (a.order >= r.order + r.nterms) r.norders = std::min((int)r.norders, a.order - r.order);
        else if (a.order + a.nterms > r.order + r.nterms) r.norders = r.nterms;
        if (b.order >= r.order + r.nterms) r.norders = std::min((int)r.norders, b.order - r.order);
        else if (b.order + b.nterms > r.order + r.nterms) r.norders = r.nterms;
        for (int o = r.order; o < r.order + r.nterms; o++) {
            bool ina = (a.order <= o) && (o < a.order + a.nterms);
            bool inb = (b.order <= o) && (o < b.order + b.nterms);
            r.terms[o - r.order] =
                (ina && inb) ? tr.add(a.terms[o - a.order], b.terms[o - b.order]) :
                    ina ? a.terms[o - a.order] :
                    inb ? b.terms[o - b.order] :
                    tr.of_int(0);
        }
        _trim_front(r);
        _trim_rear(r);
        _check(r);
        return r;
    }
    SValue addint(const SValue &a, int64_t b) {
        return add(a, of_int(b));
    }
    SValue sub(const SValue &a, const SValue &b) {
        return add(a, neg(b));
    }
    SValue addmul(const SValue &a, const SValue &b, const SValue &bfactor) {
        return add(a, mul(b, bfactor));
    }
    SValue inv(const SValue &a) {
        return neg(neginv(a));
    }
    SValue neginv(const SValue &a) {
        _check(a);
        if (unlikely(a.nterms == 0)) crash("STracer::inv(): division by zero\n");
        assert(!tr.is_zero(a.terms[0]));
        Value neginvA = tr.neginv(a.terms[0]);
        SValue small = {1, (sorder_t)(a.norders - 1), (sorder_t)(a.nterms - 1), {}};
        for (int i = 1; i < a.nterms; i++) {
            small.terms[i-1] = tr.mul(a.terms[i], neginvA);
        }
        _trim_front(small);
        if (small.nterms) {
            SValue sum = of_int(1);
            SValue smalln = small;
            for (;;) {
                sum = add(sum, smalln);
                int nextorder = smalln.order + small.order;
                if (nextorder > STERMS) break;
                if (nextorder > sum.norders) break;
                smalln = mul(small, smalln);
            }
            sum.norders = smalln.order + small.order - 1;
            SValue r = _mulone(sum, neginvA);
            r.order -= a.order;
            _check(r);
            return r;
        } else {
            SValue r = {(sorder_t)(-a.order), a.norders, 1, {neginvA}};
            _check(r);
            return r;
        }
    }
    SValue neg(const SValue &a) {
        SValue r = a;
        for (int i = 0; i < a.nterms; i++) r.terms[i] = tr.neg(r.terms[i]);
        return r;
    }
    SValue pow(const SValue &base, long exp) {
        if (exp < 0) return pow(inv(base), -exp);
        if (exp == 0) return of_int(1);
        SValue r = base;
        for (int i = 1; i < exp; i++) r = mul(r, base);
        return r;
    }
    SValue shoup_precomp(const SValue &a) {
        (void)a; crash("STracer::shoup_precomp() is not implemented\n");
    }
    SValue shoup_mul(const SValue &a, const SValue &aprecomp, const SValue &b) {
        (void)a; (void)aprecomp; (void)b; crash("STracer::shoup_mul() is not implemented\n");
    }
    SValue div(const SValue &a, const SValue &b) {
        return mul(a, inv(b));
    }
    void assert_int(const SValue &a, int64_t n) {
        (void)a; (void)n; crash("STracer::assert_int() is not implemented\n");
    }
    void add_output(const SValue &src, const char *name) {
        char buf[1024];
        if (src.order + src.norders <= maxorder) {
            crash("got an output with eps^%d .. eps^%d\n", src.order, src.order + src.norders - 1);
        }
        fprintf(stderr, "got an output with eps^%d .. eps^%d\n", src.order, src.order + src.norders - 1);
        for (int i = 0; i < src.norders; i++) {
            if (src.order + i > maxorder) continue;
            snprintf(buf, sizeof(buf), "ORDER[%s,%s^%d]", name, tr.t.input_names[varidx].c_str(), src.order + i);
            if ((i >= src.nterms) || tr.is_zero(src.terms[i])) {
                tr.add_output(tr.of_int(0), buf);
            } else {
                tr.add_output(src.terms[i], buf);
            }
        }
    }
    size_t input(const char *name, size_t len) {
        return tr.input(name, len);
    }
    size_t input(const char *name) {
        return tr.input(name);
    }
    int save(const char *path) {
        return tr.save(path);
    }
};

API STracer
stracer_init(size_t varidx, int maxorder)
{
    return STracer{tracer_init(), varidx, maxorder};
}

/* Trace to series expansion
 */

API Trace
tr_to_series(Trace &tr, size_t varidx, int maxorder)
{
    assert(code_size(tr.code) == 0);
    STracer otr = stracer_init(varidx, maxorder);
    for (size_t i = 0; i < tr.ninputs; i++) {
        otr.input(tr.input_names[i].c_str());
    }
    std::vector<SValue> data;
    data.resize(tr.nfinlocations);
    CODE_PAGEITER_BEGIN(tr.fincode, 0)
    LOOP_ITER_BEGIN(PAGE, PAGEEND)
        switch(OP) {
        case LOP_VAR: data[A] = otr.var(B); break;
        case LOP_INT: data[A] = otr.of_int((int64_t)((uint64_t)B | ((uint64_t)C << 32))); break;
        case LOP_NEGINT: data[A] = otr.of_int(-(int64_t)((uint64_t)B | ((uint64_t)C << 32))); break;
        case LOP_BIGINT: data[A] = otr.of_fmpz(&tr.constants[B]); break;
        case HOP_COPY: data[A] = data[B]; break;
        case HOP_INV: data[A] = otr.inv(data[B]); break;
        case HOP_NEGINV: data[A] = otr.neginv(data[B]); break;
        case HOP_NEG: data[A] = otr.neg(data[B]); break;
        case HOP_SHOUP_PRECOMP: data[A] = otr.shoup_precomp(data[B]); break;
        case LOP_POW: data[A] = otr.pow(data[B], C); break;
        case LOP_ADD: data[A] = otr.add(data[B], data[C]); break;
        case LOP_SUB: data[A] = otr.sub(data[B], data[C]); break;
        case LOP_MUL: data[A] = otr.mul(data[B], data[C]); break;
        case LOP_SHOUP_MUL: data[A] = otr.shoup_mul(data[B], data[C], data[D]); break;
        case LOP_ADDMUL: data[A] = otr.addmul(data[B], data[C], data[D]); break;
        case LOP_ASSERT_INT: otr.assert_int(data[B], C); break;
        case LOP_ASSERT_NEGINT: otr.assert_int(data[B], -C); break;
        case LOP_OUTPUT: otr.add_output(data[A], tr.output_names[B].c_str()); break;
        case LOP_NOP: break;
        case LOP_SETMUL: data[A] = otr.mul(data[A], data[B]); break;
        case LOP_SETADDMUL: data[A] = otr.addmul(data[A], data[B], data[C]); break;
        case LOP_HALT: goto halt;
        }
    LOOP_ITER_END(PAGE, PAGEEND)
    halt:;
    CODE_PAGEITER_END()
    return otr.tr.t;
}

#endif // RATBOX_H
