#pragma once
#include <pelib/va.hpp>

namespace pelib
{
	class peloader;

	class peloadconfig
	{
	public:
		peloadconfig(peloader* pe);
		~peloadconfig();

		void setNewBaseAddress(va_t newBaseAddress);
		void moveSections(va_t fromVirtualAddress, va_t toVirtualAddress);

	protected:


	private:
		peloader* pe;

	};
}	// end of pelib namespace
#pragma once

