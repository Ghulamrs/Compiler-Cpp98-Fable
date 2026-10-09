// PathUtil.cpp - string, path and file helpers.
#include "PathUtil.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define CPPPARSE_GETCWD _getcwd
#else
#include <dirent.h>
#include <unistd.h>
#define CPPPARSE_GETCWD getcwd
#endif

namespace cppparse {

// ---- strings ---------------------------------------------------------------

std::string trim(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string toLower(const std::string& s)
{
    std::string out(s);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
    return out;
}

bool startsWith(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool iequals(const std::string& a, const std::string& b)
{
    return toLower(a) == toLower(b);
}

void split(const std::string& s, char sep, std::vector<std::string>& out, bool keepEmpty)
{
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == sep) {
            std::string t = trim(cur);
            if (keepEmpty || !t.empty()) out.push_back(t);
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    std::string t = trim(cur);
    if (keepEmpty || !t.empty()) out.push_back(t);
}

std::string intToString(int v)
{
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

std::string replaceAll(const std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty()) return s;
    std::string out;
    size_t pos = 0;
    for (;;) {
        size_t hit = s.find(from, pos);
        if (hit == std::string::npos) { out += s.substr(pos); break; }
        out += s.substr(pos, hit - pos);
        out += to;
        pos = hit + from.size();
    }
    return out;
}

std::string padRight(const std::string& s, size_t width)
{
    if (s.size() >= width) return s + " ";
    return s + std::string(width - s.size(), ' ');
}

bool contains(const std::vector<std::string>& v, const std::string& s)
{
    for (size_t i = 0; i < v.size(); ++i) if (v[i] == s) return true;
    return false;
}

void addUnique(std::vector<std::string>& v, const std::string& s)
{
    if (!contains(v, s)) v.push_back(s);
}

std::string stripTemplateArgs(const std::string& s)
{
    std::string out;
    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '<') { ++depth; continue; }
        if (c == '>') { if (depth > 0) --depth; continue; }
        if (depth == 0) out += c;
    }
    return trim(out);
}

std::string lastComponent(const std::string& qualified)
{
    size_t pos = qualified.rfind("::");
    if (pos == std::string::npos) return qualified;
    return qualified.substr(pos + 2);
}

std::string xmlEscape(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        switch (s[i]) {
        case '&':  out += "&amp;";  break;
        case '<':  out += "&lt;";   break;
        case '>':  out += "&gt;";   break;
        case '"':  out += "&quot;"; break;
        default:   out += s[i];
        }
    }
    return out;
}

std::string xmlDecode(const std::string& s)
{
    std::string out = replaceAll(s, "&lt;", "<");
    out = replaceAll(out, "&gt;", ">");
    out = replaceAll(out, "&quot;", "\"");
    out = replaceAll(out, "&apos;", "'");
    out = replaceAll(out, "&#39;", "'");
    out = replaceAll(out, "&amp;", "&");
    return out;
}

// ---- paths -----------------------------------------------------------------

std::string normalizeSlashes(const std::string& p)
{
    std::string out(p);
    for (size_t i = 0; i < out.size(); ++i) if (out[i] == '\\') out[i] = '/';
    return out;
}

std::string dirName(const std::string& p)
{
    std::string n = normalizeSlashes(p);
    size_t pos = n.rfind('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    if (pos == 2 && n[1] == ':') return n.substr(0, 3);
    return n.substr(0, pos);
}

std::string baseName(const std::string& p)
{
    std::string n = normalizeSlashes(p);
    size_t pos = n.rfind('/');
    if (pos == std::string::npos) return n;
    return n.substr(pos + 1);
}

std::string extension(const std::string& p)
{
    std::string b = baseName(p);
    size_t pos = b.rfind('.');
    if (pos == std::string::npos) return "";
    return toLower(b.substr(pos));
}

bool isAbsolutePath(const std::string& p)
{
    std::string n = normalizeSlashes(p);
    if (n.empty()) return false;
    if (n[0] == '/') return true;
    if (n.size() >= 2 && isalpha(static_cast<unsigned char>(n[0])) && n[1] == ':') return true;
    return false;
}

std::string normalizePath(const std::string& p)
{
    std::string n = normalizeSlashes(p);
    std::string prefix;
    if (n.size() >= 2 && n[1] == ':') { prefix = n.substr(0, 2); n = n.substr(2); }
    bool rooted = !n.empty() && n[0] == '/';

    std::vector<std::string> parts, stack;
    split(n, '/', parts);
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& s = parts[i];
        if (s == ".") continue;
        if (s == "..") {
            if (!stack.empty() && stack.back() != "..") stack.pop_back();
            else if (!rooted) stack.push_back("..");
            continue;
        }
        stack.push_back(s);
    }
    std::string out = prefix;
    if (rooted) out += "/";
    for (size_t i = 0; i < stack.size(); ++i) {
        if (i) out += "/";
        out += stack[i];
    }
    if (out.empty()) out = ".";
    return out;
}

