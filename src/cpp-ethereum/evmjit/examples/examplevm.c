#include <stdlib.h>
#include <string.h>
#include "evm.h"


struct examplevm
{
    struct evm_instance instance;
    evm_query_fn query_fn;
    evm_update_fn update_fn;
    evm_call_fn call_fn;
};

static void evm_destroy(struct evm_instance* evm)
{
    free(evm);
}

/// Example options.
///
/// VMs are allowed to omit this function implementation.
int evm_set_option(struct evm_instance* evm,
                   char const* name,
                   char const* value)
{
    if (strcmp(name, "example-option") == 0)
        return 1;
    return 0;
}

static void evm_release_result(struct evm_result const* result)
{
}

static struct evm_result evm_execute(struct evm_instance* instance,
                                     struct evm_env* env,
                                     enum evm_mode mode,
                                     struct evm_uint256be code_hash,
                                     uint8_t const* code,
                                     size_t code_size,
                                     int64_t gas,
                                     uint8_t const* input,
                                     size_t input_size,
                                     struct evm_uint256be value)
{
    struct evm_result ret = {};

    // Execute code and refer to callbacks: instance->query_fn()

    ret.release = evm_release_result;
    ret.code = EVM_FAILURE;
    ret.gas_left = 0;

    return ret;
}

static struct evm_instance* evm_create(evm_query_fn query_fn,
                                       evm_update_fn update_fn,
                                       evm_call_fn call_fn)
{
    struct examplevm* vm = calloc(1, sizeof(struct examplevm));
    struct evm_instance* interface = &vm->instance;
    interface->destroy = evm_destroy;
    interface->execute = evm_execute;
    interface->set_option = evm_set_option;
    vm->query_fn = query_fn;
    vm->update_fn = update_fn;
    vm->call_fn = call_fn;
    return interface;
}

struct evm_factory examplevm_get_factory()
{
    struct evm_factory factory = {EVM_ABI_VERSION, evm_create};
    return factory;
}
