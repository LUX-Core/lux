#include <stdio.h>
#include <evmjit.h>

int main()
{
    struct evm_factory factory = evmjit_get_factory();
    if (EVM_ABI_VERSION != factory.abi_version)
    {
        printf("Error: expected ABI version %u!\n", EVM_ABI_VERSION);
        return 1;
    }
    printf("EVMJIT ABI version %u\n", factory.abi_version);
    return 0;
}
