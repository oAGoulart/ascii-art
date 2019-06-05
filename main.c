/*************************************************************************************
 *
 *  ASCII Art Animation Algorithm: Using Object-Oriented Concepts in C
 *    - Implementation using Object-Oriented concepts,
 *      most of it is just structs emulating the functionalities 
 *      of classes (mostly like in C++). e.g.: methods,
 *      constructors, and destructors. This is not a
 *      pure implementation of Object-Oriented Programming
 *      but rather an use of its concepts to try and
 *      understand it better.
 *
 *  by Augusto Goulart
 *
 *
 *  NOTE: not all Object-Oriented  concepts could be implemented in C,
 *    at least not in an easy and fast manner, this code is intended for 
 *    learning and educational pourposes only, and it may not work properly.
 *
 *                    --- Do not delete this comment block ---
 *
 *************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

/* define macros */
#if defined(_WIN32) || defined(_WIND64) || defined(__MINGW32__) || defined (__MINGW64__)
	#define WINDOWS

	/* WARNING: link Ws2_32.lib on windows or this file won't compile */
	#include <winsock2.h>
#else /* assume POSIX */
	#include <sys/select.h>
	#include <sys/time.h>
#endif

#define SECOND_MS 1000 /* how many miliseconds are there in a second */

/* declare functions (forward declarations) */
void free_memory(void** ptr);
void clear_cli();
char get_char(const long timeout_sec, const long timeout_usec);
char to_lower(const char letter);
long find_ceil(const double number);

/* define struct types */
typedef struct point_s {
	int _x;
	int _y;
} point_t;

/* define enum types */
typedef enum status_e {
	STATUS_WORK,  /* doing stuff */
	STATUS_IDLE,  /* the program is idling */
	STATUS_START, /* showing welcome message */
	STATUS_ERROR, /* something went wrong */
	STATUS_EXIT   /* terminate program */
} status_t;

typedef enum option_e {
	OPTION_NONE,              /* do nothing */
	OPTION_SHIFT_UP = 'w',    /* move image up */
	OPTION_SHIFT_DOWN = 's',  /* move image down */
	OPTION_SHIFT_LEFT = 'a',  /* move image left */
	OPTION_SHIFT_RIGHT = 'd', /* move image right */
	OPTION_SHIFT_AUTO = 'p',  /* align image automaticly */
	OPTION_EXIT = 'o'         /* exit menu */
} option_t;

/* define object types (class emulation) */
typedef struct frame_s {
	char*  _pixel_matrix; /* matrix where the pixels can be found */
	size_t _width;        /* width of the frame */
	size_t _height;       /* height of the frame */

	/* declare methods */
	void (*dtor)(struct frame_s* self);

	char (*get_pixel)(struct frame_s* self, size_t colunm, size_t line);
	void (*set_pixel)(struct frame_s* self, const char value, size_t colunm, size_t line);

	bool (*swap_matrix)(struct frame_s* self, char* array, size_t width, size_t height);
} frame_t;

typedef struct image_s {
	frame_t* _frame_array;  /* array of frame_t objects */
	size_t   _frames_count; /* number of frames in the array */
	size_t   _curr_frame;   /* frame being prepared for render */

	/* declare methods */
	void (*dtor)(struct image_s* self);

	size_t (*get_frames_count)(struct image_s* self);
	frame_t* (*get_curr_frame)(struct image_s* self);
	void (*set_curr_frame)(struct image_s* self, size_t curr_frame);

	bool (*add_frame)(struct image_s* self, frame_t* frame);
} image_t;

