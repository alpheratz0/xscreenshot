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
#include <xcb/xfixes.h>

#define SCREENSHOT_DATE_FORMAT ("%Y%m%d%H%M%S")
#define SCREENSHOT_DATE_LENGTH (sizeof("20220612093950"))
#define XCB_PLANES_ALL_PLANES ((uint32_t)(~0UL))
#define ALPHA_BLEND(a, b, alpha) (a + ((b - a) * alpha) / 255)

static void
die(const char *fmt, ...)
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
		die("%s cannot be null", name);
	return str;
}

static void
usage(void)
{
	puts("usage: xscreenshot [-chpv] [-d directory] [-w id]");
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
	xcb_get_geometry_reply_t *ggr, *rggr;
	xcb_translate_coordinates_cookie_t tcc;
	xcb_translate_coordinates_reply_t *tcr;

	gwac = xcb_get_window_attributes(conn, window);
	gwar = xcb_get_window_attributes_reply(conn, gwac, &error);

	if (NULL != error && error->error_code == XCB_WINDOW)
		die("the specified window does not exist");

	if (NULL != error)
		die("xcb_get_window_attributes failed with error code: %hhu", error->error_code);

	if (gwar->_class != XCB_WINDOW_CLASS_INPUT_OUTPUT)
		die("the specified window is not an input/output window");

	if (gwar->map_state != XCB_MAP_STATE_VIEWABLE)
		die("the specified window is not visible/mapped");

	ggc = xcb_get_geometry(conn, window);
	ggr = xcb_get_geometry_reply(conn, ggc, &error);

	if (NULL != error)
		die("xcb_get_geometry failed with error code: %hhu", error->error_code);

	/* the returned position by xcb_get_geometry is relative to the */
	/* parent window, the parent window isn't necessarily the root window */
	/* so we need to translate the top left coordinate of the window */
	/* to a coordinate relative to the root window */
	tcc = xcb_translate_coordinates(conn, window, ggr->root, 0, 0);
	tcr = xcb_translate_coordinates_reply(conn, tcc, &error);

	if (NULL != error)
		die("xcb_translate_coordinates failed with error code: %hhu", error->error_code);

	*x = tcr->dst_x;
	*y = tcr->dst_y;
	*width = ggr->width;
	*height = ggr->height;
	*root = ggr->root;

	/* obtain the root window geometry and adjust the rect */
	/* of the target window to make it that every point inside */
	/* that rect is inside the root window rect */
	ggc = xcb_get_geometry(conn, ggr->root);
	rggr = xcb_get_geometry_reply(conn, ggc, &error);

	if (NULL != error)
		die("xcb_get_geometry failed with error code: %hhu", error->error_code);

	if (*x < 0) *width += *x, *x = 0;
	if (*y < 0) *height += *y, *y = 0;
	if (*x + *width > rggr->width) *width = rggr->width - *x;
	if (*y + *height > rggr->height) *height = rggr->height - *y;

	free(gwar);
	free(ggr);
	free(rggr);
	free(tcr);
}

static void
get_focused_window_root(xcb_connection_t *conn, xcb_window_t *window)
{
	xcb_generic_error_t *error;
	xcb_get_input_focus_cookie_t gifc;
	xcb_get_input_focus_reply_t *gifr;
	xcb_get_geometry_cookie_t ggc;
	xcb_get_geometry_reply_t *ggr;

	gifc = xcb_get_input_focus(conn);
	gifr = xcb_get_input_focus_reply(conn, gifc, &error);

	if (NULL != error)
		die("xcb_get_input_focus failed with error code: %hhu", error->error_code);

	ggc = xcb_get_geometry(conn, gifr->focus);
	ggr = xcb_get_geometry_reply(conn, ggc, &error);

	if (NULL != error)
		die("xcb_get_geometry failed with error code: %hhu", error->error_code);

	*window = ggr->root;

	free(gifr);
	free(ggr);
}

