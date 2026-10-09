// lib2.cpp - exercises cppparse's own map and set headers; compiled by cppparse --compile6 and
// checked by examples/sample-lib-driver/driver-lib.cpp.
#include <map>
#include <set>
#include <string>
#include <vector>

int mapBasics()                            // 2613: 26 = 2+5+8+11, 1 erased, 3 left
{
    std::map<int, int> m;
    for (int i = 0; i < 4; ++i) m[i] = 3 * i + 2;                // 2 5 8 11
    int sum = 0;
    for (std::map<int, int>::iterator it = m.begin(); it != m.end(); ++it) sum += it->second;
    size_t erased = m.erase(2);
    int r = sum * 100 + static_cast<int>(erased) * 10 + static_cast<int>(m.size());
    if (m.find(2) != m.end() || m.find(3) == m.end() || m.count(0) != 1) return -1;
    return r;
}

int mapOfStrings()                         // 731: "cat" -> 7, 3 keys in order, "ant" first
{
    std::map<std::string, int> ages;
    ages["dog"] = 4;
    ages["cat"] = 7;
    ages["ant"] = 1;
    ages["cat"] += 0;
    std::string first = ages.begin()->first;
    std::map<std::string, int>::iterator last = ages.end();
    --last;
    int r = ages["cat"] * 100 + static_cast<int>(ages.size()) * 10;
    if (first == "ant" && last->first == "dog") r += 1;
    std::map<std::string, int> copy(ages);
    copy.erase("dog");
    if (copy.size() != 2 || ages.size() != 3) return -1;
    return r;
}

int setBasics()                            // 4253: 4 distinct of 6 inserted, 2 = count of 5 + count of 9, lower_bound(4) = 5, 3 left after erase
{
    std::set<int> s;
    int values[6] = { 5, 1, 9, 5, 3, 1 };
    for (int i = 0; i < 6; ++i) s.insert(values[i]);
    int r = static_cast<int>(s.size()) * 1000;
    r += (static_cast<int>(s.count(5)) + static_cast<int>(s.count(9)) + static_cast<int>(s.count(7))) * 100;
    r += *s.lower_bound(4) * 10;
    s.erase(s.find(9));
    r += static_cast<int>(s.size());
    int prev = -1;
    for (std::set<int>::iterator it = s.begin(); it != s.end(); ++it) { if (*it <= prev) return -1; prev = *it; }
    return r;
}

int treeStress(int n)                      // n: n keys inserted in a scrambled order, half erased, all accounted for
{
    std::map<int, int> m;
    std::vector<int> keys;
    for (int i = 0; i < n; ++i) keys.push_back((i * 7919) % n);
    for (size_t i = 0; i < keys.size(); ++i) m[keys[i]] = keys[i] * 2;
    if (static_cast<int>(m.size()) != n) return -1;
    int removed = 0;
    for (std::map<int, int>::iterator it = m.begin(); it != m.end();) {
        if (it->first % 2 == 1) { std::map<int, int>::iterator victim = it; ++it; m.erase(victim); ++removed; }
        else ++it;
    }
    int expect = 0;
    for (std::map<int, int>::iterator it = m.begin(); it != m.end(); ++it) { if (it->first != expect || it->second != expect * 2) return -2; expect += 2; }
    return removed + static_cast<int>(m.size());
}