typedef struct screen_s {
	image_t* _render_array;   /* array of pointers to image_t objects that will be rendered */
	size_t   _images_count;   /* number of images in the array */
	char*    _render_surface; /* surface to render the images */
	point_t  _relative_pos;   /* position of the frames relative to the screen */
	size_t   _width;          /* width of the screen */
	size_t   _height;         /* height of the screen */
	short    _frame_rate;     /* rate of frames per second (hertz) */
	clock_t  _frame_delta;    /* delta time between frames */
	char*    _menu;           /* array that represents the menu interface */

	/* declare methods */
	void (*dtor)(struct screen_s* self);

	size_t (*get_images_count)(struct screen_s* self);
	void (*set_size)(struct screen_s* self, size_t width, size_t height);
	point_t* (*get_relative_pos)(struct screen_s* self);
	void (*set_relative_pos)(struct screen_s* self, point_t relative_pos);
	size_t (*get_width)(struct screen_s* self);
	size_t (*get_height)(struct screen_s* self);
	short (*get_frame_rate)(struct screen_s* self);
	void (*set_frame_rate)(struct screen_s* self, const short frame_rate);
	void (*swap_menu)(struct screen_s* self, char* menu);

	bool (*add_image)(struct screen_s* self, image_t* image);
	double (*calculate_frame_delta)(struct screen_s* self);
	point_t (*calculate_pixel_pos)(struct screen_s* self, size_t colunm, size_t line);
	point_t (*find_image_aligned_pos)(struct screen_s* self, size_t image_index);
	void (*render)(struct screen_s* self);
} screen_t;

/* define methods */
/* frame_t object destructor */
static void _frame_dtor(frame_t* self)
{
	//free_memory((void**)&(self->_pixel_matrix));
}

/* get pixel value in given position */
static char _frame_get_pixel(frame_t* self, size_t colunm, size_t line)
{
	char result = '\0';

	if (self != NULL)
	 result = (colunm < self->_width && line < self->_height) ? self->_pixel_matrix[colunm + (line * self->_width)] : result;

	return result;
}

/* change pixel value in given position */
static void _frame_set_pixel(frame_t* self, const char value, size_t colunm, size_t line)
{
	if (self != NULL) {
		if (colunm < self->_width && line < self->_height)
			self->_pixel_matrix[colunm + (line * self->_width)] = value;
	}
}

/* copy @matrix to @self->_pixel_matrix */
static bool _frame_swap_matrix(frame_t* self, char* matrix, size_t width, size_t height)
{
	bool error = true;

	if (self != NULL && matrix != NULL) {
		/* store matrix values */
		self->_width = width;
		self->_height = height;
		self->_pixel_matrix = matrix;

		error = false;
	}

	return error;
}

/* image_t object destructor */
static void _image_dtor(image_t* self)
{
	free_memory((void**)&(self->_frame_array));
}

/* get amout of frames in given image */
static size_t _image_get_frames_count(image_t* self)
{
	return (self != NULL) ? self->_frames_count : 0;
}

/* get current frame being rendered in given image */
static frame_t* _image_get_curr_frame(image_t* self)
{
	return (self != NULL) ? &self->_frame_array[self->_curr_frame] : NULL;
}

/* set current frame being rendered in given image */
static void _image_set_curr_frame(image_t* self, size_t curr_frame)
{
	if (self != NULL)
		self->_curr_frame = curr_frame;
}

/* copy @frame pointer to @self->_frame_array */
static bool _image_add_frame(image_t* self, frame_t* frame)
{
	bool error = true;

	if (self != NULL && frame != NULL) {
		frame_t* tmp_ptr = NULL; /* pointer to store new memory location */

		/* avoid losing current array pointer */
		if ((tmp_ptr = realloc(self->_frame_array, sizeof(frame_t) * (self->_frames_count + 1))) != NULL) {
			/* update array location */
			self->_frame_array = tmp_ptr;
			
			/* store object to array */
			self->_frame_array[self->_frames_count] = *frame;

			/* keep track of the number of frames */
			self->_frames_count++;

			error = false;
		}
	}

	return error;
}

/* screen_t object destructor */
static void _screen_dtor(screen_t* self)
{
	free_memory((void**)&(self->_render_array));
	free_memory((void**)&(self->_render_surface));
}

/* get number of images to be rendered */
static size_t _screen_get_images_count(screen_t* self)
{
	return (self != NULL) ? self->_images_count : 0;
}

