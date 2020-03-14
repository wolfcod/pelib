#include <windows.h>
#include <pelib/pelib.hpp>
#include <pelib/utils.hpp>
#include <pelib/pesection.hpp>
#include <pelib/peimport.hpp>

namespace pelib
{
	peimport::peimport(peloader* arg)
		: pe(arg)
	{

	}

	peimport::~peimport()
	{

	}

	void peimport::setNewBaseAddress(va_t newBaseAddress)
	{
		va_t address = 0;
		size_t size;

		if (pe->getDataDirectory(DirectoryEntry::EntryImport, address, size) == 0) // no export directory...
			return;
	}

	void peimport::fixArrayOfPointers(DWORD* ptr, size_t nElements, va_t fromVirtualAddress, va_t deltaRVA)
	{
		for (size_t i = 0; i < nElements; i++) {
			ptr[i] = adjustIfAbove(ptr[i], fromVirtualAddress, deltaRVA);
		}
	}

	void peimport::moveSections(va_t fromVirtualAddress, va_t toVirtualAddress)
	{
		va_t importAddr = 0;
		size_t importSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryImport, importAddr, importSize) == false)
			return;	// no reloc ? done!

		char* importaddr = (char*)pe->rawptr(importAddr);
		char* importendaddr = (char*)pe->rawptr(importAddr + importSize);

		PIMAGE_IMPORT_DESCRIPTOR pImportDir = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(importAddr);

		va_t deltaRVA = toVirtualAddress - fromVirtualAddress;

		while (pImportDir->Characteristics != 0) {
			
			ULONG *pRvaName = (ULONG *) pe->rawptr(pImportDir->Characteristics);
			ULONG *pFirstThunk = (ULONG *) pe->rawptr(pImportDir->FirstThunk);

			// inside pRvaName there is a list of objects..
			while (*pRvaName != 0)
			{
				if ((*pRvaName & 0x80000000) == 0) { // import by name.. true
					*pRvaName = adjustIfAbove(*pRvaName, fromVirtualAddress, deltaRVA);
				}
				*pFirstThunk = adjustIfAbove(*pFirstThunk, fromVirtualAddress, deltaRVA);

				pRvaName++;
				pFirstThunk++;
			}

			// these changes must be done at end of process.. sections is with original value on this step.
			pImportDir->Characteristics = adjustIfAbove(pImportDir->Characteristics, fromVirtualAddress, deltaRVA);
			pImportDir->FirstThunk = adjustIfAbove(pImportDir->FirstThunk, fromVirtualAddress, deltaRVA);
			pImportDir->Name = adjustIfAbove(pImportDir->Name, fromVirtualAddress, deltaRVA);

			pImportDir++;
		}

		return;
	}
}