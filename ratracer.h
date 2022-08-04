/* Rational Tracer records a trace of rational operations performed
 * on a set of variables (represented modulo a 63-bit prime),
 * so that the trace could be analyzed, optimized, re-evaluated
 * multiple times, and eventually reconstructed as a rational
 * expression by the Rational Toolbox.
 */

#ifndef RATRACER_H
#define RATRACER_H

#include <assert.h>
#include <flint/fmpz.h>
#include <flint/nmod_vec.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#define API __attribute__((unused)) static
#define noreturn __attribute__((noreturn))
#define PACKED __attribute__((packed))
#define PACKED4 __attribute__((packed,aligned(4)))
#define PACKED16 __attribute__((packed,aligned(16)))
#define ASSUME_ALIGNED(ptr, n) __builtin_assume_aligned(ptr, n)
#define restrict __restrict__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define crash(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while(0)
#define SYSCALL(call) while (((call) < 0) && unlikely(errno == EINTR)) {};

/* Misc utils
 */

API void *
safe_malloc(size_t size)
{
    void *ptr = calloc(size, 1);
    if (unlikely(ptr == NULL)) {
        crash("failed to allocated %zu bytes of memory\n", size);
    }
    return ptr;
}

API void *
safe_realloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (unlikely(ptr == NULL)) {
        crash("failed to allocated %zu bytes of memory\n", size);
    }
    return ptr;
}

API void *
safe_memalign(size_t alignment, size_t size)
{
    void *ptr = NULL;
    if (unlikely(posix_memalign(&ptr, alignment, size) != 0)) {
        crash("failed to allocated %zu bytes of memory\n", size);
    }
    memset(ptr, 0, size);
    return ptr;
}

static int
memsuffix(const char *text, size_t tlen, const char *suffix, size_t slen)
{
    return (tlen >= slen) && (memcmp(text + tlen - slen, suffix, slen) == 0);
}

static char *
shell_escape(const char *prefix, const char *str, const char *suffix)
{
    size_t cap = 64;
    size_t len = 0;
    char *buf = (char*)safe_malloc(cap);
#define put(ch) if (unlikely(len >= cap)) { buf = (char*)safe_realloc((void*)buf, cap = cap * 2); }; buf[len++] = ch;
    for (const char *p = prefix; *p != 0; p++) { put(*p); }
    put('\'');
    for (const char *p = str; *p != 0; p++) {
        if (*p == '\'') { put('\''); put('\\'); put('\''); put('\''); }
        else { put(*p); }
    }
    put('\'');
    for (const char *p = suffix; *p != 0; p++) { put(*p); }
    return buf;
#undef put
}

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
    char *names = (char*)safe_malloc(nalloc*((size_t)1 << exp));
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

/* A file-backed code storage, organized as a sequence of
 * fixed-size pages.
 *
 * Each page is expected to contain a valid code sequence by
 * itself; to this end all code formats should recognize a sequence
 * of zero bytes as a no-op, or a end-of-the-page marker. This
 * is because when the code is flushed the unfilled part of the
 * page will be padded with zeros, so either one must make sure
 * that the code is never flushed with an incomplete page, or
 * the zero padding should be a valid code sequence.
 *
 * Note that the trace file format inherits the paging structure;
 * please increase RATRACER_MAGIC if the page size is adjusted.
 *
 * Thankfully the users of the Tracer interface below don't need
 * to know about these details.
 */

#define CODE_PAGESIZE (16*1024)
#define CODE_PAGELUFT 32
#define CODE_BUFALIGN 64

struct Code {
    uint8_t *buf;
    size_t buflen;
    int fd;
    size_t filesize;
};

API Code
code_init()
{
    uint8_t *buf = (uint8_t*)safe_memalign(CODE_BUFALIGN, CODE_PAGESIZE + CODE_PAGELUFT);
    const char *tmp = getenv("TMP");
    if (tmp == NULL) tmp = "/tmp";
    char *path = (char*)safe_malloc(strlen(tmp) + 24);
    sprintf(path, "%s/ratracer.XXXXXX", tmp);
    int fd = mkstemp(path);
    if (fd < 0) {
        crash("code_init(): failed to open a temporary file\n");
    }
    unlink(path);
    free(path);
    return Code{buf, 0, fd, 0};
}

