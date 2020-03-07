#include <cstdint>

size_t roundup(size_t value, size_t base)
{
    if ((value % base) == 0)
        return value;

    return ((value / base) * base + base);
}
