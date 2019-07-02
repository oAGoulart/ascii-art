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
#include <stdarg.h>

/* configurations */
#define BASE_RESOLUTION_WIDTH  1366
#define BASE_RESOLUTION_HEIGHT 768
#define BASE_ASPECT_RATIO      (BASE_RESOLUTION_WIDTH / BASE_RESOLUTION_HEIGHT)

/* console colors */
typedef enum cli_deco_e {
	DECO_RESET            = 0,
	DECO_BOLD             = 1,
	DECO_UNDERSCORE       = 4,
	DECO_NONE             = 5,
											/* foreground */
	FOREGROUND_BLACK      = 30,
	FOREGROUND_RED        = 31,
	FOREGROUND_GREEN      = 32,
	FOREGROUND_YELLOW     = 33,
	FOREGROUND_BLUE       = 34,
	FOREGROUND_MAGENTA    = 35,
	FOREGROUND_CYAN       = 36,
	FOREGROUND_WHITE      = 37,
											/* foreground bright */
	FOREGROUND_BR_BLACK   = 90,
	FOREGROUND_BR_RED     = 91,
	FOREGROUND_BR_GREEN   = 92,
	FOREGROUND_BR_YELLOW  = 93,
	FOREGROUND_BR_BLUE    = 94,
	FOREGROUND_BR_MAGENTA = 95,
	FOREGROUND_BR_CYAN    = 96,
	FOREGROUND_BR_WHITE   = 97,
											/* background */
	BACKGROUND_BLACK      = 40,
	BACKGROUND_RED        = 41,
	BACKGROUND_GREEN      = 42,
	BACKGROUND_YELLOW     = 43,
	BACKGROUND_BLUE       = 44,
	BACKGROUND_MAGENTA    = 45,
	BACKGROUND_CYAN       = 46,
	BACKGROUND_WHITE      = 47,
											/* background bright */
	BACKGROUND_BR_BLACK   = 100,
	BACKGROUND_BR_RED     = 101,
	BACKGROUND_BR_GREEN   = 102,
	BACKGROUND_BR_YELLOW  = 103,
	BACKGROUND_BR_BLUE    = 104,
	BACKGROUND_BR_MAGENTA = 105,
	BACKGROUND_BR_CYAN    = 106,
	BACKGROUND_BR_WHITE   = 107
} cli_deco_t;

/* platform specific stuff */
#if defined(_WIN32) || defined(_WIND64) || defined(__MINGW32__) || defined (__MINGW64__)
	#undef __WINDOWS__
	#define __WINDOWS__ 1
	
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

	#define MAX_DISPLAY_NAME 64 /* maximum size of display name */

	typedef struct termios console_mode_t;
#endif

/* define data types */
#if defined(__ILP32__) || defined(_ILP32) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)
	#undef __X86_ARCH__
	#define __X86_ARCH__ 1

	typedef uint32_t ulong_t;
	typedef int32_t long_t;
#else /* assume 64-bit */
	typedef uint64_t ulong_t;
	typedef int64_t long_t;
#endif

typedef unsigned char ubyte_t;

/* define function macros */
#define ABS(x) ((x < 0) ? x * -1 : x)
#define SGN(x) ((x < 0) ? -1 : (x == 0) ? 0 : 1)
#define IPART(x) ((int)x)
#define FPART(x) (x - IPART(x))
#define RFPART(x) (1 - FPART(x))
#define ROUND(x) (IPART(x + 0.5f))

/* define struct types */
typedef struct vertice_s
{
	long_t x;
	long_t y;
} vertice_t;

typedef struct console_s
{
	console_mode_t old_mode;     /* previous mode */
	vertice_t      size;         /* console size (columns X rows) */
	double         aspect_ratio; /* aspect ratio from console size */
} console_t;

