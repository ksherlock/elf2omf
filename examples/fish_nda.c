

#include <stdint.h>
#include <calypsi/intrinsics65816.h>

#include "tools.h"

#ifndef NULL
#define NULL (void *)0
#endif






/* NDA Action Codes */
#define eventAction 0x0001
#define runAction 0x0002
#define cursorAction 0x0003
#define undoAction 0x0005
#define cutAction 0x0006
#define copyAction 0x0007
#define pasteAction 0x0008
#define clearAction 0x0009
#define sysClickAction 0x000A
#define optionalCloseAction 0x000B
#define reOpenAction 0x000C

/* Event Codes */
#define nullEvt 0x0000
#define mouseDownEvt 0x0001
#define mouseUpEvt 0x0002
#define keyDownEvt 0x0003
#define autoKeyEvt 0x0005
#define updateEvt 0x0006
#define activateEvt 0x0008
#define switchEvt 0x0009
#define deskAccEvt 0x000A
#define driverEvt 0x000B
#define app1Evt 0x000C
#define app2Evt 0x000D
#define app3Evt 0x000E
#define app4Evt 0x000F

/* WFrame */
#define fHilited 0x0001                 /* Window is highlighted */
#define fZoomed 0x0002                  /* Window is zoomed */
#define fAllocated 0x0004               /* Window record was allocated */
#define fCtlTie 0x0008                  /* Window state tied to controls */
#define fInfo 0x0010                    /* Window has an information bar */
#define fVis 0x0020                     /* Window is visible */
#define fQContent 0x0040
#define fMove 0x0080                    /* Window is movable */
#define fZoom 0x0100                    /* Window is zoomable */
#define fFlex 0x0200
#define fGrow 0x0400                    /* Window has grow box */
#define fBScroll 0x0800                 /* Window has horizontal scroll bar */
#define fRScroll 0x1000                 /* Window has vertical scroll bar */
#define fAlert 0x2000
#define fClose 0x4000                   /* Window has a close box */
#define fTitle 0x8000                   /* Window has a title bar */




struct window;
typedef struct window window;
typedef struct window *WindowPtr;

extern uint32_t ReadTime(void);

extern void SetSysWindow(window *);
extern void CloseWindow(window *);

extern void BeginUpdate(window *);
extern void EndUpdate(window *);

extern window *NewWindow(void *);


extern void MoveTo(int h, int v);
extern void DrawCString(const char *);
extern void SetBackColor(uint16_t);
extern void SetSolidBackPat(uint16_t);
extern void SetBackPat(Pattern);
extern void PaintRect(RectPtr);
extern void EraseRect(RectPtr);
extern unsigned GetMasterSCB(void);

extern uint16_t CStringWidth(const char *);

extern void SetFontFlags(uint16_t);
extern void SetTextMode(uint16_t);

extern TimeRec ReadTimeHex(void);


extern window *GetPort(void);
extern void SetPort(window *);

extern void InvalRect(RectPtr);


extern void FindFontStats(FontID, uint16_t, uint16_t, FontStatRecPtr);
extern void InstallFont(uint32_t, uint16_t);
extern uint16_t FMStatus(void);

// extern void **GetFont(void);

extern uint16_t GetFontLore(FontGlobalsRecPtr, uint16_t);

static WindowPtr winPtr;


static char title[] = "\x06" " fish ";

#define WINDOW_WIDTH 230
#define WINDOW_HEIGHT 44

static WindColor colors = {
	0x0000, 0x0f00,
	0x020e, 0xf0ff,
	0x00f0 

};
static ParamList template = {

	78,					/* paramLength */
	fTitle | fClose | fVis | fMove | fAllocated,				/* wFrameBits */
	title,				/* wTitle */
	0L,					/* wRefCon */
	{0,0,0,0},				/* wZoom */
	&colors, 				/* wColor */
	0,0,					/* wYOrigin,wXOrigin */
	0,0,					/* wDataH,wDataW */
	0,0,					/* wMaxH,wMaxW */
	0,0,					/* wScrollVer,wScrollHor */
	0,0,					/* wPageVer,wPageHor */
	0,					/* wInfoRefCon */
	0,					/* wInfoHeight */
	NULL, 				/* wFrameDefProc */
	NULL, 				/* wInfoDefProc */
	NULL, 				/* wContDefProc */
	{40, 40, 40 + WINDOW_HEIGHT, 40 + WINDOW_WIDTH}, /* wPosition */
	(void *) -1L, 			/* wPlane */
	NULL					/* wStorage */

};

