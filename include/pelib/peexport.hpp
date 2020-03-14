#pragma once
#include <pelib/va.hpp>

namespace pelib
{
	class peloader;

	class peexport
	{
	public:
		peexport(peloader* pe);
		~peexport();

		void setNewBaseAddress(va_t newBaseAddress);
		void moveSections(va_t fromVirtualAddress, va_t toVirtualAddress);

	protected:
		

	private:
		void fixArrayOfPointers(DWORD* ptr, size_t nElements, va_t fromVirtualAddress, va_t deltaRVA);
		peloader* pe;

	};
}	// end of pelib namespace
