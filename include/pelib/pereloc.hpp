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



		protected:
			void update_baseaddress(short type, va_t address, va_t oldBaseAddress, va_t newBaseAddress);
		

		private:
			peloader* pe;

	};
}