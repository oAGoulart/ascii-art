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
#include <time.h>

/* configurations */
#define BASE_RESOLUTION_WIDTH  1366
#define BASE_RESOLUTION_HEIGHT 768
#define BASE_ASPECT_RATIO      (BASE_RESOLUTION_WIDTH / BASE_RESOLUTION_HEIGHT)
#define SECOND_MS              1000
#define FRAME_TIME             (SECOND_MS / 60)

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

	#define CLI_CURSOR_START_INDEX 0 /* where the console cursor starts */

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

	#define MAX_DISPLAY_NAME 64      /* maximum size of display name */
	#define CLI_CURSOR_START_INDEX 1 /* where the console cursor starts */

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

typedef struct color_s
{
	ubyte_t red;
	ubyte_t green;
	ubyte_t blue;
	ubyte_t alpha;
} color_t;

typedef struct print_style_s
{
	color_t foreground; /* foreground color */
	color_t background; /* background color */
	ubyte_t decoration; /* decoration */
} print_style_t;

typedef struct framebuffer_s
{
#ifdef __WINDOWS__
	HWND      window;        /* window handler */
	HDC       device;        /* device handler */
	vertice_t resolution;    /* window resolution in pixels */
	LONG_PTR  win_style;     /* current window style */
	LONG_PTR  win_ext_style; /* current window extended style */
	size_t    size;          /* size of screen */
#else
	int                      fd;            /* file descriptor */
	ubyte_t*                 address;       /* buffer address on memory */
	size_t                   size;          /* buffer size on memory */
	struct fb_var_screeninfo orig_var_info; /* screen original variable information */
	struct fb_var_screeninfo var_info;      /* screen current variable information */
	struct fb_fix_screeninfo fix_info;      /* screen fixed information */
#endif
} framebuffer_t;

typedef struct rect_s
{
	vertice_t start; /* rect start */
	vertice_t end;   /* rect end */
	color_t   color; /* color */
} rect_t;

typedef struct player_s
{
	vertice_t coord;     /* entity position */
	vertice_t velocity;  /* entity axis velocity */
	vertice_t accel;     /* entity axis acceleration */
	color_t   color;     /* color */
	rect_t    bound_box; /* boundaries box */
} player_t;

typedef struct scene_s
{
	console_t     cli;         /* console data */
	framebuffer_t fb;          /* frame buffer data */
	clock_t       frame_delta; /* time from last render */
	bool          game_over;   /* finish game */
	player_t      player;      /* game player */
} scene_t;

/* forward declarations */
void draw_cli_menu(scene_t* scene);
void render_game(scene_t* scene);
void handle_input(scene_t* scene);

/* swap two long_t values */
void swap(long_t* a, long_t* b)
{
	if (a != NULL && b != NULL) {
		long_t c = *a;
		*a = *b;
		*b = c;
	}
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

/* convert color_t struct to ulong_t */
ulong_t color_to_long(framebuffer_t* fb, const color_t color)
{
	if (fb != NULL)
		return (color.red << 24) | (color.green << 16) | (color.blue << 8) | color.alpha;

	return 0;
}


/* write decorated format string to stream */
void fprintfdec(FILE* stream, const print_style_t style, const char* format, ...)
{
	va_list args;
	va_start(args, format);

	if (stream != NULL) {
		/* alloc string to do formatting */
		char* str = malloc(BUFSIZ);

		if (str != NULL) {
#ifdef __WINDOWS__
			/* NOTE: old windows versions don't support colored output so don't show it */
			int size = vsprintf(str, format, args);

			DWORD written;
			WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str, size, &written, NULL);
#else
			/* add string styles */
			sprintf(str, "\e[38;2;%d;%d;%dm\e[48;2;%d;%d;%dm\e[%dm", 
				style.foreground.red, style.foreground.green, style.foreground.blue, 
				style.background.red, style.background.green, style.background.blue,
				style.decoration);

			strcat(str, format);
			strcat(str, "\e[0m");

			/* final string */
			vfprintf(stream, str, args);
#endif

			free(str);
		}
	}

	va_end(args);
}

