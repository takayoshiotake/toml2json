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
    
    struct MJTomlArrayParsing {
        bool is_static;
        std::vector<std::any> values;
    };
    
    template <typename T>
    static auto skip_ws(T itr, T end) -> T;
    template <typename T>
    static auto skip_ws_within_single_line(T itr, T end) -> T;
    template <typename T>
    static auto skip_to_newline(T itr, T end) -> T;
    
    template <typename T>
    static auto parse_keys(T itr, T end) -> std::vector<std::string>;
    
    template <typename T>
    static auto read_table(MJTomlTable * table, T itr, T end, bool is_root = false) -> T;
    template <typename T>
    static auto read_value(std::any * value, T itr, T end) -> T;
    template <typename T>
    static auto read_array(std::any * value, T itr, T end) -> T;
    template <typename T>
    static auto read_inline_table(std::any * value, T itr, T end) -> T;

    // MARK: -
    
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
    static T skip_ws_within_single_line(T itr, T end) {
        while (itr < end && (*itr == '\t' || *itr == ' ')) {
            ++itr;
        }
        return itr;
    }
    
    template <typename T>
    static auto skip_to_newline(T itr, T end) -> T {
        while (itr < end && (*itr != '\n' && *itr != '\r')) {
            ++itr;
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
    static auto read_table(MJTomlTable * table, T itr, T end, bool is_root) -> T {
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
                            else if ((*child_table)[value_key].type() == typeid(MJTomlArrayParsing)) {
                                auto ary_ptr = std::any_cast<MJTomlArrayParsing>(&(*child_table)[value_key]);
                                child_table = std::any_cast<MJTomlTable>(&ary_ptr->values.back());
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
                            (*child_table)[value_key] = MJTomlArrayParsing();
                            auto ary_ptr = std::any_cast<MJTomlArrayParsing>(&(*child_table)[value_key]);
                            ary_ptr->values.push_back(MJTomlTable());
                            child_table = std::any_cast<MJTomlTable>(&ary_ptr->values.back());
                        }
                        else {
                            auto ary_ptr = std::any_cast<MJTomlArrayParsing>(&(*child_table)[value_key]);
                            if (ary_ptr->is_static) {
                                throw std::invalid_argument("ill-formed of array: statically defined array is not appendable");
                            }
                            ary_ptr->values.push_back(MJTomlTable());
                            child_table = std::any_cast<MJTomlTable>(&ary_ptr->values.back());
                        }
                        
                        itr = read_table(child_table, itr, end);
                    }
                    else if (type == TYPE_TABLE) {
                        if (child_table->count(value_key) != 0) {
                            throw std::invalid_argument("Duplicated key");
                        }
                        
                        (*child_table)[value_key] = MJTomlTable();
                        child_table = std::any_cast<MJTomlTable>(&(*child_table)[value_key]);
                        
                        itr = read_table(child_table, itr, end);
                    }
                    else if (type == TYPE_KEY_VALUE_PAIR) {
                        if (child_table->count(value_key) != 0) {
                            throw std::invalid_argument("Duplicated key");
                        }
                        
                        std::any value;
                        itr = read_value(&value, itr, end);
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
    
    template <typename T>
    static auto read_value(std::any * value, T itr, T end) -> T {
        if (*itr == '[') {
            itr = read_array(value, itr, end);
        }
        else if (*itr == '{') {
            itr = read_inline_table(value, itr, end);
        }
        else if (::strstr(itr, "\"\"\"") == itr) {
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
                    static std::regex const re(R"(^(true)[\t\r\n #,\]])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = true;
                        return m[1].second;
                    }
                }
                {
                    static std::regex const re(R"(^(false)[\t\r\n #,\]])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = false;
                        return m[1].second;
                    }
                }
            }
            // Float
            {
                {
                    static std::regex const re(R"(^(([+-]?)inf)[\t\r\n #,\]])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = std::numeric_limits<double>::infinity() * (m[2].compare("-") == 0 ? -1 : 1);
                        return m[1].second;
                    }
                }
                {
                    static std::regex const re(R"(^([+-]?nan)[\t\r\n #,\]])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        *value = std::numeric_limits<double>::quiet_NaN();
                        return m[1].second;
                    }
                }
                {
                    static std::regex const re(R"(^([+-]?[0-9_]+(?:\.[0-9_]+(?:[eE][+-]?[0-9]+)?|(?:\.[0-9_]+)?[eE][+-]?[0-9]+))[\t|\r|\n #,\]])");
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
            // Integer
            {
                static std::regex const re(R"(^(0x([A-Fa-f0-9_]+)|0o([0-7]+)|0b([01]+)|([+-]?[0-9_]+))[\t\r\n #,\]])");
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
                        auto integer = std::string(m[5]);
                        MJTOML_LOG("integer: %s\n", integer.c_str());
                        *value = std::stoll(std::regex_replace(integer, std::regex("_"), ""));
                    }
                    return m[1].second;
                }
            }
            // Offset Date-Time, Local Date-Time, Local Date, Local Time (RFC 3339)
            {
                // Offset Date-Time, Local Date-Time
                {
                    static std::regex const re(R"(^(\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?(?:Z|[+-]\d{2}:\d{2})?)[\t\r\n #,\]])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        auto datetime = std::string(m[1]);
                        MJTOML_LOG("datetime: %s\n", datetime.c_str());
                        
                        *value = MJTomlDateTime{datetime};
                        return m[1].second;
                    }
                }
                // Local Date, Local Time
                {
                    static std::regex const re(R"(^(\d{4}-\d{2}-\d{2}|\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?)[\t\r\n #,\]])");
                    std::cmatch m;
                    if (std::regex_search(itr, end, m, re)) {
                        auto datetime = std::string(m[1]);
                        MJTOML_LOG("datetime: %s\n", datetime.c_str());
                        
                        *value = MJTomlDateTime{datetime};
                        return m[1].second;
                    }
                }
            }
            
            throw std::logic_error("not implemented");
        }
        
        return itr;
    }
    
    template <typename T>
    static auto read_array(std::any * value, T itr, T end) -> T {
        if (itr >= end || *itr != '[') {
            throw std::invalid_argument("ill-formed of array");
        }
        ++itr;
        itr = skip_ws(itr, end);
        if (itr >= end) {
            throw std::invalid_argument("ill-formed of array");
        }
        
        // Array
        MJTOML_LOG("array\n");
        *value = MJTomlArrayParsing();
        auto ary_ptr = std::any_cast<MJTomlArrayParsing>(value);
        ary_ptr->is_static = true;
        auto is_first = true;
        while (itr < end) {
            itr = skip_ws(itr, end);
            if (itr >= end) {
                throw std::invalid_argument("ill-formed of array");
            }
            
            if (*itr == '#') {
                __attribute__((unused))
                auto comment_begin = itr;
                itr = skip_to_newline(itr, end);
                
                MJTOML_LOG("comment: %s\n", std::string(comment_begin, itr).c_str());
                itr = skip_ws(itr, end);
                if (itr >= end) {
                    throw std::invalid_argument("ill-formed of array");
                }
            }
            
            if (*itr == ']') {
                // End of array
                return itr + 1;
            }
            
            if (!is_first) {
                if (*itr != ',') {
                    throw std::invalid_argument("ill-formed of array");
                }
                ++itr;
                itr = skip_ws(itr, end);
                if (itr >= end) {
                    throw std::invalid_argument("ill-formed of array");
                }
                
                if (*itr == '#') {
                    __attribute__((unused))
                    auto comment_begin = itr;
                    itr = skip_to_newline(itr, end);
                    
                    MJTOML_LOG("comment: %s\n", std::string(comment_begin, itr).c_str());
                    itr = skip_ws(itr, end);
                    if (itr >= end) {
                        throw std::invalid_argument("ill-formed of array");
                    }
                }
                if (itr >= end) {
                    throw std::invalid_argument("ill-formed of array");
                }
                
                if (*itr == ']') {
                    // End of array
                    return itr + 1;
                }
            }
            
            std::any value;
            itr = read_value(&value, itr, end);
            if (ary_ptr->values.size() > 0) {
                if (ary_ptr->values.front().type() != value.type()) {
                    throw std::invalid_argument("mixed type array");
                }
            }
            ary_ptr->values.push_back(std::move(value));
            is_first = false;
        }
        throw std::invalid_argument("ill-formed of array");
    }
    
    // NOTE: Inline table must be one line
    template <typename T>
    static auto read_inline_table(std::any * value, T itr, T end) -> T {
        if (itr >= end || *itr != '{') {
            throw std::invalid_argument("ill-formed of inline table");
        }
        ++itr;
        itr = skip_ws_within_single_line(itr, end);
        if (itr >= end) {
            throw std::invalid_argument("ill-formed of inline table");
        }
        
        // Inline table
        MJTOML_LOG("inline table\n");
        *value = MJTomlTable();
        auto table = std::any_cast<MJTomlTable>(value);
        auto is_first = true;
        while (itr < end) {
            itr = skip_ws_within_single_line(itr, end);
            if (itr >= end) {
                throw std::invalid_argument("ill-formed of inline table");
            }
            
            if (*itr == '}') {
                // End of inline table
                return itr + 1;
            }
            if (!is_first) {
                if (*itr != ',') {
                    throw std::invalid_argument("ill-formed of inline table");
                }
                ++itr;
                itr = skip_ws_within_single_line(itr, end);
                if (itr >= end) {
                    throw std::invalid_argument("ill-formed of inline table");
                }
                
                if (*itr == '}') {
                    // End of inline table
                    return itr + 1;
                }
            }
            // Dotted keys, includes Bare keys and Quoted keys
            std::vector<std::string> dotted_keys;
            {
                static std::regex const re(R"(^(.*?)[\t ]*=[\t ]*)");
                std::cmatch m;
                
                if (std::regex_search(itr, end, m, re)) {
                    // The itr points the beginning of the value.
                    itr = m[0].second;
                    
                    auto keys = std::string(m[1]);
                    MJTOML_LOG("keys: %s\n", keys.c_str());
                    
                    dotted_keys = parse_keys(keys.cbegin(), keys.cend());
                }
            }
            if (dotted_keys.empty()) {
                throw std::invalid_argument("ill-formed of inline table");
            }
            else {
                MJTomlTable * child_table = table;
                auto value_key = dotted_keys.front();
                for (auto key = dotted_keys.cbegin() + 1; key < dotted_keys.cend(); ++key) {
                    if (child_table->count(value_key) != 0) {
                        if ((*child_table)[value_key].type() == typeid(MJTomlTable)) {
                            child_table = std::any_cast<MJTomlTable>(&(*child_table)[value_key]);
                        }
                        else if ((*child_table)[value_key].type() == typeid(MJTomlArrayParsing)) {
                            auto ary_ptr = std::any_cast<MJTomlArrayParsing>(&(*child_table)[value_key]);
                            child_table = std::any_cast<MJTomlTable>(&ary_ptr->values.back());
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
                itr = read_value(&value, itr, end);
                (*child_table)[value_key] = value;
                is_first = false;
            }
        }
        throw std::invalid_argument("ill-formed of inline table");
    }
    
    // MARK: -
    
    static auto convert_types(MJTomlTable * table) -> void;
    static auto convert_types(MJTomlArray * array) -> void;
    
    static auto convert_types(MJTomlTable * table) -> void {
        for (auto itr = table->begin(); itr != table->end(); ++itr) {
            if (itr->second.type() == typeid(MJTomlTable)) {
                convert_types(std::any_cast<MJTomlTable>(&itr->second));
            }
            else if (itr->second.type() == typeid(MJTomlArrayParsing)) {
                itr->second = std::any_cast<MJTomlArrayParsing>(&itr->second)->values;
                convert_types(std::any_cast<MJTomlArray>(&itr->second));
            }
        }
    }
    
    static auto convert_types(MJTomlArray * array) -> void {
        for (auto itr = array->begin(); itr != array->end(); ++itr) {
            if (itr->type() == typeid(MJTomlTable)) {
                convert_types(std::any_cast<MJTomlTable>(&*itr));
            }
            else if (itr->type() == typeid(MJTomlArrayParsing)) {
                *itr = std::any_cast<MJTomlArrayParsing>(&*itr)->values;
                convert_types(std::any_cast<MJTomlArray>(&*itr));
            }
        }
    }
    
    // MARK: -
    
    static auto string_json(MJTomlTable const & table, int indent, bool is_strict) -> std::string;
    static auto string_json(MJTomlArray const & array, int indent, bool is_strict) -> std::string;
    static auto string_json(std::any const & value, int indent, bool is_strict) -> std::string;
    
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
            ss << string_json(itr->second, indent, is_strict);
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
            ss << string_json(*itr, indent, is_strict);
            joiner = ",\n";
        }
        ss << std::endl << root_space << "]";
        return ss.str();
    }
        
    static auto string_json(std::any const & value, int indent, bool is_strict) -> std::string {
        std::stringstream ss;
        if (value.type() == typeid(MJTomlTable)) {
            ss << string_json(*std::any_cast<MJTomlTable>(&value), indent + 1, is_strict);
        }
        else if (value.type() == typeid(MJTomlArray)) {
            ss << string_json(*std::any_cast<MJTomlArray>(&value), indent + 1, is_strict);
        }
        else if (value.type() == typeid(MJTomlString)) {
            auto str_ptr = std::any_cast<MJTomlString>(&value);
            ss << "\"" << *str_ptr << "\"";
        }
        else if (value.type() == typeid(MJTomlBoolean)) {
            auto bool_ptr = std::any_cast<MJTomlBoolean>(&value);
            ss << (*bool_ptr ? "true" : "false");
        }
        else if (value.type() == typeid(MJTomlInteger)) {
            auto int_ptr = std::any_cast<MJTomlInteger>(&value);
            ss << *int_ptr;
        }
        else if (value.type() == typeid(MJTomlFloat)) {
            auto flt_ptr = std::any_cast<MJTomlFloat>(&value);
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
        else if (value.type() == typeid(MJTomlDescribedFloat)) {
            auto flt_ptr = std::any_cast<MJTomlDescribedFloat>(&value);
            ss << flt_ptr->description;
        }
        else if (value.type() == typeid(MJTomlDateTime)) {
            auto dt_ptr = std::any_cast<MJTomlDateTime>(&value);
            ss << "\"" << dt_ptr->value << "\"";
        }
        return ss.str();
    }
}

namespace MoonJelly {
    
MJToml parse_toml(std::string_view str) {
    MJToml toml;
    ::read_table(&toml.table, str.cbegin(), str.cend(), true);
    // MJTomlArrayParsing => MJTomlArray
    ::convert_types(&toml.table);
    return toml;
}

std::string string_json(MJToml const & toml, int indent, bool is_strict) {
    return ::string_json(toml.table, indent, is_strict);
}

}
