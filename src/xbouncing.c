#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>
#include <time.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "img/dvdlogo.xbm"
#include "img/dvdlogo_mask.xbm"
_Static_assert(dvdlogo_width == dvdlogo_mask_width);
_Static_assert(dvdlogo_height == dvdlogo_mask_height);

#ifndef M_PI
#define M_PI		3.14159265358979323846	/* pi */
#endif

static unsigned long FILL_COLORS[] = {
	0xbe00ffff,
	0x00feffff,
	0xff8300ff,
	0x0026ffff,
	0xfffa01ff,
	0xff2600ff,
	0xff008bff,
	0x25ff01ff
};
#define NB_FILL_COLORS (sizeof(FILL_COLORS) / sizeof(unsigned long))

struct {
	Display *dpy;
	int scr_num;
	Screen *scr;
	unsigned int depth;
	unsigned long wndmask;
	XSetWindowAttributes wndattr;
	Window rwnd, mywnd;
	short refresh_rate;
	struct timespec refresh_interval;
	Pixmap pm[NB_FILL_COLORS];
	GC gc[NB_FILL_COLORS];
	Pixmap mask;
} x11;

struct {
	double vel;
} param;

struct vec2 {
	double x, y;
};

struct {
	struct vec2 dir;
	struct vec2 p1, p2;
	double w, h;
	size_t color_ptr;
} state;

static int main_loop (void);
static bool init_x11 (void);
static void deinit_x11 (void);
static bool is_networked_x11 (Display *d);
static bool MakeAlwaysOnTop (Display* display, Window root, Window mywin);

/*
 * --monitor
 * --logo
 * --width
 * --height
 */
int main (const int argc, const char **argv) {
	int ec = 0;

	param.vel = 300;
	state.dir.x = sin(45.0 / 360.0 * M_PI * 2.0);
	state.dir.y = cos(45.0 / 360.0 * M_PI * 2.0);
	state.p1.x = 0;
	state.p1.y = 0;
	state.p2.x = state.w = dvdlogo_width;
	state.p2.y = state.h = dvdlogo_height;

	if (!init_x11()) {
		ec = 1;
		goto END;
	}

	// - load xbm
	// - init X11
	// - Select monitor
	// - do the main loop

	ec = main_loop();

END:
	deinit_x11();
	return ec;
}

static void update_screen_info (void) {
	XRRScreenConfiguration *conf = XRRGetScreenInfo(x11.dpy, x11.mywnd);

	if (conf == NULL) {
		return;
	}

	x11.refresh_rate = XRRConfigCurrentRate(conf);
	XRRFreeScreenConfigInfo(conf);

	if (x11.refresh_rate <= 0) {
		x11.refresh_rate = 10;
	}
	else if (is_networked_x11(x11.dpy) && x11.refresh_rate > 60) { // TODO: parameterise
		// "human eyes can't see 120hz"
		// that amount of requests per second won't be possible over the
		// networked X11 connection anyways.
		x11.refresh_rate = 60;
	}
	x11.refresh_interval.tv_sec = 0;
	x11.refresh_interval.tv_nsec = 1000000000 / x11.refresh_rate;
}

static void print_l18n_xopen_err (void) {
	const char *LANG = getenv("LANG");
	const char *msg = "Could not open X display.";

	if (LANG != NULL) {
		if (strncmp(LANG, "ko", 2) == 0) {
			msg = "X 서버 연결 실패.";
		}
	}

	fprintf(stderr, "%s\n", msg);
}