/* put pixel into screen */
void put_pixel(framebuffer_t* fb, const vertice_t position, const color_t color)
{
	if (fb != NULL) {
#ifdef __WINDOWS__
		SetPixel(fb->device, position.x, position.y, RGB(color.red, color.green, color.blue));
#else
		void* address = fb->address + (position.y * fb->fix_info.line_length) + (position.x * (fb->var_info.bits_per_pixel / 8) + 4);

		/* TODO: Fix color not working on Linux */
		if ((ulong_t)address <= (ulong_t)(fb->address + fb->size))
			memset(address, color_to_long(fb, color), (fb->var_info.bits_per_pixel / 8));
#endif
	}
}

/* draw a line into screen */
void draw_line(framebuffer_t* fb, vertice_t start, vertice_t end, const color_t color)
{
	if (fb != NULL) {
#ifdef __WINDOWS__
		HGDIOBJ original = SelectObject(fb->device, GetStockObject(DC_PEN));

		if (original != NULL) {
			/* switch pen */
			SelectObject(fb->device, GetStockObject(DC_PEN));
			SetDCPenColor(fb->device, RGB(color.red, color.green, color.blue));

			/* draw line */
			MoveToEx(fb->device, start.x, start.y, NULL);
			LineTo(fb->device, end.x, end.y);

			SelectObject(fb->device, original);
		}
#else
		/* find delta and sign values */
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
			put_pixel(fb, start, color);

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
#endif
	}
}

/* draw rectangle into screen */
void draw_rect(framebuffer_t* fb, vertice_t start, vertice_t end, const color_t color)
{
	if (fb != NULL) {
		/* check if vertices are swapped */
		if (start.x > end.x)
			swap(&start.x, &end.x);
		if (start.y > end.y)
			swap(&start.y, &end.y);

#ifdef __WINDOWS__
		RECT screen = { start.x, start.y, end.x, end.y };
		HBRUSH brush = CreateSolidBrush(RGB(color.red, color.green, color.blue));

		FillRect(fb->device, &screen, brush);
		DeleteObject(brush);
#else
		vertice_t position = { start.x + 1, start.y };
		vertice_t delta = { end.x - start.x, end.y - start.y };
		size_t size = delta.x * delta.y;

		for (size_t i = 0; i < size; i++) {
			put_pixel(fb, position, color);

			/* las pixel rendered */
			if (position.x == end.x && position.y == end.y)
				break;

			if (position.x == end.x) {
				position.x = start.x;
				position.y++;
			}

			position.x++;
		}
#endif
	}
}

/* NOTE: only used for drawing triangles on Linux */
#ifndef __WINDOWS__
/* bottom flat triangle */
static void fill_bottom_flat_triangle(framebuffer_t* fb, vertice_t a, vertice_t b, vertice_t c, const color_t color)
{
	if (fb != NULL) {
		float rev_slope_left = (float)(b.x - a.x) / (b.y - a.y);
		float rev_slope_right = (float)(c.x - a.x) / (c.y - a.y);
		float curr_x_left = a.x;
		float curr_x_right = a.x;

		for (long_t scanline = a.y; scanline <= b.y; scanline++) {
			vertice_t line_start = { (long_t)curr_x_left, scanline };
			vertice_t line_end = { (long_t)curr_x_right, scanline };

			draw_line(fb, line_start, line_end, color);

			curr_x_left += rev_slope_left;
			curr_x_right += rev_slope_right;
		}
	}
}

/* top flat triangle */
static void fill_top_flat_triangle(framebuffer_t* fb, vertice_t a, vertice_t b, vertice_t c, const color_t color)
{
	if (fb != NULL) {
		float rev_slope_left = (float)(c.x - a.x) / (c.y - a.y);
		float rev_slope_right = (float)(c.x - b.x) / (c.y - b.y);
		float curr_x_left = c.x;
		float curr_x_right = c.x;

		for (long_t scanline = c.y; scanline > a.y; scanline--) {
			vertice_t line_start = { (long_t)curr_x_left, scanline };
			vertice_t line_end = { (long_t)curr_x_right, scanline };

			draw_line(fb, line_start, line_end, color);

			curr_x_left -= rev_slope_left;
			curr_x_right -= rev_slope_right;
		}
	}
}

