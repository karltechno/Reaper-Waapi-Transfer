#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct GLFWwindow;

namespace ImGuiHandler
{

bool InitWindow(char const* windowName, int width, int height);
void ShutdownWindow();

GLFWwindow* GetGLFWWindow();
HWND GetNativeWindow();

void BeginFrame();
void EndFrame();
}

