/**
 **/
#include <string>
#include <cstdint>
#include <list>

#ifndef __WINDOWS
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
    class pefile
    {
       public:
        /** default constructor... */
        pefile();

        /** destructor... */
        ~pefile();

        bool load(const std::string &filename);
        bool save(const std::string &filename);

    protected:

        bool load32bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS32 &pe_header);
        bool load64bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS64 &pe_header);

        bool fetch_first_header_section(HANDLE hFile, size_t header_size, const size_t max_sections, IMAGE_SECTION_HEADER &section);

        bool fetch_data(HANDLE hFile, void *buffer, size_t size);
        
    private:
        char*   _dosstub;       // vector of "dos stub" and raw headers...
        size_t  _stubsize;      // stub size (dos + raw headers)

        PIMAGE_DOS_HEADER   pDosHeader; // an alias of "dos stub"
    };
}