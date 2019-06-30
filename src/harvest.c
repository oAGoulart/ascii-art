/*
 * Copyright 2019 José Augusto dos Santos Goulart
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* platform specific stuff */
#if defined(_WIN32) || defined(_WIND64) || defined(__MINGW32__) || defined (__MINGW64__)
	#define WINDOWS
	
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	/* NOTE: link Gdi_32.lib on windows */
	#include <conio.h>
	#include <wincon.h>

	typedef DWORD console_mode_t;
#else /* assume POSIX */
	#include <linux/fb.h>
	#include <sys/ioctl.h>
	#include <sys/select.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <sys/mman.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <termios.h>

	typedef struct termios console_mode_t;
#endif

/* define data types */
#if defined(__ILP32__) || defined(_ILP32) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)
	#define X86

	typedef uint32_t ulong_t;
#else /* assume 64-bit */
	typedef uint64_t ulong_t;
#endif

typedef unsigned char ubyte_t;

/* define struct types */
typedef struct position_s
{
	size_t x;
	size_t y;
} position_t;

typedef struct console_s
{
	console_mode_t mode;         /* current mode */
	console_mode_t old_mode;     /* previous mode */
	position_t     size;         /* console size (columns X rows) */
	double         aspect_ratio; /* aspect ratio from console size */
} console_t;

typedef struct framebuffer_s
{
#ifdef WINDOWS
	HWND       window;             /* window handler */
	HDC        device;             /* device handler */
	position_t resolution;         /* window resolution in pixels */
	LONG_PTR   orig_win_style;     /* original window style */
	LONG_PTR   win_style;          /* current window style */
	LONG_PTR   orig_win_ext_style; /* original window extended style */
	LONG_PTR   win_ext_style;      /* current window extended style */
#else
	int                      fd;            /* file descriptor */
	ubyte_t*                 address;       /* buffer address on memory */
	size_t                   size;          /* buffer size on memory */
	struct fb_var_screeninfo orig_var_info; /* screen original variable information */
	struct fb_var_screeninfo var_info;      /* screen current variable information */
	struct fb_fix_screeninfo fix_info;      /* screen fixed information */
#endif
} framebuffer_t;


/* get char from stdin without blocking */
int get_char()
{
	char result = EOF;

#ifdef WINDOWS
	/* verify keyboard was hit */
	if (_kbhit()) /* FIXME: having some delay */
		result = _getch();
#else
	/* define select() values */
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	struct timeval timeout = { 0, 0 };

	/* verify stdin is not empty */
	if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout))
		result = getchar();
#endif

	return result;
}

/* initialize screen frame buffer */
bool init_screen_framebuffer(framebuffer_t* fb)
{
	bool success = false;

	if (fb != NULL) {
#ifdef WINDOWS
		/* get window and device */
		if ((fb->window = GetConsoleWindow()) != NULL) {
			if ((fb->device = GetDC(fb->window)) != NULL) {
				/* get original window styles */
				fb->orig_win_style = GetWindowLongPtr(fb->window, GWL_STYLE);
				fb->orig_win_ext_style = GetWindowLongPtr(fb->window, GWL_EXSTYLE);

				/* modify styles */
				fb->win_style = fb->orig_win_style;
				fb->win_style &= ~WS_BORDER;
				fb->win_style &= ~WS_DLGFRAME;
				fb->win_style &= ~WS_THICKFRAME;

				fb->win_ext_style = fb->orig_win_ext_style;
				fb->win_ext_style &= ~WS_EX_WINDOWEDGE;

				SetWindowLongPtr(fb->window, GWL_STYLE, fb->win_style | WS_POPUP);
				SetWindowLongPtr(fb->window, GWL_EXSTYLE, fb->win_ext_style | WS_EX_TOPMOST);

				/* get screen size */
				fb->resolution.x = GetSystemMetrics(SM_CXSCREEN);
				fb->resolution.y = GetSystemMetrics(SM_CYSCREEN);

				/* resize window (windowed full screen) */
				success = (MoveWindow(fb->window, 0, 0, fb->resolution.x, fb->resolution.y, true) && ShowWindow(fb->window, SW_SHOWMAXIMIZED));
			}
		}

#else
		/* set system default frame buffer */
		system("export FRAMEBUFFER=/dev/fb0");

		/* open frame buffer file */
		if ((fb->fd = open("/dev/fb0", O_RDWR)) != -1) {
			if (!ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->fix_info)) {
				if (!ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->var_info)) {
					/* make copy of original variable information */
					memcpy(&fb->orig_var_info, &fb->var_info, sizeof(struct fb_var_screeninfo));

					/* change buffer info */
					fb->var_info.bits_per_pixel = 8;
					fb->var_info.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;

					if (!ioctl(fb->fd, FBIOPUT_VSCREENINFO, &fb->var_info)) {
						/* get size of buffer */
						fb->size = fb->fix_info.smem_len;

						/* map buffer to memory */
						fb->address = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);

						success = (fb->address != NULL);
					}
				}
			}
		}
#endif
	}

	return success;
}

