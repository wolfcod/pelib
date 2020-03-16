#include <windows.h>
#include <pelib/pelib.hpp>
#include <pelib/utils.hpp>
#include <pelib/pesection.hpp>
#include <pelib/peloadconfig.hpp>

namespace pelib
{
	peloadconfig::peloadconfig(peloader* arg)
		: pe(arg)
	{

	}

	peloadconfig::~peloadconfig()
	{

	}

	void peloadconfig::setNewBaseAddress(va_t newBaseAddress)
	{
		va_t address = 0;
		size_t size;

		if (pe->getDataDirectory(DirectoryEntry::EntryLoadConfig, address, size) == 0) // no export directory...
			return;
	}

	template<typename T, typename S>
	void SEH_fix(void* data, va_t fromVirtualAddress, va_t toVirtualAddress)
	{
		T pImageLoadConfig = reinterpret_cast<T>(data);

		if (pImageLoadConfig->SEHandlerTable != 0) {
			// update SEHandlerTable

			pImageLoadConfig->SEHandlerTable = adjustIfAbove(pImageLoadConfig->SEHandlerTable, fromVirtualAddress, toVirtualAddress - fromVirtualAddress);

			S* SEHandlerTable = (S*)pe->rawptr(pImageLoadConfig->SEHandlerTable);

			UINT SEHandlerCount = pImageLoadConfig->SEHandlerCount;

			while (SEHandlerCount > 0) {
				*SEHandlerTable = adjustIfAbove(*SEHandlerTable, fromVirtualAddress, toVirtualAddress - fromVirtualAddress);

				SEHandlerTable++;
				SEHandlerCount--;
			}
		}
	}

	void peloadconfig::moveSections(va_t fromVirtualAddress, va_t toVirtualAddress)
	{
		va_t loadAddr = 0;
		size_t loadSize = 0;

		if (pe->getDataDirectory(DirectoryEntry::EntryLoadConfig, loadAddr, loadSize) == false)
			return;	// no reloc ? done!

		if (pe->is64Bit()) {
			SEH_fix<PIMAGE_LOAD_CONFIG_DIRECTORY64, ULONGLONG>(pe->rawptr(loadAddr), fromVirtualAddress, toVirtualAddress);
		}
		else {
			SEH_fix<PIMAGE_LOAD_CONFIG_DIRECTORY32, ULONG>(pe->rawptr(loadAddr), fromVirtualAddress, toVirtualAddress);
		}
		return;
	}

}