#pragma once

#include <stdexcept>
#include <filesystem>
#include <cstdio>
#include "windows_sane.h"

namespace test {

  class FuncFailed : public std::runtime_error
  {
  public:
    FuncFailed(const char* func)
      : std::runtime_error(msg(func)) {}
    FuncFailed(const char* func, unsigned long res)
      : std::runtime_error(msg(func, nullptr, &res)) {}
    FuncFailed(const char* func, const char* arg1)
      : std::runtime_error(msg(func, arg1)) {}
    FuncFailed(const char* func, const char* arg1, unsigned long res)
      : std::runtime_error(msg(func, arg1, &res)) {}
    FuncFailed(const char* func, const char* what, const char* arg1)
      : std::runtime_error(msg(func, arg1, nullptr, what)) {}
    FuncFailed(const char* func, const char* what, const char* arg1, unsigned long res)
      : std::runtime_error(msg(func, arg1, &res, what)) {}

  private:
    std::string msg(const char* func, const char* arg1 = nullptr, const unsigned long* res = nullptr, const char* what = nullptr);
  };

  class WinFuncFailed : public std::runtime_error
  {
  public:
    using runtime_error::runtime_error;
  };

  class WinFuncFailedGenerator
  {
  public:
    WinFuncFailedGenerator(DWORD gle = GetLastError()) : m_gle(gle) {}
    WinFuncFailedGenerator(const WinFuncFailedGenerator&) = delete;

    DWORD lastError() const { return m_gle; }

    WinFuncFailed operator()(const char* func);
    WinFuncFailed operator()(const char* func, unsigned long res);
    WinFuncFailed operator()(const char* func, const char* arg1);
    WinFuncFailed operator()(const char* func, const char* arg1, unsigned long res);

  private:
    DWORD m_gle;
  };

  // trick to guarantee the evalutation of GetLastError() before the evalution of the parameters to the WinFuncFailed message generation
#define throw_testWinFuncFailed(...) do { ::test::WinFuncFailedGenerator exceptionGenerator; throw exceptionGenerator(__VA_ARGS__); } while (false)

  class ScopedFILE {
  public:
    ScopedFILE(FILE* f = nullptr) : m_f(f) {}
    ScopedFILE(const ScopedFILE&) = delete;
    ScopedFILE(ScopedFILE&& other) : m_f(other.m_f) { other.m_f = nullptr; }
    ~ScopedFILE() { if (m_f) fclose(m_f); }

    void close() { if (m_f) { fclose(m_f); m_f = nullptr; } }

    operator bool() const { return m_f; }
    operator FILE*() const { return m_f; }
    operator FILE**() { return &m_f; }
  private:
    FILE* m_f;
  };

  using std::experimental::filesystem::path;

  // path functions assume they are called by a test executable
  // (calculate the requested path relative to the current executable path)

  path path_of_test_bin(const path& relative = path());
  path path_of_test_temp(const path& relative = path());
  path path_of_test_fixtures(const path& relative = path());
  path path_of_usvfs_lib(const path& relative = path());

  std::string platform_dependant_executable(const char* name, const char* ext = "exe", const char* platform = nullptr);

  // if full_path is a subfolder of base returns only the relative path,
  // if full_path and base are the same folder "." is returned,
  // otherwise full_path is returned unchanged
  path path_as_relative(const path& base, const path& full_path);

  std::vector<char> read_small_file(const path& file, bool binary = true);

  // true iff the the contents of the two files is exactly the same
  bool compare_files(const path& file1, const path& file2, bool binary = true);

  // return true iff the given path is an empty (optionally true also if path doesn't exist)
  bool is_empty_folder(const path& dpath, bool or_doesnt_exist = false);

  void delete_file(const path& file);

  // Recursively deletes the given path and all the files and directories under it
  // Use with care!!!
  void recursive_delete_files(const path& dpath);

  // Recursively copies all files and directories from srcPath to destPath
  void recursive_copy_files(const path& src_path, const path& dest_path, bool overwrite);

  class ScopedLoadLibrary {
  public:
    ScopedLoadLibrary(const wchar_t* dll_path);
    ~ScopedLoadLibrary();

    // returns zero if load library failed
    operator HMODULE() const { return m_mod; }

  private:
    HMODULE m_mod;
  };
};