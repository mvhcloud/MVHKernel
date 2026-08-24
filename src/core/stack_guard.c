#include <stdint.h>
#include "mvh/panic.h"

uintptr_t __stack_chk_guard = (uintptr_t)0xA5D39E47C61B82F1ull;

void __stack_chk_fail(void)
{
    kernel_panic("compiler stack protector detected corruption");
}
