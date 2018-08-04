//
//  MJToml.cpp
//  MoonJelly
//
//  Created by OTAKE Takayoshi on 2018/07/28.
//

#include "MJToml.hpp"

#define DEBUG_MJTOML
#ifdef DEBUG_MJTOML
#include <cstdio>
#define MJTOML_LOG(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
#else
#define MJTOML_LOG(fmt, ...)
#endif

#include <cstring>
#include <sstream>
#include <regex>

namespace {
    using namespace MoonJelly;
    
    template <typename T>
    static T skip_ws(T itr, T end) {
        while (itr < end && (*itr == '\t' || *itr == ' ' || *itr == '\n' || *itr == '\r')) {
            if (*itr == '\r') {
                if (itr + 1 < end && *(itr + 1) != '\n') {
                    break;
                }
                ++itr;
            }
            ++itr;
        }
        return itr;
    }
    
    template <typename T>
    static auto skip_to_newline(T itr, T end) -> T {
        while (*itr != '\n' && *itr != '\r') {
            ++itr;
        }
        return itr;
    }
    
    template <typename T>
    static auto parse_basic_string(std::any * value, T itr, T end) -> T {
        auto string_begin = ++itr;
        while (itr <= end) {
            if (itr == end) {
                throw std::invalid_argument("ill-formed of basic strings");
            }
            
            if (*itr == '\\') {
                ++itr;
                if (itr == end) {
                    throw std::invalid_argument("ill-formed of basic strings");
                }
                ++itr;
            }
            else if (*itr == '"') {
                break;
            }
            ++itr;
        }
        
        // Trim
        auto string = std::regex_replace(std::string(string_begin, itr), std::regex(R"(\\\r?\n[ \t\r\n]*)"), "");
        MJTOML_LOG("string: %s\n", string.c_str());
        *value = std::move(string);
        
        // Skip last "
        ++itr;
        return itr;
    }
    
    template <typename T>
    static auto parse_literal_string(std::any * value, T itr, T end) -> T {
        auto string_begin = ++itr;
        while (itr <= end) {
            if (itr == end) {
                throw std::invalid_argument("ill-formed of literal strings");
            }
            
            if (*itr == '\'') {
                break;
            }
            ++itr;
        }
        
        // Escape
        auto string = std::regex_replace(std::string(string_begin, itr), std::regex(R"(\\)"), "\\\\");
        string = std::regex_replace(string, std::regex(R"(\")"), "\\\"");
        MJTOML_LOG("string: %s\n", string.c_str());
        *value = std::move(string);
        
        // Skip last '
        ++itr;
        return itr;
    }
    
    template <typename T>
    static auto parse_value(std::any * value, T itr, T end) -> T {
        if (::strstr(itr, "\"\"\"") == itr) {
            // Multi-line basic strings
            static std::regex const re(R"(^\"\"\"([\s\S]*?)\"\"\")");
            std::cmatch m;
            if (!std::regex_search(itr, end, m, re)) {
                throw std::invalid_argument("ill-formed of multi-line basic strings");
            }
            itr = m[0].second;
            
            // A newline immediately following the opening delimiter will be trimmed.
            auto string_begin = m[1].first;
            if (m[1].second - m[1].first > 1 && *m[1].first == '\n') {
                ++string_begin;
            }
            else if (m[1].second - m[1].first > 2 && m[1].first[0] == '\r' && m[1].first[1] == '\n') {
                string_begin += 2;
            }
            
            // Trim
            auto string = std::regex_replace(std::string(string_begin, m[1].second), std::regex(R"(\\\r?\n[ \t\r\n]*)"), "");
            string = std::regex_replace(string, std::regex("\r"), "\\r");
            string = std::regex_replace(string, std::regex("\n"), "\\n");
            MJTOML_LOG("string: %s\n", string.c_str());
            *value = std::move(string);
        }
        else if (*itr == '"') {
            // Basic strings
            itr = parse_basic_string(value, itr, end);
        }
        else if (::strstr(itr, "'''") == itr) {
            // Multi-line literal strings
            static std::regex const re(R"(^'''([\s\S]*?)''')");
            std::cmatch m;
            if (!std::regex_search(itr, end, m, re)) {
                throw std::invalid_argument("ill-formed of multi-line literal strings");
            }
            itr = m[0].second;
            
            // A newline immediately following the opening delimiter will be trimmed.
            auto string_begin = m[1].first;
            if (m[1].second - m[1].first > 1 && *m[1].first == '\n') {
                ++string_begin;
            }
            else if (m[1].second - m[1].first > 2 && m[1].first[0] == '\r' && m[1].first[1] == '\n') {
                string_begin += 2;
            }
            
            // Escape
            auto string = std::regex_replace(std::string(string_begin, m[1].second), std::regex(R"(\\)"), "\\\\");
            string = std::regex_replace(string, std::regex("\r"), "\\r");
            string = std::regex_replace(string, std::regex("\n"), "\\n");
            MJTOML_LOG("string: %s\n", string.c_str());
            *value = std::move(string);
        }
        else if (*itr == '\'') {
            // Literal strings
            itr = parse_literal_string(value, itr, end);
        }
        else {
            {
                static std::regex const re(R"(^true[\t\r\n ])");
                std::cmatch m;
                if (std::regex_search(itr, end, m, re)) {
                    *value = true;
                    return m[0].second;
                }
            }
            {
                static std::regex const re(R"(^false[\t\r\n ])");
                std::cmatch m;
                if (std::regex_search(itr, end, m, re)) {
                    *value = false;
                    return m[0].second;
                }
            }
            
            throw std::logic_error("not implemented");
        }
        
        return itr;
    }

