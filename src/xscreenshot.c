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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "bitmap.h"
#include "numdef.h"
#include "debug.h"

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
	puts("Usage: xscreenshot [ -hv ]");
	puts("Options are:");
	print_opt("-h", "--help", "display this message and exit");
	print_opt("-v", "--version", "display the program version");
	exit(0);
}

static void
version(void)
{
	puts("xscreenshot version "VERSION);
	exit(0);
}

static char *
screenshot_filename(void)
{
	struct tm *tm_info;
	size_t len;
	char *filename;

	len = 30;
	filename = malloc(sizeof(char) * len);
	tm_info = localtime((const time_t[1]) { time(NULL) });

	strftime(filename, len - 1, "%Y%m%d%H%M%S.ppm", tm_info);

	return filename;
}

static bitmap_t *
screenshot(xcb_connection_t *connection, xcb_screen_t *screen)
{
	bitmap_t *bmp;
	xcb_get_image_reply_t *reply;
	u8 *data;

	bmp = bitmap_create(screen->width_in_pixels, screen->height_in_pixels);

	reply = xcb_get_image_reply(
		connection,
		xcb_get_image_unchecked(
			connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
			screen->root, 0, 0, screen->width_in_pixels, screen->height_in_pixels,
			(u32)(~0UL)
		),
		NULL
	);

	data = xcb_get_image_data(reply);

	for (u32 i = 0; i < screen->width_in_pixels * screen->height_in_pixels; ++i) {
		bitmap_set(
			bmp,
			i % screen->width_in_pixels,
			i / screen->width_in_pixels,
			data[i * 4 + 2] << 16 | data[i * 4 + 1] << 8 | data[i * 4]
		);
	}

	free(reply);

	return bmp;
}

int
main(int argc, char **argv)
{
	if (++argv, --argc > 0) {
		if (match_opt(*argv, "-h", "--help")) usage();
		else if (match_opt(*argv, "-v", "--version")) version();
		else if (**argv == '-') dief("invalid option %s", *argv);
		else dief("unexpected argument: %s", *argv);
	}

	xcb_connection_t *connection;
	xcb_screen_t *screen;
	char *filename;
	bitmap_t *bmp;

	if (xcb_connection_has_error(connection = xcb_connect(NULL, NULL))) {
		die("can't open display");
	}

	if (!(screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data)) {
		xcb_disconnect(connection);
		die("can't get default screen");
	}

	filename = screenshot_filename();
	bmp = screenshot(connection, screen);

	bitmap_save(bmp, filename);

	free(filename);
	bitmap_free(bmp);
	xcb_disconnect(connection);

	return 0;
}