/* set screen size */
static void _screen_set_size(screen_t* self, size_t width, size_t height)
{
	if (self != NULL) {
		self->_width = width;
		self->_height = height;
	}
}

/* get frames relative position */
static point_t* _screen_get_relative_pos(screen_t* self)
{
	return (self != NULL) ? &self->_relative_pos : NULL;
}

/* set frames relative position */
static void _screen_set_relative_pos(screen_t* self, point_t relative_pos)
{
	if (self != NULL) {
		self->_relative_pos._x = relative_pos._x;
		self->_relative_pos._y = relative_pos._y;
	}
}

/* get screen width */
static size_t _screen_get_width(screen_t* self)
{
	return (self != NULL) ? self->_width : 0;
}

/* get screen height */
static size_t _screen_get_height(screen_t* self)
{
	return (self != NULL) ? self->_height : 0;
}

/* get rate of frames per second */
static short _screen_get_frame_rate(screen_t* self)
{
	return (self != NULL) ? self->_frame_rate : 0;
}

/* set rate of frames per second */
static void _screen_set_frame_rate(screen_t* self, const short frame_rate)
{
	if (self != NULL)
		self->_frame_rate = frame_rate;
}

/* copy @interface value to @self->_interface */
static void _screen_swap_menu(screen_t* self, char* menu)
{
	if (self != NULL && menu != NULL)
		self->_menu = menu;
}

/* copy @image pointer to @self->_render_array */
static bool _screen_add_image(screen_t* self, image_t* image)
{
	bool error = true;

	if (self != NULL && image != NULL) {
		image_t* tmp_ptr = NULL; /* pointer to store new memory location */

		/* avoid losing current array pointer */
		if ((tmp_ptr = realloc(self->_render_array, sizeof(image_t) * (self->_images_count + 1))) != NULL) {
			/* update array location */
			self->_render_array = tmp_ptr;
			
			/* store pointer to array */
			self->_render_array[self->_images_count] = *image;

			/* keep track of the number of images */
			self->_images_count++;

			error = false;
		}
	}

	return error;
}

/* calculate the frame delta time */
static double _screen_calculate_frame_delta(screen_t* self)
{
	return (self != NULL) ? (double)SECOND_MS / self->_frame_rate : 0.0;
}

/* calculate pixel position (using @self->_relative_pos) on screen */
static point_t _screen_calculate_pixel_pos(screen_t* self, size_t colunm, size_t line)
{
	point_t result = { (int)colunm, (int)line };

	if (self != NULL) {
		/* calculate and store actual colunm/line position after shift */
		result._x = (int)(((self->_width + self->_relative_pos._x) + colunm) % self->_width);
		result._y = (int)(((self->_height + self->_relative_pos._y) + line) % self->_height);
	}

	return result;
}

/* find image aligned position */
static point_t _screen_find_image_aligned_pos(screen_t* self, size_t image_index)
{
	point_t result = { 0, 0 };
	image_t* tmp_image_ptr = &self->_render_array[image_index];
	frame_t* tmp_frame_ptr = &tmp_image_ptr->_frame_array[tmp_image_ptr->_curr_frame];

	if (self != NULL) {
		result._x = find_ceil((double)(self->_width - tmp_frame_ptr->_width - 2) / 2);
		result._y = find_ceil((double)(self->_height - tmp_frame_ptr->_height - 2) / 2);
	}

	return result;
}

