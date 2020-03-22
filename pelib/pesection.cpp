#include <windows.h>
#include <stdio.h>
#include <pelib/peloader.hpp>
#include <pelib/utils.hpp>
#include <pelib/pesection.hpp>

namespace pelib
{

    /** initialize an empty section... */
    pesection::pesection()
        : _data(nullptr),
        _VirtualAddress(0),
        _VirtualSize(0),
        _RawSize(0)
    {
        memset(&header, 0, sizeof(header));
    }

    /** used by pelib section loader... */
    pesection::pesection(const PIMAGE_SECTION_HEADER pHeader, const char* data, const size_t size)
    {
        memcpy(&header, pHeader, sizeof(IMAGE_SECTION_HEADER));
        _VirtualAddress = pHeader->VirtualAddress;
        _VirtualSize = pHeader->Misc.VirtualSize;
        _RawSize = pHeader->SizeOfRawData;

        if (pHeader->SizeOfRawData != 0)
        {
            size_t block_len = max(pHeader->Misc.VirtualSize, pHeader->SizeOfRawData);
            
            _data = new char[block_len];
            memset(_data, 0, block_len);    // fill with 00 all memory allocated...

            if (data != nullptr)    // can be null for new section...
                memcpy(_data, data, size);      // transfer data into allocated block
        }
    }

    pesection::~pesection()
    {
        if (_data != nullptr)
            delete _data;
    }

    void pesection::fill(BYTE pattern)
    {
        if (_data != nullptr)
        {
            memset(_data, pattern, _RawSize);
        }
    }

    bool pesection::memread(void* dst, va_t address, size_t size)
    {
        if (_data != nullptr && address >= _VirtualAddress)
        {
            va_t endaddress = address + size;
            if (endaddress > (_VirtualAddress + size))
                endaddress = _VirtualAddress + size;

            size_t blocksize = endaddress - address;

            memcpy(dst, _data + address, blocksize);

            return true;
        }

        return false;
    }

    bool pesection::memwrite(void* src, size_t size, va_t address)
    {
        if (_data != nullptr && address >= _VirtualAddress)
            {
                va_t endaddress = address + size;
                if (endaddress > (_VirtualAddress + size))
                    endaddress = _VirtualAddress + size;

                size_t blocksize = endaddress - address;

                memcpy(_data + address, src, blocksize);

                return true;
            }

            return false;
    }

};  // end of pelib namespace



