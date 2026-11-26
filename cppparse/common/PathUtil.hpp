// PathUtil.hpp - string, path and file helpers shared by the tool.
// Strict C++98.
#ifndef CPPPARSE_PATHUTIL_HPP
#define CPPPARSE_PATHUTIL_HPP

#include "Int64.hpp"

#include <string>
#include <vector>

namespace cppparse {

// strings
std::string trim(const std::string& s);
std::string toLower(const std::string& s);
bool        startsWith(const std::string& s, const std::string& prefix);
bool        endsWith(const std::string& s, const std::string& suffix);
bool        iequals(const std::string& a, const std::string& b);
void        split(const std::string& s, char sep, std::vector<std::string>& out, bool keepEmpty = false);
std::string intToString(int v);
std::string int64ToString(int64 v);
std::string replaceAll(const std::string& s, const std::string& from, const std::string& to);
std::string padRight(const std::string& s, size_t width);
bool        contains(const std::vector<std::string>& v, const std::string& s);
void        addUnique(std::vector<std::string>& v, const std::string& s);
void        addUniqueInt(std::vector<int>& v, int x);
std::string stripTemplateArgs(const std::string& s);   // "A<T>::B<U>" -> "A::B"
bool        isMacroLike(const std::string& word);      // "SAMPLE_API": all caps, more than one char
// where a library function name used in analysed code comes from (data about the analysed program, not the host)
enum LibraryOrigin {
    OriginUnknown, OriginC90Function, OriginC90Macro, OriginC99Function, OriginCpp98, OriginCppLater,
    OriginPosix, OriginMsvcCrt, OriginWin32Function, OriginWin32Macro
};
LibraryOrigin libraryNameOrigin(const std::string& name);
const char*   libraryOriginText(LibraryOrigin origin);
bool        isStandardLibraryName(const std::string& name); // ISO C90 or C++98: printf, assert, sort, ...
bool        isKnownLibraryName(const std::string& name);    // any origin, including POSIX, MSVC and Win32
bool        isDoubleMathFunction(const std::string& name);  // <cmath> function taking and returning only double: fabs, sqrt, pow, ... (also std::)
std::string lastComponent(const std::string& qualified); // "a::b::c" -> "c"
std::string xmlEscape(const std::string& s);
std::string xmlDecode(const std::string& s);

// paths (all use forward slashes internally)
std::string normalizeSlashes(const std::string& p);
std::string dirName(const std::string& p);     // "a/b/c.h" -> "a/b", "c.h" -> "."
std::string baseName(const std::string& p);    // "a/b/c.h" -> "c.h"
std::string extension(const std::string& p);   // ".h" lower-cased, "" if none
bool        isAbsolutePath(const std::string& p);
std::string normalizePath(const std::string& p); // resolves "." and ".."
std::string joinPath(const std::string& dir, const std::string& rel);
std::string currentDirectory();
std::string absolutePath(const std::string& p);
std::string relativeTo(const std::string& base, const std::string& path); // display helper
bool        samePath(const std::string& a, const std::string& b);
bool        isHeaderExtension(const std::string& ext);
bool        isSourceExtension(const std::string& ext);

// files and folders
bool fileExists(const std::string& p);
bool isDirectory(const std::string& p);
// "src\*.cpp" -> every matching file ('*' and '?' in the last path component);
// a pattern without wildcards is returned as is. Returns the number of matches.
int  expandWildcards(const std::string& pattern, std::vector<std::string>& out);
// "@list.txt": paths, patterns and options separated by white space (any number per line), a path with spaces
// in double quotes, as cl's response files; a line starting with '#' is a comment
bool readResponseFile(const std::string& path, std::vector<std::string>& args);
// recursively collects header and source files under 'dir' (folders whose
// name starts with '.' are skipped); 'dirsWithHeaders' receives every folder
// that contains at least one header, for use as include directories.
// 'excluded' (absolute paths, --exclude): folders below 'dir' that are skipped
// with everything under them; each one met is added to 'skipped'
void findSourceFiles(const std::string& dir, std::vector<std::string>& headers, std::vector<std::string>& sources,
                     std::vector<std::string>& dirsWithHeaders, int depth = 0,
                     const std::vector<std::string>* excluded = 0, std::vector<std::string>* skipped = 0);
// the file's text: its bytes without a UTF-8 byte order mark at their start
bool readFile(const std::string& p, std::string& out);
// the file's bytes as they are on disk, and the byte order mark taken from a text read that way
bool readFileBytes(const std::string& p, std::string& out);
void dropByteOrderMark(std::string& text);
// a source file as a debugger is told of it, and as cl records it: the absolute path with "." and ".." resolved,
// with backslashes
std::string debugPath(const std::string& p);
bool writeFile(const std::string& p, const std::string& content);

} // namespace cppparse

#endif