    template <typename T>
    static auto parse_keys(T itr, T end) -> std::vector<std::string> {
        std::vector<std::string> dotted_keys;
        while (itr < end) {
            // Bare keys
            static std::regex const re("^([A-Za-z0-9_-]+)");
            std::smatch m;
            if (std::regex_search(itr, end, m, re)) {
                itr = m[1].second;
                
                auto key = std::string(m[1]);
                MJTOML_LOG("key: %s\n", key.c_str());
                dotted_keys.push_back(key);
            }
            else {
                // Quoted keys
                if (*itr == '"' || *itr == '\'') {
                    std::any quoted_key;
                    if (*itr == '"') {
                        itr = parse_basic_string(&quoted_key, itr, end);
                    }
                    else if (*itr == '\'') {
                        itr = parse_literal_string(&quoted_key, itr, end);
                    }
                    else {
                        throw std::logic_error("Never reached");
                    }
                    auto key = *std::any_cast<std::string>(&quoted_key);
                    MJTOML_LOG("key: %s\n", key.c_str());
                    dotted_keys.push_back(key);
                }
                else {
                    throw std::invalid_argument("ill-formed of keys");
                }
            }
            
            itr = skip_ws(itr, end);
            if (itr < end && *itr == '.') {
                ++itr;
                itr = skip_ws(itr, end);
            }
            else if (itr != end) {
                throw std::invalid_argument("ill-formed of keys");
            }
        }
        return dotted_keys;
    }
    
    template <typename T>
    static auto parse_table(MJTomlTable * table, T itr, T end, bool is_root = false) -> T {
        itr = skip_ws(itr, end);
        while (itr < end) {
            if (*itr == '#') {
                __attribute__((unused))
                auto comment_begin = itr;
                itr = skip_to_newline(itr, end);
                
                MJTOML_LOG("comment: %s\n", std::string(comment_begin, itr).c_str());
            }
            else {
                std::vector<std::string> dotted_keys;
                // TODO: Table, Array of tables
                
                // Dotted keys, includes Bare keys and Quoted keys
                {
                    static std::regex const re(R"(^(.*?)[\t ]*=[\t ]*)");
                    std::cmatch m;
                    
                    if (std::regex_search(itr, end, m, re)) {
                        // The itr points the beginning of the value.
                        itr = m[0].second;
                        
                        auto keys = std::string(m[1]);
                        MJTOML_LOG("keys: %s\n", keys.c_str());
                        
                        dotted_keys = parse_keys(keys.cbegin(), keys.cend());
                        
                        MJTomlTable * child_table = table;
                        auto value_key = dotted_keys.front();
                        for (auto key = dotted_keys.cbegin() + 1; key < dotted_keys.cend(); ++key) {
                            if (child_table->count(value_key) != 0) {
                                if ((*child_table)[value_key].type() == typeid(MJTomlTable)) {
                                    child_table = std::any_cast<MJTomlTable>(&(*child_table)[value_key]);
                                }
                                else {
                                    throw std::invalid_argument("Invalid key");
                                }
                            }
                            else {
                                (*child_table)[value_key] = MJTomlTable();
                                child_table = std::any_cast<MJTomlTable>(&(*child_table)[value_key]);
                            }
                            value_key = *key;
                        }
                        
                        if (child_table->count(value_key) != 0) {
                            throw std::invalid_argument("Duplicated key");
                        }
                        
                        std::any value;
                        itr = parse_value(&value, itr, end);
                        
                        (*child_table)[value_key] = value;
                    }
                    else {
                        throw std::invalid_argument("ill-formed of toml");
                    }
                }
            }

            itr = skip_ws(itr, end);
        }
        return itr;
    }
}

namespace MoonJelly {
    
MJTomlTable parse_toml(std::string_view str) {
    MJTomlTable root_table;
    parse_table(&root_table, str.cbegin(), str.cend(), true);
    return root_table;
}

std::string string_json(MJTomlTable const & toml, int indent) {
    std::stringstream ss;
    std::string joiner = "";
    
    std::string root_space;
    for (int i = 0; i < indent; ++i) {
        root_space += "  ";
    }
    std::string space = root_space + "  ";
    
    ss << "{" << std::endl;
    for (auto itr = toml.begin(); itr != toml.end(); ++itr) {
        ss << joiner << space << "\"" << itr->first << "\": ";
        if (itr->second.type() == typeid(MJTomlTable)) {
            ss << string_json(*std::any_cast<MJTomlTable>(&itr->second), indent + 1);
        }
        else if (itr->second.type() == typeid(MJTomlString)) {
            auto str_ptr = std::any_cast<MJTomlString>(&itr->second);
            ss << "\"" << *str_ptr << "\"";
        }
        else if (itr->second.type() == typeid(MJTomlBoolean)) {
            auto bool_ptr = std::any_cast<MJTomlBoolean>(&itr->second);
            ss << (*bool_ptr ? "true" : "false");
        }
        joiner = ",\n";
    }
    ss << std::endl << root_space << "}";
    return ss.str();
}
    
}
