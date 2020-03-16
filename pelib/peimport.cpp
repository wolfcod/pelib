#include <windows.h>
#include <delayimp.h>
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

	bool peimport::getImportDescriptor(PIMAGE_IMPORT_DESCRIPTOR *ppImageImportDescriptor)
	{
		va_t importAddr = 0;
		size_t importSize = 0;

		*ppImageImportDescriptor = nullptr;

		if (pe->getDataDirectory(DirectoryEntry::EntryImport, importAddr, importSize) == false)
			return false;

		*ppImageImportDescriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(pe->rawptr(importAddr));

		return true;
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

		PIMAGE_IMPORT_DESCRIPTOR pImportDir = nullptr;
		
		if (getImportDescriptor(&pImportDir) == false)
			return;

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

	/** return the va_t address of symbol for a specific lib/function */
	va_t peimport::getImportByName(const char* szLibName, const char* szFuncName)
	{
		PIMAGE_IMPORT_DESCRIPTOR pImportDir = nullptr;
		if (getImportDescriptor(&pImportDir) == false)
			return (va_t) 0;

		while (pImportDir->Characteristics != 0) {
			const char* szImportLibName = (const char*)pe->rawptr(pImportDir->Name);

			if (strcmp(szImportLibName, szLibName) == 0) {
				// must process inside this address, but an import may exists different times..
				ULONG* pFirstThunk = (ULONG*)pe->rawptr(pImportDir->FirstThunk);
				ULONG* pRvaName = (ULONG*)pe->rawptr(pImportDir->Characteristics);
				
				// inside pRvaName there is a list of objects..
				while (*pRvaName != 0)
				{
					if ((*pRvaName & 0x80000000) == 0) { // import by name.. true

						const char* szImportFuncName = (const char*)pe->rawptr(*pRvaName);

						if (strcmp(szImportFuncName, szFuncName) == 0)
							return (va_t) (*pFirstThunk);
					}

					pRvaName++;
					pFirstThunk++;
				}

				
			}
			
			pImportDir++;
		}

		return 0;
	}

	/** return the va_t address of symbol for a specific lib/function */
	va_t peimport::getImportByOrdinal(const char* szLibName, DWORD dwOrdinalName)
	{
		PIMAGE_IMPORT_DESCRIPTOR pImportDir = nullptr;
		if (getImportDescriptor(&pImportDir) == false)
			return (va_t) 0;

		while (pImportDir->Characteristics != 0) {
			const char* szImportLibName = (const char*)pe->rawptr(pImportDir->Name);

			if (strcmp(szImportLibName, szLibName) == 0) {
				// must process inside this address, but an import may exists different times..
				ULONG* pFirstThunk = (ULONG*)pe->rawptr(pImportDir->FirstThunk);
				ULONG* pRvaName = (ULONG*)pe->rawptr(pImportDir->Characteristics);

				// inside pRvaName there is a list of objects..
				while (*pRvaName != 0)
				{
					if ((*pRvaName & 0x80000000) != 0) { // import by name.. true
						if ((dwOrdinalName & *pRvaName) == dwOrdinalName)
							return (va_t)(*pFirstThunk);
					}

					pRvaName++;
					pFirstThunk++;
				}


			}

			pImportDir++;
		}

		return 0;
	}

	pedelayimport::pedelayimport(peloader* arg)
		: pe(arg)
	{

	}

	pedelayimport::~pedelayimport()
	{

	}

	void pedelayimport::setNewBaseAddress(va_t newBaseAddress)
	{
		va_t address = 0;
		size_t size;

		if (pe->getDataDirectory(DirectoryEntry::EntryDelayImport, address, size) == 0) // no export directory...
			return;
	}

	bool pedelayimport::getImportDescriptor(PImgDelayDescr* ppImageImportDescriptor)
	{
		va_t importAddr = 0;
		size_t importSize = 0;

		*ppImageImportDescriptor = nullptr;

		if (pe->getDataDirectory(DirectoryEntry::EntryDelayImport, importAddr, importSize) == false)
			return false;

		*ppImageImportDescriptor = reinterpret_cast<PImgDelayDescr>(pe->rawptr(importAddr));

		return true;
	}

	void pedelayimport::moveSections(va_t fromVirtualAddress, va_t toVirtualAddress)
	{
		va_t importAddr = 0;
		size_t importSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryDelayImport, importAddr, importSize) == false)
			return;	// no reloc ? done!

		PImgDelayDescr pImportDir = nullptr;

		if (getImportDescriptor(&pImportDir) == false)
			return;

		va_t deltaRVA = toVirtualAddress - fromVirtualAddress;

		while (pImportDir->grAttrs != 0) {
			
			pImportDir->rvaBoundIAT = adjustIfAbove(pImportDir->rvaBoundIAT, fromVirtualAddress, deltaRVA);
			pImportDir->rvaDLLName = adjustIfAbove(pImportDir->rvaDLLName, fromVirtualAddress, deltaRVA);
			pImportDir->rvaHmod = adjustIfAbove(pImportDir->rvaHmod, fromVirtualAddress, deltaRVA);
			pImportDir->rvaINT = adjustIfAbove(pImportDir->rvaINT, fromVirtualAddress, deltaRVA);
			pImportDir->rvaUnloadIAT = adjustIfAbove(pImportDir->rvaUnloadIAT, fromVirtualAddress, deltaRVA);

			pImportDir++;
		}

		return;
	}

}