typedef struct framebuffer_s
{
#ifdef __WINDOWS__
	HWND       window;             /* window handler */
	HDC        device;             /* device handler */
	vertice_t  resolution;         /* window resolution in pixels */
	LONG_PTR   win_style;          /* current window style */
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

/* swap two integer values */
void swap(long_t* a, long_t* b)
{
	int c = *a;
	*a = *b;
	*b = c;
}

/* get char from stdin without blocking */
int get_char()
{
	char result = EOF;

#ifdef __WINDOWS__
	/* verify keyboard was hit */
	/* NOTE: lagging input */
	if (_kbhit())
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

/* write decorated format string to stream */
void fprintfdec(FILE* stream, const cli_deco_t frgd_color, const cli_deco_t bkgd_color, const cli_deco_t style, const char* format, ...)
{
	va_list args;
	va_start(args, format);

	/* alloc string to do formatting */
	char* str = malloc(BUFSIZ);

	if (str != NULL) {
#ifdef __WINDOWS__
		/* NOTE: old windows versions don't support colored output so don't show it */
		int size = vsprintf(str, format, args);

		DWORD written;
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), str, size, &written, NULL);
#else
		/* add string styles */
		sprintf(str, "\e[%dm\e[%dm\e[%dm", frgd_color, bkgd_color, style);
		strcat(str, format);
		strcat(str, "\e[0m");

		/* final string */
		vfprintf(stream, str, args);
#endif

		free(str);
	}

	va_end(args);
}

/* put pixel into screen */
void put_pixel(framebuffer_t* fb, vertice_t position, ubyte_t color)
{
#ifdef __WINDOWS__
	/* TODO: change to a better approche than SetPixel (this is painful slow) */
	SetPixel(fb->device, position.x, position.y, RGB(color, color, color));
#else
	void* address = fb->address + (position.y * fb->fix_info.line_length) + (position.x * 4 + 4);

	if ((ulong_t)address <= (ulong_t)(fb->address + fb->size))
		memset(address, color, 4);
#endif
}

/* draw a line into screen */
void draw_line(framebuffer_t* fb, vertice_t start, vertice_t end)
{
	vertice_t delta = { end.x - start.x, end.y - start.y };
	int sign[4] = {
		SGN(delta.x),
		SGN(delta.y),
		SGN(delta.x),
		0
	};

	/* get longest and shortest delta */
	size_t longest = ABS(delta.x);
	size_t shortest = ABS(delta.y);

	if (longest < shortest) {
		longest = ABS(delta.y);
		shortest = ABS(delta.x);
		sign[3] = SGN(delta.y);
		sign[2] = 0;
	}

	int numerator = longest >> 1;

	/* calculate each pixel */
	for (size_t i = 0; i <= longest; i++) {
		put_pixel(fb, start, 255);

		numerator += shortest;

		if (numerator > longest) {
			numerator -= longest;

			start.x += sign[0];
			start.y += sign[1];
		}
		else {
			start.x += sign[2];
			start.y += sign[3];
		}
	}
}

/* draw rectangle into screen */
void draw_rect(framebuffer_t* fb, vertice_t start, vertice_t end)
{
	if (start.x > end.x)
		swap(&start.x, &end.x);
	if (start.y > end.y)
		swap(&start.y, &end.y);

	vertice_t position = { start.x + 1, start.y };
	vertice_t delta = { end.x - start.x, end.y - start.y };
	size_t size = delta.x * delta.y;

	for (size_t i = 0; i < size; i++) {
		put_pixel(fb, position, 255);

		if (position.x == end.x && position.y == end.y)
			break;

		if (position.x == end.x) {
			position.x = start.x;
			position.y++;
		}

		position.x++;
	}
}

/* clear screen */
void clear_screen(framebuffer_t* fb)
{
#ifdef __WINDOWS__
	vertice_t position = { 0, 0 };

	for (size_t i = 0; i < fb->resolution.x * fb->resolution.y; i++) {
		put_pixel(fb, position, 0);

		if (position.x == fb->resolution.x && position.y == fb->resolution.y)
			break;

		if (position.x == fb->resolution.x) {
			position.x = 0;
			position.y++;
		}

		position.x++;
	}
#else
	memset(fb->address, 0x0, fb->size);
#endif
}

