#include <stdio.h>
#include "mvh/net.h"

int main(void)
{
    if (net_self_test() != 0) {
        puts("network protocol test failed");
        return 1;
    }
    puts("network protocol test passed");
    return 0;
}
