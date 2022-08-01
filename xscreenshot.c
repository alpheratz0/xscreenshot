/*
	Copyright (C) 2022 <alpheratz99@protonmail.com>

	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public License version 2 as published by the
	Free Software Foundation.

	This program is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place, Suite 330, Boston, MA 02111-1307 USA

	 _______________
	( screenshotubi )
	 ---------------
	  o
	   o
	      /  \~~~/  \
	     (    ..     )----,
	      \__     __/      \
	        )|  /)         |\
	         | /\  /___\   / ^
	          "-|__|   |__|

*/

#define _POSIX_C_SOURCE 1
#define _XOPEN_SOURCE 500

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define SCREENSHOT_DATE_FORMAT ("%Y%m%d%H%M%S")
#define SCREENSHOT_DATE_LENGTH (sizeof("20220612093950"))
#define XCB_PLANES_ALL_PLANES ((uint32_t)(~0UL))

enum {
	FLAG_PRINT_FILE_NAME,
	FLAG_COUNT
};

static int flags[FLAG_COUNT];

static void
die(const char *err)
{
	fprintf(stderr, "xscreenshot: %s\n", err);
	exit(1);
}

static void
dief(const char *err, ...)
{
	va_list list;
	fputs("xscreenshot: ", stderr);
	va_start(list, err);
	vfprintf(stderr, err, list);
	va_end(list);
	fputc('\n', stderr);
	exit(1);
}

static int
match_opt(const char *in, const char *sh, const char *lo)
{
	return (strcmp(in, sh) == 0) || (strcmp(in, lo) == 0);
}

static inline void
print_opt(const char *sh, const char *lo, const char *desc)
{
	printf("%7s | %-25s %s\n", sh, lo, desc);
}

static void
usage(void)
{
	puts("Usage: xscreenshot [ -hvp ] [ -d DIRECTORY ]");
	puts("Options are:");
	print_opt("-h", "--help", "display this message and exit");
	print_opt("-v", "--version", "display the program version");
	print_opt("-p", "--print", "print the screenshot path to stdout");
	print_opt("-d", "--directory", "set the directory to save the screenshot");
	exit(0);
}

static void
version(void)
{
	puts("xscreenshot version "VERSION);
	exit(0);
}

static void
screenshot(xcb_connection_t *conn, xcb_screen_t *screen, const char *dir)
{
	FILE *file;
	uint16_t width, height;
	xcb_generic_error_t *error;
	xcb_get_image_cookie_t cookie;
	xcb_get_image_reply_t *reply;
	uint8_t *pixels, pixel[3];
	int i, spixels, bpp, npixels;
	time_t t;
	const struct tm *now;
	struct stat sb;
	char date[SCREENSHOT_DATE_LENGTH];
	char path[PATH_MAX], abpath[PATH_MAX];

	width = screen->width_in_pixels;
	height = screen->height_in_pixels;
	error = NULL;

	cookie = xcb_get_image(
		conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
		screen->root, 0, 0, width, height,
		XCB_PLANES_ALL_PLANES
	);

	reply = xcb_get_image_reply(conn, cookie, &error);

	if (NULL != error) {
		dief("xcb_get_image failed with error code: %d",
				(int)(error->error_code));
	}

	pixels = xcb_get_image_data(reply);
	spixels = xcb_get_image_data_length(reply);
	bpp = (spixels * 8) / (width * height);

	if (bpp != 32) {
		dief("invalid pixel format received, expected: 32bpp got: %dbpp", bpp);
	}

	npixels = spixels / sizeof(uint32_t);
	t = time(NULL);
	now = localtime(&t);

	strftime(date, SCREENSHOT_DATE_LENGTH, SCREENSHOT_DATE_FORMAT, now);
	snprintf(path, PATH_MAX, "%s/%s_%d.ppm", dir, date, getpid() % 10);

	if (stat(dir, &sb) < 0) {
		switch (errno) {
			case ENOENT:
				dief("directory does not exist: %s", dir);
				break;
			case EACCES:
				dief("permission denied: %s", dir);
				break;
			default:
				dief("stat failed: %s", strerror(errno));
				break;
		}
	}

	if (!S_ISDIR(sb.st_mode)) {
		dief("not a directory: %s", dir);
	}

	if (NULL == (file = fopen(path, "wb"))) {
		switch (errno) {
			case EACCES:
				dief("permission denied: %s", path);
				break;
			default:
				dief("fopen failed: %s", strerror(errno));
				break;
		}
	}

	if (flags[FLAG_PRINT_FILE_NAME]) {
		printf("%s\n", realpath(path, abpath) == NULL ? path : abpath);
	}

	fprintf(file, "P6\n%hu %hu 255\n", width, height);

	for (i = 0; i < npixels; ++i) {
		pixel[0] = pixels[i*4+2];
		pixel[1] = pixels[i*4+1];
		pixel[2] = pixels[i*4+0];

		fwrite(
			pixel, sizeof(pixel[0]),
			sizeof(pixel) / sizeof(pixel[0]),
			file
		);
	}

	fclose(file);
	free(reply);
}

int
main(int argc, char **argv)
{
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	const char *dir = ".";

	while (++argv, --argc > 0) {
		if (match_opt(*argv, "-h", "--help")) usage();
		else if (match_opt(*argv, "-v", "--version")) version();
		else if (match_opt(*argv, "-p", "--print")) flags[FLAG_PRINT_FILE_NAME] = 1;
		else if (match_opt(*argv, "-d", "--directory")) --argc, dir = *++argv;
		else if (**argv == '-') dief("invalid option %s", *argv);
		else dief("unexpected argument: %s", *argv);
	}

	if (NULL == dir || dir[0] == '-') {
		die("expected a directory");
	}

	if (xcb_connection_has_error(conn = xcb_connect(NULL, NULL))) {
		die("can't open display");
	}

	if (NULL == (screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data)) {
		xcb_disconnect(conn);
		die("can't get default screen");
	}

	screenshot(conn, screen, dir);
	xcb_disconnect(conn);

	return 0;
}