static Rect bounds = { 0, 0, WINDOW_HEIGHT, WINDOW_WIDTH};

uint32_t now = 0;

unsigned click = 0;
unsigned fm = 0;

// unix -
// 1/1/1904: -2082826800
// 2/24/1921: -1541617200
// 1/26/2016: 1453784400
const uint32_t av_dob = 0x20423400; // feb 24, 1921 
const uint32_t av_dod = 0xd2cc6780; // january 26, 2016



const char *get_status() {
	// xd0 - en dash
	// xd1 - em dash
	// xd5 - fancy '

	if ((click & 0x0e) == 0x0e) {

		return click & 0x01 ?
			"Jan 21, 1967 \xd0 Apr 24, 2011" :
			"In memory of Ryan Suenaga";
	}

	if (now > av_dod) {
		return click & 0x01 ?
			"Feb 24, 1921 \xd0 Jan 26, 2016" :
			"Abe Vigoda is dead.";
	}
	if (now > av_dob)
		return click & 0x01 ?
			"Feb 24, 1921 \xd0 " :
			"Abe Vigoda is alive.";
	
	return "Abe Vigoda isn\xd5t.";
}

void DrawWindow(void) {


	// a nice yellow pattern in 640 mode.
	#define _(x) x, x, x, x
	static Pattern pattern = {
		_(0xee),
		_(0xee),
		_(0xee),
		_(0xee),
		_(0xee),
		_(0xee),
		_(0xee),
		_(0xee)
	};
	#undef _


	// static FontGlobalsRecord fgr;
	// GetFontLore(&fgr, sizeof(fgr));
	if (fm) {
		static FontID FF = {
			{ venice, 0, 14}
		};
		InstallFont(FF.fidLong, 0);
	}

	SetFontFlags(4); // dithered text background color
	// SetTextMode(0);

	if (GetMasterSCB() & 0x80) {
		// 640 mode
		SetBackPat(pattern);
		SetBackColor(0xeeee);
	} else {
		// 320 mode
		// 0x99 is yellow but it's too yellow.
		// title bar will still be grey but eh.
		SetSolidBackPat(0xffff);
		SetBackColor(0xffff);
	}
	EraseRect(&bounds);

	const char *msg = get_status();

	unsigned w = CStringWidth(msg);
	if (w < WINDOW_WIDTH)
		MoveTo( (WINDOW_WIDTH - w)>> 1, 25);
	else
		MoveTo(10, 25);

	DrawCString(msg);
}


#if 0
void InvalidateWindow(WindowPtr win) {

	if (!win) return;
	WindowPtr port = GetPort();
	SetPort(win);
	InvalRect(&win->portRect);
	SetPort(port);
}
#endif

extern unsigned _toolErr;

#if 0
void **font_handle = NULL;

void LoadFont(void) {

	static FontID FF = {
		{ venice, 0, 14}
	};
	FontStatRec fsr;

	FindFontStats(FF, 0, 1, &fsr);
	if (_toolErr || (fsr.resultStats & notFoundBit))
		return;

	InstallFont(FF, 0);
	if (_toolErr) return;

	font_handle = (Handle)GetFont();
}
#endif

// __attribute__((task)) means pseudo registers
// don't need to be saved and restored
// (which is true since these are the entry points.)

__attribute__((task))
void nda_init(int code) {
	if (!code && winPtr) {
		CloseWindow(winPtr);
		winPtr = 0;
	}
}

__attribute__((task))
void nda_close(void) {
	if (winPtr) {
		CloseWindow(winPtr);
		winPtr = 0;
	}
}

__attribute__((task))
window *nda_open(void) {

	winPtr = NewWindow(&template);
	if (winPtr) {
		SetSysWindow(winPtr);
	}

	now = ReadTime();
	fm = FMStatus();
	click = 0;

	return winPtr;
}


__attribute__((task))
int nda_action(int code, unsigned long param) {

	EventRecordPtr evPtr;
	switch (code) {
	case eventAction:
		evPtr = (EventRecordPtr)param;
		if (evPtr->what == updateEvt) {
			// n.b. - BeginUpdate / EndUpdate save/restore the port
			BeginUpdate(winPtr);
			DrawWindow();
			EndUpdate(winPtr);
			return 1;
		}
		if (evPtr->what == mouseDownEvt) {
			++click;
			WindowPtr port = GetPort();
			SetPort(winPtr);
			InvalRect(&bounds);
			SetPort(port);
			return 1;
		}
		break;
	}
	return 0;
}