/* get intersection bewtween two lines */
static bool line_intersection(vertice_t a, vertice_t b, vertice_t c, vertice_t d, vertice_t* output)
{
	/* represent lines as formulas */
	float a1 = b.y - a.y;
	float b1 = a.x - b.x;
	float c1 = a1 * (a.x) + b1 * (a.y);
	float a2 = d.y - c.y;
	float b2 = c.x - d.x;
	float c2 = a2 * (c.x) + b2 * (c.y);

	/* calculate determinant */
	float determinant = a1 * b2 - a2 * b1;

	/* check lines aren't in pararell */
	if (determinant != 0) {
		output->x = (long_t)((b2 * c1 - b1 * c2) / determinant);
		output->y = (long_t)((a1 * c2 - a2 * c1) / determinant);

		return true;
	}

	return false;
}
#endif

/* draw triangle into screen */
void draw_triangle(framebuffer_t* fb, vertice_t a, vertice_t b, vertice_t c, const color_t color)
{
	if (fb != NULL) {
#ifdef __WINDOWS__
		HGDIOBJ original = SelectObject(fb->device, GetStockObject(DC_PEN));

		if (original != NULL) {
			/* switch pen */
			SelectObject(fb->device, GetStockObject(DC_PEN));
			SetDCPenColor(fb->device, RGB(color.red, color.green, color.blue));

			/* convert vertices to points */
			POINT points[3] = { { a.x, a.y }, { b.x, b.y }, { c.x, c.y} };

			/* draw triangle */
			Polygon(fb->device, points, 3);

			SelectObject(fb->device, original);
		}
#else
		/* sort vertices by y-coordinate */
		if (a.y > b.y) {
			swap(&a.x, &b.x);
			swap(&a.y, &b.y);
		}
		if (b.y > c.y) {
			swap(&b.x, &c.x);
			swap(&b.y, &c.y);
		}
		if (a.y > c.y) {
			swap(&a.x, &c.x);
			swap(&a.y, &c.y);
		}

		/* check for general cases */
		if (b.y == c.y)
			fill_bottom_flat_triangle(fb, a, b, c, color);
		else if (a.y == b.y)
			fill_top_flat_triangle(fb, a, b, c, color);
		else {
			/* split triangle into bottom flat and top flat */
			vertice_t d = { c.x + b.x, b.y };

			if (line_intersection(a, c, b, d, &d)) {
				fill_bottom_flat_triangle(fb, a, b, d, color);
				fill_top_flat_triangle(fb, b, d, c, color);
			}
		}
#endif
	}
}

/* clear screen */
void clear_screen(framebuffer_t* fb)
{
	if (fb != NULL) {
#ifdef __WINDOWS__
		RECT screen = { 0, 0, fb->resolution.x, fb->resolution.y };
		HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));

		FillRect(fb->device, &screen, brush);
		DeleteObject(brush);
#else
		memset(fb->address, 0x0, fb->size);
#endif
	}
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

/* calculate box position and size due to scaling */
rect_t get_scaled_box(framebuffer_t* fb, const long_t left, const long_t top, const long_t bottom, const long_t right, const color_t color)
{
	rect_t rect = { { left, top }, { right, bottom }, color };

	if (fb != NULL) {
#ifdef __WINDOWS__
		rect.start.y *= fb->resolution.y / BASE_RESOLUTION_HEIGHT;
		rect.start.x *= fb->resolution.x / BASE_RESOLUTION_HEIGHT;
		rect.end.y *= rect.start.y / top;
		rect.end.x *= (rect.start.x / left) / ((fb->resolution.x / fb->resolution.y) / BASE_ASPECT_RATIO);
#else
		rect.start.y *= fb->var_info.yres / BASE_RESOLUTION_HEIGHT;
		rect.start.x *= fb->var_info.xres / BASE_RESOLUTION_HEIGHT;
		rect.end.y *= rect.start.y / top;
		rect.end.x *= (rect.start.x / left) / ((fb->var_info.xres / fb->var_info.yres) / BASE_ASPECT_RATIO);
#endif
	}

	return rect;
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
			char cmd[BUFSIZ];

			/* refresh display output */
			sprintf(cmd, "xrandr --output %s --off && xrandr --output %s --auto", display, display);
			system(cmd);
		}