/* render images to screen (call this on a loop) */
static void _screen_render(screen_t* self)
{
	if (self != NULL) {
		if (self->_render_array != NULL) {
			/* store buffer size */
			size_t buff_size = self->_width * self->_height;

			/* only update screen after frame delta time */
			if ((clock() - self->_frame_delta) / (CLOCKS_PER_SEC / 1000) > self->calculate_frame_delta(self)) {
				/* free up memory */
				free_memory((void**)&self->_render_surface);

				/* verify memory was allocated */
				if ((self->_render_surface = calloc(buff_size, 1)) != NULL) {
					/* reset frame delta */
					self->_frame_delta = clock();

					/* for each image */
					for (size_t i = 0; i < self->_images_count; i++) {
						/* store current image */
						image_t* tmp_image_ptr = &self->_render_array[i];

						/* get current frame */
						frame_t* tmp_frame_ptr = &tmp_image_ptr->_frame_array[tmp_image_ptr->_curr_frame];

						/* iterate through pixel matrix and calculate each element position */
						for (size_t j = 0, k = 0; k < tmp_frame_ptr->_height; j++) {
							/* control counters */
							if (j >= tmp_frame_ptr->_width) {
								j = 0;
								k++;
							}
							if (k == tmp_frame_ptr->_height)
								break;

							/* get new position */
							point_t tmp_pixel_pos = self->calculate_pixel_pos(self, j, k);

							/* store pixel to buffer */
							self->_render_surface[tmp_pixel_pos._x + (tmp_pixel_pos._y * self->_width)] = tmp_frame_ptr->get_pixel(tmp_frame_ptr, j, k); 
						}

						/* prepare next frame */
						if (tmp_image_ptr->_curr_frame + 1 < tmp_image_ptr->_frames_count)
							tmp_image_ptr->_curr_frame++;
						else
							tmp_image_ptr->_curr_frame = 0;
					}

					/* clear CLI */
					clear_cli();

					/* render screen surface */
					for (size_t j = 0; j < buff_size; j++) {
						/* output to stdout */
						if (self->_render_surface[j] == '\0')
							putchar(' ');
						else
							putchar(self->_render_surface[j]);

						/* break output on end of line */
						if ((j + 1) % self->_width == 0)
							putchar('\n');
					}

					/* render screen menu */
					if (self->_menu != NULL)
						printf("%s", self->_menu);
				}
			}
		}
	}
}

/* define constructors */
/* frame_t object constructor */
static void frame_ctor(frame_t* self)
{
	if (self != NULL) {
		self->_pixel_matrix = NULL;
		self->_width = 0;
		self->_height = 0;
		self->dtor = &_frame_dtor;
		self->get_pixel = &_frame_get_pixel;
		self->set_pixel = &_frame_set_pixel;
		self->swap_matrix = &_frame_swap_matrix;
	}
}

/* image_t object constructor */
static void image_ctor(image_t* self)
{
	if (self != NULL) {
		self->_frame_array = NULL;
		self->_frames_count = 0;
		self->_curr_frame = 0;
		self->dtor = &_image_dtor;
		self->get_frames_count = &_image_get_frames_count;
		self->get_curr_frame = &_image_get_curr_frame;
		self->set_curr_frame = &_image_set_curr_frame;
		self->add_frame = &_image_add_frame;
	}
}

/* screen_t object constructor */
static void screen_ctor(screen_t* self)
{
	if (self != NULL) {
		self->_render_array = NULL;
		self->_images_count = 0;
		self->_render_surface = NULL;
		self->_relative_pos._x = 0;
		self->_relative_pos._y = 0;
		self->_width = 0;
		self->_height = 0;
		self->_frame_rate = 0;
		self->_frame_delta = clock();
		self->_menu = NULL;
		self->dtor = &_screen_dtor;
		self->get_images_count = &_screen_get_images_count;
		self->set_size = &_screen_set_size;
		self->get_relative_pos = &_screen_get_relative_pos;
		self->set_relative_pos = &_screen_set_relative_pos;
		self->get_width = &_screen_get_width;
		self->get_height = &_screen_get_height;
		self->get_frame_rate = &_screen_get_frame_rate;
		self->set_frame_rate = &_screen_set_frame_rate;
		self->swap_menu = &_screen_swap_menu;
		self->add_image = &_screen_add_image;
		self->calculate_frame_delta = &_screen_calculate_frame_delta;
		self->calculate_pixel_pos = &_screen_calculate_pixel_pos;
		self->find_image_aligned_pos = &_screen_find_image_aligned_pos;
		self->render = &_screen_render;
	}
}

