#include <windows.h>
#include <pelib/peloader.hpp>
#include <pelib/utils.hpp>
#include <pelib/pesection.hpp>
#include <pelib/peresource.hpp>

namespace pelib
{
	peresource::peresource(peloader* arg)
		: pe(arg)
	{

	}

	peresource::~peresource()
	{

	}

	/** recursive function to update base address */
	void peresource::update_rsrc(PIMAGE_RESOURCE_DIRECTORY root, va_t newBaseAddress, va_t imageBaseAddress)
	{
		char* p = (char*)root;

		PIMAGE_RESOURCE_DIRECTORY_ENTRY pDirEntry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)p + sizeof(IMAGE_RESOURCE_DIRECTORY);

		for (int i = 0; i < root->NumberOfIdEntries; i++) {
			if (pDirEntry[i].DataIsDirectory) {	// directory flag enabled.. scan as directory
				PIMAGE_RESOURCE_DIRECTORY pDirectory = (PIMAGE_RESOURCE_DIRECTORY)(p + (pDirEntry[i].OffsetToData & 0x7fffffff));

				update_rsrc(pDirectory, newBaseAddress, imageBaseAddress);
			}
			else {	// element.. update
				PIMAGE_RESOURCE_DATA_ENTRY pEntry = (PIMAGE_RESOURCE_DATA_ENTRY)(p + (pDirEntry[i].OffsetToData));

				pEntry->OffsetToData -= imageBaseAddress;	// remove previous image base address
				pEntry->OffsetToData += newBaseAddress;		// add new base address
			}
		}
			
	}

	void peresource::setNewBaseAddress(va_t newBaseAddress)
	{
		va_t address = 0;
		size_t size;

		if (pe->getDataDirectory(DirectoryEntry::EntryResource, address, size) == 0) // no rsrc section
			return;

		PIMAGE_RESOURCE_DIRECTORY pRsrcDirectory = (PIMAGE_RESOURCE_DIRECTORY) pe->rawptr(address);

		update_rsrc(pRsrcDirectory, newBaseAddress, pe->getImageBase());
	}
}