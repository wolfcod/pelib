#pragma once
#include <delayimp.h>
#include <pelib/va.hpp>

namespace pelib
{
	class peloader;

	class peimport
	{
	public:
		peimport(peloader* pe);
		~peimport();

		void setNewBaseAddress(va_t newBaseAddress);
		void moveSections(va_t fromVirtualAddress, va_t toVirtualAddress);

		va_t getImportByName(const char* szLibName, const char* szFuncName);
		va_t getImportByOrdinal(const char* szLibName, DWORD dwOrdinalName);
	protected:


	private:
		bool getImportDescriptor(PIMAGE_IMPORT_DESCRIPTOR* ppImageImportDescriptor);

		void fixArrayOfPointers(DWORD* ptr, size_t nElements, va_t fromVirtualAddress, va_t deltaRVA);
		peloader* pe;

	};

	class pedelayimport
	{
	public:
		pedelayimport(peloader* pe);
		~pedelayimport();

		void setNewBaseAddress(va_t newBaseAddress);
		void moveSections(va_t fromVirtualAddress, va_t toVirtualAddress);

	private:
		bool getImportDescriptor(PImgDelayDescr* ppImageImportDescriptor);

		peloader* pe;

	};
}	// end of pelib namespace
#pragma once
