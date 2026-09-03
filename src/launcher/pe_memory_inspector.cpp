#include "pe_memory_inspector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace penumbra_vr::launcher {
namespace {

template <typename T>
bool ReadRemote(HANDLE process, std::uintptr_t address, T& value, std::wstring& error) {
    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            process,
            reinterpret_cast<const void*>(address),
            &value,
            sizeof(value),
            &bytes_read) ||
        bytes_read != sizeof(value)) {
        error = L"ReadProcessMemory failed with Win32 error " + std::to_wstring(GetLastError());
        return false;
    }
    return true;
}

double ShannonEntropy(const std::uint8_t* bytes, std::size_t size) {
    if (size == 0) {
        return 0.0;
    }

    std::array<std::size_t, 256> frequencies{};
    for (std::size_t index = 0; index < size; ++index) {
        ++frequencies[bytes[index]];
    }

    double entropy = 0.0;
    for (const std::size_t frequency : frequencies) {
        if (frequency == 0) {
            continue;
        }
        const double probability = static_cast<double>(frequency) / static_cast<double>(size);
        entropy -= probability * std::log2(probability);
    }
    return entropy;
}

bool IsTextSection(const IMAGE_SECTION_HEADER& section) {
    constexpr char kText[] = ".text";
    return std::memcmp(section.Name, kText, sizeof(kText) - 1) == 0;
}

} // namespace

