#pragma once


namespace pelib
{
    class peloader;

    /** a generic wrapper of section... */
    class pesection
    {
    public:
        friend class peloader; // pelib can be manipulate directly a pesection..

        ~pesection();

        inline size_t VirtualSize() const { return header.Misc.VirtualSize; };
        inline size_t RawSize() const { return _RawSize; };
        inline size_t SizeOfRawData() const { return header.SizeOfRawData; };
        inline va_t VirtualAddress() const { return _VirtualAddress; };
        inline va_t VirtualEndAddress() const { return _VirtualAddress + _VirtualSize; };

        void    fill(BYTE pattern);
        bool    memread(void *dst, va_t address, size_t size);
        bool    memwrite(void *src, size_t size, va_t address);

        bool    isExecutable() const { return (header.Characteristics & IMAGE_SCN_CNT_CODE) == IMAGE_SCN_CNT_CODE; };
        bool    isReadonly() const { return (header.Characteristics & IMAGE_SCN_MEM_WRITE) != IMAGE_SCN_MEM_WRITE; };

    protected:
        pesection();    // default constructor not allowed

        pesection(const PIMAGE_SECTION_HEADER pHeader, const char* data, const size_t size);

        void    setVirtualAddress(va_t va) { _VirtualAddress = va; header.VirtualAddress = va; };

        

    private:
        IMAGE_SECTION_HEADER header;
        va_t    _VirtualAddress;
        size_t  _VirtualSize;
        size_t  _RawSize;

        char *_data;    // data inside section..
    };

}   // end of namespace pelib