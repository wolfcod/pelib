#include <windows.h>
#include <pelib/pelib.hpp>
#include <pelib/utils.hpp>
#include <pelib/pesection.hpp>
#include <pelib/peexport.hpp>

namespace pelib
{
	peexport::peexport(peloader* arg)
		: pe(arg)
	{

	}

	peexport::~peexport()
	{

	}

	void peexport::setNewBaseAddress(va_t newBaseAddress)
	{
		va_t address = 0;
		size_t size;

		if (pe->getDataDirectory(DirectoryEntry::EntryExport, address, size) == 0) // no export directory...
			return;
	}

	void peexport::fixArrayOfPointers(DWORD* ptr, size_t nElements, va_t fromVirtualAddress, va_t deltaRVA)
	{
		for (size_t i = 0; i < nElements; i++) {
			ptr[i] = adjustIfAbove(ptr[i], fromVirtualAddress, deltaRVA);
		}
	}

	void peexport::moveSections(va_t fromVirtualAddress, va_t toVirtualAddress)
	{
		va_t exportAddress = 0;
		size_t exportSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryExport, exportAddress, exportSize) == false)
			return;	// no reloc ? done!

		char* exportaddr = (char*)pe->rawptr(exportAddress);
		char* exportendaddr = (char*)pe->rawptr(exportAddress + exportSize);

		PIMAGE_EXPORT_DIRECTORY pExportDir = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(exportaddr);

		va_t deltaRVA = toVirtualAddress - fromVirtualAddress;
		
		// on first step.. all elements must be changed..
		fixArrayOfPointers((DWORD*)pe->rawptr(pExportDir->AddressOfNames), (size_t)pExportDir->NumberOfNames, fromVirtualAddress, deltaRVA);
		fixArrayOfPointers((DWORD*)pe->rawptr(pExportDir->AddressOfFunctions), (size_t)pExportDir->NumberOfFunctions, fromVirtualAddress, deltaRVA);

		// after, we chan fix address...
		pExportDir->AddressOfFunctions = adjustIfAbove(pExportDir->AddressOfFunctions, fromVirtualAddress, deltaRVA);
		pExportDir->AddressOfNameOrdinals = adjustIfAbove(pExportDir->AddressOfNameOrdinals, fromVirtualAddress, deltaRVA);
		pExportDir->AddressOfNames = adjustIfAbove(pExportDir->AddressOfNames, fromVirtualAddress, deltaRVA);
		pExportDir->Name = adjustIfAbove(pExportDir->Name, fromVirtualAddress, deltaRVA);

		return;
	}
}