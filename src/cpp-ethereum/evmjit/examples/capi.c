#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "evm.h"


struct evm_uint256be balance(struct evm_env* env, struct evm_uint160be address)
{
    struct evm_uint256be ret = {.bytes = {1, 2, 3, 4}};
    return ret;
}

struct evm_uint160be address(struct evm_env* env)
{
    struct evm_uint160be ret = {.bytes = {1, 2, 3, 4}};
    return ret;
}

static void query(union evm_variant* result,
                  struct evm_env* env,
                  enum evm_query_key key,
                  const union evm_variant* arg) {
    printf("EVM-C: QUERY %d\n", key);
    switch (key) {
    case EVM_GAS_LIMIT:
        result->int64 = 314;
        break;

    case EVM_BALANCE:
        result->uint256be = balance(env, arg->address);
        break;

    case EVM_ADDRESS:
        result->address = address(env);
        break;

    default:
        result->int64 = 0;
    }
}

static void update(struct evm_env* env,
                   enum evm_update_key key,
                   const union evm_variant* arg1,
                   const union evm_variant* arg2)
{
    printf("EVM-C: UPDATE %d\n", key);
}

static int64_t call(
    struct evm_env* _opaqueEnv,
    enum evm_call_kind _kind,
    int64_t _gas,
    const struct evm_uint160be* _address,
    const struct evm_uint256be* _value,
    uint8_t const* _inputData,
    size_t _inputSize,
    uint8_t* _outputData,
    size_t _outputSize
)
{
    printf("EVM-C: CALL %d\n", _kind);
    return EVM_CALL_FAILURE;
}

/// Example how the API is supposed to be used.
int main(int argc, char *argv[]) {
    struct evm_factory factory = examplevm_get_factory();
    if (factory.abi_version != EVM_ABI_VERSION)
        return 1;  // Incompatible ABI version.

    struct evm_instance* jit = factory.create(query, update, call);

    uint8_t const code[] = "Place some EVM bytecode here";
    const size_t code_size = sizeof(code);
    struct evm_uint256be code_hash = {.bytes = {1, 2, 3,}};
    uint8_t const input[] = "Hello World!";
    struct evm_uint256be value = {{1, 0, 0, 0}};

    int64_t gas = 200000;
    struct evm_result result =
        jit->execute(jit, NULL, EVM_HOMESTEAD, code_hash, code, code_size, gas,
                     input, sizeof(input), value);

    printf("Execution result:\n");
    if (result.code != EVM_SUCCESS) {
      printf("  EVM execution failure: %d\n", result.code);
    } else {
        printf("  Gas used: %ld\n", gas - result.gas_left);
        printf("  Gas left: %ld\n", result.gas_left);
        printf("  Output size: %zd\n", result.output_size);

        printf("  Output: ");
        size_t i = 0;
        for (i = 0; i < result.output_size; i++) {
            printf("%02x ", result.output_data[i]);
        }
        printf("\n");
    }

    result.release(&result);
    jit->destroy(jit);
}