API void
code_clear(Code &code)
{
    close(code.fd);
    free(code.buf);
}

API void
code_reset(Code &code)
{
    lseek(code.fd, 0, SEEK_SET);
    ftruncate(code.fd, 0);
    code.buflen = 0;
    code.filesize = 0;
}

API void
code_flush(Code &code)
{
    if (code.buflen > 0) {
        ssize_t n;
        memset(code.buf + code.buflen, 0, CODE_PAGESIZE - code.buflen);
        SYSCALL(n = write(code.fd, code.buf, CODE_PAGESIZE));
        if (unlikely(n != CODE_PAGESIZE)) {
            crash("code_flush(): failed to write all the data\n");
        }
        memset(code.buf, 0, code.buflen);
        code.filesize += CODE_PAGESIZE;
        code.buflen = 0;
    }
}

API void
code_append_pages(Code &code, void *instructions, size_t size)
{
    assert((size % CODE_PAGESIZE) == 0);
    assert(code.buflen == 0);
    ssize_t n;
    SYSCALL(n = write(code.fd, instructions, size));
    if (unlikely(n != (ssize_t)size)) {
        crash("code_append_pages(): failed to write all the data\n");
    }
    code.filesize += size;
}

API size_t
code_size(const Code &code)
{
    return code.filesize + code.buflen;
}

API void
code_truncate(Code &code, size_t size)
{
    assert(size <= code_size(code));
    if (size > code.filesize) {
        code.buflen = size - code.filesize;
    } else {
        size_t page = size & ~(size_t)(CODE_PAGESIZE - 1);
        size_t buflen = size - page;
        if (buflen > 0) {
            ssize_t n;
            SYSCALL(n = pread(code.fd, code.buf, CODE_PAGESIZE, page)); 
            if (unlikely(n != CODE_PAGESIZE)) crash("code_truncate: read() failed\n");
            memset(code.buf + buflen, 0, CODE_PAGESIZE - buflen);
        }
        lseek(code.fd, page, SEEK_SET);
        ftruncate(code.fd, page);
        code.filesize = size;
        code.buflen = buflen;
    }
}

#define code_pack(code, align, type, ...) \
    do { \
        if (unlikely((code).buflen > CODE_PAGESIZE - sizeof(type) )) { \
            code_flush(code); \
        } \
        *(type*)ASSUME_ALIGNED((code).buf + (code).buflen, align) = type(__VA_ARGS__); \
        (code).buflen += sizeof(type); \
    } while(0)

/* Forward code iteration
 */

#define CODE_PAGESUBITER_BEGIN(fd, buf, i1, i2, wr) \
{ \
    int _fd = (fd); \
    void *_buf = (buf); \
    ssize_t _start = (i1); \
    ssize_t _end = (i2); \
    assert((_start % CODE_PAGESIZE) == 0); \
    assert((_end % CODE_PAGESIZE) == 0); \
    bool PAGEWRITE = (wr); \
    for (; _start < _end; _start += CODE_PAGESIZE) { \
        ssize_t _n; \
        SYSCALL(_n = pread(_fd, _buf, CODE_PAGESIZE, _start)); \
        if (unlikely(_n != CODE_PAGESIZE)) crash("code_pageiter: read() failed\n"); \
        uint8_t *PAGE = (uint8_t*)ASSUME_ALIGNED(_buf, CODE_BUFALIGN); \
        uint8_t *PAGEEND = PAGE + CODE_PAGESIZE; (void)PAGEEND; \
        { \

#define CODE_PAGESUBITER_END() \
        } \
        if (PAGEWRITE) { \
            ssize_t _n; \
            SYSCALL(_n = pwrite(_fd, _buf, CODE_PAGESIZE, _start)); \
            if (unlikely(_n != CODE_PAGESIZE)) crash("code_pageiter: write() failed\n"); \
        } \
    } \
}

#define CODE_PAGEITER_BEGIN(code, rw) \
    assert(code.buflen == 0); \
    CODE_PAGESUBITER_BEGIN(code.fd, code.buf, 0, (code).filesize, rw)

#define CODE_PAGEITER_END() CODE_PAGESUBITER_END()