bool InspectRemoteTextSection(
    HANDLE process,
    std::uintptr_t module_base,
    const std::filesystem::path& executable_path,
    const std::filesystem::path* reconstructed_image_path,
    TextSectionInspection& inspection,
    std::wstring& error) {
    inspection = {};
    error.clear();

    IMAGE_DOS_HEADER dos{};
    if (!ReadRemote(process, module_base, dos, error) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        if (error.empty()) {
            error = L"The remote module has no valid DOS header";
        }
        return false;
    }

    DWORD signature = 0;
    const std::uintptr_t nt_address = module_base + static_cast<std::uint32_t>(dos.e_lfanew);
    if (!ReadRemote(process, nt_address, signature, error) || signature != IMAGE_NT_SIGNATURE) {
        if (error.empty()) {
            error = L"The remote module has no valid PE signature";
        }
        return false;
    }

    IMAGE_FILE_HEADER file_header{};
    if (!ReadRemote(process, nt_address + sizeof(signature), file_header, error)) {
        return false;
    }
    if (file_header.NumberOfSections == 0 || file_header.NumberOfSections > 96) {
        error = L"The remote module has an unreasonable section count";
        return false;
    }

    const std::uintptr_t section_table = nt_address + sizeof(signature) +
        sizeof(IMAGE_FILE_HEADER) + file_header.SizeOfOptionalHeader;
    IMAGE_SECTION_HEADER text_section{};
    bool found_text = false;
    for (WORD index = 0; index < file_header.NumberOfSections; ++index) {
        IMAGE_SECTION_HEADER section{};
        if (!ReadRemote(
                process,
                section_table + static_cast<std::uintptr_t>(index) * sizeof(section),
                section,
                error)) {
            return false;
        }
        if (IsTextSection(section)) {
            text_section = section;
            found_text = true;
            break;
        }
    }
    if (!found_text) {
        error = L"The remote module has no .text section";
        return false;
    }

    constexpr std::uint32_t kMaximumSectionSize = 64U * 1024U * 1024U;
    if (text_section.Misc.VirtualSize == 0 || text_section.Misc.VirtualSize > kMaximumSectionSize ||
        text_section.SizeOfRawData == 0 || text_section.SizeOfRawData > kMaximumSectionSize) {
        error = L"The .text section size is invalid or exceeds the inspection limit";
        return false;
    }

    std::vector<std::uint8_t> memory_bytes(text_section.Misc.VirtualSize);
    SIZE_T memory_bytes_read = 0;
    if (!ReadProcessMemory(
            process,
            reinterpret_cast<const void*>(module_base + text_section.VirtualAddress),
            memory_bytes.data(),
            memory_bytes.size(),
            &memory_bytes_read) ||
        memory_bytes_read != memory_bytes.size()) {
        error = L"Could not read the complete remote .text section (Win32 error " +
            std::to_wstring(GetLastError()) + L")";
        return false;
    }

    std::ifstream file(executable_path, std::ios::binary);
    if (!file) {
        error = L"Could not open the executable for an on-disk comparison";
        return false;
    }
    file.seekg(text_section.PointerToRawData, std::ios::beg);
    std::vector<std::uint8_t> disk_bytes(text_section.SizeOfRawData);
    file.read(reinterpret_cast<char*>(disk_bytes.data()), static_cast<std::streamsize>(disk_bytes.size()));
    if (file.gcount() != static_cast<std::streamsize>(disk_bytes.size())) {
        error = L"Could not read the complete on-disk .text section";
        return false;
    }

    if (reconstructed_image_path != nullptr) {
        file.clear();
        file.seekg(0, std::ios::end);
        const std::streamoff file_size = file.tellg();
        if (file_size <= 0 ||
            static_cast<std::uint64_t>(file_size) > std::numeric_limits<std::uint32_t>::max()) {
            error = L"The executable size is invalid for a PE32 analysis image";
            return false;
        }
        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> image_bytes(static_cast<std::size_t>(file_size));
        file.read(
            reinterpret_cast<char*>(image_bytes.data()),
            static_cast<std::streamsize>(image_bytes.size()));
        if (file.gcount() != static_cast<std::streamsize>(image_bytes.size())) {
            error = L"Could not read the complete executable for reconstruction";
            return false;
        }

        const std::size_t copy_size = std::min(memory_bytes.size(), disk_bytes.size());
        const std::size_t raw_offset = text_section.PointerToRawData;
        if (raw_offset > image_bytes.size() || copy_size > image_bytes.size() - raw_offset) {
            error = L"The .text raw-data range exceeds the executable file";
            return false;
        }
        std::copy_n(memory_bytes.begin(), copy_size, image_bytes.begin() + raw_offset);

        std::error_code directory_error;
        const std::filesystem::path parent = reconstructed_image_path->parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, directory_error);
        }
        if (directory_error) {
            error = L"Could not create the analysis-image directory (error " +
                std::to_wstring(directory_error.value()) + L")";
            return false;
        }

        HANDLE output = CreateFileW(
            reconstructed_image_path->c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (output == INVALID_HANDLE_VALUE) {
            error = L"Could not create a new reconstructed analysis image (Win32 error " +
                std::to_wstring(GetLastError()) + L")";
            return false;
        }

        DWORD bytes_written = 0;
        const bool wrote_image = WriteFile(
            output,
            image_bytes.data(),
            static_cast<DWORD>(image_bytes.size()),
            &bytes_written,
            nullptr) != FALSE;
        CloseHandle(output);
        if (!wrote_image || bytes_written != static_cast<DWORD>(image_bytes.size())) {
            error = L"Could not write the complete reconstructed analysis image";
            DeleteFileW(reconstructed_image_path->c_str());
            return false;
        }
    }

    const std::size_t compare_size = std::min(memory_bytes.size(), disk_bytes.size());
    std::size_t different_bytes = 0;
    std::size_t first_difference = std::numeric_limits<std::size_t>::max();
    for (std::size_t index = 0; index < compare_size; ++index) {
        if (memory_bytes[index] != disk_bytes[index]) {
            if (first_difference == std::numeric_limits<std::size_t>::max()) {
                first_difference = index;
            }
            ++different_bytes;
        }
    }

    inspection.rva = text_section.VirtualAddress;
    inspection.virtual_size = text_section.Misc.VirtualSize;
    inspection.raw_size = text_section.SizeOfRawData;
    inspection.compared_bytes = compare_size;
    inspection.different_bytes = different_bytes;
    inspection.first_difference_rva = first_difference == std::numeric_limits<std::size_t>::max()
        ? 0
        : text_section.VirtualAddress + static_cast<std::uint32_t>(first_difference);
    inspection.disk_entropy = ShannonEntropy(disk_bytes.data(), compare_size);
    inspection.memory_entropy = ShannonEntropy(memory_bytes.data(), compare_size);
    return true;
}

} // namespace penumbra_vr::launcher
