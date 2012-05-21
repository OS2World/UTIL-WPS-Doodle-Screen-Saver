#define  INC_DOS
#define  INCL_DOSSEMAPHORES
#define  INCL_DOSEXCEPTIONS
#define  INCL_DOSPROCESS
#define  INCL_DOSERRORS
#define  INCL_WIN
#define  INCL_GPI

#include <os2.h>

//avoid mutliple time including
#ifndef _HWVIDEO_
#define _HWVIDEO_

//C++ case
#ifdef __cplusplus
extern "C" {
#endif

// GRADD function class
#define VA2_FUNCTION_CLASS     "Video Acceleration 2"

typedef struct _HWVIDEOCAPS {
  ULONG       ulLength;
  ULONG       ulCapsFlags;     //flags, describing HW capability
  SIZEL       szlSrcMax;       //maximum source size (pixels)
  RECTL       rctlDstMargin;   //destination rectangle margins
  ULONG       fccDstColor;     //screen FOURCC
  ULONG       ulScanAlign;     //requered scanline aligment-1
  ULONG       ulNumColors;     //count of supported source FOURCC
  PULONG      fccColorType;    //array of supported FOURCC
  ULONG       ulAttrCount;     //count of viewport attributes
} HWVIDEOCAPS;
typedef HWVIDEOCAPS * PHWVIDEOCAPS;

//ulCapsFlag defines
#define HWVIDEOCAPS_MINIFY               0x00000001 //Chip can perform downscaling
#define HWVIDEOCAPS_FILTER               0x00000002 //Image filtering present
#define HWVIDEOCAPS_NONINTEGER_SCALE     0x00000004 //allow scale to noninteger ratio
#define HWVIDEOCAPS_COLOR_KEYING         0x00000008 //allow color keying
#define HWVIDEOCAPS_OVERLAY              0x00000010 //overlay-type HW
#define HWVIDEOCAPS_SEPARATE_OUT         0x00000020 //used separate output connector (like TV out)
#define HWVIDEOCAPS_DECODE               0x00000040 //support non-RAW data
#define HWVIDEOCAPS_NEEDPROTECT          0x00000080 //HW need to lock VRAM

typedef struct _HWATTRIBUTE {
  ULONG       ulLength;          //size of structure in bytes
  CHAR        szAttrDesc[64];    //string, describing attribute
  ULONG       ulAttrType;        //type of attribute, check ATTRTYPE_* const
  ULONG       ulValueSize;       //size in bytes of each value member
  ULONG       ulValueCount;      //count of value members
  PVOID       pValueList;        //list of supported values
  ULONG       ulCurrentValue;    //current value
  ULONG       ulDefaultValue;    //default value
  ULONG       ulAttribFlags;     //flags to define some additional properties

} HWATTRIBUTE;
typedef HWATTRIBUTE * PHWATTRIBUTE;

//types of attributes.
// if ATTRTYPE_BOOLEAN, ATTRTYPE_STATICTEXT or ATTRTYPE_BYTE,
// ulValueCount & ulValueSize undefined,
// pValueList can be NULL,
// else this must be actual value of allocated chunk of memory.

//attribute of ON/OFF type
#define ATTRTYPE_BOOLEAN               1
//attribute can be member of set string type
#define ATTRTYPE_AGGREGATE_STRING      2
//attribute have no value - this is static text string
#define ATTRTYPE_STATICTEXT            3
//attribute can be any value in 0..255 margins
#define ATTRTYPE_BYTE                  4


//defines for ulAttribFlags field
//changing this attribute affect HW capability, so application
//must re-read Caps after changing value of this attribute
//application must not assume preserving any capability when this
//attribute was changed
#define ATTRIB_CHANGE_CAPS        0x01L

//some common attribute names
#define ATTRIBUTE_BRIGHTNESS   "Brightness"
#define ATTRIBUTE_CONTRAST     "Contrast"
#define ATTRIBUTE_SATURATION   "Saturation"
#define ATTRIBUTE_HUE          "Hue"
#define ATTRIBUTE_FILTERING    "Filtering"
#define ATTRIBUTE_TVOUT        "Output to TV"
#define ATTRIBUTE_COLORKEY     "Color Keying"

typedef struct _HWVIDEOSETUP {
  ULONG       ulLength;         //size of structure in bytes
  RECTL       rctlDstRect;      //destination screen rectangle
  SIZEL       szlSrcSize;       //source image size
  ULONG       ulSrcPitch;       //byte offset between image rows
  ULONG       fccColor;         //image format
  ULONG       ulKeyColor;       //color key
  RECTL       rctlSrcRect;      //displayed subrectangle
}HWVIDEOSETUP;

typedef HWVIDEOSETUP * PHWVIDEOSETUP;


//Init HWVideo subsystem
ULONG APIENTRY HWVIDEOInit(void);
//check for presence and avilability of accelerated HW, if present and
//available - lock it for this process.

//Get HW capability
ULONG APIENTRY HWVIDEOCaps(PHWVIDEOCAPS pCaps);
//return filled structure. When called, ulNumColors must be
//set to actual allocated size of fccColorType array.
//if call returned with error, then need to check returned
//ulNumColors, allocate larger space and call again.


//Set HWVideo viewport
ULONG APIENTRY HWVIDEOSetup(PHWVIDEOSETUP pSetup);
//Check HW capability to handle this setup, allocate buffers.
//one special case: pSetup==NULL - disable video and free all
//buffers


//Get HWVideo buffer pointer
ULONG APIENTRY HWVIDEOBeginUpdate(PVOID *ppBuffer, PULONG pulPhysBuffer);
//return linear pointer to overlay buffer and it's physical address

//Display HWVideo buffer
ULONG APIENTRY HWVIDEOEndUpdate(void);
//set pending state for last accessed videobuffer, switch
//buffers on next VSYNC

//Get current HW attributes
ULONG APIENTRY HWVIDEOGetAttrib(ULONG ulAttribNum,PHWATTRIBUTE pAttrib);
//0<=ulAttribNum<pCaps->ulAttrCount, else error returned.

//Set new attribute value
ULONG APIENTRY HWVIDEOSetAttrib(ULONG ulAttribNum,PHWATTRIBUTE pAttrib);
//0<=ulAttribNum<pCaps.ulAttrCount, else error returned.
//pAttrib->ulCurrentValue filled with new value

//Close HWVideo
ULONG APIENTRY HWVIDEOClose(void);


//return codes
//no error
#define HWVIDEO_DONE             0
//unspecified error
#define HWVIDEO_ERROR            3
//FS session active, accelerator not available
#define HWVIDEO_ERROR_BACKGROUND 4
//HW not available
#define HWVIDEO_ERROR_NO_HW      6
//incorrect parameter
#define HWVIDEO_ERROR_PARAMETER  7
//too low offscreen VRAM to handle setup
#define HWVIDEO_ERROR_LOW_VRAM   8
//HW already in use
#define HWVIDEO_ERROR_USED       9

#ifdef _HWVIDEO_INTERNAL_
//HWVIDEO GRADD interface extended commands
#define EXT_CMD_HWVIDEO_CONNECT      1          //check availability of accelerator
#define EXT_CMD_HWVIDEO_QUERYCAPS    2          //get HW characteristics
#define EXT_CMD_HWVIDEO_GETATTRIB    3          //get viewport attribute value
#define EXT_CMD_HWVIDEO_SETATTRIB    4          //set new viewport value
#define EXT_CMD_HWVIDEO_SETUP        5          //setup viewport
#define EXT_CMD_HWVIDEO_DISPLAY      6          //switch buffer to pending state
#define EXT_CMD_HWVIDEO_HIDE         7          //close viewport, but still posessed
#define EXT_CMD_HWVIDEO_DISCONNECT   8          //close viewport and free HW

/* internal functions - please don't use in production */

#define EXT_CMD_HWVIDEO_GETHWINFO    25         //internal debugging function
#define EXT_CMD_HWVIDEO_GETSTATINFO  26         //internal debugging

typedef struct _HWINFO {
  ULONG ulLength;
  ULONG ulPCIid;     //low word is a VendorID, high word - DeviceID
  ULONG ulPCIaddr;   //byte 0 - bus number, byte 1 - device function
  ULONG ulVRAMPhys;  //physical address of card's memory
  ULONG ulDispStart; //offset in bytes from start of VRAM to desktop's pixel 0,0
  PBYTE pMMIO;       //global linear mapping of MMIO
  PBYTE pAddBase;    //global linear mapping of additional space (chip-specific)
  ULONG ulIOBase;    //base address of chip's IO space (if any)
  ULONG fccScreen;   //desktop color encoding
  ULONG ulModeId;    //desktop mode number
  ULONG ulXres;
  ULONG ulYres;
  ULONG ulPitch;
  ULONG ulVRAMSize;
}HWINFO,* PHWINFO;

typedef struct _STATINFO {
  ULONG ulLength;
  ULONG ulSetModeCount;
  ULONG ulEventForegroundCount;
  ULONG ulEventBackgroundCount;
  PID   LastSetModePID;
  TID   LastSetModeTID;
}STATINFO,* PSTATINFO;


#include "gradd.h"
#endif //_HWVIDEO_INTERNAL_
#ifdef __cplusplus
}
#endif  //__cplusplus

#endif //_HWVIDEO_
