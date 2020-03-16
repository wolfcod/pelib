#pragma once
#include <pelib/va.hpp>

namespace pelib
{
	class peloader;

	class peresource
	{
		public:
			peresource(peloader * pe);
			~peresource();

			void setNewBaseAddress(va_t newBaseAddress);

		protected:
			void update_rsrc(PIMAGE_RESOURCE_DIRECTORY root, va_t newBaseAddress, va_t imageBaseAddress);

		private:
			peloader* pe;

	};
}	// end of pelib namespace