#define CODE_ITER_BEGIN(code, rw) \
    CODE_PAGEITER_BEGIN(code, rw) \
        HIOP_ITER_BEGIN(PAGE, PAGEEND) \

#define CODE_ITER_END() \
        HIOP_ITER_END(PAGE, PAGEEND) \
    CODE_PAGEITER_END()

/* Reverse code iteration
 */

#define CODE_REVPAGEITER_BEGIN(code, wr) \
{ \
    assert(code.buflen == 0); \
    const Code &_code = (code); \
    bool _wr = (wr); \
    for (ssize_t _end = _code.filesize; _end > 0; _end -= CODE_PAGESIZE) { \
        ssize_t _start = _end - CODE_PAGESIZE; \
        ssize_t _n; \
        SYSCALL(_n = pread(_code.fd, _code.buf, CODE_PAGESIZE, _start)); \
        if (unlikely(_n != CODE_PAGESIZE)) crash("code_revpageiter: read() failed\n"); \
        uint8_t *PAGE = (uint8_t*)ASSUME_ALIGNED(_code.buf, CODE_BUFALIGN); \
        uint8_t *PAGEEND = PAGE + CODE_PAGESIZE; (void)PAGEEND; \
        { \

#define CODE_REVPAGEITER_END() \
        } \
        if (_wr) { \
            ssize_t _n; \
            SYSCALL(_n = pwrite(_code.fd, _code.buf, CODE_PAGESIZE, _start)); \
            if (unlikely(_n != CODE_PAGESIZE)) crash("code_revpageiter: write() failed\n"); \
        } \
    } \
}

#define CODE_REVITER_BEGIN(code, wr) \
    CODE_REVPAGEITER_BEGIN(code, wr) \
        HIOP_REVITER_BEGIN(PAGE, PAGEEND)

#define CODE_REVITER_END() \
        HIOP_REVITER_END(PAGE, PAGEEND) \
    CODE_REVPAGEITER_END()

/* High-level instruction set
 */

enum HighLevelOpcode {
    HOP_HALT,
    HOP_VAR,
    HOP_INT,
    HOP_NEGINT,
    HOP_BIGINT,
    HOP_COPY,
    HOP_INV,
    HOP_NEGINV,
    HOP_NEG,
    HOP_SHOUP_PRECOMP,
    HOP_POW,
    HOP_ADD,
    HOP_SUB,
    HOP_MUL,
    HOP_SHOUP_MUL,
    HOP_ADDMUL,
    HOP_ASSERT_INT,
    HOP_ASSERT_NEGINT,
    HOP_OUTPUT,
    HOP_NOP
};

struct PACKED16 HiOp { uint8_t op; uint64_t a:40, b:40, c:40; }; // 16 bytes

#define code_pack_HiOp1(code, op, a) code_pack(code, 16, HiOp, {op, a, 0, 0})
#define code_pack_HiOp2(code, op, a, b) code_pack(code, 16, HiOp, {op, a, b, 0})
#define code_pack_HiOp3(code, op, a, b, c) code_pack(code, 16, HiOp, {op, a, b, c})

#define code_pack_HiOp1(code, op, a) code_pack(code, 16, HiOp, {op, a, 0, 0})
#define code_pack_HiOp2(code, op, a, b) code_pack(code, 16, HiOp, {op, a, b, 0})
#define code_pack_HiOp3(code, op, a, b, c) code_pack(code, 16, HiOp, {op, a, b, c})

#define HIOP_ITER_BEGIN(from, to) \
{\
    HiOp *INSTR = (HiOp*)ASSUME_ALIGNED((from), 16); \
    HiOp *_end = (HiOp*)((uint8_t*)(to) - (sizeof(HiOp) - 1)); \
    for (; INSTR < _end; INSTR++) { \
        const HiOp _instr = *INSTR; \
        uint8_t OP = _instr.op; \
        uint64_t A = _instr.a, B = _instr.b, C = _instr.c; \
        (void)OP; (void)A; (void)B; (void)C; \
        {

#define HIOP_ITER_END(from, to) \
        } \
    } \
}

