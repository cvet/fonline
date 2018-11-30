#include <stdint.h>

int64_t llvm_nacl_atomic_cmpxchg_i64(int64_t* reg, int64_t oldval, int64_t newval)
{
	int64_t old_reg_val = *reg;
	if (old_reg_val == oldval)
		*reg = newval;
	return old_reg_val;
}
