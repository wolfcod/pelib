#pragma once
#include <pelib/va.hpp>

namespace pelib
{
	class peloader;

	typedef struct {
		short type;
		va_t address;
	} RelocationEntry;
	
	class pereloc
	{
		public:
			pereloc(peloader* pe);
			~pereloc();

			void setNewBaseAddress(va_t newBaseAddress);

			// add a symbol into relocation entry..
			bool addRelocationEntry(short type, va_t va);

			// remove a symbol from relocation entry
			bool removeRelocationEntry(va_t va);

			// 
			bool removeRelocationEntry(va_t begin, va_t end);

			// a section will be moved from X segment to Y segment..
			bool moveSection(va_t fromRVA, va_t toRVA);

			inline va_t lowRVA(va_t rva) { return rva & ~(0xfff); };
			inline va_t highRVA(va_t rva) { return rva | 0xfff; };

			void relocs(std::list<va_t>& relocs, va_t begin = 0, va_t end = 0);

		protected:
			void update_baseaddress(short type, va_t address, va_t oldBaseAddress, va_t newBaseAddress);
			void processSectionEntry(va_t fromRVA, va_t toRVA, PIMAGE_BASE_RELOCATION pBaseRelocation);

		private:
			PIMAGE_BASE_RELOCATION openRelocPage();
			PIMAGE_BASE_RELOCATION nextRelocPage(PIMAGE_BASE_RELOCATION pBaseRelocation);

			peloader* pe;

	};
}