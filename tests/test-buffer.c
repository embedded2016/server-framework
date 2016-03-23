#include "buffer.h"

int main(void)
{
    void *buf = Buffer.new(0);

    Buffer.destroy(buf);

    return 0;
}