/* terminate screen frame buffer */
void terminate_screen_framebuffer(framebuffer_t* fb)
{
	if (fb != NULL) {
#ifdef WINDOWS
		if (fb->window != NULL) {
			SetWindowLongPtr(fb->window, GWL_STYLE, fb->orig_win_style);
			SetWindowLongPtr(fb->window, GWL_EXSTYLE, fb->orig_win_ext_style);
			ShowWindow(fb->window, SW_SHOWDEFAULT);

			if (fb->device != NULL) {
				ReleaseDC(fb->window, fb->device);
				DeleteDC(fb->device);
			}
		}
#else
		if (fb->fd)
			fb->orig_var_info.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
			ioctl(fb->fd, FBIOPUT_VSCREENINFO, &fb->orig_var_info);
			close(fb->fd);
		if (fb->address != NULL) {
			munmap(fb->address, fb->size);
			fb->address = NULL;
		}

		/* get current output display */
		char display[64] = "VGA1"; /* TODO: make this get the primary output source */

		/* refresh display output */
		char cmd[256];
		sprintf(cmd, "xrandr --output %s --off && xrandr --output %s --auto", display, display);
		system(cmd);
#endif
	}
}

/* change terminal mode and return old one */
console_mode_t set_cli_input_mode(const console_mode_t mode)
{
	console_mode_t old_mode;

#ifdef WINDOWS
	HANDLE console = GetStdHandle(STD_INPUT_HANDLE);

	if (GetConsoleMode(console, &old_mode))
		SetConsoleMode(console, mode);
#else
	tcgetattr(STDIN_FILENO, &old_mode);
	tcsetattr(STDIN_FILENO, TCSANOW, &mode);
#endif

	return old_mode;
}

/* change cli input cursor status */
void show_cli_input_cursor(const bool show)
{
#ifdef WINDOWS
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_CURSOR_INFO info = (show) ? (CONSOLE_CURSOR_INFO){ 25, true } : (CONSOLE_CURSOR_INFO){ 1, false };
	SetConsoleCursorInfo(console, &info);
#else
	if (show)
		printf("\e[?25h");
	else
		printf("\e[?25l");
#endif
}

/* get cli cursor position */
void set_cli_cursor_pos(const size_t column, const size_t row)
{
#ifdef WINDOWS
	COORD position = { (short)column, (short)row };

	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), position);
#else
	printf("\033[%d;%dH", row, column);
#endif
}

/* get cli size */
position_t get_cli_size()
{
	position_t size = { 0, 0 };

#ifdef WINDOWS
	CONSOLE_SCREEN_BUFFER_INFO info;

	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
		size.x = info.dwSize.X;
		size.y = info.dwSize.Y;
	}
#else
	struct winsize win_sz;
	
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win_sz) != -1) {
		size.x = win_sz.ws_col;
		size.y = win_sz.ws_row;
	}
#endif

	return size;
}

/* clear cli output stream */
void clear_cli()
{
#ifdef WINDOWS
	system("cls");
#else
	system("clear && printf '\e[3J'");
#endif
}

int main()
{
	/* get cli raw input */
	console_t cli;

#ifdef WINDOWS
	cli.mode = ~(ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT | ENABLE_INSERT_MODE);
	cli.old_mode = set_cli_input_mode(cli.mode);
#else
	cfmakeraw(&cli.mode);
	cli.old_mode = set_cli_input_mode(cli.mode);
#endif

	/* hide cli text cursor */
	show_cli_input_cursor(false);

	/* clear cli */
	clear_cli();

	/* init frame buffer */
	framebuffer_t fb;

	while (true) {
		if (init_screen_framebuffer(&fb)) {
#ifdef WINDOWS
			for (int i = 0; i < 9000; i++) {
				int x = rand() % fb.resolution.x;
				int y = rand() % fb.resolution.y;
				SetPixel(fb.device, x, y, RGB(rand() % 255, rand() % 255, rand() % 255));
			}
#else
			memset(fb.address, 0x0, fb.size);
			for (int i = 0; i < 9000; i++) {
				size_t pos = rand() % fb.size;
				*(fb.address + pos) = (ubyte_t)rand();
			}
#endif

			if (get_char() == ' ')
				break;
		}
	}

	terminate_screen_framebuffer(&fb);

	while (true) {
		/* store cli size and aspect ratio */
		cli.size = get_cli_size();
		cli.aspect_ratio = (double)cli.size.x / cli.size.y;

		for (size_t i = 1, j = 1; j <= cli.size.y; i++) {
			if (i > cli.size.x) {
				i = 1;
				j++;
			}

			set_cli_cursor_pos(i, j);
			if (1 == j)
				printf("\x1b[102m%s", "\x1b[92m_");
			else if (i == 1)
				printf("\x1b[102m%s", "\x1b[92m|");
			else
				printf("\x1b[107m%s", "\x1b[97m.");
		}

		if (get_char() == ' ')
			break;
	}

	/* reset terminal values */
	clear_cli();
	set_cli_input_mode(cli.old_mode);
	show_cli_input_cursor(true);

	return EXIT_SUCCESS;
}
