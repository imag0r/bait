// fwdexports.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <filesystem>
#include <fstream>
#include <iostream>

#include "pe_bliss.h"

namespace stdfs = std::experimental::filesystem;

std::string get_filename(const std::string& path, bool with_extension = true)
{
    auto filename = stdfs::path(path).filename();
    if (!with_extension)
    {
        filename.replace_extension();
    }
    return filename.string();
}

void forward_exports(const std::string& stub_dll, const std::string& source_dll, const std::string& output_dll, const std::string& fwd_path)
{
    std::ifstream source;
    source.exceptions(std::ios::badbit | std::ios::failbit);
    source.open(source_dll, std::ios::in | std::ios::binary);

    // read source exports
    auto source_pe = pe_bliss::pe_factory::create_pe(source);
    pe_bliss::export_info info;
    auto exports = pe_bliss::get_exported_functions(source_pe, info);
    // convert them to forwarders
    for (auto& exp : exports)
    {
        exp.set_forwarded_name(fwd_path + "." + exp.get_name());
    }

    std::ifstream stub;
    stub.exceptions(std::ios::badbit | std::ios::failbit);
    stub.open(stub_dll, std::ios::in | std::ios::binary);
    
    auto stub_pe = pe_bliss::pe_factory::create_pe(stub);
    pe_bliss::section section;
    section.get_raw_data().resize(1);
    section.set_name(".fwd");
    section.readable(true);
    auto& attached = stub_pe.add_section(section);

    pe_bliss::rebuild_exports(stub_pe, info, exports, attached);

    std::ofstream output;
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.open(output_dll, std::ios::out | std::ios::binary | std::ios::trunc);

    pe_bliss::rebuild_pe(stub_pe, output);
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 4 || argc > 5)
        {
            throw std::runtime_error("Usage: fwdexports.exe <stub.dll> <source.dll> <output.dll> [fwd_path]");
        }

        const std::string stub_dll = argv[1];
        const std::string source_dll = argv[2];
        const std::string output_dll = argv[3];
        const std::string fwd_path = argc > 4 ? argv[4] : get_filename(source_dll, false);

        forward_exports(stub_dll, source_dll, output_dll, fwd_path);

        return 0;
    }
    catch (const std::exception& exc)
    {
        std::cerr << exc.what() << std::endl;
        return 1;
    }
}
