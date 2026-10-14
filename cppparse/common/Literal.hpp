// Literal.hpp - the values of character and string literals as cl gives them, shared by the preprocessor, sema,
// IR generation and the back end. Strict C++98.
#ifndef CPPPARSE_LITERAL_HPP
#define CPPPARSE_LITERAL_HPP

#include <string>
#include <vector>

namespace cppparse {

// the code units of a string literal's array without the terminator. text holds one literal or several adjacent
// ones as written ("\x41" "b", L"a" L"b"); each piece's escapes are decoded on their own before the pieces are
// joined (C++98 2.1, phase 5 before phase 6), so a hex escape never takes the next piece's digits. Returns true
// for a wide literal (any piece with the L prefix): units of 16 bits, else of 8
bool stringLiteralUnits(const std::string& text, std::vector<unsigned long>& units);
long stringLiteralLength(const std::string& text);

// the value of a character literal and the number of characters it holds (more than one: a multicharacter
// constant of type int; L'ab' is L'a' as with cl). A single narrow character is a signed char
long charLiteralValue(const std::string& text, int& count);

} // namespace cppparse

#endif
