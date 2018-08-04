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
    using MJTomlFloat = double;
    using MJTomlBoolean = bool;
    
    using MJTomlArray = std::vector<std::any>;
    using MJTomlTable = std::map<std::string, std::any>;
    
    extern MJTomlTable parse_toml(std::string_view str);
    extern std::string string_json(MJTomlTable const & toml, int indent = 0);
    
}
