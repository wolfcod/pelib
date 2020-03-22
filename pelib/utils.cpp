#include <cstdint>
#include <pelib/va.hpp>

size_t roundup(size_t value, size_t base)
{
    if ((value % base) == 0)
        return value;

    return ((value / base) * base + base);
}

bool va_in_range(va_t low, va_t high, va_t x)
{
    if (x >= low && x <= high)
        return true;

    return false;
}

va_t adjustIfAbove(va_t value, va_t aboveValue, va_t delta)
{
    if (value < aboveValue)
        return value;

    value += delta;
    return value;
}