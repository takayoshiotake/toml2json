//
//  MJToml.hpp
//  MoonJelly
//
//  Created by OTAKE Takayoshi on 2018/07/28.
//

#pragma once

#include <cfloat>

#if __has_include(<any>)
#include <any>
#else
#include <experimental/any>
namespace std {
    using namespace experimental;
}
#endif

#include <string>
#include <map>
#include <vector>

namespace MoonJelly {
    
    // Value
    using MJTomlString = std::string;
    using MJTomlInteger = std::int64_t;
    using MJTomlFloat = double; // Normal float, excepted inf and nan, will be parsed as MJTomlDescribedFloat
    using MJTomlBoolean = bool;
    
    using MJTomlArray = std::vector<std::any>;
    using MJTomlTable = std::map<std::string, std::any>;
    
    struct MJTomlDescribedFloat {
        MJTomlFloat value;
        std::string description; // This will keep the original string as possible, is compatible with JSON string
    };
    
    struct MJToml {
        MJTomlTable table;
    };
    
    extern MJToml parse_toml(std::string_view str);
    extern std::string string_json(MJToml const & toml, int indent = 0, bool is_strict = true);
    
}