static bool init_x11 (void) {
	XSizeHints hints = { 0, };
	XGCValues gcv = { 0, };

	x11.dpy = XOpenDisplay(NULL);
	if (x11.dpy == NULL) {
		print_l18n_xopen_err();
		return false;
	}
	// after this point, X11 will call exit() upon error.
	x11.scr_num = DefaultScreen(x11.dpy); // TODO: parameterise
	x11.scr = XScreenOfDisplay(x11.dpy, x11.scr_num);
	x11.rwnd = RootWindowOfScreen(x11.scr);
	x11.depth = DefaultDepth(x11.dpy, x11.scr_num);

	x11.wndmask = CWBackingPixel | CWOverrideRedirect;
	x11.wndattr.override_redirect = True;

	hints.flags = PPosition | PSize;
	hints.x = (int)state.p1.x;
	hints.y = (int)state.p1.y;
	hints.width = state.w;
	hints.height = state.h;

	x11.mywnd = XCreateWindow(
		x11.dpy,
		x11.rwnd,
		0,
		0,
		state.w,
		state.h,
		0,
		x11.depth,
		InputOutput,
		CopyFromParent,
		x11.wndmask,
		&x11.wndattr);
	XSetNormalHints(x11.dpy, x11.mywnd, &hints);
	XSelectInput(
		x11.dpy,
		x11.mywnd,
		ExposureMask | VisibilityChangeMask | KeyPressMask);
	XMapWindow(x11.dpy, x11.mywnd);
	MakeAlwaysOnTop(x11.dpy, x11.rwnd, x11.mywnd);

	for (size_t i = 0; i < NB_FILL_COLORS; i += 1) {
		x11.pm[i] = XCreatePixmapFromBitmapData(
			x11.dpy,
			x11.mywnd,
			dvdlogo_bits,
			dvdlogo_width,
			dvdlogo_height,
			0x00000000,
			FILL_COLORS[i],
			x11.depth);

		gcv.function = GXcopy;
		gcv.fill_style = FillTiled;
		gcv.ts_x_origin = 0;
		gcv.ts_y_origin = 0;
		gcv.tile = x11.pm[i];
		x11.gc[i] = XCreateGC(
			x11.dpy,
			x11.mywnd,
			GCFunction | GCTile | GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle,
			&gcv);
	}

	x11.mask = XCreateBitmapFromData(
		x11.dpy,
		x11.mywnd,
		dvdlogo_mask_bits,
		dvdlogo_mask_width,
		dvdlogo_mask_height);

	XFlush(x11.dpy);

	update_screen_info();

	return true;
}

static void deinit_x11 (void) {
	if (x11.dpy != NULL) {
		XCloseDisplay(x11.dpy);
	}
}

static bool is_ssh (void) {
	const char *env = getenv("SSH_CONNECTION");
	const bool ret = env != NULL && strlen(env) > 0;

	if (false && ret) {
		fprintf(stderr, "is_ssh() returned true.\n"); // FIXME
	}

	return ret;
}

static bool is_networked_display (Display *d) {
	const int fd = ConnectionNumber(d);
	struct sockaddr_storage addr;
	socklen_t sl = sizeof(addr);

	if (getsockname(fd, (struct sockaddr*)&addr, &sl) != 0) {
		return false;
	}

	switch (addr.ss_family) {
	case AF_INET:
	case AF_INET6:
		// FIXME
		false && fprintf(stderr, "is_networked_display() returned true.\n");
		return true;
	}
	return false;
}

static bool is_networked_x11 (Display *d) {
	return is_networked_display(d) || is_ssh();
}

#define _NET_WM_STATE_REMOVE		0	// remove/unset property
#define _NET_WM_STATE_ADD			1	// add/set property
#define _NET_WM_STATE_TOGGLE		2	// toggle property

