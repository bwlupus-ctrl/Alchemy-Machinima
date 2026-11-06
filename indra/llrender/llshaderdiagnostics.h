/** Shader-load-only diagnostics. The parser is independent of GL and settings. */
#ifndef LL_SHADER_DIAGNOSTICS_H
#define LL_SHADER_DIAGNOSTICS_H

#include <charconv>
#include <string>
#include <vector>

struct LLShaderInjection
{
    std::string kind;
    std::string target;
    int value = 0;

    // Invalid entries are rejected independently; a nonempty input still disables caching.
    static std::vector<LLShaderInjection> parse(const std::string& input,
                                               std::vector<std::string>& errors)
    {
        std::vector<LLShaderInjection> result;
        size_t start = 0;
        while (start < input.size())
        {
            size_t end = input.find(';', start);
            if (end == std::string::npos) end = input.size();
            std::string entry = input.substr(start, end - start);
            start = end + 1;
            if (entry.empty()) continue;
            const auto colon = entry.find(':');
            LLShaderInjection item;
            bool valid = colon != std::string::npos;
            if (valid)
            {
                item.kind = entry.substr(0, colon);
                item.target = entry.substr(colon + 1);
                if (item.kind == "compile" || item.kind == "overflow")
                {
                    const auto last = item.target.rfind(':');
                    valid = last != std::string::npos;
                    if (valid)
                    {
                        const std::string number = item.target.substr(last + 1);
                        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), item.value);
                        valid = !number.empty() && parsed.ec == std::errc() &&
                            parsed.ptr == number.data() + number.size() && item.value >= 0;
                        item.target.resize(last);
                    }
                }
                else valid = item.kind == "link" || item.kind == "binary" || item.kind == "missing";
                valid = valid && !item.target.empty() && item.target.find(':') == std::string::npos;
            }
            if (valid) result.push_back(item);
            else errors.push_back(entry);
        }
        return result;
    }
};

#endif
