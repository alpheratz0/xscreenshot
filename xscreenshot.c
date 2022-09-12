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

#include <png.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define SCREENSHOT_DATE_FORMAT ("%Y%m%d%H%M%S")
#define SCREENSHOT_DATE_LENGTH (sizeof("20220612093950"))
#define XCB_PLANES_ALL_PLANES ((uint32_t)(~0UL))

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

static const char *
enotnull(const char *str, const char *name)
{
	if (NULL == str)
		dief("%s cannot be null", name);
	return str;
}

static void
usage(void)
{
	puts("usage: xscreenshot [-hpv] [-d directory] [-w id]");
	exit(0);
}

static void
version(void)
{
	puts("xscreenshot version "VERSION);
	exit(0);
}

static int
parse_window_id(const char *s, xcb_window_t *id)
{
	xcb_window_t o;

	o = 0;

	if (*s++ != '0' || *s++ != 'x' || *s == '\0')
		return -1;

	while (*s) {
		if (*s >= '0' && *s <= '9') o = o * 16 + *s - '0';
		else if (*s >= 'a' && *s <= 'f') o = o * 16 + *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'F') o = o * 16 + *s - 'A' + 10;
		else return -1;
		++s;
	}

	*id = o;

	return 0;
}

static void
get_window_info(xcb_connection_t *conn, xcb_window_t window,
                int16_t *x, int16_t *y, uint16_t *width, uint16_t *height,
                xcb_window_t *root)
{
	xcb_generic_error_t *error;
	xcb_get_window_attributes_cookie_t gwac;
	xcb_get_window_attributes_reply_t *gwar;
	xcb_get_geometry_cookie_t ggc;
	xcb_get_geometry_reply_t *ggr;
	xcb_translate_coordinates_cookie_t tcc;
	xcb_translate_coordinates_reply_t *tcr;

	gwac = xcb_get_window_attributes(conn, window);
	gwar = xcb_get_window_attributes_reply(conn, gwac, &error);

	if (NULL != error && error->error_code == XCB_WINDOW)
		die("the specified window does not exist");

	if (NULL != error)
		dief("xcb_get_window_attributes failed with error code: %d",
				(int)(error->error_code));

	if (gwar->_class != XCB_WINDOW_CLASS_INPUT_OUTPUT)
		die("the specified window is not an input/output window");

	if (gwar->map_state != XCB_MAP_STATE_VIEWABLE)
		die("the specified window is not visible/mapped");

	ggc = xcb_get_geometry(conn, window);
	ggr = xcb_get_geometry_reply(conn, ggc, &error);

	if (NULL != error)
		dief("xcb_get_geometry failed with error code: %d",
				(int)(error->error_code));

	/* the returned position by xcb_get_geometry is relative to the */
	/* parent window, the parent window isn't necessarily the root window */
	/* so we need to translate the top left coordinate of the window */
	/* to a coordinate relative to the root window */
	tcc = xcb_translate_coordinates(conn, window, ggr->root, 0, 0);
	tcr = xcb_translate_coordinates_reply(conn, tcc, &error);

	if (NULL != error)
		dief("xcb_translate_coordinates failed with error code: %d",
				(int)(error->error_code));

	*x = tcr->dst_x;
	*y = tcr->dst_y;
	*width = ggr->width;
	*height = ggr->height;
	*root = ggr->root;

	free(gwar);
	free(ggr);
	free(tcr);
}

