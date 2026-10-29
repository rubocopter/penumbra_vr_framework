// In-memory implementation of the HPL config boundary for product host tests.
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <list>
#include <string>
#include <vector>

namespace hpl {
using tString = std::string;
using tWString = std::wstring;
struct cString {
    static tString ToLowerCase(tString value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }
    static tWString To16Char(const char* value) { return tWString(value, value + std::char_traits<char>::length(value)); }
};
inline void Log(const char*, ...) {}
class cConfigFile {
public:
    std::map<tString, tString> values;
    void SetString(tString section, tString key, tString value) { values[section + "/" + key] = value; }
    void SetInt(tString section, tString key, int value) { SetString(section, key, std::to_string(value)); }
    void SetFloat(tString section, tString key, float value) { SetString(section, key, std::to_string(value)); }
    void SetBool(tString section, tString key, bool value) { SetString(section, key, value ? "true" : "false"); }
    tString GetString(tString section, tString key, tString fallback) {
        auto it = values.find(section + "/" + key);
        return it == values.end() ? fallback : it->second;
    }
    // Match the existing HPL atoi/atof and true-only bool conversions.
    int GetInt(tString section, tString key, int fallback) {
        auto it = values.find(section + "/" + key);
        return it == values.end() ? fallback : std::atoi(it->second.c_str());
    }
    float GetFloat(tString section, tString key, float fallback) {
        auto it = values.find(section + "/" + key);
        return it == values.end() ? fallback : static_cast<float>(std::atof(it->second.c_str()));
    }
    bool GetBool(tString section, tString key, bool fallback) {
        auto it = values.find(section + "/" + key);
        return it == values.end() ? fallback : cString::ToLowerCase(it->second) == "true";
    }
};
}
