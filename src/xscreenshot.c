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

#include <errno.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "debug.h"

#define SCREENSHOT_DATE_FORMAT ("%Y%m%d%H%M%S")
#define SCREENSHOT_DATE_LENGTH (sizeof("20220612093950"))
#define XCB_PLANES_ALL_PLANES ((uint32_t)(~0UL))

static bool
match_opt(const char *in, const char *sh, const char *lo)
{
	return (strcmp(in, sh) == 0) ||
		   (strcmp(in, lo) == 0);
}

static inline void
print_opt(const char *sh, const char *lo, const char *desc)
{
	printf("%7s | %-25s %s\n", sh, lo, desc);
}

static void
usage(void)
{
	puts("Usage: xscreenshot [ -hv ] [ -d DIRECTORY ]");
	puts("Options are:");
	print_opt("-h", "--help", "display this message and exit");
	print_opt("-v", "--version", "display the program version");
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
	int pixel_index, pixel_count;
	const struct tm *now;
	struct stat sb;
	char date[SCREENSHOT_DATE_LENGTH];
	char path[PATH_MAX];

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
	pixel_index = 0;
	pixel_count = xcb_get_image_data_length(reply) / sizeof(uint32_t);

	now = localtime((const time_t[1]) { time(NULL) });

	strftime(date, SCREENSHOT_DATE_LENGTH, SCREENSHOT_DATE_FORMAT, now);
	snprintf(path, PATH_MAX, "%s/%s_%d.ppm", dir, date, getpid() % 10);

	if (stat(dir, &sb) == -1) {
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

	fprintf(file, "P6\n%hu %hu 255\n", width, height);

	while (pixel_index < pixel_count) {
		pixel[0] = pixels[pixel_index*4+2];
		pixel[1] = pixels[pixel_index*4+1];
		pixel[2] = pixels[pixel_index*4];

		fwrite(pixel, sizeof(pixel[0]), sizeof(pixel), file);

		pixel_index++;
	}

	fclose(file);
	free(reply);
}

int
main(int argc, char **argv)
{
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	char *dir = ".";

	if (++argv, --argc > 0) {
		if (match_opt(*argv, "-h", "--help")) usage();
		else if (match_opt(*argv, "-v", "--version")) version();
		else if (match_opt(*argv, "-d", "--directory")) {
			if (--argc > 0) dir = *++argv;
			else die("expected a directory");
		}
		else if (**argv == '-') dief("invalid option %s", *argv);
		else dief("unexpected argument: %s", *argv);
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
