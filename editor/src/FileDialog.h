#pragma once

#include <string>

struct GLFWwindow;

namespace forge {

// Native Win32 open-file dialog. Returns empty string on cancel.
// filter format: "Display Name\0*.ext;*.ext2\0" (Win32 double-NUL convention).
std::string OpenFileDialog(GLFWwindow* owner, const char* filter);

// Native Win32 save-file dialog. Returns empty string on cancel.
// defaultExt: appended when the user types no extension (no leading dot, e.g. "stl").
std::string SaveFileDialog(GLFWwindow* owner, const char* filter, const char* defaultExt);

} // namespace forge