#define HIOP_REVITER_BEGIN(from, to) \
{ \
    HiOp *INSTR = (HiOp*)ASSUME_ALIGNED((to), 16); \
    HiOp *_end = (HiOp*)(from); \
    for (INSTR--; INSTR >= _end; INSTR--) { \
        const HiOp _instr = *INSTR; \
        uint8_t OP = _instr.op; \
        uint64_t A = _instr.a, B = _instr.b, C = _instr.c; \
        (void)OP; (void)A; (void)B; (void)C; \
        {

#define HIOP_REVITER_END(from, to) \
        } \
    } \
}

/* Low-level instruction set (not used here)
 */

enum LowLevelOpcode {
    LOP_HALT,
    LOP_VAR,
    LOP_INT,
    LOP_NEGINT,
    LOP_BIGINT,
    LOP_COPY,
    LOP_INV,
    LOP_NEGINV,
    LOP_NEG,
    LOP_SHOUP_PRECOMP,
    LOP_POW,
    LOP_ADD,
    LOP_SUB,
    LOP_MUL,
    LOP_SHOUP_MUL,
    LOP_ADDMUL,
    LOP_ASSERT_INT,
    LOP_ASSERT_NEGINT,
    LOP_OUTPUT,
    LOP_NOP,
    LOP_SETMUL,
    LOP_SETADDMUL,
    LOP_COUNT
};

struct PACKED4 LoOp0 { uint32_t op; }; // 1*4 bytes
struct PACKED4 LoOp1 { uint32_t op; uint32_t a; }; // 2*4 bytes
struct PACKED4 LoOp2 { uint32_t op; uint32_t a, b; }; // 3*4 bytes
struct PACKED4 LoOp3 { uint32_t op; uint32_t a, b, c; }; // 4*4 bytes
struct PACKED4 LoOp4 { uint32_t op; uint32_t a, b, c, d; }; // 5*4 bytes

static const uint8_t LoOpSize[LOP_COUNT] = {
    sizeof(LoOp0), // HALT
    sizeof(LoOp2), // VAR
    sizeof(LoOp3), // INT
    sizeof(LoOp3), // NEGINT
    sizeof(LoOp2), // BIGINT
    sizeof(LoOp2), // COPY
    sizeof(LoOp2), // INV
    sizeof(LoOp2), // NEGINV
    sizeof(LoOp2), // NEG
    sizeof(LoOp2), // SHOUP_PRECOMP
    sizeof(LoOp3), // POW
    sizeof(LoOp3), // ADD
    sizeof(LoOp3), // SUB
    sizeof(LoOp3), // MUL
    sizeof(LoOp4), // SHOUP_MUL
    sizeof(LoOp4), // ADDMUL
    sizeof(LoOp2), // ASSERT_INT
    sizeof(LoOp2), // ASSERT_NEGINT
    sizeof(LoOp2), // OUTPUT
    sizeof(LoOp0), // NOP
    sizeof(LoOp2), // SETMUL
    sizeof(LoOp3), // SETADDMUL
};

static const char *LoOpName[LOP_COUNT] = {
    "halt",
    "var",
    "int",
    "negint",
    "bigint",
    "copy",
    "inv",
    "neginv",
    "neg",
    "shoup_precomp",
    "pow",
    "add",
    "sub",
    "mul",
    "shoup_mul",
    "addmul",
    "assert_int",
    "assert_negint",
    "output",
    "nop",
    "setmul",
    "setaddmul",
};

#define LOOP_ITER_BEGIN(from, to) \
{ \
    uint8_t *INSTR = (uint8_t*)ASSUME_ALIGNED((from), 4); \
    uint8_t *_end = (uint8_t*)(to) - (sizeof(LoOp4) - 1); \
    for (; INSTR < _end;) { \
        const LoOp4 _instr = *(LoOp4*)INSTR; \
        const uint8_t OP = _instr.op; \
        uint32_t A = _instr.a, B = _instr.b, C = _instr.c, D = _instr.d; \
        (void)OP; (void)A; (void)B; (void)C; (void)D; \
        {

#define LOOP_ITER_END(from, to) \
        } \
        INSTR = (uint8_t*)INSTR + LoOpSize[OP]; \
    } \
}

/* Traces
 */

#define IMM_MAX 0xFFFFFFFFFFll
#define LOC_MAX IMM_MAX

