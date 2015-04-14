#include <stdio.h>
#include <windows.h>
#include "resources.h"

int pal;
int fps, changefps, spcclck;
double spcclck2, spcclck3;
int drawcount;
/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);

/*  Make the class name into a global variable  */
char szClassName[] = "WindowsApp";
HWND ghwnd;
int infocus = 1;
int romloaded = 0;

FILE* snemlogf;
void snemlog(const char* format, ...)
{
	char buf[256];
	return;
	if (!snemlogf)
		snemlogf = fopen("snemlog.txt", "wt");
	// return;
	va_list ap;
	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	fputs(buf, snemlogf);
	fflush(snemlogf);
}
void resizewindow(int xsize, int ysize)
{
	RECT r;
	GetWindowRect(ghwnd, &r);
	MoveWindow(ghwnd, r.left, r.top,
			   xsize + (GetSystemMetrics(SM_CXFIXEDFRAME) * 2) + 2,
			   ysize + (GetSystemMetrics(SM_CYFIXEDFRAME) * 2) +
				   GetSystemMetrics(SM_CYMENUSIZE) +
				   GetSystemMetrics(SM_CYCAPTION) + 3,
			   TRUE);
}

int quited = 0;
int soundrunning = 0;
static HANDLE soundobject;
void soundthread(PVOID pvoid)
{
	soundrunning = 1;
	while (!quited) {
		WaitForSingleObject(soundobject, 100);
		pollsound();
		//                sleep(1);
	}
	soundrunning = 0;
}

void wakeupsoundthread()
{
	SetEvent(soundobject);
}

void closesoundthread()
{
	int c = 0;
	wakeupsoundthread();
	while (soundrunning && c < 500) {
		sleep(1);
		c++;
	}
}

static HANDLE frameobject;
void wakeupmainthread()
{
	SetEvent(frameobject);
}

int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance,
				   LPSTR lpszArgument, int nFunsterStil)

{
	char s[256];
	int soundframe = 0;
	int d = 0;
	HWND hwnd;		  /* This is the handle for our window */
	MSG messages;	 /* Here messages to the application are saved */
	WNDCLASSEX wincl; /* Data structure for the windowclass */

	/* The Window structure */
	wincl.hInstance = hThisInstance;
	wincl.lpszClassName = szClassName;
	wincl.lpfnWndProc =
		WindowProcedure;	  /* This function is called by windows */
	wincl.style = CS_DBLCLKS; /* Catch double-clicks */
	wincl.cbSize = sizeof(WNDCLASSEX);

	/* Use default icon and mouse-pointer */
	wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
	wincl.lpszMenuName = NULL; /* No menu */
	wincl.cbClsExtra = 0;	  /* No extra bytes after the window class */
	wincl.cbWndExtra = 0;	  /* structure or the window instance */
	/* Use Windows's default color as the background of the window */
	wincl.hbrBackground = (HBRUSH)COLOR_BACKGROUND;

	/* Register the window class, and if it fails quit the program */
	if (!RegisterClassEx(&wincl))
		return 0;

	/* The class is registered, let's create the program */
	ghwnd = hwnd = CreateWindowEx(
		0,					 /* Extended possibilites for variation */
		szClassName,		 /* Classname */
		"NeuSneM",			 /* Title Text */
		WS_OVERLAPPEDWINDOW, /* default window */
		CW_USEDEFAULT,		 /* Windows decides the position */
		CW_USEDEFAULT,		 /* where the window ends up on the screen */
		512 + (GetSystemMetrics(SM_CXFIXEDFRAME) * 2) +
			1, /* The programs width */
		448 + (GetSystemMetrics(SM_CYFIXEDFRAME) * 2) +
			GetSystemMetrics(SM_CYMENUSIZE) + GetSystemMetrics(SM_CYCAPTION) +
			2,		  /* and height in pixels */
		HWND_DESKTOP, /* The window is a child-window to desktop */
		LoadMenu(hThisInstance, TEXT("MainMenu")), /* No menu */
		hThisInstance, /* Program Instance handler */
		NULL		   /* No Window Creation data */
		);
	/* Make the window visible on the screen */
	ShowWindow(hwnd, nFunsterStil);
	win_set_window(hwnd);
	timeBeginPeriod(1);
	initsnem();
	_beginthread(soundthread, 0, NULL);
	soundobject = CreateEvent(NULL, FALSE, FALSE, NULL);
	//        atexit(closesoundthread);
	frameobject = CreateEvent(NULL, FALSE, FALSE, NULL);
	resetsnem();
	//        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	/* Run the message loop. It will run until GetMessage() returns 0 */
	while (!quited) {
		/*                if (soundframe>=((pal)?5:6))
						{
								soundframe-=((pal)?5:6);
								refillbuffer();
								snemlog("Buffer refill\n");
						} */
		if (!drawcount)
			WaitForSingleObject(frameobject, 100);
		if (infocus && romloaded) // && drawcount)
		{
			//                        snemlog("Drawcount %i\n",drawcount);
			while (drawcount) {
				execframe();
				drawcount--;
				soundframe++;
			}
		}
		//                else
		//                   sleep(1);
		if (changefps) {
			if (romloaded)
				sprintf(s, "NeuSneM v0.1 - %i fps", fps);
			else
				sprintf(s, "NeuSneM v0.1");
			set_window_title(s);
			changefps = 0;
		}
		//                sleep(0);
		if (PeekMessage(&messages, NULL, 0, 0, PM_REMOVE)) {
			if (messages.message == WM_QUIT)
				quited = 1;
			/* Translate virtual-key messages into character messages */
			TranslateMessage(&messages);
			/* Send message to WindowProcedure */
			DispatchMessage(&messages);
		}
	}
	wakeupsoundthread();
	while (soundrunning) {
		sleep(1);
	}
	timeEndPeriod(1);
	/* The program return-value is 0 - The value that PostQuitMessage() gave */
	return messages.wParam;
}

char openfilestring[260];
int getfile(HWND hwnd, char* f)
{
	OPENFILENAME ofn; // common dialog box structure
	char szFile[260]; // buffer for file name

	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = openfilestring;
	//
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not
	// use the contents of szFile to initialize itself.
	//
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(openfilestring);
	ofn.lpstrFilter = f; //"All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	// Display the Open dialog box.

	if (GetOpenFileName(&ofn))
		return 0;
	return 1;
}

LRESULT CALLBACK
WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HMENU hmenu;
	switch (message) /* handle the messages */
	{
	case WM_DESTROY:
		PostQuitMessage(0); /* send a WM_QUIT to the message queue */
		break;
	case WM_SETFOCUS:
		infocus = 1;
		break;
	case WM_KILLFOCUS:
		infocus = 0;
		break;
	case WM_COMMAND:
		hmenu = GetMenu(hwnd);
		switch (LOWORD(wParam)) {
		case IDM_FILE_RESET:
			resetsnem();
			return 0;
		case IDM_FILE_LOAD:
			if (!getfile(hwnd,
						 "ROM image (*.SMC)\0*.SMC\0All files (*.*)\0*.*\0")) {
				romloaded = 1;
				loadrom(openfilestring);
				resetsnem();
				drawcount = 0;
			}
			return 0;
		case IDM_FILE_EXIT:
			PostQuitMessage(0);
			return 0;
		}
	default: /* for messages that we don't deal with */
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}