/* define functions */
/* deallocate memory */
void free_memory(void** ptr)
{
	if (*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}
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

/* get char from stdin with timeout */
char get_char(const long timeout_sec, const long timeout_usec)
{
	char result = EOF;

	/* define select() values */
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	struct timeval timeout = { timeout_sec, timeout_usec };

	/* verify stdin is not empty */
	if (select(1, &readfds, NULL, NULL, &timeout))
		result = getchar();

	return result;
}

/* translate to lower case letter */
char to_lower(const char letter)
{
	return (letter >= 65 && letter <= 90) ? letter + 32 : letter;
}

/* find ceiling */
long find_ceil(const double number)
{
	return ((long)number) + 1;
}

/* entry point */
int main()
{
	/* program status */
	status_t status = STATUS_ERROR;

	/* initialize stuff */
	char menu_array[] = "\nPlease, enter an option:\n| W - Move up | S - Move down | A - Move left | D - Move right |\n| P - Align automaticly | O - EXIT |\n";

	char* pixel_matrix[] = { 
		"  o   |#|  _|_ ",
		" |o|   #  _| |_",
		" =o=   O   | | ",
		" ;o;   H   1 1 "
	};

	frame_t frame;
	frame_ctor(&frame);
	image_t image;
	image_ctor(&image);
	screen_t screen;
	screen_ctor(&screen);

	for (int i = 0; i < 4; ++i) {
		if (!frame.swap_matrix(&frame, pixel_matrix[i], 5, 3)) {
			if (image.add_frame(&image, &frame))
				break;
		}
	}

	screen.set_size(&screen, 20, 10);
	screen.set_frame_rate(&screen, 8);
	screen.swap_menu(&screen, menu_array);

	if (!screen.add_image(&screen, &image))
		status = STATUS_START;

	/* keep track of the image shift */
	point_t shift = { 7, 5 };

	/* main loop */
	bool run = true;
	while (run) {
		/* handle program status */
		switch (status) {
			case STATUS_EXIT:
				/* terminate program */
				printf("\nBye, Human!\n");
				run = false;
				break;

			case STATUS_ERROR:
				/* print error message and exit */
				printf("\nERROR: Something went wrong, Human!\n");
				run = false;
				break;

			case STATUS_START:
				/* print welcome message */
				printf("\nASCII Art Animation Algorithm: Using Object-Oriented Concepts in C\nby Augusto Goulart\n");
				printf("\nWelcome, Human!\n");

			case STATUS_IDLE:
				/* do nothing till user enters input */
				printf("Enter something to start: ");
				getchar();

			case STATUS_WORK:
				/* render stuff */
				screen.render(&screen);

				/* handle screen menu options */
				char tmp_ch = '\0';

				if ((tmp_ch = get_char(0, 0)) != EOF) {
					switch (to_lower(tmp_ch)) {
						case OPTION_SHIFT_UP:
							shift._y += -1;
							break;

						case OPTION_SHIFT_DOWN:
							shift._y += 1;
							break;

						case OPTION_SHIFT_LEFT:
							shift._x += -1;
							break;

						case OPTION_SHIFT_RIGHT:
							shift._x += 1;
							break;

						case OPTION_SHIFT_AUTO:
							shift = screen.find_image_aligned_pos(&screen, 0);
							break;

						case OPTION_EXIT:
							status = STATUS_EXIT;
							break;

						default:
							status = STATUS_WORK;
					}
				}
				else
					status = STATUS_WORK;

				/* update images position for next render */
				screen.set_relative_pos(&screen, shift);
				break;


			default:
				/* set back to idle */
				status = STATUS_IDLE;
		}
	}

	/* destruct objects */
	screen.dtor(&screen);
	image.dtor(&image);
	frame.dtor(&frame);

	/* exit type */
	if (status == STATUS_ERROR)
		return EXIT_FAILURE;
	else
		return EXIT_SUCCESS;
}
