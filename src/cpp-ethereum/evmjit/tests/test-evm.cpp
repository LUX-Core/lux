#include <evm.h>

int64_t test_call_failure_constant(int64_t a)
{
    return a | EVM_CALL_FAILURE;
}
