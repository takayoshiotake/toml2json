//
//  main.cpp
//  toml2json
//
//  Created by OTAKE Takayoshi on 2018/07/28.
//

#include <iostream>
#include <fstream>

#include "MJToml.hpp"

int main(int argc, const char * argv[]) {
    if (argc < 2) {
        std::cout << "Usage: toml2json tomlfile" << std::endl;
        return 1;
    }
    
    std::string str;
    {
        std::ifstream ifs(argv[1]);
        if (ifs.fail()) {
            std::cerr << "Error: File not found" << std::endl;
            return 2;
        }
        
        str = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }
    
    auto toml = MoonJelly::parse_toml(str);
    std::cout << MoonJelly::string_json(toml) << std::endl;
    return 0;
}