#endif
	}
}

/* initialize screen frame buffer for full screen */
bool init_screen_framebuffer(framebuffer_t* fb)
{
	bool success = false;

	if (fb != NULL) {
#ifdef __WINDOWS__
		/* get window and device */
		if ((fb->window = GetConsoleWindow()) != NULL) {
			/* modify styles */
			fb->win_style = GetWindowLongPtr(fb->window, GWL_STYLE);
			fb->win_style &= ~WS_BORDER;
			fb->win_style &= ~WS_DLGFRAME;
			fb->win_style &= ~WS_THICKFRAME;

			fb->win_ext_style = GetWindowLongPtr(fb->window, GWL_EXSTYLE);
			fb->win_ext_style &= ~WS_EX_WINDOWEDGE;

			/* set new styles */
			SetWindowLongPtr(fb->window, GWL_STYLE, fb->win_style | WS_POPUP);
			SetWindowLongPtr(fb->window, GWL_EXSTYLE, fb->win_ext_style | WS_EX_TOPMOST);

			/* get screen size */
			fb->resolution.x = GetSystemMetrics(SM_CXSCREEN);
			fb->resolution.y = GetSystemMetrics(SM_CYSCREEN);

			fb->size = fb->resolution.x * fb->resolution.y;

			/* resize window (windowed full screen) */
			/* FIXME: not resizing well if already maximized */
			MoveWindow(fb->window, 0, 0, fb->resolution.x, fb->resolution.y, true);
			ShowWindow(fb->window, SW_SHOWMAXIMIZED);

			success = ((fb->device = GetDC(fb->window)) != NULL);
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
					fb->var_info.grayscale = 0;
					fb->var_info.bits_per_pixel = 32;
					fb->var_info.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;

					if (!ioctl(fb->fd, FBIOPUT_VSCREENINFO, &fb->var_info)) {
						/* get size of buffer */
						fb->size = fb->fix_info.smem_len;

						/* map buffer to memory */
						success = ((fb->address = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0)) != NULL);
					}
				}
			}
		}
#endif
	}

	/* clean up if not successful */
	if (!success)
		terminate_screen_framebuffer(fb);

	return success;
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
	SetConsoleTitle("Harvest");

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
	scene_t scene;

	/* get cli raw input */
	scene.cli.old_mode = set_cli_raw_mode();

	/* hide cli text cursor */
	show_cli_output_cursor(false);

	clear_cli();

	while (true) {
		/* store cli size and aspect ratio */
		scene.cli.size = get_cli_size();
		scene.cli.aspect_ratio = (double)scene.cli.size.x / scene.cli.size.y;

		/* render */
		draw_cli_menu(&scene);

		/* stop if any key press */
		if (get_char() != EOF)
			break;
	}

	clear_cli();

	/* init frame buffer */
	if (init_screen_framebuffer(&scene.fb)) {
		clear_screen(&scene.fb);

		/* initialize player */
		scene.player.bound_box = get_scaled_box(&scene.fb, 663, 703, 753, 703, (color_t){ 255, 255, 255, 255 });

		scene.game_over = false;

		while (!scene.game_over) {
			/* render game */
			render_game(&scene);
			handle_input(&scene);
		}

		clear_screen(&scene.fb);
		terminate_screen_framebuffer(&scene.fb);
	}

	/* reset terminal values */
	set_cli_input_mode(scene.cli.old_mode);
	show_cli_output_cursor(true);

	return EXIT_SUCCESS;
}

