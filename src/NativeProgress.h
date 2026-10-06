// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
struct CDecompressStat;
struct CHashBundle;
#include <cstdint>
#include <string>
namespace PortProgress {
void Completion(const CDecompressStat *statistics, const CHashBundle *hash) noexcept;
bool Enabled() noexcept;
void Begin(const char *mode, const wchar_t *name = nullptr, bool preserveFileTotal = false) noexcept;
void Current(const wchar_t *name, bool directory) noexcept;
void Status(const char *status, bool force = false) noexcept;
void Scanning(std::uint64_t files, std::uint64_t bytes, const wchar_t *path, bool directory) noexcept;
void UpdateOperation(unsigned operation, const wchar_t *name, bool directory) noexcept;
void Error(int code, bool encrypted, const wchar_t *name, const wchar_t *message = nullptr) noexcept;
void Total(std::uint64_t size) noexcept;
void Completed(const std::uint64_t *size) noexcept;
void Ratio(const std::uint64_t *input, const std::uint64_t *output) noexcept;
void Files(std::uint64_t count) noexcept;
void FileDone() noexcept;
void Finish() noexcept;
void Record(const std::string &json) noexcept;
}