static void
screenshot(xcb_connection_t *conn, xcb_window_t window,
           const char *dir, bool print_path)
{
	FILE *fp;
	int16_t x, y;
	uint16_t width, height;
	xcb_window_t root;
	const xcb_setup_t *setup;
	xcb_generic_error_t *error;
	xcb_get_image_cookie_t cookie;
	xcb_get_image_reply_t *reply;
	uint8_t *pixels;
	int i, spixels, bpp;
	int choff[3];
	time_t t;
	const struct tm *now;
	struct stat sb;
	char date[SCREENSHOT_DATE_LENGTH];
	char path[PATH_MAX], abpath[PATH_MAX];
	png_struct *png;
	png_info *pnginfo;
	png_byte *row;

	get_window_info(conn, window, &x, &y, &width, &height, &root);

	setup = xcb_get_setup(conn);

	cookie = xcb_get_image(
		conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
		root, x, y, width, height,
		XCB_PLANES_ALL_PLANES
	);

	reply = xcb_get_image_reply(conn, cookie, &error);

	if (NULL != error)
		dief("xcb_get_image failed with error code: %d",
				(int)(error->error_code));

	pixels = xcb_get_image_data(reply);
	spixels = xcb_get_image_data_length(reply);
	bpp = (spixels * 8) / (width * height);

	if (bpp != 32)
		dief("invalid pixel format received, expected: 32bpp got: %dbpp", bpp);

	t = time(NULL);
	now = localtime(&t);

	strftime(date, SCREENSHOT_DATE_LENGTH, SCREENSHOT_DATE_FORMAT, now);
	snprintf(path, PATH_MAX, "%s/%s_%d.png", dir, date, getpid() % 10);

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

	if (!S_ISDIR(sb.st_mode))
		dief("not a directory: %s", dir);

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

	if (print_path)
		printf("%s\n", realpath(path, abpath) == NULL ? path : abpath);

	/*                      setup->image_byte_order                        */
	/*     0 -> XCB_IMAGE_ORDER_LSB_FIRST (bgra) -> [ r:2, g: 1, b:0 ]     */
	/*     1 -> XCB_IMAGE_ORDER_MSB_FIRST (argb) -> [ r:1, g: 2, b:3 ]     */
	for (i = 0; i < 3; ++i) {
		switch (setup->image_byte_order) {
			case XCB_IMAGE_ORDER_MSB_FIRST: choff[i] = i + 1; break;
			case XCB_IMAGE_ORDER_LSB_FIRST: choff[i] = 2 - i; break;
		}
	}

	if (NULL == (png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
		die("png_create_write_struct failed");

	if (NULL == (pnginfo = png_create_info_struct(png)))
		die("png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png)) != 0)
		die("aborting due to libpng error");

	png_init_io(png, fp);

	png_set_IHDR(
		png, pnginfo, width, height, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE
	);

	png_write_info(png, pnginfo);
	png_set_compression_level(png, 3);

	row = malloc(width * 3);

	for (y = 0; y < height; ++y) {
		for (x = 0; x < width; ++x) {
			row[x*3+0] = pixels[4*(y*width+x)+choff[0]];
			row[x*3+1] = pixels[4*(y*width+x)+choff[1]];
			row[x*3+2] = pixels[4*(y*width+x)+choff[2]];
		}
		png_write_row(png, row);
	}

	png_write_end(png, NULL);
	png_free_data(png, pnginfo, PNG_FREE_ALL, -1);
	png_destroy_write_struct(&png, NULL);
	fclose(fp);
	free(row);
	free(reply);
}

int
main(int argc, char **argv)
{
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	xcb_window_t wid;
	const char *dir, *swid;
	bool print_path;

	dir = ".";
	swid = NULL;
	print_path = false;

	while (++argv, --argc > 0) {
		if ((*argv)[0] == '-' && (*argv)[1] != '\0' && (*argv)[2] == '\0') {
			switch ((*argv)[1]) {
				case 'h': usage(); break;
				case 'v': version(); break;
				case 'p': print_path = true; break;
				case 'd': --argc; dir = enotnull(*++argv, "directory"); break;
				case 'w': --argc; swid = enotnull(*++argv, "id"); break;
				default: dief("invalid option %s", *argv); break;
			}
		} else {
			dief("unexpected argument: %s", *argv);
		}
	}

	if (xcb_connection_has_error(conn = xcb_connect(NULL, NULL)))
		die("can't open display");

	if (NULL == swid) {
		if (NULL == (screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data)) {
			xcb_disconnect(conn);
			die("can't get default screen");
		}
		wid = screen->root;
	} else if (parse_window_id(swid, &wid) < 0) {
		xcb_disconnect(conn);
		die("invalid window id format");
	}

	screenshot(conn, wid, dir, print_path);
	xcb_disconnect(conn);

	return 0;
}
