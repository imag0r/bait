// gendef.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <fstream>
#include <iostream>
#include <regex>

int wmain(int argc, wchar_t* argv[])
{
    const auto dll_name_regex = std::regex("^Dump of file (.+)$", std::wregex::icase | std::wregex::optimize);
    const auto function_name_regex = std::regex("^\\s+\\d+\\s+[0-9a-f]+\\s+(?:[0-9a-f]+)?\\s(\\S+).*$", std::wregex::icase | std::wregex::optimize);

    std::string dll_path;
    std::string line;

    std::ifstream ifs(argv[1]);

    std::cout << "#pragma once\n";

    while (std::getline(ifs, line))
    {
        std::smatch match;
        if (std::regex_match(line, match, dll_name_regex))
        {
            dll_path = match[1].str();
            // strip extension
            const auto dot_pos = dll_path.rfind('.');
            if (dot_pos != dll_path.npos)
            {
                dll_path.resize(dot_pos);
            }
            std::replace(std::begin(dll_path), std::end(dll_path), '\\', '/');
            continue;
        }
        
        if (dll_path.empty())
        {
            continue;
        }

        if (std::regex_match(line, match, function_name_regex))
        {
            const auto function_name = match[1].str();

            std::cout << "#pragma comment(linker, \"/export:" << function_name << "=" << dll_path << "." << function_name << "\")\n";
        }
    }

    return 0;
}
