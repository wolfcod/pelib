#include <windows.h>
#include <pelib/pelib.hpp>
#include <pelib/pesection.hpp>
#include <pelib/utils.hpp>

namespace pelib
{
    /** default ctor... */
    peloader::peloader()
    {

    }
    /** default dtor... */
    peloader::~peloader()
    {

    }
    
    /** retrieve a block of data from stream... */
    bool peloader::fetch_data(HANDLE hFile, void *buffer, size_t size)
    {
        DWORD numberOfBytesread = 0;

        if (ReadFile(hFile, buffer, (DWORD) size, &numberOfBytesread, NULL) == 0)
            return false;

        if (size != numberOfBytesread)
            return false;

        return true;
    }

    bool peloader::fetch_first_header_section(HANDLE hFile, size_t header_size, const size_t max_sections, IMAGE_SECTION_HEADER &section)
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


    bool peloader::load32bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS32 &pe_header)
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

            if (ReadFile(hFile, _dosstub, (first_section.PointerToRawData != 0) ? first_section.PointerToRawData : 0x1000, &numberOfBytesread, NULL) == FALSE)
                return false;

            // set pointer "DOS_HEADER" and "NT_HEADER" to our "alias"
            pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(_dosstub);
            pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(_dosstub + pDosHeader->e_lfanew);

            return true;
    }

    bool peloader::load64bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS64 &pe_header)
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

            if (ReadFile(hFile, _dosstub, (first_section.PointerToRawData != 0) ? first_section.PointerToRawData : 0x1000, &numberOfBytesread, NULL) == FALSE)
                return false;

            // set pointer "DOS_HEADER" and "NT_HEADER" to our "alias"
            pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(_dosstub);
            pNtHeader64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(_dosstub + pDosHeader->e_lfanew);
    
            return true;
    }

    bool peloader::load(const std::string &filename)
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
            return false;
        }

        if (pe_header.Signature != IMAGE_NT_SIGNATURE)
        {
            CloseHandle(hFile);
            return false;
        }

        if (pe_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
        {
            if (load32bit(hFile, dos_header, pe_header))
            {
                load_sections(hFile, pe_header.FileHeader.NumberOfSections, IMAGE_FIRST_SECTION(pNtHeader));
            }
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

            if (load64bit(hFile, dos_header, pe64_header))
            {
                load_sections(hFile, pe64_header.FileHeader.NumberOfSections, IMAGE_FIRST_SECTION(pNtHeader64));
            }

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

    bool peloader::save(const std::string &filename)
    {
        return false;
    }

    /** */
    bool peloader::load_sections(HANDLE hFile, WORD NumberOfSections, PIMAGE_SECTION_HEADER pFirstSection)
    {
        for (WORD n = 0; n < NumberOfSections; n++, pFirstSection++)
        {   // point to right section on disk...
            SetFilePointer(hFile, pFirstSection->PointerToRawData, NULL, FILE_BEGIN);

            if (pFirstSection->PointerToRawData != 0 && pFirstSection->SizeOfRawData > 0) 
            {   // a region with valid data on disk...
                DWORD dwNumberOfBytesRead = 0;
                void* rawdata = malloc(pFirstSection->SizeOfRawData);
                ReadFile(hFile, rawdata, pFirstSection->SizeOfRawData, &dwNumberOfBytesRead, NULL);
                if (dwNumberOfBytesRead == pFirstSection->SizeOfRawData)
                {   // right...

                }

                _sections.push_back(new pesection(pFirstSection, rawdata, dwNumberOfBytesRead));
                free(rawdata);
            }
            else
            {   // a region.. bss or packed region to be loaded only on memory!
                _sections.push_back(new pesection(pFirstSection, nullptr, 0));
            }
        }
        return true;
    }
};

/** */
bool peloader::write_sections(HANDLE hFile, WORD NumberOfSections, PIMAGE_SECTION_HEADER pFirstSection)
{
    //for (WORD n = 0; n < NumberOfSections; n++, pFirstSection++)
    //{   // point to right section on disk...
    //    SetFilePointer(hFile, pFirstSection->PointerToRawData, NULL, FILE_BEGIN);

    //    if (pFirstSection->PointerToRawData != 0 && pFirstSection->SizeOfRawData > 0)
    //    {   // a region with valid data on disk...
    //        DWORD dwNumberOfBytesRead = 0;
    //        void* rawdata = malloc(pFirstSection->SizeOfRawData);
    //        ReadFile(hFile, rawdata, pFirstSection->SizeOfRawData, &dwNumberOfBytesRead, NULL);
    //        if (dwNumberOfBytesRead == pFirstSection->SizeOfRawData)
    //        {   // right...

    //        }

    //        _sections.push_back(new pesection(pFirstSection, rawdata, dwNumberOfBytesRead));
    //        free(rawdata);
    //    }
    //    else
    //    {   // a region.. bss or packed region to be loaded only on memory!
    //        _sections.push_back(new pesection(pFirstSection, nullptr, 0));
    //    }
    //}
    return true;
}
};