std::string joinPath(const std::string& dir, const std::string& rel)
{
    if (isAbsolutePath(rel)) return normalizePath(rel);
    if (dir.empty() || dir == ".") return normalizePath(rel);
    return normalizePath(dir + "/" + rel);
}

std::string currentDirectory()
{
    char buf[4096];
    if (CPPPARSE_GETCWD(buf, sizeof(buf)) == 0) return ".";
    return normalizeSlashes(buf);
}

std::string absolutePath(const std::string& p)
{
    return joinPath(currentDirectory(), p);
}

std::string relativeTo(const std::string& base, const std::string& path)
{
    std::string b = toLower(normalizePath(base));
    std::string n = normalizePath(path);
    if (!endsWith(b, "/")) b += "/";
    if (startsWith(toLower(n), b)) return n.substr(b.size());
    return n;
}

bool samePath(const std::string& a, const std::string& b)
{
    return toLower(normalizePath(a)) == toLower(normalizePath(b));
}

bool isHeaderExtension(const std::string& ext)
{
    return ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".hh" ||
           ext == ".inl" || ext == ".tli" || ext == ".tlh" || ext == ".ipp";
}

bool isSourceExtension(const std::string& ext)
{
    return ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".c" || ext == ".c++";
}

// ---- files -----------------------------------------------------------------

bool fileExists(const std::string& p)
{
    std::ifstream in(p.c_str(), std::ios::in | std::ios::binary);
    return in.good();
}

bool isDirectory(const std::string& p)
{
    std::string n = normalizePath(p);
    if (n.size() > 1 && n[n.size() - 1] == '/') n.erase(n.size() - 1);
    struct stat st;
    if (stat(n.c_str(), &st) != 0) return false;
    return (st.st_mode & S_IFDIR) != 0;
}

namespace {

void listDirectory(const std::string& dir, std::vector<std::string>& names)
{
#ifdef _WIN32
    std::string pattern = dir + "/*";
    struct _finddata_t fd;
    intptr_t h = _findfirst(pattern.c_str(), &fd);
    if (h == -1) return;
    do {
        std::string name = fd.name;
        if (name != "." && name != "..") names.push_back(name);
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    for (struct dirent* e = readdir(d); e; e = readdir(d)) {
        std::string name = e->d_name;
        if (name != "." && name != "..") names.push_back(name);
    }
    closedir(d);
#endif
}

} // namespace

void findSourceFiles(const std::string& dir, std::vector<std::string>& headers, std::vector<std::string>& sources,
                     std::vector<std::string>& dirsWithHeaders, int depth)
{
    if (depth > 32) return;
    std::string base = normalizePath(dir);
    std::vector<std::string> names;
    listDirectory(base, names);
    bool hasHeader = false;
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string& name = names[i];
        if (name.empty() || name[0] == '.') continue;
        std::string full = base + "/" + name;
        if (isDirectory(full)) {
            findSourceFiles(full, headers, sources, dirsWithHeaders, depth + 1);
            continue;
        }
        std::string ext = extension(name);
        if (isHeaderExtension(ext)) { headers.push_back(full); hasHeader = true; }
        else if (isSourceExtension(ext)) sources.push_back(full);
    }
    if (hasHeader) dirsWithHeaders.push_back(base);
}

bool readFile(const std::string& p, std::string& out)
{
    std::ifstream in(p.c_str(), std::ios::in | std::ios::binary);
    if (!in) return false;
    std::ostringstream oss;
    oss << in.rdbuf();
    out = oss.str();
    // drop a UTF-8 byte order mark
    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF &&
        static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF)
        out.erase(0, 3);
    return true;
}

bool writeFile(const std::string& p, const std::string& content)
{
    std::ofstream out(p.c_str(), std::ios::out | std::ios::binary);
    if (!out) return false;
    out << content;
    return out.good();
}

} // namespace cppparse