static void
screenshot(xcb_connection_t *conn, xcb_window_t window,
           bool include_cursor, const char *dir, bool print_path)
{
	FILE *fp;
	int16_t x, y;
	uint16_t width, height;
	xcb_window_t root;
	const xcb_setup_t *setup;
	xcb_generic_error_t *error;
	xcb_get_image_cookie_t gic;
	xcb_get_image_reply_t *gir;
	xcb_xfixes_query_version_cookie_t xfqvc;
	xcb_xfixes_query_version_reply_t *xfqvr;
	xcb_xfixes_get_cursor_image_cookie_t xfgcic;
	xcb_xfixes_get_cursor_image_reply_t *xfgcir;
	uint8_t *pixels, *cur_pixels, *pixel, *cur_pixel;
	int bpp, choff[3], cur_rel_x, cur_rel_y;
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

	gic = xcb_get_image(
		conn, XCB_IMAGE_FORMAT_Z_PIXMAP, root, x, y,
		width, height, XCB_PLANES_ALL_PLANES
	);

	gir = xcb_get_image_reply(conn, gic, &error);

	if (NULL != error)
		die("xcb_get_image failed with error code: %hhu", error->error_code);

	pixels = xcb_get_image_data(gir);
	bpp = (xcb_get_image_data_length(gir) * 8) / (width * height);

	if (bpp != 32)
		die("invalid pixel format received, expected: 32bpp got: %dbpp", bpp);

	/*                      setup->image_byte_order                        */
	/*     0 -> XCB_IMAGE_ORDER_LSB_FIRST (bgra) -> [ r:2, g: 1, b:0 ]     */
	/*     1 -> XCB_IMAGE_ORDER_MSB_FIRST (argb) -> [ r:1, g: 2, b:3 ]     */
	if (setup->image_byte_order == XCB_IMAGE_ORDER_LSB_FIRST) {
		choff[0] = 2; choff[1] = 1; choff[2] = 0;
	} else if (setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST) {
		choff[0] = 1; choff[1] = 2; choff[2] = 3;
	}

	if (include_cursor) {
		xfqvc = xcb_xfixes_query_version(conn, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
		xfqvr = xcb_xfixes_query_version_reply(conn, xfqvc, &error);

		if (NULL != error)
			die("xcb_xfixes_query_version failed with error code: %hhu", error->error_code);

		xfgcic = xcb_xfixes_get_cursor_image(conn);
		xfgcir = xcb_xfixes_get_cursor_image_reply(conn, xfgcic, &error);

		if (NULL != error)
			die("xcb_xfixes_get_cursor_image failed with error code: %hhu", error->error_code);

		cur_pixels = (uint8_t *)(xcb_xfixes_get_cursor_image_cursor_image(xfgcir));
		cur_rel_x = xfgcir->x - xfgcir->xhot - x;
		cur_rel_y = xfgcir->y - xfgcir->yhot - y;

		for (y = 0; y < xfgcir->height; ++y) {
			if (y + cur_rel_y < 0 || y + cur_rel_y >= height)
				continue;
			for (x = 0; x < xfgcir->width; ++x) {
				if (x + cur_rel_x < 0 || x + cur_rel_x >= width)
					continue;
				pixel = &pixels[(y + cur_rel_y) * 4 * width + (x + cur_rel_x) * 4];
				cur_pixel = &cur_pixels[y * 4 * xfgcir->width + x * 4];
				pixel[choff[0]] = ALPHA_BLEND(pixel[choff[0]], cur_pixel[2], cur_pixel[3]);
				pixel[choff[1]] = ALPHA_BLEND(pixel[choff[1]], cur_pixel[1], cur_pixel[3]);
				pixel[choff[2]] = ALPHA_BLEND(pixel[choff[2]], cur_pixel[0], cur_pixel[3]);
			}
		}

		free(xfgcir);
		free(xfqvr);
	}

	t = time(NULL);
	now = localtime(&t);

	strftime(date, SCREENSHOT_DATE_LENGTH, SCREENSHOT_DATE_FORMAT, now);
	snprintf(path, PATH_MAX, "%s/%s_%d.png", dir, date, getpid() % 10);

	if (stat(dir, &sb) < 0)
		die("stat failed: %s", strerror(errno));

	if (!S_ISDIR(sb.st_mode))
		die("not a directory: %s", dir);

	if (NULL == (fp = fopen(path, "wb")))
		die("fopen failed: %s", strerror(errno));

	if (print_path)
		printf("%s\n", realpath(path, abpath) == NULL ? path : abpath);

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
			row[x * 3 + 0] = pixels[4 * (y * width + x) + choff[0]];
			row[x * 3 + 1] = pixels[4 * (y * width + x) + choff[1]];
			row[x * 3 + 2] = pixels[4 * (y * width + x) + choff[2]];
		}
		png_write_row(png, row);
	}

	png_write_end(png, NULL);
	png_free_data(png, pnginfo, PNG_FREE_ALL, -1);
	png_destroy_write_struct(&png, NULL);
	fclose(fp);
	free(row);
	free(gir);
}

int
main(int argc, char **argv)
{
	xcb_connection_t *conn;
	xcb_window_t wid;
	const char *dir, *swid;
	bool print_path, include_cursor;

	dir = ".";
	swid = NULL;
	include_cursor = print_path = false;

	while (++argv, --argc > 0) {
		if ((*argv)[0] == '-' && (*argv)[1] != '\0' && (*argv)[2] == '\0') {
			switch ((*argv)[1]) {
				case 'h': usage(); break;
				case 'v': version(); break;
				case 'p': print_path = true; break;
				case 'c': include_cursor = true; break;
				case 'd': --argc; dir = enotnull(*++argv, "directory"); break;
				case 'w': --argc; swid = enotnull(*++argv, "id"); break;
				default: die("invalid option %s", *argv); break;
			}
		} else {
			die("unexpected argument: %s", *argv);
		}
	}

	if (xcb_connection_has_error(conn = xcb_connect(NULL, NULL)))
		die("can't open display");

	if (NULL == swid) {
		get_focused_window_root(conn, &wid);
	} else if (parse_window_id(swid, &wid) < 0) {
		xcb_disconnect(conn);
		die("invalid window id format");
	}

	screenshot(conn, wid, include_cursor, dir, print_path);
	xcb_disconnect(conn);

	return 0;
}
