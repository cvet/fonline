#include<windows.h>

int APIENTRY WinMain(HINSTANCE hInstance,
		     HINSTANCE hPrevInstance,
		     LPSTR lpszCmdLine,
		     int nCmdShow) {
    HICON hIcon;
    hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    return hIcon ? 0 : 1;
}