static bool MakeAlwaysOnTop (Display* display, Window root, Window mywin) {
	Atom wmStateAbove = XInternAtom( display, "_NET_WM_STATE_ABOVE", 1 );
	if( wmStateAbove == None ) {
		return false;
	}

	Atom wmNetWmState = XInternAtom( display, "_NET_WM_STATE", 1 );
	if( wmNetWmState == None ) {
		return false;
	}

	// set window always on top hint
	if( wmStateAbove != None )
	{
		XClientMessageEvent xclient;
		memset( &xclient, 0, sizeof (xclient) );
		//
		//window  = the respective client window
		//message_type = _NET_WM_STATE
		//format = 32
		//data.l[0] = the action, as listed below
		//data.l[1] = first property to alter
		//data.l[2] = second property to alter
		//data.l[3] = source indication (0-unk,1-normal app,2-pager)
		//other data.l[] elements = 0
		//
		xclient.type = ClientMessage;
		xclient.window = mywin;			  // GDK_WINDOW_XID(window);
		xclient.message_type = wmNetWmState; //gdk_x11_get_xatom_by_name_for_display( display, "_NET_WM_STATE" );
		xclient.format = 32;
		xclient.data.l[0] = _NET_WM_STATE_ADD; // add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
		xclient.data.l[1] = wmStateAbove;	  //gdk_x11_atom_to_xatom_for_display (display, state1);
		xclient.data.l[2] = 0;				 //gdk_x11_atom_to_xatom_for_display (display, state2);
		xclient.data.l[3] = 0;
		xclient.data.l[4] = 0;
		//gdk_wmspec_change_state( FALSE, window,
		//  gdk_atom_intern_static_string ("_NET_WM_STATE_BELOW"),
		//  GDK_NONE );
		XSendEvent(display,
			//mywin - wrong, not app window, send to root window!
			root, // <-- DefaultRootWindow( display )
			false,
			SubstructureRedirectMask | SubstructureNotifyMask,
			(XEvent *)&xclient );

		return true;
	}

	return false;
}

static void proc_key (XKeyEvent *evt, bool *loop_flag) {
	int len;
	char buf[2] = { 0, };

	len = XLookupString(evt, buf, 1, NULL, NULL);
	if (len > 0) {
		switch (buf[0]) {
		case 0x1B:
		case 'q':
		case 'Q':
			*loop_flag = false;
			break;
		}
	}
}

static void draw (void) {
	XShapeCombineMask(
		x11.dpy,
		x11.mywnd,
		ShapeBounding,
		0,
		0,
		x11.mask,
		ShapeSet);
	XFillRectangle(
		x11.dpy,
		x11.mywnd,
		x11.gc[state.color_ptr],
		0,
		0,
		state.w,
		state.h);
}

static void proc_event (bool *loop_flag, bool *redraw) {
	XEvent evt;

	while (XPending(x11.dpy)) {
		XNextEvent(x11.dpy, &evt);

		switch (evt.type) {
		case Expose:
			*redraw = true;
			break;
		case KeyPress:
			proc_key((XKeyEvent*)&evt, loop_flag);
			break;
		}
	}
}

static int get_geo_nullable (
		Display *dpy,
		Drawable d,
		Window *out_wnd,
		int *out_x,
		int *out_y,
		unsigned int *out_w,
		unsigned int *out_h,
		unsigned int *out_bw,
		unsigned int *out_dph)
{
#define CHK_ASS(l, r) if ((l) != NULL) { *(l) = (r); }
	Window wnd;
	int x, y;
	unsigned int w, h, bw, dph;
	int ret;

	ret = XGetGeometry(dpy, d, &wnd, &x, &y, &w, &h, &bw, &dph);
	if (ret) {
		CHK_ASS(out_wnd, wnd);
		CHK_ASS(out_x, x);
		CHK_ASS(out_y, y);
		CHK_ASS(out_w, w);
		CHK_ASS(out_h, h);
		CHK_ASS(out_bw, bw);
		CHK_ASS(out_dph, dph);
	}

	return ret;
#undef CHK_ASS
}

