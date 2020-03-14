#ifndef UTILS_HPP_
#define UTILS_HPP_

size_t roundup(size_t value, size_t base);
bool va_in_range(va_t low, va_t high, va_t x);

va_t adjustIfAbove(va_t value, va_t aboveValue, va_t delta);

#endif
