/**
 **/
#include <string>
#include <cstdint>
#include <list>
#include <map>

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
    class pereloc;
    class peexport;
    class peimport;
    class pedelayimport;
    class peloadconfig;
    

    enum class DirectoryEntry {
        EntryExport = IMAGE_DIRECTORY_ENTRY_EXPORT,
        EntryImport = IMAGE_DIRECTORY_ENTRY_IMPORT,
        EntryResource = IMAGE_DIRECTORY_ENTRY_RESOURCE,
        EntryException = IMAGE_DIRECTORY_ENTRY_EXCEPTION,
        EntrySecurity = IMAGE_DIRECTORY_ENTRY_SECURITY,
        EntryBaseReloc = IMAGE_DIRECTORY_ENTRY_BASERELOC,
        EntryDebug = IMAGE_DIRECTORY_ENTRY_DEBUG,
        EntryArchitecture = 7,  // IMAGE_DIRECTORY_ENTRY_ARCHITECTURE,
        EntryGlobalPtr = IMAGE_DIRECTORY_ENTRY_GLOBALPTR,
        EntryTls = IMAGE_DIRECTORY_ENTRY_TLS,
        EntryLoadConfig = IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG,
        EntryBoundImport = IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT,
        EntryIAT = IMAGE_DIRECTORY_ENTRY_IAT,
        EntryDelayImport = IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT,
        EntryComDescriptor = IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR
    };

    class peloader
    {
        friend class pereloc;   // pereloc
        friend class peresource;
        friend class peexport;
        friend class peimport;
        friend class peloadconfig;
        friend class pedelayimport;

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

        bool    readByte(uint8_t &dst, const va_t va);
        bool    readWord(uint16_t& dst, const va_t va);
        bool    readDword(uint32_t& dst, const va_t va);
        bool    readQword(uint64_t& dst, const va_t va);

        void    writeByte(const uint8_t src, const va_t va);
        void    writeWord(const uint16_t src, const va_t va);
        void    writeDword(const uint32_t src, const va_t va);
        void    writeQword(const uint64_t src, const va_t va);

        bool    sort();    // resort sections..

        inline bool    is64Bit() const { return pNtHeader64 != nullptr; };

        size_t  xref(va_t va, std::list<va_t> &xrefs);

    public:
        bool getDataDirectory(DirectoryEntry entry, va_t& address, size_t& size);
        bool setDataDirectory(DirectoryEntry entry, va_t address, size_t size);

        va_t getEntryPoint();
        void setEntryPoint(va_t va);

        va_t minVa();
        va_t maxVa();

    public: /* section block...*/
        pesection* addSection(const std::string sectionName, size_t size);
        pesection* addSection(const std::string sectionName, va_t va, size_t size);

        bool removeSection(const std::string sectionName);
        bool removeSection(va_t va);

        /** merge two or more section in single section with characteristics of first and data of all sections! */
        pesection *mergeSection(pesection *first, pesection *last);

        pesection* sectionByAddress(va_t va);

        void* rawptr(va_t va);  // promoted as public.. too friend classes!

    protected:
        void moveSections(va_t fromVirtualAddress, size_t delta);   // move all sections after "fromVirtualAddress"
        void updateHeaders(va_t fromVirtualAddress, size_t delta);
        void updateDataDirectory(va_t fromVirtualAddress, size_t delta);

        bool load32bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS32 &pe_header);
        bool load64bit(HANDLE hFile, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS64 &pe_header);

        bool fetch_first_header_section(HANDLE hFile, size_t header_size, const size_t max_sections, IMAGE_SECTION_HEADER &section);

        bool fetch_data(HANDLE hFile, void *buffer, size_t size);
        
        // sections management...
        bool load_sections(HANDLE hFile, WORD NumberOfSections, PIMAGE_SECTION_HEADER pFirstSection);
        bool write_sections(HANDLE hFile, WORD NumberOfSections, PIMAGE_SECTION_HEADER pFirstSection);

        bool update_nt_header();

        size_t section_alignment();
        size_t file_alignment();

        va_t nextSectionAddress();  // return the first address available..

        template<typename T> T ptr(va_t va);

    private:
        void initializeCallbacks();
  
        void onUpdateDebugDirectory(va_t fromVirtualAddress, size_t delta);
        void onUpdateExportDirectory(va_t fromVirtualAddress, size_t delta);
        void onUpdateImportDirectory(va_t fromVirtualAddress, size_t delta);
        void onUpdateDelayImportDirectory(va_t fromVirtualAddress, size_t delta);
        void onUpdateLoadConfigDirectory(va_t fromVirtualAddress, size_t delta);

        void onUpdateBaseReloc(va_t fromVirtualAddress, size_t delta);

        std::map<DirectoryEntry, void (peloader::*)(va_t, size_t)> callbacks;

        char*   _dosstub;       // vector of "dos stub" and raw headers...
        size_t  _stubsize;      // stub size (dos + raw headers)

        PIMAGE_DOS_HEADER   pDosHeader; // an alias of "dos stub"
        PIMAGE_NT_HEADERS32 pNtHeader;
        PIMAGE_NT_HEADERS64 pNtHeader64;

        va_t    _ImageBase;     // store the image base
        std::list<pelib::pesection *> _sections; // a list of sections loaded..

    };
}