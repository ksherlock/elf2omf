

typedef uint8_t Pattern[32];

struct Point {
	uint16_t v;
	uint16_t h;
};
typedef struct Point Point, *PointPtr;

struct Rect {
	uint16_t v1;
	uint16_t h1;
	uint16_t v2;
	uint16_t h2;
};
typedef struct Rect Rect, *RectPtr, **RectHndl;


struct TimeRec {
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t year;
	uint8_t day;
	uint8_t month;
	uint8_t extra;
	uint8_t weekDay;
};
typedef struct TimeRec TimeRec, *TimeRecPtr, **TimeRecHndl;


struct EventRecord {
	uint16_t what;
	uint32_t message;
	uint32_t when;
	Point where;
	uint16_t modifiers;
	uint32_t wmTaskData;
	uint32_t wmTaskMask;
	uint32_t wmLastClickTick;
	uint16_t wmClickCount;
	uint32_t wmTaskData2;
	uint32_t wmTaskData3;
	uint32_t wmTaskData4;
	Point wmLastClickPt;
};
typedef struct EventRecord EventRecord, *EventRecordPtr, **EventRecordHndl;



struct WindColor {
	uint16_t frameColor;                     /* Color of window frame */
	uint16_t titleColor;                     /* Color of title and bar */
	uint16_t tBarColor;                      /* Color/pattern of title bar */
	uint16_t growColor;                      /* Color of grow box */
	uint16_t infoColor;                      /* Color of information bar */
};
typedef struct WindColor WindColor, *WindColorPtr, **WindColorHndl;


struct ParamList {
	uint16_t paramLength;
	uint16_t wFrameBits;
	void * wTitle;
	uint32_t wRefCon;
	Rect wZoom;
	WindColorPtr wColor;
	uint16_t wYOrigin;
	uint16_t wXOrigin;
	uint16_t wDataH;
	uint16_t wDataW;
	uint16_t wMaxH;
	uint16_t wMaxW;
	uint16_t wScrollVer;
	uint16_t wScrollHor;
	uint16_t wPageVer;
	uint16_t wPageHor;
	uint32_t wInfoRefCon;
	uint16_t wInfoHeight;                    /* height of information bar */
	void * wFrameDefProc;
	void * wInfoDefProc;
	void * wContDefProc;
	Rect wPosition;
	void * wPlane;
	void * wStorage;
};
typedef struct ParamList ParamList, *ParamListPtr, **ParamListHndl;


/* Font Family Numbers */
#define chicago 0xFFFD
#define shaston 0xFFFE
#define systemFont0 0x0000
#define systemFont1 0x0001
#define newYork 0x0002
#define geneva 0x0003
#define monaco 0x0004
#define venice 0x0005
#define london 0x0006
#define athens 0x0007
#define sanFrancisco 0x0008
#define toronto 0x0009
#define cairo 0x000B
#define losAngeles 0x000C
#define zapfDingbats 0x000D
#define bookman 0x000E
#define helveticaNarrow 0x000F
#define palatino 0x0010
#define zapfChancery 0x0012
#define times 0x0014
#define helvetica 0x0015
#define courier 0x0016
#define symbol 0x0017
#define taliesin 0x0018
#define avanteGarde 0x0021
#define newCenturySchoolbook 0x0022
#define baseOnlyBit 0x0020              /* FamSpecBits */
#define notBaseBit 0x0020               /* FamStatBits */
#define memOnlyBit 0x0001               /* FontSpecBits */
#define realOnlyBit 0x0002              /* FontSpecBits */
#define anyFamBit 0x0004                /* FontSpecBits */
#define anyStyleBit 0x0008              /* FontSpecBits */
#define anySizeBit 0x0010               /* FontSpecBits */
#define memBit 0x0001                   /* FontStatBits */
#define unrealBit 0x0002                /* FontStatBits */
#define apFamBit 0x0004                 /* FontStatBits */
#define apVarBit 0x0008                 /* FontStatBits */
#define purgeBit 0x0010                 /* FontStatBits */
#define notDiskBit 0x0020               /* FontStatBits */
#define notFoundBit 0x8000              /* FontStatBits */
#define dontScaleBit 0x0001             /* Scale Word */


union FontID {
	struct {
		uint16_t famNum;
		uint8_t fontStyle;
		uint8_t fontSize;
	} fidRec;
	uint32_t fidLong;
};
typedef union FontID FontID, *FontIDPtr, **FontIDHndl;


struct FontStatRec {
	FontID resultID;
	uint16_t resultStats;
};
typedef struct FontStatRec FontStatRec, *FontStatRecPtr, **FontStatRecHndl;



struct FontGlobalsRecord {
	uint16_t fgFontID;                       /* currently 12 bytes long, but may be expanded */
	uint16_t fgStyle; 
	uint16_t fgSize; 
	uint16_t fgVersion; 
	uint16_t fgWidMax; 
	uint16_t fgFBRExtent; 
};
typedef struct FontGlobalsRecord FontGlobalsRecord, *FontGlobalsRecPtr, **FontGlobalsRecHndl;
