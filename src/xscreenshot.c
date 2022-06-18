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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "debug.h"

#define SCREENSHOT_FILENAME_LENGTH (sizeof("20220612093950.ppm"))

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
screenshot(xcb_connection_t *connection, xcb_screen_t *screen, const char *dir)
{
	FILE *file;
	xcb_get_image_cookie_t cookie;
	xcb_get_image_reply_t *reply;
	xcb_generic_error_t *error;
	char filename[SCREENSHOT_FILENAME_LENGTH], savepath[1024];
	uint32_t width, height;
	uint8_t *pixels, pixel[3];

	width = (uint32_t)(screen->width_in_pixels);
	height = (uint32_t)(screen->height_in_pixels);
	error = NULL;

	cookie = xcb_get_image(
		connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
		screen->root, 0, 0, width, height,
		(uint32_t)(~0UL)
	);

	reply = xcb_get_image_reply(connection, cookie, &error);

	if (NULL != error) {
		dief("xcb_get_image failed with error code: %d",
				(int)(error->error_code));
	}

	pixels = xcb_get_image_data(reply);

	strftime(
		filename, sizeof(filename), "%Y%m%d%H%M%S.ppm",
		localtime((const time_t[1]) { time(NULL) })
	);

	snprintf(savepath, sizeof(savepath), "%s/%s", dir, filename);

	if (NULL == (file = fopen(savepath, "wb"))) {
		dief("fopen failed: %s", strerror(errno));
	}

	fprintf(file, "P6\n%u %u 255\n", width, height);

	for (uint32_t i = 0; i < width * height; ++i) {
		pixel[0] = pixels[i*4+2];
		pixel[1] = pixels[i*4+1];
		pixel[2] = pixels[i*4];

		fwrite(pixel, sizeof(pixel[0]), sizeof(pixel), file);
	}

	fclose(file);
	free(reply);
}

int
main(int argc, char **argv)
{
	xcb_connection_t *connection;
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

	if (xcb_connection_has_error(connection = xcb_connect(NULL, NULL))) {
		die("can't open display");
	}

	if (!(screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data)) {
		xcb_disconnect(connection);
		die("can't get default screen");
	}

	screenshot(connection, screen, dir);
	xcb_disconnect(connection);

	return 0;
}
