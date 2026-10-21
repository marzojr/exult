#include "ucfunc.h"

#include <cassert>
#include <iomanip>
#include <sstream>

static std::string declaration(const FuncMap& functions, unsigned int id, unsigned int args) {
	UCFunc function;
	std::ostringstream output;
	output << std::hex << std::setfill('0');
	function.output_ucs_funcname(output, functions, id, args, nullptr);
	return output.str();
}

int main() {
	FuncMap functions;
	assert(declaration(functions, 0x900, 2) == "void Func0900 0x900 (var var0000, var var0001)");
	assert(declaration(functions, 0x123, 0) == "void Func0123 shape#(0x123) ()");
	assert(declaration(functions, 0x456, 0) == "void Func0456 object#(0x456) ()");
	std::map<unsigned int, std::string> variables{{0, "right"}, {1, "left"}};
	functions.emplace(0x900, UCFuncSet(0x900, 2, true, false, false, "&known", Usecode_symbol::fun_defined, variables));
	assert(declaration(functions, 0x900, 2) == "var known 0x900 (var left, var right)");
	functions.emplace(0x901, UCFuncSet(0x901, 2, false, false, true, "method", Usecode_symbol::fun_defined, variables));
	assert(declaration(functions, 0x901, 2) == "void method 0x901 (var right)");
}
