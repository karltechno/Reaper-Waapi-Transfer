#include "ImGuiHandler.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "examples/imgui_impl_opengl3.h"

namespace ImGuiHandler
{
static GLFWwindow* g_window;

static const char* ImGui_GetClipboardText(void* user_data)
{
	return glfwGetClipboardString((GLFWwindow*)user_data);
}

static void ImGui_SetClipboardText(void* user_data, const char* text)
{
	glfwSetClipboardString((GLFWwindow*)user_data, text);
}

static bool g_MouseJustPressed[ImGuiMouseButton_COUNT] = {};

static void ImGui_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(g_MouseJustPressed))
		g_MouseJustPressed[button] = true;
}

static void ImGui_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH += (float)xoffset;
	io.MouseWheel += (float)yoffset;
}

static void ImGui_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (action == GLFW_PRESS)
		io.KeysDown[key] = true;
	if (action == GLFW_RELEASE)
		io.KeysDown[key] = false;

	// Modifiers are not reliable across systems
	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
	io.KeySuper = false;
}

static void ImGui_CharCallback(GLFWwindow* window, unsigned int c)
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(c);
}

static void ImGui_ImplGlfw_UpdateMousePosAndButtons()
{
	// Update buttons
	ImGuiIO& io = ImGui::GetIO();
	for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
	{
		// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
		io.MouseDown[i] = g_MouseJustPressed[i] || glfwGetMouseButton(g_window, i) != 0;
		g_MouseJustPressed[i] = false;
	}

	// Update mouse position
	const ImVec2 mouse_pos_backup = io.MousePos;
	io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	const bool focused = GetForegroundWindow() == glfwGetWin32Window(g_window);

	if (focused)
	{
		if (io.WantSetMousePos)
		{
			glfwSetCursorPos(g_window, (double)mouse_pos_backup.x, (double)mouse_pos_backup.y);
		}
		else
		{
			double mouse_x, mouse_y;
			glfwGetCursorPos(g_window, &mouse_x, &mouse_y);
			io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
		}
	}
}

// TODO
/*
static void ImGui_ImplGlfw_UpdateMouseCursor()
{
	ImGuiIO& io = ImGui::GetIO();
	if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(g_glfwWindow, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
		return;

	ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
	if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
	{
		// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
		glfwSetInputMode(g_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}
	else
	{
		// Show OS mouse cursor
		// FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
		glfwSetCursor(g_glfwWindow, g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
		glfwSetInputMode(g_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}
*/

bool InitWindow(char const* windowName, int width, int height)
{
	if (glfwInit() != GLFW_TRUE)
	{
		// TODO: Error
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, windowName, nullptr, nullptr);

	if (!window)
	{
		return false;
	}

	glfwMakeContextCurrent(window);

	if (gladLoadGL() == 0)
	{
		glfwDestroyWindow(window);
		return false;
	}

	ImGui::CreateContext();

	if (!ImGui_ImplOpenGL3_Init())
	{
		glfwDestroyWindow(window);
		return false;
	}

	g_window = window;

	// Setup ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.SetClipboardTextFn = ImGui_SetClipboardText;
	io.GetClipboardTextFn = ImGui_GetClipboardText;
	// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
	io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
	io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
	io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
	io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
	io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
	io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
	io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
	io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
	io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
	io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
	io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
	io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

	io.ImeWindowHandle = (void*)glfwGetWin32Window(g_window);
	glfwSetMouseButtonCallback(g_window, ImGui_MouseButtonCallback);
	glfwSetScrollCallback(g_window, ImGui_ScrollCallback);
	glfwSetKeyCallback(g_window, ImGui_KeyCallback);
	glfwSetCharCallback(g_window, ImGui_CharCallback);

	glfwShowWindow(g_window);

	return true;
}

void ShutdownWindow()
{
	glfwDestroyWindow(g_window);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
}

GLFWwindow* GetGLFWWindow()
{
	return g_window;
}

HWND GetNativeWindow()
{
	if (g_window)
	{
		return glfwGetWin32Window(g_window);
	}

	return 0;
}

void BringToForeground()
{
	if (g_window)
	{
		SetForegroundWindow(glfwGetWin32Window(g_window));
	}
}

void BeginFrame()
{
	// Setup display size (every frame to accommodate for window resizing)
	ImGuiIO& io = ImGui::GetIO();

	int w, h;
	int display_w, display_h;
	glfwGetWindowSize(g_window, &w, &h);
	glfwGetFramebufferSize(g_window, &display_w, &display_h);
	io.DisplaySize = ImVec2((float)w, (float)h);
	if (w > 0 && h > 0)
	{
		io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);
	}

	io.DeltaTime = 1.0f / 30.0f; // todo

	glfwPollEvents();
	ImGui_ImplGlfw_UpdateMousePosAndButtons();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
}

void EndFrame()
{
	ImGui::Render();

	glClearColor(0, 0, 0, ~0);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(g_window);

}


}