typedef long long gint64;
typedef unsigned long long guint64;
typedef unsigned int guint32;
#define NULL ((void *)0)
#define HOST_ANDROID 1
#define HOST_X86 1
#include "canonical-i64.h"

extern void *mmap(void *, unsigned int, int, int, int, int);
extern int fork(void);
extern int waitpid(int, int *, int);
extern void _exit(int) __attribute__((noreturn));
extern int write(int, const void *, unsigned int);

struct Storage { unsigned int guard; volatile gint64 value; unsigned int end_guard; };
_Static_assert(_Alignof(gint64) == 4, "i386 ABI alignment");
_Static_assert(__builtin_offsetof(struct Storage, value) == 4, "field must exercise 4 mod 8 address");
struct Shared {
    struct Storage aligned_by_four;
    char padding[48];
    volatile gint64 aligned_by_eight;
    unsigned int tickets[6250];
};

static int scalar(volatile gint64 *p)
{
    mono_atomic_store_i64(p, 7);
    if (mono_atomic_cas_i64(p, 9, 1) != 7 || mono_atomic_load_i64(p) != 7) return 1;
    if (mono_atomic_cas_i64(p, 9, 7) != 7 || mono_atomic_load_i64(p) != 9) return 2;
    if (mono_atomic_fetch_add_i64(p, 10) != 9 || mono_atomic_add_i64(p, 2) != 21) return 3;
    if (mono_atomic_inc_i64(p) != 22 || mono_atomic_dec_i64(p) != 21) return 4;
    if (mono_atomic_xchg_i64(p, -9) != 21 || mono_atomic_load_i64(p) != -9) return 5;
    mono_atomic_store_i64(p, 0x7fffffffffffffffLL);
    if (mono_atomic_inc_i64(p) != (gint64)0x8000000000000000ULL) return 6;
    if (mono_atomic_dec_i64(p) != 0x7fffffffffffffffLL) return 7;
    mono_atomic_store_i64(p, 0);
    return 0;
}

static int exercise(struct Shared *shared, volatile gint64 *p)
{
    int rc = scalar(p);
    if (rc) return rc;
    for (int k = 0; k < 6250; ++k) shared->tickets[k] = 0;
    int children[4];
    for (int k = 0; k < 4; ++k) {
        children[k] = fork();
        if (children[k] == -1) return 8;
        if (children[k] == 0) {
            for (int i = 0; i < 50000; ++i) {
                gint64 ticket = mono_atomic_fetch_add_i64(p, 1);
                if (ticket < 0 || ticket >= 200000) _exit(9);
                unsigned int bit = 1u << ((unsigned int)ticket % 32);
                if (__sync_fetch_and_or(&shared->tickets[(unsigned int)ticket / 32], bit) & bit) _exit(10);
            }
            _exit(0);
        }
    }
    for (int k = 0; k < 4; ++k) {
        int status;
        if (waitpid(children[k], &status, 0) != children[k] || status) return 11;
    }
    if (mono_atomic_load_i64(p) != 200000) return 12;
    for (int k = 0; k < 6250; ++k) if (shared->tickets[k] != 0xffffffffu) return 13;

    mono_atomic_store_i64(p, 0x55555555aaaaaaaaLL);
    for (int k = 0; k < 4; ++k) {
        children[k] = fork();
        if (children[k] == -1) return 14;
        if (children[k] == 0) {
            for (int i = 0; i < 100000; ++i) {
                gint64 old;
                if (k == 0) mono_atomic_store_i64(p, 0x55555555aaaaaaaaLL);
                else if (k == 1) mono_atomic_store_i64(p, (gint64)0xaaaaaaaa55555555ULL);
                else {
                    old = k == 2 ? mono_atomic_load_i64(p) : mono_atomic_xchg_i64(p, 0x55555555aaaaaaaaLL);
                    if (old != 0x55555555aaaaaaaaLL && old != (gint64)0xaaaaaaaa55555555ULL) _exit(15);
                }
            }
            _exit(0);
        }
    }
    for (int k = 0; k < 4; ++k) {
        int status;
        if (waitpid(children[k], &status, 0) != children[k] || status) return 16;
    }
    return 0;
}

int main(void)
{
    struct Shared *shared = mmap(NULL, sizeof(struct Shared), 3, 0x21, -1, 0);
    if (shared == (void *)-1) return 100;
    if ((unsigned int)&shared->aligned_by_four.value % 8 != 4) return 101;
    if ((unsigned int)&shared->aligned_by_eight % 8 != 0) return 102;
    shared->aligned_by_four.guard = 0x12345678;
    shared->aligned_by_four.end_guard = 0x98765432;
    int rc = exercise(shared, &shared->aligned_by_four.value);
    if (!rc) rc = exercise(shared, &shared->aligned_by_eight);
    if (shared->aligned_by_four.guard != 0x12345678 || shared->aligned_by_four.end_guard != 0x98765432) rc = 103;
    if (!rc) {
        char message[] = "PASS: eight atomic APIs; 4/8 alignment; 400000 unique tickets; 800000 tear checks/stores; wraparound; guards\n";
        (void)write(1, message, sizeof(message) - 1);
    }
    return rc;
}

__asm__(".globl _start\n_start:\n andl $-16, %esp\n call main\n movl %eax, %ebx\n movl $1, %eax\n int $0x80\n");
