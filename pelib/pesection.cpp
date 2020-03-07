#include <windows.h>
#include <stdio.h>
#include <pelib/pesection.hpp>

namespace pelib
{

void pelib::fill(BYTE pattern)
{
    if (_data != nullptr)
    {
        memset(_data, pattern, _RawSize);
    }
}

};  // end of pelib namespace