typedef uint64_t nloc_t;
typedef mp_limb_t ncoef_t;
struct Value { nloc_t loc; ncoef_t n; };

struct Trace {
    nloc_t ninputs;
    nloc_t noutputs;
    nloc_t nfinlocations;
    nloc_t nextloc;
    Code fincode;
    Code code;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<fmpz> constants;
};

API void
tr_flush(Trace &tr)
{
    code_flush(tr.fincode);
    code_flush(tr.code);
    tr.nextloc = tr.nfinlocations + code_size(tr.code)/sizeof(HiOp);
}

/* Trace export to file
 *
 * The file format is:
 * - TraceFileHeader{...}
 * - { u16 len; u8 name[len]; } for each input
 * - { u16 len; u8 name[len]; } for each output
 * - { u32 len; u8 value[len]; } for each big constant (GMP format)
 * - Instruction{...} for each finalized instruction
 * - Instruction{...} for each instruction
 */

struct PACKED TraceFileHeader {
    uint64_t magic;
    uint32_t ninputs;
    uint32_t noutputs;
    uint32_t nconstants;
    uint32_t nfinlocations;
    uint64_t fincodesize;
    uint64_t codesize;
};

static const uint64_t RATRACER_MAGIC = 0x3330303043524052ull;

API int
tr_export_to_FILE(Trace &t, FILE *f)
{
    tr_flush(t);
    assert(t.ninputs < UINT32_MAX);
    assert(t.noutputs < UINT32_MAX);
    assert(t.constants.size() < UINT32_MAX);
    assert(t.nfinlocations < UINT32_MAX);
    TraceFileHeader h = {
        RATRACER_MAGIC,
        (uint32_t)t.ninputs,
        (uint32_t)t.noutputs,
        (uint32_t)t.constants.size(),
        (uint32_t)t.nfinlocations,
        t.fincode.filesize,
        t.code.filesize
    };
    if (fwrite(&h, sizeof(TraceFileHeader), 1, f) != 1) goto fail;
    for (size_t i = 0; i < t.ninputs; i++) {
        if (i < t.input_names.size()) {
            const auto &n = t.input_names[i];
            assert(n.size() < UINT16_MAX);
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
            assert(n.size() < UINT16_MAX);
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
    CODE_PAGEITER_BEGIN(t.fincode, 0)
        if (fwrite(PAGE, PAGEEND - PAGE, 1, f) != 1) goto fail;
    CODE_PAGEITER_END()
    CODE_PAGEITER_BEGIN(t.code, 0)
        if (fwrite(PAGE, PAGEEND - PAGE, 1, f) != 1) goto fail;
    CODE_PAGEITER_END()
    return 0;
fail:;
    return 1;
}

API int
tr_export(Trace &t, const char *filename)
{
    size_t len = strlen(filename);
    const char *cmd = NULL;
    if (memsuffix(filename, len, ".bz2", 4)) {
        cmd = "bzip2 -c > ";
    } else if (memsuffix(filename, len, ".gz", 3)) {
        cmd = "gzip -c > ";
    } else if (memsuffix(filename, len, ".xz", 3)) {
        cmd = "xz -c > ";
    } else if (memsuffix(filename, len, ".zst", 4)) {
        cmd = "zstd -c > ";
    } else {
        FILE *f = fopen(filename, "wb");
        if (f == NULL) return 1;
        int r1 = tr_export_to_FILE(t, f);
        int r2 = fclose(f);
        return r1 || r2;
    }
    char *buf = shell_escape(cmd, filename, "");
    FILE *f = popen(buf, "w");
    free(buf);
    if (f == NULL) return 1;
    int r1 = tr_export_to_FILE(t, f);
    int r2 = pclose(f);
    return r1 || r2;
}

/* Trace construction
 */

struct Tracer {
    nmod_t mod;
    Trace t;
    std::unordered_map<int64_t, Value> const_cache;
    std::unordered_map<size_t, Value> var_cache;
    NameTable var_names;

    void clear();
    size_t checkpoint();
    void rollback(size_t checkpoint);
    Value var(size_t idx);
    Value of_int(int64_t x);
    Value of_fmpz(const fmpz_t x);
    Value mul(const Value &a, const Value &b);
    Value mulint(const Value &a, int64_t b);
    Value add(const Value &a, const Value &b);
    Value addint(const Value &a, int64_t b);
    Value sub(const Value &a, const Value &b);
    Value addmul(const Value &a, const Value &b, const Value &bfactor);
    Value inv(const Value &a);
    Value neginv(const Value &a);
    Value neg(const Value &a);
    Value pow(const Value &base, long exp);
    Value shoup_precomp(const Value &a);
    Value shoup_mul(const Value &a, const Value &aprecomp, const Value &b);
    Value div(const Value &a, const Value &b);
    void assert_int(const Value &a, int64_t n);
    nloc_t add_output(const Value &src, const char *name);
    size_t input(const char *name, size_t len);
    size_t input(const char *name);
    int save(const char *path);
};

API Tracer
tracer_init()
{
    Tracer tr = {};
    nmod_init(&tr.mod, 0x7FFFFFFFFFFFFFE7ull); // 2^63-25
    tr.t.fincode = code_init();
    tr.t.code = code_init();
    return tr;
}

#define tr (*this)

void
Tracer::clear()
{
    code_clear(tr.t.fincode);
    code_clear(tr.t.code);
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

size_t
Tracer::checkpoint()
{
    return code_size(tr.t.code);
}

void
Tracer::rollback(size_t checkpoint)
{
    code_truncate(tr.t.code, checkpoint);
    tr.t.nextloc = tr.t.nfinlocations + code_size(tr.t.code)/sizeof(HiOp);
}

Value
Tracer::var(size_t idx)
{
    auto it = tr.var_cache.find(idx);
    if (it == tr.var_cache.end()) {
        ncoef_t val = ncoef_hash(idx, tr.mod.n);
        code_pack_HiOp1(tr.t.code, HOP_VAR, (nloc_t)idx);
        if (idx + 1 > tr.t.ninputs) tr.t.ninputs = idx + 1;
        Value v = Value{tr.t.nextloc++, val};
        tr.var_cache[idx] = v;
        return v;
    } else {
        return it->second;
    }
}

Value
Tracer::of_int(int64_t x)
{
    auto it = tr.const_cache.find(x);
    if (it == tr.const_cache.end()) {
        ncoef_t c;
        if (x >= 0) {
            code_pack_HiOp1(tr.t.code, HOP_INT, (nloc_t)x);
            NMOD_RED(c, x, tr.mod);
        } else {
            code_pack_HiOp1(tr.t.code, HOP_NEGINT, (nloc_t)-x);
            NMOD_RED(c, -x, tr.mod);
            c = nmod_neg(c, tr.mod);
        }
        Value v = Value{tr.t.nextloc++, c};
        tr.const_cache[x] = v;
        return v;
    } else {
        return it->second;
    }
}

/* This function is missing in FLINT 2.8.2 */
extern "C" mp_limb_t
_fmpz_get_nmod(const fmpz_t aa, nmod_t mod)
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

/* This function is missing in FLINT 2.8.2 */
static mp_limb_t
_nmod_addmul(mp_limb_t a, mp_limb_t b, mp_limb_t c, nmod_t mod)
{
    NMOD_ADDMUL(a, b, c, mod);
    return a;
}

Value
Tracer::of_fmpz(const fmpz_t x)
{
    if (fmpz_fits_si(x) && (fmpz_bits(x) < 40)) {
        return tr.of_int(fmpz_get_si(x));
    } else {
        code_pack_HiOp1(tr.t.code, HOP_BIGINT, (nloc_t)tr.t.constants.size());
        fmpz xx;
        fmpz_init_set(&xx, x);
        tr.t.constants.push_back(xx);
        return Value{tr.t.nextloc++, _fmpz_get_nmod(x, tr.mod)};
    }
}

Value
Tracer::mul(const Value &a, const Value &b)
{
    code_pack_HiOp2(tr.t.code, HOP_MUL, a.loc, b.loc);
    return Value{tr.t.nextloc++, nmod_mul(a.n, b.n, tr.mod)};
}

Value
Tracer::mulint(const Value &a, int64_t b)
{
    return tr.mul(a, tr.of_int(b));
}

Value
Tracer::add(const Value &a, const Value &b)
{
    code_pack_HiOp2(tr.t.code, HOP_ADD, a.loc, b.loc);
    return Value{tr.t.nextloc++, _nmod_add(a.n, b.n, tr.mod)};
}

Value
Tracer::addint(const Value &a, int64_t b)
{
    return tr.add(a, tr.of_int(b));
}

Value
Tracer::sub(const Value &a, const Value &b)
{
    code_pack_HiOp2(tr.t.code, HOP_SUB, a.loc, b.loc);
    return Value{tr.t.nextloc++, _nmod_sub(a.n, b.n, tr.mod)};
}

Value
Tracer::addmul(const Value &a, const Value &b, const Value &bfactor)
{
    code_pack_HiOp3(tr.t.code, HOP_ADDMUL, a.loc, b.loc, bfactor.loc);
    return Value{tr.t.nextloc++, _nmod_addmul(a.n, b.n, bfactor.n, tr.mod)};
}

Value
Tracer::inv(const Value &a)
{
    code_pack_HiOp1(tr.t.code, HOP_INV, a.loc);
    return Value{tr.t.nextloc++, nmod_inv(a.n, tr.mod)};
}

Value
Tracer::neginv(const Value &a)
{
    code_pack_HiOp1(tr.t.code, HOP_NEGINV, a.loc);
    return Value{tr.t.nextloc++, nmod_inv(nmod_neg(a.n, tr.mod), tr.mod)};
}

Value
Tracer::neg(const Value &a)
{
    code_pack_HiOp1(tr.t.code, HOP_NEG, a.loc);
    return Value{tr.t.nextloc++, nmod_neg(a.n, tr.mod)};
}

Value
Tracer::pow(const Value &base, long exp)
{
    if (exp < 0) return tr.inv(tr.pow(base, -exp));
    switch (exp) {
    case 0: return tr.of_int(1);
    case 1: return base;
    case 2: return tr.mul(base, base);
    default:
        code_pack_HiOp2(tr.t.code, HOP_POW, base.loc, (uint64_t)exp);
        return Value{tr.t.nextloc++, nmod_pow_ui(base.n, exp, tr.mod)};
    }
}

Value
Tracer::shoup_precomp(const Value &a)
{
    code_pack_HiOp1(tr.t.code, HOP_SHOUP_PRECOMP, a.loc);
    return Value{tr.t.nextloc++, n_mulmod_precomp_shoup(a.n, tr.mod.n)};
}

Value
Tracer::shoup_mul(const Value &a, const Value &aprecomp, const Value &b)
{
    code_pack_HiOp3(tr.t.code, HOP_SHOUP_MUL, a.loc, aprecomp.loc, b.loc);
    return Value{tr.t.nextloc++, n_mulmod_shoup(a.n, b.n, aprecomp.n, tr.mod.n)};
}

Value
Tracer::div(const Value &a, const Value &b)
{
    return tr.mul(a, tr.inv(b));
}

void
Tracer::assert_int(const Value &a, int64_t n)
{
    if (n >= 0) {
        code_pack_HiOp2(tr.t.code, HOP_ASSERT_INT, a.loc, (uint64_t)n);
    } else {
        code_pack_HiOp2(tr.t.code, HOP_ASSERT_NEGINT, a.loc, (uint64_t)-n);
    }
    tr.t.nextloc++;
}

nloc_t
Tracer::add_output(const Value &src, const char *name)
{
    nloc_t n = tr.t.noutputs++;
    tr.t.output_names.push_back(std::string(name));
    code_pack_HiOp2(tr.t.code, HOP_OUTPUT, src.loc, n);
    tr.t.nextloc++;
    return n;
}

size_t
Tracer::input(const char *name, size_t len)
{
    ssize_t i = nt_lookup(tr.var_names, name, len);
    if (unlikely(i < 0)) {
        size_t n = tr.t.ninputs++;
        tr.t.input_names.push_back(std::string(name, len));
        nt_resize(tr.var_names, tr.t.ninputs);
        nt_set(tr.var_names, n, name, len);
        return n;
    } else {
        return i;
    }
}

size_t
Tracer::input(const char *name)
{
    return tr.input(name, strlen(name));
}

int
Tracer::save(const char *path)
{
    return tr_export(tr.t, path);
}

#undef tr

#endif // RATRACER_H
