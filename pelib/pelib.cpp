#include <windows.h>
#include <algorithm>
#include <string.h>
#include <pelib/pelib.hpp>
#include <pelib/pesection.hpp>
#include <pelib/utils.hpp>
#include <pelib/pereloc.hpp>

bool operator < (const pelib::pesection& a, const pelib::pesection& b)
{
    OutputDebugString(L"operator <");
    return (a.VirtualAddress() < b.VirtualAddress());
}

bool operator == (const pelib::pesection& a, const pelib::pesection& b)
{
    OutputDebugString(L"operator ==");
    return (a.VirtualAddress() == b.VirtualAddress());
}

struct {
    bool operator() (const pelib::pesection *a, const pelib::pesection *b)
    {
        return *a < *b;
    }
} SortSection;

namespace pelib
{
    /** default ctor... */
    peloader::peloader()
    {

    }
    /** default dtor... */
    peloader::~peloader()
    {
        for (auto s : _sections)
           delete s;

        _sections.clear();  // remove all reference...
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
            pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(_dosstub + pDosHeader->e_lfanew);
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

    bool peloader::update_nt_header()
    {
        pNtHeader->FileHeader.NumberOfSections = _sections.size();

        DWORD dwSizeOfCode = 0;
        DWORD dwSizeOfImage = 0;

        const size_t vsAlignment = section_alignment();
        const size_t fsAlignment = file_alignment();

        DWORD PointerToRawData = 0; // this is the pointer to raw data...
        
        if (pNtHeader64 != nullptr)
            PointerToRawData = pNtHeader64->OptionalHeader.FileAlignment;
        else
            PointerToRawData = pNtHeader->OptionalHeader.FileAlignment;

        for (auto s : _sections)
        {
            dwSizeOfImage += roundup(s->VirtualSize(), vsAlignment);
            if (s->isExecutable())
                dwSizeOfCode += roundup(s->VirtualSize(), vsAlignment);

            PointerToRawData += roundup(s->SizeOfRawData(), fsAlignment);
        }

        if (pNtHeader64 != nullptr) {
            pNtHeader64->OptionalHeader.SizeOfCode = dwSizeOfCode;
            pNtHeader64->OptionalHeader.SizeOfImage = dwSizeOfImage;
        }
        else {
            pNtHeader->OptionalHeader.SizeOfCode = dwSizeOfCode;
            pNtHeader->OptionalHeader.SizeOfImage = dwSizeOfImage;
        }

        return true;
    }

    bool peloader::save(const std::string &filename)
    {
        sort(); // resort section... 

        // through pNtHeader we can write also "pNtHeader64"
        pNtHeader->FileHeader.NumberOfSections = _sections.size();

        // update size..

    
        if (pNtHeader64 != nullptr)
        {   // 64 bit required..
            
        }
        else
        {

        }
        return false;
    }

    /** */
    bool peloader::load_sections(HANDLE hFile, WORD NumberOfSections, PIMAGE_SECTION_HEADER pFirstSection)
    {
        for (WORD n = 0; n < NumberOfSections; n++, pFirstSection++)
        {   // point to right section on disk...
            SetFilePointer(hFile, pFirstSection->PointerToRawData, NULL, FILE_BEGIN);

            DWORD dwNumberOfBytesRead = 0;
            void* rawdata = nullptr;
                
            if (pFirstSection->SizeOfRawData > 0)
            {
                rawdata = malloc(pFirstSection->SizeOfRawData);
                ReadFile(hFile, rawdata, pFirstSection->SizeOfRawData, &dwNumberOfBytesRead, NULL);
                if (dwNumberOfBytesRead == pFirstSection->SizeOfRawData)
                {   // right...
                }
            }

            _sections.push_back(new pesection(pFirstSection, (const char *) rawdata, dwNumberOfBytesRead));
                
            if (rawdata != nullptr)
                free(rawdata);
        }
        return true;
    }

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

    /** ImageBase */
    va_t peloader::getImageBase()
    {
        if (pNtHeader64 != nullptr)
            return pNtHeader64->OptionalHeader.ImageBase;

        return pNtHeader->OptionalHeader.ImageBase;
    }

    void peloader::setImageBase(va_t NewImageBase)
    {
        const va_t ImageBase = getImageBase();

        if (ImageBase == NewImageBase) // nothing to do...
            return;

        // update image base...
        pereloc reloc(this);
        reloc.setNewBaseAddress(NewImageBase);

        // final step.. update NT_HEADER
        if (pNtHeader64 != nullptr)
        {
            pNtHeader64->OptionalHeader.ImageBase = NewImageBase;
        }
        else
        {
            pNtHeader->OptionalHeader.ImageBase = (DWORD) NewImageBase;
        }

    }

    void peloader::fill(BYTE pattern)
    {
        for (auto s : _sections)
            s->fill(pattern);
    }

    bool peloader::memread(void* dst, va_t address, size_t size)
    {
        char* cdst = (char *)dst;

        for (auto s : _sections)
        {
            va_t start = s->VirtualAddress();
            va_t end = s->VirtualEndAddress();

            if (end < address)
            {   // wrong section..
                continue;
            }

            size_t available = end - address;

            if (available > size)
                available = size;

            s->memread(cdst, address, available);
            size -= available;

            cdst += available;

            if (size == 0)
                break;
        }

        return true;
    }

    bool peloader::memwrite(void* src, size_t size, va_t address)
    {
        char* csrc = (char*)src;

        for (auto s : _sections)
        {
            va_t start = s->VirtualAddress();
            va_t end = s->VirtualEndAddress();

            if (end < address)
            {   // wrong section..
                continue;
            }

            size_t available = end - address;

            if (available > size)
                available = size;

            s->memwrite(csrc, available, address);
            size -= available;

            csrc += available;
            address += available;

            if (size == 0)
                break;
        }

        return true;
    }

    /** sort section in list.. */
    bool peloader::sort()
    {
        _sections.sort(SortSection);

        return true;
    }

    bool peloader::getDataDirectory(DirectoryEntry entry, va_t& address, size_t& size)
    {
        DWORD n = (DWORD)entry;

        if (pNtHeader64 != nullptr) {
            address = pNtHeader64->OptionalHeader.DataDirectory[n].VirtualAddress;
            size = pNtHeader64->OptionalHeader.DataDirectory[n].Size;
        }
        else {
            address = pNtHeader->OptionalHeader.DataDirectory[n].VirtualAddress;
            size = pNtHeader->OptionalHeader.DataDirectory[n].Size;
        }

        if (address == 0 || size == 0)
            return false;

        return true;
    }

    bool peloader::setDataDirectory(DirectoryEntry entry, va_t address, size_t size)
    {
        DWORD n = (DWORD)entry;

        if (pNtHeader64 != nullptr) {
            pNtHeader64->OptionalHeader.DataDirectory[n].VirtualAddress = (DWORD) address;
            pNtHeader64->OptionalHeader.DataDirectory[n].Size = (DWORD)size;
        }
        else {
            pNtHeader->OptionalHeader.DataDirectory[n].VirtualAddress = (DWORD)address;
            pNtHeader->OptionalHeader.DataDirectory[n].Size = (DWORD)size;
        }

        return true;
    }

    size_t peloader::section_alignment()
    {
        if (pNtHeader64 != nullptr)
            return pNtHeader64->OptionalHeader.SectionAlignment;

        return pNtHeader->OptionalHeader.SectionAlignment;
    }

    size_t peloader::file_alignment()
    {
        if (pNtHeader64 != nullptr)
            return pNtHeader64->OptionalHeader.FileAlignment;

        return pNtHeader->OptionalHeader.FileAlignment;
    }

    /** find the highest address allocated by section.. must be _sections[count()-1] but choose to lookup inside list */
    va_t peloader::nextSectionAddress()
    {
        va_t latest = 0;

        for (auto s : _sections)
            if (s->VirtualEndAddress() > latest)
                latest = s->VirtualEndAddress();

        return roundup(latest, section_alignment());
    }

    /** return the right section accessible via va */
    pesection* peloader::sectionByAddress(va_t va)
    {
        for (auto s : _sections)
            if (va_in_range(s->VirtualAddress(), s->VirtualEndAddress(), va))
                return s;
        return nullptr;
    }

    /** add a virtual section of N bytes at trail */
    pesection* peloader::addSection(const std::string sectionName, size_t size)
    {
        IMAGE_SECTION_HEADER section = { 0 };

        section.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
        strcpy_s((char *)section.Name, sizeof(section.Name), sectionName.c_str());
        section.SizeOfRawData = roundup(size, this->file_alignment());
        section.Misc.VirtualSize = size;
        section.VirtualAddress = nextSectionAddress();

        pesection* nsect = new pesection(&section, nullptr, section.SizeOfRawData);

        _sections.push_back(nsect);

        return nsect;
    }

    pesection* peloader::addSection(const std::string sectionName, va_t va, size_t size)
    {
        IMAGE_SECTION_HEADER section = { 0 };

        section.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
        strcpy_s((char*)section.Name, sizeof(section.Name), sectionName.c_str());
        section.SizeOfRawData = roundup(size, this->file_alignment());
        section.Misc.VirtualSize = size;
        section.VirtualAddress = va;

        va_t delta = roundup(size, this->section_alignment());

        // update header and status...
        for (auto s : _sections)
            if (s->VirtualAddress() >= va)
                s->setVirtualAddress(s->VirtualAddress() + delta);

        pesection* nsect = new pesection(&section, nullptr, section.SizeOfRawData);

        _sections.push_back(nsect);   // put the section inside...
        _sections.push_back(nsect);   // put the section inside...
        sort();

        // propagate address change..

        return nsect;
    }

    /** */
    bool peloader::removeSection(va_t va)
    {
        pesection* remove = sectionByAddress(va);
        
        if (remove != nullptr)
        {
            va_t begin = remove->VirtualAddress();
            va_t end = remove->VirtualEndAddress();
            va_t delta = roundup(remove->VirtualSize(), this->section_alignment());

            _sections.remove(remove);
            delete remove;

            // remove all elements with link to this section...

            // update header and status...
            for (auto s : _sections)
                if (s->VirtualAddress() >= begin)
                    s->setVirtualAddress(s->VirtualAddress() - delta);
        }

        return false;
    }

    bool peloader::removeSection(const std::string sectionName)
    {
        va_t address = 0;

        for (auto s : _sections) {
            size_t length = strlen((const char *)s->header.Name);
            if (length > 8) length = 8;

            if (memcmp(s->header.Name, sectionName.c_str(), length) == 0) {
                address = s->VirtualAddress();
                break;
            }
        }

        if (address != 0) {
            return removeSection(address);
        }

        return false;
    }

    /** merge two section... */
    pesection* peloader::mergeSection(pesection* first, pesection* last)
    {
        if (first == nullptr || last == nullptr)
            return nullptr;

        if (first == last)  // same section..
            return nullptr;

        if (first->VirtualAddress() > last->VirtualAddress()) { // swap pointer..
            pesection* tmp = last;
            last = first;
            first = tmp;
        }

        IMAGE_SECTION_HEADER sectionHeader = { 0 };
        memcpy(&sectionHeader, &first->header, sizeof(IMAGE_SECTION_HEADER));   // transfer data...

        const size_t size_raw_1 = roundup(first->VirtualSize(), section_alignment());  // first section size must be "VirtualSize" + alignment..
        const size_t size_raw_2 = last->SizeOfRawData();
        const size_t new_raw_size = size_raw_1 + size_raw_2;

        char* mergedData = (char*)malloc(new_raw_size);
        
        size_t mergedSize = roundup(first->VirtualSize(), section_alignment()) +
            roundup(last->VirtualSize(), section_alignment());

        memset(mergedData, 0, new_raw_size);
        memcpy(mergedData, first->_data, first->_RawSize);  // copy first section..
        memcpy(&mergedData[size_raw_1], last->_data, size_raw_2);

        // header reflect new structure
        sectionHeader.Misc.VirtualSize = mergedSize;
        sectionHeader.SizeOfRawData = new_raw_size;

        pesection* merged = new pesection(sectionHeader, mergedData, mergedSize);
        
        // [todo: replace element... ]
        _sections.remove(first);
        _sections.remove(last);
        
        delete first;   // destroy section...
        delete last;
        
        _sections.push_back(merged);
        sort(); // sort..

        return merged;
    }

    /** return a pointer to manage directly data.. used for structure  and massive operations */
    void* peloader::rawptr(va_t va)
    {
        pesection *s = sectionByAddress(va);

        if (s == nullptr)
            return nullptr;

        return &s->_data[va - s->_VirtualAddress];
    }
};
