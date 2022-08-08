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
	CHANNEL_RED,
	CHANNEL_GREEN,
	CHANNEL_BLUE
};

static void
die(const char *err)
{
	fprintf(stderr, "xscreenshot: %s\n", err);
	exit(1);
}

static void
dief(const char *fmt, ...)
{
	va_list args;

	fputs("xscreenshot: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	exit(1);
}

static void
usage(void)
{
	puts("usage: xscreenshot [-hpv] [-d directory]");
	exit(0);
}

static void
version(void)
{
	puts("xscreenshot version "VERSION);
	exit(0);
}

static void
screenshot(xcb_connection_t *conn, xcb_screen_t *screen,
           const char *dir, int print_path)
{
	FILE *fp;
	uint16_t width, height;
	const xcb_setup_t *setup;
	xcb_generic_error_t *error;
	xcb_get_image_cookie_t cookie;
	xcb_get_image_reply_t *reply;
	uint8_t *pixels, pixel[3];
	int i, spixels, bpp, npixels;
	int channel_pos[3];
	time_t t;
	const struct tm *now;
	struct stat sb;
	char date[SCREENSHOT_DATE_LENGTH];
	char path[PATH_MAX], abpath[PATH_MAX];

	setup = xcb_get_setup(conn);
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
	npixels = spixels / sizeof(uint32_t);
	bpp = (spixels * 8) / (width * height);

	if (bpp != 32) {
		dief("invalid pixel format received, expected: 32bpp got: %dbpp", bpp);
	}

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

	if (NULL == (fp = fopen(path, "wb"))) {
		switch (errno) {
			case EACCES:
				dief("permission denied: %s", path);
				break;
			default:
				dief("fopen failed: %s", strerror(errno));
				break;
		}
	}

	if (print_path) {
		printf("%s\n", realpath(path, abpath) == NULL ? path : abpath);
	}

	fprintf(fp, "P6\n%hu %hu 255\n", width, height);

	/*                      setup->image_byte_order                        */
	/*     0 -> XCB_IMAGE_ORDER_LSB_FIRST (bgra) -> [ r:2, g: 1, b:0 ]     */
	/*     1 -> XCB_IMAGE_ORDER_MSB_FIRST (argb) -> [ r:1, g: 2, b:3 ]     */
	for (i = 0; i < 3; ++i) {
		channel_pos[i] = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
			i + 1 :
			2 - i;
	}

	for (i = 0; i < npixels; ++i) {
		pixel[CHANNEL_RED] = pixels[i*4+channel_pos[CHANNEL_RED]];
		pixel[CHANNEL_GREEN] = pixels[i*4+channel_pos[CHANNEL_GREEN]];
		pixel[CHANNEL_BLUE] = pixels[i*4+channel_pos[CHANNEL_BLUE]];

		fwrite(
			pixel, sizeof(pixel[0]),
			sizeof(pixel) / sizeof(pixel[0]),
			fp
		);
	}

	fclose(fp);
	free(reply);
}

int
main(int argc, char **argv)
{
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	const char *dir = ".";
	int print_path = 0;

	while (++argv, --argc > 0) {
		if (!strcmp(*argv, "-h")) usage();
		else if (!strcmp(*argv, "-v")) version();
		else if (!strcmp(*argv, "-p")) print_path = 1;
		else if (!strcmp(*argv, "-d")) --argc, dir = *++argv;
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

	screenshot(conn, screen, dir, print_path);
	xcb_disconnect(conn);

	return 0;
}
