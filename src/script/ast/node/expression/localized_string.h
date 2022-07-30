#pragma once
#include "identifier.h"
#include <string>

namespace compiler
{
	namespace ast
	{
		struct LocalizedString : Expression
		{
			std::string reference;
			virtual void print(Printer& out) override
			{
				out.print("localized string %s", reference.c_str());
			}
		};
	}; // namespace ast
};	   // namespace compiler