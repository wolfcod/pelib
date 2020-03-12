#include <windows.h>
#include <list>
#include <pelib/pelib.hpp>
#include <pelib/utils.hpp>
#include <pelib/pesection.hpp>
#include <pelib/pereloc.hpp>

bool operator < (const pelib::RelocationEntry& a, const pelib::RelocationEntry& b)
{
	return (a.address < b.address);
}

bool operator == (const pelib::RelocationEntry& a, const pelib::RelocationEntry& b)
{
	return (a.address == b.address);
}

namespace pelib
{
	pereloc::pereloc(peloader* arg)
		: pe(arg)
	{

	}

	pereloc::~pereloc()
	{

	}

	void pereloc::update_baseaddress(short type, va_t address, va_t oldBaseAddress, va_t newBaseAddress)
	{
		if (type == IMAGE_REL_BASED_HIGHLOW) {
			DWORD* pData = (DWORD*)pe->rawptr(address);
			DWORD uNewValue = *pData - oldBaseAddress + newBaseAddress;
			*pData = uNewValue;
		}
		else if (type == IMAGE_REL_BASED_DIR64) {
			uint64_t* pData = (uint64_t*)pe->rawptr(address);
			uint64_t uNewValue = *pData - oldBaseAddress + newBaseAddress;
			*pData = uNewValue;
		}
	}

	typedef struct {
		short type;
		va_t  va;
	} entry_reloc;

	typedef std::list<entry_reloc> ENTRY_RELOC;

	/** used by ENTRY_RELOC.sort() */
	bool operator < (const entry_reloc& a, const entry_reloc& b)
	{
		return (a.va < b.va);
	}
	
	struct is_equal_entry {
		va_t va;

		bool operator() (const entry_reloc& a) { return a.va == va; };
	};

	bool operator == (const entry_reloc& a, const entry_reloc& b)
	{
		return (a.va == b.va);
	}

	/** return a pointer to specific RVA page element ... */
	static PIMAGE_BASE_RELOCATION moveToPage(PIMAGE_BASE_RELOCATION pBaseRelocation, DWORD PageRVA)
	{

		while (pBaseRelocation->SizeOfBlock > 0) {
			if (pBaseRelocation->VirtualAddress == PageRVA)
				return pBaseRelocation;

			char* p = (char*)pBaseRelocation;
			pBaseRelocation = (PIMAGE_BASE_RELOCATION)(p + pBaseRelocation->SizeOfBlock);
		}

		return pBaseRelocation;
	}