void draw_cli_menu(scene_t* scene)
{
	if (scene != NULL) {
		vertice_t start = { CLI_CURSOR_START_INDEX + (scene->cli.size.x / 2) - 20, CLI_CURSOR_START_INDEX + (scene->cli.size.y / 2) - 5 };
		vertice_t end = { CLI_CURSOR_START_INDEX + (scene->cli.size.x / 2) + 20, CLI_CURSOR_START_INDEX + (scene->cli.size.y / 2) + 5 };

		/* draw box */
		for (size_t i = CLI_CURSOR_START_INDEX, j = CLI_CURSOR_START_INDEX; j <= scene->cli.size.y; i++) {
			if (i > scene->cli.size.x) {
				i = CLI_CURSOR_START_INDEX;
				j++;
			}

			set_cli_cursor_pos(i, j);

			if (j == start.y && i == start.x)
				fprintfdec(stdout, (print_style_t){ (color_t){ 40, 50, 80, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%s", "\u250C");
			else if (j == end.y && i == end.x)
				fprintfdec(stdout, (print_style_t){ (color_t){ 40, 50, 80, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%s", "\u2518");
			else if (j == end.y && i == start.x)
				fprintfdec(stdout, (print_style_t){ (color_t){ 40, 50, 80, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%s", "\u2514");
			else if (j == start.y && i == end.x)
				fprintfdec(stdout, (print_style_t){ (color_t){ 40, 50, 80, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%s", "\u2510");
			else if ((j == start.y || j == end.y) && i > start.x && i < end.x)
				fprintfdec(stdout, (print_style_t){ (color_t){ 40, 50, 80, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%s", "\u2500");
			else if (j > start.y && j < end.y && (i == start.x || i == end.x))
				fprintfdec(stdout, (print_style_t){ (color_t){ 40, 50, 80, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%s", "\u2502");
			else
				fprintfdec(stdout, (print_style_t){ (color_t){ 232, 215, 162, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%c", ' ');
		}

		/* draw text */
		set_cli_cursor_pos(start.x + 7, start.y + 3);
		fprintfdec(stdout, (print_style_t){ (color_t){ 40, 50, 80, 255 }, (color_t){ 232, 215, 162, 255 }, 5 }, "%s", "Space Force Simulator 3049");

		/* draw button */
		set_cli_cursor_pos(start.x + 17, start.y + 7);
		fprintfdec(stdout, (print_style_t){ (color_t){ 232, 215, 162, 255 }, (color_t){ 40, 50, 80, 255 }, 5 }, "%s", " START! ");
	}
}

/* render game to screen */
void render_game(scene_t* scene)
{
	if (scene != NULL) {
		/* only draw maximum num of frames */
#ifdef __WINDOWS__
		if (clock()) {
#else
		if ((clock() - scene->frame_delta) / (CLOCKS_PER_SEC / 10000) > FRAME_TIME) {
#endif
			scene->frame_delta = clock();

			/* clear screen */
			clear_screen(&scene->fb);

			/* draw player */
			vertice_t a = { (scene->player.bound_box.end.x - scene->player.bound_box.start.x) / 2 + scene->player.bound_box.start.x, scene->player.bound_box.start.y };
			vertice_t b = { scene->player.bound_box.start.x, scene->player.bound_box.end.y };

			draw_triangle(&scene->fb, a, b, scene->player.bound_box.end, scene->player.bound_box.color);

			/* draw enemies */
		}
	}
}

/* handle player input */
void handle_input(scene_t* scene)
{
	if (scene != NULL) {
#ifdef __WINDOWS__
		long_t player_max_pos_left = 10;
		long_t player_max_pos_right = scene->fb.resolution.x - 10;
#else
		long_t player_max_pos_left = 10;
		long_t player_max_pos_right = scene->fb.var_info.xres - 10;
#endif

		/* handle keyboard input */
		ubyte_t ch = get_char();

		switch (ch) {
			case 'D':
			case 'd':
				if (player_max_pos_right > scene->player.bound_box.end.x + 12) {
					scene->player.bound_box.start.x += 12;
					scene->player.bound_box.end.x += 12;
				}
				break;

			case 'A':
			case 'a':
				if (player_max_pos_left < scene->player.bound_box.start.x - 12) {
					scene->player.bound_box.start.x -= 12;
					scene->player.bound_box.end.x -= 12;
				}
				break;

			case '\e':
				scene->game_over = true;
		}
	}
}