static void do_move (bool *redraw, const double v_factor) {
	double dx, dy;
	const unsigned int root_w = XWidthOfScreen(x11.scr);
	const unsigned int root_h = XHeightOfScreen(x11.scr);

	dx = state.dir.x * param.vel / x11.refresh_rate * v_factor;
	dy = state.dir.y * param.vel / x11.refresh_rate * v_factor;
	state.p1.x += dx;
	state.p1.y += dy;
	state.p2.x += dx;
	state.p2.y += dy;
	XMoveWindow(x11.dpy, x11.mywnd, (int)state.p1.x, (int)state.p1.y);

	// north
	if (state.p1.y < 0) {
		state.p1.y = 0;
		state.p2.y = state.h;
		state.dir.y = -state.dir.y;
		state.color_ptr = (state.color_ptr + 1) % NB_FILL_COLORS;
		*redraw = true;
	}
	// east
	if (state.p2.x > root_w) {
		state.p2.x = root_w;
		state.p1.x = root_w - state.w;
		state.dir.x = -state.dir.x;
		state.color_ptr = (state.color_ptr + 1) % NB_FILL_COLORS;
		*redraw = true;
	}
	// south
	if (state.p2.y > root_h) {
		state.p2.y = root_h;
		state.p1.y = root_h - state.h;
		state.dir.y = -state.dir.y;
		state.color_ptr = (state.color_ptr + 1) % NB_FILL_COLORS;
		*redraw = true;
	}
	// west
	if (state.p1.x < 0) {
		state.p1.x = 0;
		state.p2.x = state.w;
		state.dir.x = -state.dir.x;
		state.color_ptr = (state.color_ptr + 1) % NB_FILL_COLORS;
		*redraw = true;
	}
}

static void sub_ts (
		struct timespec *out,
		const struct timespec *a,
		const struct timespec *b)
{
	if (a->tv_nsec < b->tv_nsec) {
		out->tv_sec = a->tv_sec - 1 - b->tv_sec;
		out->tv_nsec = 1000000000 + a->tv_nsec - b->tv_nsec;
	}
	else {
		out->tv_sec = a->tv_sec - b->tv_sec;
		out->tv_nsec = a->tv_nsec - b->tv_nsec;
	}
}

static int cmp_ts (const struct timespec *a, const struct timespec *b) {
	if (a->tv_sec < b->tv_sec) {
		return -1;
	}
	else if (a->tv_sec > b->tv_sec) {
		return 1;
	}

	return a->tv_nsec < b->tv_nsec ? -1 : a->tv_nsec > b->tv_nsec ? 1 : 0;
}

static double ts2d (const struct timespec *x) {
	return (double)x->tv_nsec / 1000000000 + x->tv_sec;
}

static int main_loop (void) {
	bool loop_flag = true;
	bool redraw;
	struct timespec ts_now;
	struct timespec ts_last_tick;
	struct timespec ts_delta;
	double v_factor = 1.0;

	clock_gettime(CLOCK_MONOTONIC, &ts_now);
	memcpy(&ts_last_tick, &ts_now, sizeof(struct timespec));
	while (true) {
		redraw = false;

		proc_event(&loop_flag, &redraw);

		if (!loop_flag) {
			break;
		}

		if (false) {
			fprintf(stderr, "v_factor: %.6lf\n", v_factor);
		}

		do_move(&redraw, v_factor);
		if (redraw) {
			draw();
		}
		XFlush(x11.dpy);

		if (false) {
			// manually introduce network delay for testing
			ts_delta.tv_sec = 0;
			ts_delta.tv_nsec = 50000000; // 50ms
			nanosleep(&ts_delta, NULL);
		}

		clock_gettime(CLOCK_MONOTONIC, &ts_now);
		sub_ts(&ts_delta, &ts_now, &ts_last_tick);

		v_factor = 1.0 + ts2d(&ts_delta) / ts2d(&x11.refresh_interval);

		if (cmp_ts(&ts_delta, &x11.refresh_interval) < 0) {
			struct timespec ts_sleep;

			sub_ts(&ts_sleep, &x11.refresh_interval, &ts_delta);
			nanosleep(&ts_sleep, NULL);
		}

		clock_gettime(CLOCK_MONOTONIC, &ts_last_tick);
	}

	return 0;
}
