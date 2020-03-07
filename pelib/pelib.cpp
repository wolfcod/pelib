#include <windows.h>
#include <pelib/pelib.hpp>
#include <pelib/utils.hpp>

namespace pelib
{
    /** default ctor... */
    pefile::pefile()
    {

    }
    /** default dtor... */
    pefile::~pefile()
    {

    }
    
    /** retrieve a block of data from stream... */
    bool pefile::fetch_data(HANDLE hFile, void *buffer, size_t size)
    {
        DWORD numberOfBytesread = 0;

        if (ReadFile(hFile, buffer, (DWORD) size, &numberOfBytesread, NULL) == 0)
            return false;

        if (size != numberOfBytesread)
            return false;

        return true;
    }

    bool pefile::fetch_first_header_section(HANDLE hFile, size_t header_size, const size_t max_sections, IMAGE_SECTION_HEADER &section)
    {
        SetFilePointer(hFile, (LONG) header_size, NULL, FILE_BEGIN);   // set pointer on first section...

        memset(&section, 0, sizeof(IMAGE_SECTION_HEADER));

        size_t section_number = 0;

        while(section_number < max_sections && section.PointerToRawData == 0)
        {
            if (fetch_data(hFile, &section, sizeof(section)) == false)
                return false;
        }

        return true;
    }

    bool pefile::load32bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS32 &pe_header)
    {
            DWORD sizeHeader = dos_header.e_lfanew + 
                pe_header.FileHeader.SizeOfOptionalHeader + 
                sizeof(pe_header.FileHeader) + 4;

            IMAGE_SECTION_HEADER first_section;
            DWORD numberOfBytesread = 0;
            ZeroMemory(&first_section, sizeof(IMAGE_SECTION_HEADER));

            if (fetch_first_header_section(hFile, sizeHeader, pe_header.FileHeader.NumberOfSections, first_section) == false)
                return false;

            SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            _stubsize = roundup(first_section.PointerToRawData, 0x1000);    // must be aligned with section values...

            _dosstub = new char[_stubsize];

            if (this->_dosstub == nullptr) {
                // insufficient memory or anything wrong with pe file..
                return false;
            }

            BOOL bResult = ReadFile(hFile, _dosstub, (first_section.PointerToRawData != 0) ? first_section.PointerToRawData : 0x1000, &numberOfBytesread, NULL);

                    if (bResult == FALSE)
            {
                DebugBreak();
            }

            // set pointer "DOS_HEADER" and "NT_HEADER" to our "alias"
            pDosHeader = (PIMAGE_DOS_HEADER) _dosstub;
            _lpNtHeader = CALC_OFFSET(PIMAGE_NT_HEADERS32, pDosHeader, pDosHeader->e_lfanew);
            
            struct _file_support *stream = get_by_image_format(_lpNtHeader->FileHeader.Machine);

            stream->read(this, hFile, &_sections);	// read from file


    }

    bool pefile::load64bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS64 &pe_header)
    {
            DWORD sizeHeader = dos_header.e_lfanew + 
                pe_header.FileHeader.SizeOfOptionalHeader + 
                sizeof(pe_header.FileHeader) + 4;

            IMAGE_SECTION_HEADER first_section;
            DWORD numberOfBytesread = 0;

            ZeroMemory(&first_section, sizeof(IMAGE_SECTION_HEADER));

            if (fetch_first_header_section(hFile, sizeHeader, pe_header.FileHeader.NumberOfSections, first_section) == false)
                return false;

            SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            _stubsize = roundup(first_section.PointerToRawData, 0x1000);    // must be aligned with section values...

            _dosstub = new char[_stubsize];

            SetFilePointer(hFile, 0, NULL, SEEK_SET);

            BOOL bResult = ReadFile(hFile, _dosstub, first_section.PointerToRawData, &numberOfBytesread, NULL);

            // set pointer "DOS_HEADER" and "NT_HEADER" to our "alias"
            pDosHeader = (PIMAGE_DOS_HEADER) _dosstub;
            _lpNtHeader64 = CALC_OFFSET(PIMAGE_NT_HEADERS64, _lpDosHeader, _lpDosHeader->e_lfanew);

            struct _file_support *stream = get_by_image_format(_lpNtHeader64->FileHeader.Machine);

            stream->read(this, hFile, &_sections);	// read from file

    }

    bool pefile::load(const std::string &filename)
    {
        HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
            return NULL;

        DWORD numberOfBytesread;
        IMAGE_DOS_HEADER dos_header;
        ZeroMemory(&dos_header, sizeof(dos_header));

        if (ReadFile(hFile, &dos_header, sizeof(dos_header), &numberOfBytesread, NULL) == FALSE)
        {
            CloseHandle(hFile);
            return NULL;
        }

        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE || dos_header.e_lfanew == 0)
        {
            CloseHandle(hFile);
            return NULL;
        }

        IMAGE_NT_HEADERS32 pe_header;
        ZeroMemory(&pe_header, sizeof(pe_header));

        SetFilePointer(hFile, dos_header.e_lfanew, NULL, FILE_BEGIN);

        if (ReadFile(hFile, &pe_header, sizeof(pe_header), &numberOfBytesread, NULL) == FALSE)
        {
            CloseHandle(hFile);
            return NULL;
        }

        if (pe_header.Signature != IMAGE_NT_SIGNATURE)
        {
            CloseHandle(hFile);
            return NULL;
        }

        if (pe_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
        {
            load32bit(hFile, dos_header, pe_header);
        }
        else if (pe_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
        {
            IMAGE_NT_HEADERS64 pe64_header;
            ZeroMemory(&pe64_header, sizeof(pe_header));

            SetFilePointer(hFile, dos_header.e_lfanew, NULL, FILE_BEGIN);

            if (ReadFile(hFile, &pe64_header, sizeof(pe64_header), &numberOfBytesread, NULL) == FALSE)
            {
                CloseHandle(hFile);
                return NULL;
            }

            load64bit(hFile, dos_header, pe64_header);
        }
        else
        {	// unsupported file machine
        }

        // update alias!
        if (_dosstub != nullptr) 
        {   // upgrade pointers...
            pDosHeader = (PIMAGE_DOS_HEADER) _dosstub;
        }

        CloseHandle(hFile);
        return true;
    }

    bool pefile::save(const std::string &filename)
    {

    }

};