	static void ExpandRelocPage(PIMAGE_BASE_RELOCATION pBaseRelocation, va_t PageRVA, ENTRY_RELOC& outList)
	{
		char* relocaddr = (char*)pBaseRelocation;

		while (pBaseRelocation->SizeOfBlock > 0) {
			DWORD nEntries = (pBaseRelocation->SizeOfBlock - 8) / sizeof(WORD);	//SizeOfBlock contain 8 byte of IMAGE_BASE_RELOCATION

			if (pBaseRelocation->VirtualAddress == PageRVA) {
				WORD* pEntries = reinterpret_cast<WORD*>(relocaddr + 8);

				while (nEntries > 0) {
					short type = (*pEntries & 0xf000) >> 12;
					va_t address = (va_t)(*pEntries & 0x0fff) + pBaseRelocation->VirtualAddress;

					outList.push_back({ type, address });
					nEntries--;
				}
			}

			relocaddr += pBaseRelocation->SizeOfBlock;	// move relocaddr to next block...
			pBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);	// and update pointer
		}
	}

	static char *CompressRelocPage(size_t &space_required, const ENTRY_RELOC &outList)
	{
		size_t memory_required = (outList.size() * 2) + sizeof(IMAGE_BASE_RELOCATION);

		memory_required = roundup(memory_required, 8);

		char* buffer = (char *) malloc(memory_required);
		memset(buffer, 0, memory_required);

		PIMAGE_BASE_RELOCATION pWrite = (PIMAGE_BASE_RELOCATION)buffer;
		WORD* pRelocWriter = (WORD*)(buffer + sizeof(IMAGE_BASE_RELOCATION));

		DWORD PageRVA = 0;
		pWrite->SizeOfBlock = memory_required;
		
		for (entry_reloc e : outList) {
			PageRVA = e.va & (~0xfff);
			*pRelocWriter++ = (e.type << 12) | (e.va & 0xfff);
		}

		space_required = memory_required;
		return buffer;
	}

	/** iterate in each element... */
	void pereloc::setNewBaseAddress(va_t newBaseAddress)
	{
		va_t oldBaseAddress = pe->getImageBase();

		va_t relocAddress = 0;
		size_t relocSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, relocSize) == false)
			return;	// no reloc ? done!

		char* relocaddr = (char*)pe->rawptr(relocAddress);

		if (relocaddr == nullptr)
			return;	// corrupted file?

		PIMAGE_BASE_RELOCATION pBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);

		while (pBaseRelocation->SizeOfBlock > 0) {
			DWORD nEntries = (pBaseRelocation->SizeOfBlock - 8) / sizeof(WORD);	//SizeOfBlock contain 8 byte of IMAGE_BASE_RELOCATION

			WORD* pEntries = reinterpret_cast<WORD*>(relocaddr + 8);

			while (nEntries > 0) {
				short type = (*pEntries & 0xf000) >> 12;
				va_t address = (va_t)(*pEntries & 0x0fff) + pBaseRelocation->VirtualAddress;

				update_baseaddress(type, address, oldBaseAddress, newBaseAddress);
				nEntries--;
			}

			relocaddr += pBaseRelocation->SizeOfBlock;	// move relocaddr to next block...
			pBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);	// and update pointer
		}
		return;
	}

	/** used internally.. return the free space in this block */
	static size_t space_in_section(PIMAGE_BASE_RELOCATION pImageBaseRelocation)
	{
		const char* ptr = (const char*)pImageBaseRelocation;

		size_t used_space = pImageBaseRelocation->SizeOfBlock - 8;

		WORD* scan = reinterpret_cast<WORD*>(pImageBaseRelocation + 8);

		while (*scan++ != 0)
			used_space -= 2;

		return pImageBaseRelocation->SizeOfBlock - used_space;
	}

	bool pereloc::addRelocationEntry(short type, va_t va)
	{
		va_t relocAddress = 0;
		size_t relocSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, relocSize) == false)
			return false;	// no reloc ? done!
		
		pesection* relocsection = pe->sectionByAddress(relocAddress);

		if (relocsection->VirtualEndAddress() < (relocAddress + relocSize + 10))	// worst case..
			return false;

		char* relocaddr = (char*)pe->rawptr(relocAddress);
		char* relocendaddr = (char*)pe->rawptr(relocAddress + relocSize);

		PIMAGE_BASE_RELOCATION pBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);
		DWORD PageRVA = (DWORD)va;
		PageRVA = PageRVA & ~0xfff;	// from this page..

		ENTRY_RELOC entries;	// a list of all entries..
		ExpandRelocPage(pBaseRelocation, (va_t)PageRVA, entries);
		
		entries.push_back({ type, va });	// put the element into list..
		entries.sort();	// sort list..

		size_t newRelocSize = 0;
		const char* newPage = CompressRelocPage(newRelocSize, entries);
		
		PIMAGE_BASE_RELOCATION pPageToModify = moveToPage(pBaseRelocation, PageRVA);
		
		if (pPageToModify->SizeOfBlock == newRelocSize) {	// best option.. we can replace buffer!
			memcpy(pPageToModify, newPage, newRelocSize);
			newRelocSize = relocSize;	// output size is equal to...
		}
		else {	// we need to move buffer...
			char* copyFrom = (char*)pPageToModify + pPageToModify->SizeOfBlock;
			char* copyTo = (char*)pPageToModify + newRelocSize;

			size_t bytesToMove = relocendaddr - copyFrom;

			relocSize = pPageToModify->SizeOfBlock + newRelocSize;
			memmove(copyTo, copyFrom, relocendaddr - copyFrom);
			memcpy(copyFrom, newPage, newRelocSize);	// replace current section with new data..
			newRelocSize = relocSize;
		}

		free((void *) newPage);

		pe->setDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, newRelocSize);
		return true;
	}

	bool pereloc::removeRelocationEntry(va_t va)
	{
		va_t relocAddress = 0;
		size_t relocSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, relocSize) == false)
			return false;	// no reloc ? done!

		pesection* relocsection = pe->sectionByAddress(relocAddress);

		char* relocaddr = (char*)pe->rawptr(relocAddress);
		char* relocendaddr = (char*)pe->rawptr(relocAddress + relocSize);

		PIMAGE_BASE_RELOCATION pBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);
		DWORD PageRVA = (DWORD)va;
		PageRVA = PageRVA & ~0xfff;	// from this page..

		ENTRY_RELOC entries;	// a list of all entries..
		ExpandRelocPage(pBaseRelocation, (va_t)PageRVA, entries);

		is_equal_entry to_remove;
		to_remove.va = va;

		entries.remove_if(to_remove);

		entries.sort();	// sort list..

		size_t newRelocSize = 0;
		const char* newPage = CompressRelocPage(newRelocSize, entries);

		PIMAGE_BASE_RELOCATION pPageToModify = moveToPage(pBaseRelocation, PageRVA);

		if (pPageToModify->SizeOfBlock == newRelocSize) {	// best option.. we can replace buffer!
			memcpy(pPageToModify, newPage, newRelocSize);
			newRelocSize = relocSize;	// output size is equal to...
		}
		else {	// we need to move buffer...
			char* copyFrom = (char*)pPageToModify + pPageToModify->SizeOfBlock;
			char* copyTo = (char*)pPageToModify + newRelocSize;

			size_t bytesToMove = relocendaddr - copyFrom;

			relocSize = pPageToModify->SizeOfBlock + newRelocSize;
			memmove(copyTo, copyFrom, relocendaddr - copyFrom);
			memcpy(copyFrom, newPage, newRelocSize);	// replace current section with new data..
			newRelocSize = relocSize;
		}

		free((void*)newPage);

		pe->setDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, newRelocSize);
		return true;

	}


};	// end of pelib namespace