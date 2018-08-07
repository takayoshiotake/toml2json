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

#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <regex>
#include <iomanip>

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
            static std::regex const re(R"(^\"(.*?[^\\])?\")");
            std::cmatch m;
            if (!std::regex_search(itr, end, m, re)) {
                throw std::invalid_argument("ill-formed of basic strings");
            }
            itr = m[0].second;
            
            auto string = std::string(m[1]);
            MJTOML_LOG("string: %s\n", string.c_str());
            *value = std::move(string);
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
            static std::regex const re(R"(^'(.*?)')");
            std::cmatch m;
            if (!std::regex_search(itr, end, m, re)) {
                throw std::invalid_argument("ill-formed of literal strings");
            }
            itr = m[0].second;
            
            auto string = std::regex_replace(std::string(m[1]), std::regex(R"(\\)"), "\\\\");
            string = std::regex_replace(string, std::regex(R"(\")"), "\\\"");
            MJTOML_LOG("string: %s\n", string.c_str());
            *value = std::move(string);
        }
        else {
            // Boolean
            {
                {
                    static std::regex const re(R"(^(true)[\t\r\n #])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = true;
                        return m[1].second;
                    }
                }
                {
                    static std::regex const re(R"(^(false)[\t\r\n #])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = false;
                        return m[1].second;
                    }
                }
            }
            {
                // Float
                {
                    static std::regex const re(R"(^(([+-]?)inf)[\t\r\n #])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = std::numeric_limits<double>::infinity() * (m[2].compare("-") == 0 ? -1 : 1);
                        return m[1].second;
                    }
                }
                {
                    static std::regex const re(R"(^([+-]?nan)[\t\r\n #])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = std::numeric_limits<double>::quiet_NaN();
                        return m[1].second;
                    }
                }
                {
                    static std::regex const re(R"(^([+-]?[0-9_]+(?:\.[0-9_]+)?(?:[eE][+-]?[0-9]+)?)[\t|\r|\n #])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        auto flt = std::string(m[1]);
                        MJTOML_LOG("float: %s\n", flt.c_str());
                        
                        auto flt_description = std::regex_replace(flt, std::regex("_"), "");
                        flt_description = std::regex_replace(flt_description, std::regex(R"(^\+)"), "");
                        auto flt_value = std::stod(flt_description);
                        
                        *value = MJTomlDescribedFloat{flt_value, flt_description};
                        return m[1].second;
                    }
                }
            }
            {
                // Integer
                static std::regex const re(R"(^(0x([A-Fa-f0-9_]+)|0o([0-7]+)|0b([01]+)|([+-]?[0-9_]+))[\t\r\n #])");
                std::cmatch m;
                if (std::regex_search(itr, end, m, re)) {
                    if (m[2].length() != 0) {
                        auto integer = std::string(m[2]);
                        MJTOML_LOG("hexadecimal: 0x%s\n", integer.c_str());
                        *value = std::stoll(std::regex_replace(integer, std::regex("_"), ""), 0, 16);
                    }
                    else if (m[3].length() != 0) {
                        auto integer = std::string(m[3]);
                        MJTOML_LOG("octal: 0o%s\n", integer.c_str());
                        *value = std::stoll(std::regex_replace(integer, std::regex("_"), ""), 0, 8);
                    }
                    else if (m[4].length() != 0) {
                        auto integer = std::string(m[4]);
                        MJTOML_LOG("binary: 0b%s\n", integer.c_str());
                        *value = std::stoll(std::regex_replace(integer, std::regex("_"), ""), 0, 2);
                    }
                    else if (m[5].length() != 0) {
                        auto integer = std::string(m[4]);
                        MJTOML_LOG("integer: %s\n", integer.c_str());
                        *value = std::stoll(std::regex_replace(integer, std::regex("_"), ""));
                    }
                    return m[1].second;
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
            // Bare keys, Quoted keys
            static std::regex const re(R"(^([A-Za-z0-9_-]+)|\"(.*?[^\\])\"|'(.+?)'))");
            std::smatch m;
            if (std::regex_search(itr, end, m, re)) {
                itr = m[0].second;
                if (m[1].length() != 0) {
                    auto key = std::string(m[1]);
                    MJTOML_LOG("key: %s\n", key.c_str());
                    dotted_keys.push_back(key);
                }
                else if (m[2].length() != 0) {
                    auto key = std::string(m[2]);
                    MJTOML_LOG("key: %s\n", key.c_str());
                    dotted_keys.push_back(key);
                }
                else if (m[3].length() != 0) {
                    auto key = std::regex_replace(std::string(m[3]), std::regex(R"(\\)"), "\\\\");
                    key = std::regex_replace(key, std::regex(R"(\")"), "\\\"");
                    MJTOML_LOG("key: %s\n", key.c_str());
                    dotted_keys.push_back(key);
                }
            }
            else {
                throw std::invalid_argument("ill-formed of keys");
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
    static auto parse_table(MJToml & toml, MJTomlTable * table, T itr, T end, bool is_root = false) -> T {
        itr = skip_ws(itr, end);
        while (itr < end) {
            if (*itr == '#') {
                __attribute__((unused))
                auto comment_begin = itr;
                itr = skip_to_newline(itr, end);
                
                MJTOML_LOG("comment: %s\n", std::string(comment_begin, itr).c_str());
            }
            else {
                static int const TYPE_ARRAY_OF_TABLE = 0;
                static int const TYPE_TABLE = 1;
                static int const TYPE_KEY_VALUE_PAIR = 2;
                
                std::vector<std::string> dotted_keys;
                int type = -1;
                
                // Array of table
                if (type == -1) {
                    // seeAlso: Table
                    static std::regex const re(R"(^\[\[((?:[A-Za-z0-9_-]+|\".*?[^\\]\"|'.+?'|[\t ]+|\.)+)\]\])");
                    std::cmatch m;
                    
                    if (std::regex_search(itr, end, m, re)) {
                        if (!is_root) {
                            // End the array of table, return to root table
                            return itr;
                        }
                        
                        itr = m[0].second;
                        
                        auto keys_begin = skip_ws(m[1].first, m[1].second);
                        auto keys = std::string(keys_begin, m[1].second);
                        MJTOML_LOG("keys: %s\n", keys.c_str());
                        
                        dotted_keys = parse_keys(keys.cbegin(), keys.cend());
                        type = TYPE_ARRAY_OF_TABLE;
                    }
                }
                // Table
                if (type == -1) {
                    // key_pattern = [A-Za-z0-9_-]+|\".*?[^\\]\"|'.+?'
                    // space_and_dot = [\t ]+|\.
                    // keys_pattern = (?:<key_pattern>|<space_and_dot>)+
                    static std::regex const re(R"(^\[((?:[A-Za-z0-9_-]+|\".*?[^\\]\"|'.+?'|[\t ]+|\.)+)\])");
                    std::cmatch m;
                    
                    if (std::regex_search(itr, end, m, re)) {
                        if (!is_root) {
                            // End the table, return to root table
                            return itr;
                        }
                        
                        itr = m[0].second;
                        
                        auto keys_begin = skip_ws(m[1].first, m[1].second);
                        auto keys = std::string(keys_begin, m[1].second);
                        MJTOML_LOG("keys: %s\n", keys.c_str());
                        
                        dotted_keys = parse_keys(keys.cbegin(), keys.cend());
                        type = TYPE_TABLE;
                    }
                }
                // Dotted keys, includes Bare keys and Quoted keys
                if (type == -1) {
                    static std::regex const re(R"(^(.*?)[\t ]*=[\t ]*)");
                    std::cmatch m;
                    
                    if (std::regex_search(itr, end, m, re)) {
                        // The itr points the beginning of the value.
                        itr = m[0].second;
                        
                        auto keys = std::string(m[1]);
                        MJTOML_LOG("keys: %s\n", keys.c_str());
                        
                        dotted_keys = parse_keys(keys.cbegin(), keys.cend());
                        type = TYPE_KEY_VALUE_PAIR;
                    }
                }
                
                if (dotted_keys.empty() || type == -1) {
                    throw std::invalid_argument("ill-formed of toml");
                }
                else {
                    MJTomlTable * child_table = table;
                    auto value_key = dotted_keys.front();
                    for (auto key = dotted_keys.cbegin() + 1; key < dotted_keys.cend(); ++key) {
                        if (child_table->count(value_key) != 0) {
                            if ((*child_table)[value_key].type() == typeid(MJTomlTable)) {
                                child_table = std::any_cast<MJTomlTable>(&(*child_table)[value_key]);
                            }
                            else if ((*child_table)[value_key].type() == typeid(MJTomlArray)) {
                                auto ary_ptr = std::any_cast<MJTomlArray>(&(*child_table)[value_key]);
                                child_table = std::any_cast<MJTomlTable>(&ary_ptr->back());
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

                    if (type == TYPE_ARRAY_OF_TABLE) {
                        if (child_table->count(value_key) == 0) {
                            (*child_table)[value_key] = MJTomlArray();
                            auto ary_ptr = std::any_cast<MJTomlArray>(&(*child_table)[value_key]);
                            ary_ptr->push_back(MJTomlTable());
                            child_table = std::any_cast<MJTomlTable>(&ary_ptr->back());
                        }
                        else {
                            auto ary_ptr = std::any_cast<MJTomlArray>(&(*child_table)[value_key]);
                            ary_ptr->push_back(MJTomlTable());
                            child_table = std::any_cast<MJTomlTable>(&ary_ptr->back());
                        }
                        
                        itr = parse_table(toml, child_table, itr, end);
                    }
                    else if (type == TYPE_TABLE) {
                        if (child_table->count(value_key) != 0) {
                            throw std::invalid_argument("Duplicated key");
                        }

                        (*child_table)[value_key] = MJTomlTable();
                        child_table = std::any_cast<MJTomlTable>(&(*child_table)[value_key]);
                        
                        itr = parse_table(toml, child_table, itr, end);
                    }
                    else if (type == TYPE_KEY_VALUE_PAIR) {
                        if (child_table->count(value_key) != 0) {
                            throw std::invalid_argument("Duplicated key");
                        }
                        
                        std::any value;
                        itr = parse_value(&value, itr, end);
                        (*child_table)[value_key] = value;
                    }
                    else {
                        throw std::logic_error("Never reached");
                    }
                }
            }

            itr = skip_ws(itr, end);
        }
        return itr;
    }
    
    
    static auto string_json(MJTomlTable const & table, int indent, bool is_strict) -> std::string;
    static auto string_json(MJTomlArray const & array, int indent, bool is_strict) -> std::string;
    
    static auto string_json(MJTomlTable const & table, int indent, bool is_strict) -> std::string {
        std::stringstream ss;
        std::string joiner = "\n";
        
        std::string root_space;
        for (int i = 0; i < indent; ++i) {
            root_space += "  ";
        }
        std::string space = root_space + "  ";
        
        ss << "{";
        for (auto itr = table.begin(); itr != table.end(); ++itr) {
            ss << joiner << space << "\"" << itr->first << "\": ";
            if (itr->second.type() == typeid(MJTomlTable)) {
                ss << string_json(*std::any_cast<MJTomlTable>(&itr->second), indent + 1, is_strict);
            }
            else if (itr->second.type() == typeid(MJTomlArray)) {
                ss << string_json(*std::any_cast<MJTomlArray>(&itr->second), indent + 1, is_strict);
            }
            else if (itr->second.type() == typeid(MJTomlString)) {
                auto str_ptr = std::any_cast<MJTomlString>(&itr->second);
                ss << "\"" << *str_ptr << "\"";
            }
            else if (itr->second.type() == typeid(MJTomlBoolean)) {
                auto bool_ptr = std::any_cast<MJTomlBoolean>(&itr->second);
                ss << (*bool_ptr ? "true" : "false");
            }
            else if (itr->second.type() == typeid(MJTomlInteger)) {
                auto int_ptr = std::any_cast<MJTomlInteger>(&itr->second);
                ss << *int_ptr;
            }
            else if (itr->second.type() == typeid(MJTomlFloat)) {
                auto flt_ptr = std::any_cast<MJTomlFloat>(&itr->second);
                if (std::isinf(*flt_ptr)) {
                    if (is_strict) {
                        ss << "\"" << (*flt_ptr < 0 ? "-" : "") << "Infinity" << "\"";
                    }
                    else {
                        ss << (*flt_ptr < 0 ? "-" : "") << "Infinity";
                    }
                }
                else if (std::isnan(*flt_ptr)) {
                    if (is_strict) {
                        ss << "\"NaN\"";
                    }
                    else {
                        ss << "NaN";
                    }
                }
                else {
                    ss << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10) << *flt_ptr;
                }
            }
            else if (itr->second.type() == typeid(MJTomlDescribedFloat)) {
                auto flt_ptr = std::any_cast<MJTomlDescribedFloat>(&itr->second);
                ss << flt_ptr->description;
            }
            joiner = ",\n";
        }
        ss << std::endl << root_space << "}";
        return ss.str();
    }
        
    static auto string_json(MJTomlArray const & array, int indent, bool is_strict) -> std::string {
        std::stringstream ss;
        std::string joiner = "\n";
        
        std::string root_space;
        for (int i = 0; i < indent; ++i) {
            root_space += "  ";
        }
        std::string space = root_space + "  ";
        
        ss << "[";
        for (auto itr = array.begin(); itr != array.end(); ++itr) {
            ss << joiner << space;
            if (itr->type() == typeid(MJTomlTable)) {
                ss << string_json(*std::any_cast<MJTomlTable>(&*itr), indent + 1, is_strict);
            }
            joiner = ",\n";
        }
        ss << std::endl << root_space << "]";
        return ss.str();
    }
}

namespace MoonJelly {
    
MJToml parse_toml(std::string_view str) {
    MJToml toml;
    ::parse_table(toml, &toml.table, str.cbegin(), str.cend(), true);
    return toml;
}

std::string string_json(MJToml const & toml, int indent, bool is_strict) {
    return ::string_json(toml.table, indent, is_strict);
}

}