#ifndef __WINDOWS__
/* get primary display output name */
bool get_active_display_name(char* name, const size_t name_sz)
{
	bool success = false;

	/* get primary display name and output to tmp file */
	system("xrandr | grep \" connected primary\" | awk '{ print $1 }' >/tmp/harvest_config");

	char line[name_sz];
	FILE* file = fopen("/tmp/harvest_config", "r");

	if (file != NULL) {
		/* get file stored string */
		if ((fgets(line, name_sz, file)) != NULL) {
			for (int i = 0; i < name_sz; i++) {
				if (line[i] != '\n')
					name[i] = line[i];
				else {
					name[i] = '\0';
					break;
				}
			}

			success = true;
		}

		/* clean up */
		fclose(file);
	}

	/* remove temporary file */
	remove("/tmp/harvest_config");

	return success;
}
#endif

/* initialize screen frame buffer for full screen */
bool init_screen_framebuffer(framebuffer_t* fb)
{
	bool success = false;

	if (fb != NULL) {
#ifdef __WINDOWS__
		/* get window and device */
		if ((fb->window = GetConsoleWindow()) != NULL) {
			if ((fb->device = GetDC(fb->window)) != NULL) {
				/* modify styles */
				fb->win_style = GetWindowLongPtr(fb->window, GWL_STYLE);
				fb->win_style &= ~WS_BORDER;
				fb->win_style &= ~WS_DLGFRAME;
				fb->win_style &= ~WS_THICKFRAME;

				fb->win_ext_style = GetWindowLongPtr(fb->window, GWL_EXSTYLE);
				fb->win_ext_style &= ~WS_EX_WINDOWEDGE;

				SetWindowLongPtr(fb->window, GWL_STYLE, fb->win_style | WS_POPUP);
				SetWindowLongPtr(fb->window, GWL_EXSTYLE, fb->win_ext_style | WS_EX_TOPMOST);

				/* get screen size */
				fb->resolution.x = GetSystemMetrics(SM_CXSCREEN);
				fb->resolution.y = GetSystemMetrics(SM_CYSCREEN);

				/* resize window (windowed full screen) */
				/* FIXME: not resizing well if already maximized */
				success = (MoveWindow(fb->window, 0, 0, fb->resolution.x, fb->resolution.y, true) && ShowWindow(fb->window, SW_SHOWMAXIMIZED));
			}
		}

#else
		/* NOTE: this is not the finest approach but it is the fastest */
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

/* terminate screen frame buffer for full screen */
void terminate_screen_framebuffer(framebuffer_t* fb)
{
	if (fb != NULL) {
#ifdef __WINDOWS__
		if (fb->window != NULL) {
			SetWindowLongPtr(fb->window, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			SetWindowLongPtr(fb->window, GWL_EXSTYLE, WS_EX_OVERLAPPEDWINDOW);
			ShowWindow(fb->window, SW_SHOWDEFAULT);
			MoveWindow(fb->window, 0, 0, 480, 360, true);

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
		char display[MAX_DISPLAY_NAME];

		if (get_active_display_name(display, MAX_DISPLAY_NAME)) {
			/* refresh display output */
			char cmd[BUFSIZ];
			sprintf(cmd, "xrandr --output %s --off && xrandr --output %s --auto", display, display);
			system(cmd);
		}
#endif
	}
}

/* change terminal mode and return old one */
console_mode_t set_cli_input_mode(const console_mode_t mode)
{
	console_mode_t old_mode;

#ifdef __WINDOWS__
	HANDLE console = GetStdHandle(STD_INPUT_HANDLE);

	if (GetConsoleMode(console, &old_mode))
		SetConsoleMode(console, mode);
#else
	if (!tcgetattr(STDIN_FILENO, &old_mode))
		tcsetattr(STDIN_FILENO, TCSANOW, &mode);
#endif

	return old_mode;
}

/* change terminal mode to raw and return old one */
console_mode_t set_cli_raw_mode()
{
	console_mode_t mode;

#ifdef __WINDOWS__
	mode = ~(ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT | ENABLE_INSERT_MODE);
#else
	cfmakeraw(&mode);
#endif

	return set_cli_input_mode(mode);
}

/* change cli output cursor status */
void show_cli_output_cursor(const bool show)
{
#ifdef __WINDOWS__
	CONSOLE_CURSOR_INFO info = (show) ? (CONSOLE_CURSOR_INFO){ 25, true } : (CONSOLE_CURSOR_INFO){ 1, false };

	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
#else
	if (show)
		printf("\e[?25h");
	else
		printf("\e[?25l");
#endif
}

/* set cli cursor position */
void set_cli_cursor_pos(const size_t column, const size_t row)
{
#ifdef __WINDOWS__
	COORD position = { (short)column, (short)row };

	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), position);
#else
	printf("\e[%d;%dH", row, column);
#endif
}

/* get cli size */
vertice_t get_cli_size()
{
	vertice_t size = { 0, 0 };

#ifdef __WINDOWS__
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
#ifdef __WINDOWS__
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;

	if(GetConsoleScreenBufferInfo(console, &info)) {
		COORD pos = { 0, 0 };
		DWORD written;
		DWORD size = info.dwSize.X * info.dwSize.Y;

		/* fill console screen */
		FillConsoleOutputCharacter(console, (TCHAR)' ', size, pos, &written);

		/* set console attributer */
		FillConsoleOutputAttribute(console, info.wAttributes, size, pos, &written);

		/* reset cursor */
		set_cli_cursor_pos(0, 0);
	}
#else
	system("clear && printf '\e[3J'");
#endif
}

int main()
{
	console_t cli;

	/* get cli raw input */
	cli.old_mode = set_cli_raw_mode();

	/* hide cli text cursor */
	show_cli_output_cursor(false);

	/* clear cli */
	clear_cli();

	while (true) {
		/* store cli size and aspect ratio */
		cli.size = get_cli_size();
		cli.aspect_ratio = (double)cli.size.x / cli.size.y;

#ifdef __WINDOWS__
		const size_t init_index = 0;
#else
		const size_t init_index = 1;
#endif

		for (size_t i = init_index, j = init_index; j <= cli.size.y; i++) {
			if (i > cli.size.x) {
				i = init_index;
				j++;
			}

			set_cli_cursor_pos(i, j);

			if (init_index == j) {
				if (i == init_index)
					fprintfdec(stdout, FOREGROUND_CYAN, BACKGROUND_MAGENTA, DECO_NONE, "%s", "\u250C"); /* TODO: Make Windows support Unicode */
				else
					fprintfdec(stdout, FOREGROUND_CYAN, BACKGROUND_MAGENTA, DECO_NONE, "%s", "\u2500");
			}
			else if (i == init_index)
				fprintfdec(stdout, FOREGROUND_CYAN, BACKGROUND_MAGENTA, DECO_NONE, "%s", "\u2502");
			else
				fprintfdec(stdout, FOREGROUND_BR_WHITE, BACKGROUND_BR_WHITE, DECO_NONE, "%c", '.');
		}

		if (get_char() == ' ')
			break;
	}

	/* init frame buffer */
	framebuffer_t fb;

	if (init_screen_framebuffer(&fb)) {
		clear_screen(&fb);

		while (true) {
			draw_line(&fb, (vertice_t){0,0}, (vertice_t){1300,700});
			draw_rect(&fb, (vertice_t){100,100}, (vertice_t){300,300});

			if (get_char() == ' ')
				break;
		}

		clear_screen(&fb);
		terminate_screen_framebuffer(&fb);
	}

	/* reset terminal values */
	clear_cli();
	set_cli_input_mode(cli.old_mode);
	show_cli_output_cursor(true);

	return EXIT_SUCCESS;
}
