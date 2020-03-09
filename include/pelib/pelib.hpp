/**
 **/
#include <string>
#include <cstdint>
#include <list>

#ifndef _WIN32
typedef uint8_t     BYTE;
typedef uint16_t    WORD;
typedef uint32_t    DWORD;
typedef uint64_t    QWORD;
#endif

#ifdef _WIN64
typedef uint64_t    va_t;
#else
typedef uint32_t    va_t;
#endif

#ifndef _SIZE_T_DEFINED
typedef unsigned int    size_t;
#endif

namespace pelib
{
    class pesection;

    class peloader
    {
       public:
        /** default constructor... */
           peloader();

        /** destructor... */
        ~peloader();

        bool load(const std::string &filename);
        bool save(const std::string &filename);


        va_t    getImageBase();
        void    setImageBase(va_t NewImageBase);

        void    fill(BYTE pattern);
        bool    memread(void* dst, va_t address, size_t size);
        bool    memwrite(void* src, size_t size, va_t address);

    protected:

        bool load32bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS32 &pe_header);
        bool load64bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS64 &pe_header);

        bool fetch_first_header_section(HANDLE hFile, size_t header_size, const size_t max_sections, IMAGE_SECTION_HEADER &section);

        bool fetch_data(HANDLE hFile, void *buffer, size_t size);
        
        // sections management...
        bool load_sections(HANDLE hFile, WORD NumberOfSections, PIMAGE_SECTION_HEADER pFirstSection);
        bool write_sections(HANDLE hFile, WORD NumberOfSections, PIMAGE_SECTION_HEADER pFirstSection);

    private:
        char*   _dosstub;       // vector of "dos stub" and raw headers...
        size_t  _stubsize;      // stub size (dos + raw headers)

        PIMAGE_DOS_HEADER   pDosHeader; // an alias of "dos stub"
        PIMAGE_NT_HEADERS32 pNtHeader;
        PIMAGE_NT_HEADERS64 pNtHeader64;

        std::list<pelib::pesection *> _sections; // a list of sections loaded..

    };
}