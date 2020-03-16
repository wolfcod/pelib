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
			DWORD* pData = pe->ptr<DWORD*>(address);
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

	/** return a pointer to first IMAGE_BASE RELOCATION */
	PIMAGE_BASE_RELOCATION pereloc::openRelocPage()
	{
		va_t relocAddress = 0;
		size_t relocSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, relocSize) == false)
			return nullptr;	// no reloc ? done!

		char* relocaddr = (char*)pe->rawptr(relocAddress);

		if (relocaddr == nullptr)
			return nullptr;	// corrupted file?

		return reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);

	}

	/** return a pointer to the next IMAGE_BASE_RELOCATION */
	PIMAGE_BASE_RELOCATION pereloc::nextRelocPage(PIMAGE_BASE_RELOCATION pBaseRelocation) {
		char* p = (char*)pBaseRelocation;
		return reinterpret_cast<PIMAGE_BASE_RELOCATION>(p + pBaseRelocation->SizeOfBlock);
	}

	WORD* pointerToEntries(PIMAGE_BASE_RELOCATION pImageBaseRelocation)
	{
		char* p = (char*)pImageBaseRelocation;

		return (WORD*)(p + sizeof(IMAGE_BASE_RELOCATION));
	}

	static void ExpandRelocPage(PIMAGE_BASE_RELOCATION pBaseRelocation, va_t PageRVA, ENTRY_RELOC& outList)
	{
		char* relocaddr = (char*)pBaseRelocation;

		while (pBaseRelocation->SizeOfBlock > 0) {
			DWORD nEntries = (pBaseRelocation->SizeOfBlock - 8) / sizeof(WORD);	//SizeOfBlock contain 8 byte of IMAGE_BASE_RELOCATION

			if (pBaseRelocation->VirtualAddress == PageRVA) {
				WORD* pEntries = pointerToEntries(pBaseRelocation);

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
		WORD* pRelocWriter = pointerToEntries(pWrite);

		DWORD PageRVA = 0;
		pWrite->SizeOfBlock = memory_required;
		
		for (entry_reloc e : outList) {
			PageRVA = e.va & (~0xfff);
			*pRelocWriter++ = (e.type << 12) | (e.va & 0xfff);
		}

		pWrite->VirtualAddress = PageRVA;
		space_required = memory_required;
		return buffer;
	}

	/** iterate in each element... */
	void pereloc::setNewBaseAddress(va_t newBaseAddress)
	{
		va_t oldBaseAddress = pe->getImageBase();

		PIMAGE_BASE_RELOCATION pBaseRelocation = openRelocPage();

		if (pBaseRelocation == nullptr)
			return;

		while (pBaseRelocation->SizeOfBlock > 0) {
			DWORD nEntries = (pBaseRelocation->SizeOfBlock - 8) / sizeof(WORD);	//SizeOfBlock contain 8 byte of IMAGE_BASE_RELOCATION

			WORD* pEntries = pointerToEntries(pBaseRelocation);

			while (nEntries != 0) {
				short type = (*pEntries & 0xf000) >> 12;
				va_t address = (va_t)(*pEntries & 0x0fff) + pBaseRelocation->VirtualAddress;

				update_baseaddress(type, address, oldBaseAddress, newBaseAddress);
				nEntries--;
				pEntries++;
			}

			pBaseRelocation = nextRelocPage(pBaseRelocation);
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

	bool pereloc::removeRelocationEntry(va_t begin, va_t end)
	{
		va_t relocAddress = 0;
		size_t relocSize = 0;

		begin = lowRVA(begin);
		end = highRVA(end);
		if (pe->getDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, relocSize) == false)
			return false;	// no reloc ? done!

		pesection* relocsection = pe->sectionByAddress(relocAddress);

		char* relocaddr = (char*)pe->rawptr(relocAddress);
		char* relocendaddr = (char*)pe->rawptr(relocAddress + relocSize);

		char* newReloc = (char*)malloc(relocSize);
		size_t newRelocSize = 0;

		PIMAGE_BASE_RELOCATION pBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);
		
		while (pBaseRelocation->SizeOfBlock != 0)
		{
			if (va_in_range(begin, end, pBaseRelocation->VirtualAddress) == false) {
				memcpy(newReloc + newRelocSize, pBaseRelocation, pBaseRelocation->SizeOfBlock);
				newRelocSize += pBaseRelocation->SizeOfBlock;
			}

			pBaseRelocation = nextRelocPage(pBaseRelocation);
		}

		if (newRelocSize != relocSize) {
			memset(relocaddr, 0, relocSize);	// remove all previous elements..
			memcpy(relocaddr, newReloc, newRelocSize);
			pe->setDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, newRelocSize);
		}

		free(newReloc);
		return true;

	}

	void getEntry(WORD entry, PIMAGE_BASE_RELOCATION pBaseRelocation, short& type, va_t& va)
	{
		type = (entry & 0xf000) >> 12;
		va = (entry & 0x0fff) + pBaseRelocation->VirtualAddress;
	}

	/** retrieve any value on this page and update eventually values involved.. */
	void pereloc::processSectionEntry(va_t fromRVA, va_t toRVA, PIMAGE_BASE_RELOCATION pBaseRelocation)
	{
		WORD* pEntries = pointerToEntries(pBaseRelocation);

		while (*pEntries != 0x00) {
			short type = 0;
			va_t addr = 0;

			getEntry(*pEntries, pBaseRelocation, type, addr);

			if (type == IMAGE_REL_BASED_HIGHLOW) {
				DWORD* pData = pe->ptr<DWORD*>(addr);
				if (*pData < (fromRVA + pe->getImageBase())) {
					// not interested...
				}
				else {
					*pData += (toRVA - fromRVA);
				}
			}
			else if (type == IMAGE_REL_BASED_DIR64) {
				uint64_t* pData = (uint64_t*)pe->rawptr(addr);
				if (*pData < (fromRVA + pe->getImageBase())) {
					// not interested...
				}
				else {
					*pData += (toRVA - fromRVA);
				}
			}


			pEntries++;
		}
	}

	/** */
	bool pereloc::moveSection(va_t fromRVA, va_t toRVA)
	{
		va_t relocAddress = 0;
		size_t relocSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, relocSize) == false)
			return false;	// no reloc ? done!

		char* relocaddr = (char*)pe->rawptr(relocAddress);
		char* relocendaddr = (char*)pe->rawptr(relocAddress + relocSize);

		// on first step.. all address will be changed
		PIMAGE_BASE_RELOCATION pBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(relocaddr);

		va_t deltaRVA = toRVA - fromRVA;
		while (pBaseRelocation->SizeOfBlock != 0)
		{
			processSectionEntry(fromRVA, toRVA, pBaseRelocation);
			if (pBaseRelocation->VirtualAddress >= fromRVA)
			{
				pBaseRelocation += deltaRVA;
			}
			pBaseRelocation = nextRelocPage(pBaseRelocation);
		}

		return true;
	}

	PIMAGE_BASE_RELOCATION pereloc::tailRelocPage()
	{
		va_t relocAddress = 0;
		size_t relocSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryBaseReloc, relocAddress, relocSize) == false)
			return nullptr;	// no reloc ? done!

		char* relocaddr = (char*)pe->rawptr(relocAddress);
		char* relocendaddr = (char*)pe->rawptr(relocAddress + relocSize);

		return (PIMAGE_BASE_RELOCATION)relocendaddr;
	}

	void pereloc::relocs(std::list<va_t>& relocs, va_t begin, va_t end)
	{
		PIMAGE_BASE_RELOCATION pReloc = openRelocPage();

		if (begin == 0)
			begin = pe->minVa();

		if (end == 0)
			end = pe->maxVa();

		while (pReloc != nullptr && pReloc->SizeOfBlock != 0)
		{
			if (pReloc->VirtualAddress >= begin && pReloc->VirtualAddress <= end) {	// interested to explore this range...
				WORD* pEntries = pointerToEntries(pReloc);

				while (*pEntries != 0) {
					short type = 0;
					va_t addr = 0;

					getEntry(*pEntries, pReloc, type, addr);

					relocs.push_back(addr);

					pEntries++;
				}
			}

			pReloc = nextRelocPage(pReloc);
		}
	}

	reloc_it pereloc::begin()
	{
		return reloc_it(this);
	}

	reloc_it pereloc::end()
	{
		return reloc_it(this);
	}

	reloc_it::reloc_it(pereloc* r)
		: _reloc(r), curr(nullptr)
	{
		fromAddress = 0;
		toAddress = 0;
		
		//toAddress = r->tailRelocPage();

		fetchVa();
	}

	/** used to declare the end.. on this version! */
	reloc_it::reloc_it(pereloc* r, PIMAGE_BASE_RELOCATION from)
		: _reloc(r), curr(from)
	{
		
	}

	reloc_it::reloc_it(pereloc* r, va_t fromAddress)
		: _reloc(r), curr(nullptr)
	{
		this->fromAddress = fromAddress;
		fetchVa();
	}
	
	reloc_it::reloc_it(pereloc* r, va_t fromAddress, va_t toAddress)
		:	_reloc(r), curr(nullptr)
	{
		this->fromAddress = fromAddress;
		this->toAddress = toAddress;

		fetchVa();
	}
	
	void reloc_it::fetchVa()
	{
		if (curr == nullptr) {	// on first fetch.. set curr to base address..
			curr = _reloc->openRelocPage();
		}

		if (fromAddress != 0) {
			while (curr->VirtualAddress < fromAddress)
				curr = _reloc->nextRelocPage(curr);
		}

		if (toAddress != 0) {
			if (curr->VirtualAddress > toAddress) {
				va = 0;
				curr = _reloc->tailRelocPage();
				return;
			}
		}

		WORD nElements = (curr->SizeOfBlock - 8) / sizeof(WORD);
		WORD* values = pointerToEntries(curr);

		WORD n = values[pos++];
		if (n != 0) {
			va = curr->VirtualAddress + (n & 0xfff);
		}
		else {
			pos = 0;
			curr = _reloc->nextRelocPage(curr);
			va = curr->VirtualAddress + (n & 0xfff);
		}
	

	}

	va_t& reloc_it::operator*()
	{
		return va;
	}
	
	reloc_it& reloc_it::operator++()
	{
		fetchVa();

		return *this;
	}
	
	bool reloc_it::operator != (const reloc_it& i) const
	{
		if (curr != i.curr)
			return true;

		return false;
	}


};	// end of pelib namespace