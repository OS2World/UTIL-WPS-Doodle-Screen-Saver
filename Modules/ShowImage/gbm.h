/*

gbm.h - Generalised Bitmap Module

Data is stored as an array of lines.
Lines are stored with bottom line first, moving upwards.
Each line is an array of pixels, leftmost first.
Lines are padded to be a multiple of a dword long.
Palettised pixels are either a 1 bit, 4 bit, or 8 bit indexes.
Alternately a B, G, R triple in that order is stored.
This format exactly matches the format used by OS/2 and Windows bitmaps.
For 32 and 64 bit pixels the data format is BGRA (A - alpha channel).
24 and 32 bit formats encode B, G, R and A as 1 byte each, while
48 and 64 bit formats encode them as 2 byte each in platform specific
byte ordering (little endian on x86). Thus C pointers can be naturally used.

One notable point: a 1 in a 1bpp image denotes colour 1, as found by
looking at palette entry 1. Data is not inversed when passed to and from GBM.

This interface file provides access to routines for reading and writing
bitmaps in a variety of image file formats.

Normally file I/O is done using lseek,read and write. Occasionally GBM
needs to access additional files, and it uses open and close to do this.
Sometimes it needs to create a new file, and GBM uses create and close
for this. The 'create' function is an invokation of open with
GBM_O_CREAT|GBM_O_TRUNC combined with the mode flags it is passed, and
GBM_S_IREAD|GBM_S_IWRITE passed as the additional optional parameter.

You can trick GBM into using your own versions of open, create, close,
lseek, read and write routines, by calling gbm_io_setup.

One example use of this is that the file descriptor could then be an index
into a array of pointers to C++ iostreams, thus allowing GBM to read and
write file data to and from memory.

On some platforms, the GBM file I/O library may be provided in DLL form.
Therefore it can have its own C run time library, and on some platforms
file descriptors obtained by an executable do not make sense to the
C run time which is a part of the DLL. Hence GBM will be unable to use
the file descriptor. One solution is to use gbm_io_setup to get the GBM
library to call back into the calling application and use its C run time.
Another solution is to have the application use the GBM libraries C run
time to open the file - this is made possible via the gbm_io_ routines.
This is the easier solution, and is used by the sample GBM programs.
The particular offending platform is Visual C++ on Windows NT, everything
works fine for VisualAge C++ on OS/2.

gbm_read_imgcount shall seek to the start of the stream identified by
the file descriptor argument and then shall invoke the bitmap image count
reader routine identified by the format type variable.

gbm_read_header shall seek to the start of the stream indentified by the
file descriptor argument and then shall invoke the bitmap header reader
routine identified by the format type variable.

gbm_read_palette may only be legally invoked after an immediately
preceeding gbm_read_header.

gbm_read_data may only be legally invoked after an immediately preceeding
gbm_read_palette. In the case of a 24, 32 and 64bpp file (which therefore has no
palette), gbm_read_data is additionally allowed to follow a gbm_read_header.


Note on backward compatibility for creating DLL version of GBM:
---------------------------------------------------------------

Be aware that when your application uses API functions added in a
later release, they gain functionality but loose backward compatibility
to older DLL versions. Use the API change log below for reference.

1.30: New format codes for enhanced colour depths
      - GBM_FT_R16, GBM_FT_R32, GBM_FT_R48, GBM_FT_R64
      - GBM_FT_W16, GBM_FT_W32, GBM_FT_W48, GBM_FT_W64

1.35: New API functions
      - gbm_restore_io_setup()
      - gbm_read_imgcount()

      New compiler independent GBM_O_ modes (still backward compatible)
      - GBM_O_BINARY
      - GBM_O_RDONLY
      - GBM_O_WRONLY
      - GBM_O_RDWR
      - GBM_O_EXCL
      - GBM_O_NOINHERIT

      New compiler independent GBM_SEEK_ defines (still backward compatible)
      - GBM_SEEK_SET
      - GBM_SEEK_CUR
      - GBM_SEEK_END

1.40: Obsolete format codes GBM_FT_R16 and GBM_FT_W16 as these are not
      unique interpretable. Use 24bpp or 48bpp color depths instead.
      There has been no codec that used them so far anyway.
*/

#ifndef GBM_H
#define	GBM_H

#ifndef BOOLEAN_DEFINED
#define	BOOLEAN_DEFINED
typedef	int BOOLEAN;
#define	TRUE  1
#define	FALSE 0
#endif

#ifndef BASICTYPES_DEFINED
#define	BASICTYPES_DEFINED
typedef unsigned  char  byte;
typedef unsigned short  word;
typedef unsigned  long dword;
#endif

typedef int GBM_ERR;
#define	GBM_ERR_OK		((GBM_ERR) 0)
#define	GBM_ERR_MEM		((GBM_ERR) 1)
#define	GBM_ERR_NOT_SUPP	((GBM_ERR) 2)
#define	GBM_ERR_BAD_OPTION	((GBM_ERR) 3)
#define	GBM_ERR_NOT_FOUND	((GBM_ERR) 4)
#define	GBM_ERR_BAD_MAGIC	((GBM_ERR) 5)
#define	GBM_ERR_BAD_SIZE	((GBM_ERR) 6)
#define	GBM_ERR_READ		((GBM_ERR) 7)
#define	GBM_ERR_WRITE		((GBM_ERR) 8)
#define	GBM_ERR_BAD_ARG		((GBM_ERR) 9)

/* default color depths (backward compatible) */
#define	GBM_FT_R1		0x0001
#define	GBM_FT_R4		0x0002
#define	GBM_FT_R8		0x0004
#define	GBM_FT_R24		0x0008
#define	GBM_FT_W1		0x0010
#define	GBM_FT_W4		0x0020
#define	GBM_FT_W8		0x0040
#define	GBM_FT_W24		0x0080

/* extended color depths (optional, since GBM version 1.30) */
#define	GBM_FT_R32		0x0400
#define	GBM_FT_R48		0x0800
#define	GBM_FT_R64		0x1000
#define	GBM_FT_W32		0x8000
#define	GBM_FT_W48		0x10000
#define	GBM_FT_W64		0x20000

/* File open modes for gbm_io_open().
 *
 * The mode defines below can be combined via |.
 *
 * Don't use compiler specific defines O_* for portability reasons.
 * TEXT translation mode is not supported. BINARY or RAW mode is always used.
 */
#if defined(__OS2__) || defined(OS2)
  /* The new defines are backward compatible to IBM VAC++ 3.0 to keep
   * programs using GBM.DLL running event though the DLL is compiled
   * with an other compiler (e.g. Open Watcom).
   * For backward compatibility O_BINARY or O_RAW of IBM VAC++ 3.0 defines
   * is accepted but new code or recompiles should use the new defines.
   */
  #define GBM_O_BINARY        0x00008000                  /* binary mode */
  #define GBM_O_RDONLY        (0x00000004 | GBM_O_BINARY) /* open for read only                      */
  #define GBM_O_WRONLY        (0x00000001 | GBM_O_BINARY) /* open for write only                     */
  #define GBM_O_RDWR          (0x00000002 | GBM_O_BINARY) /* open for read and write                 */
  #define GBM_O_EXCL          (0x00000400 | GBM_O_BINARY) /* open only if file doesn't already exist */
  #define GBM_O_NOINHERIT     0x00000080                  /* child process doesn't inherit file      */

#else
  /* Add backward compatibility mapping of other platforms the DLL has been
   * published in binary format (see OS2 description).
   */

  /* defaults where no portability requirements exist (e.g. static build with same compiler) */
  #ifdef O_BINARY
    #define GBM_O_BINARY        O_BINARY                /* binary mode */
  #else
    #define GBM_O_BINARY        0                       /* binary mode */
  #endif
  #define GBM_O_RDONLY        (O_RDONLY | GBM_O_BINARY) /* open for read only                      */
  #define GBM_O_WRONLY        (O_WRONLY | GBM_O_BINARY) /* open for write only                     */
  #define GBM_O_RDWR          (O_RDWR   | GBM_O_BINARY) /* open for read and write                 */
  #define GBM_O_EXCL          (O_EXCL   | GBM_O_BINARY) /* open only if file doesn't already exist */
  #define GBM_O_NOINHERIT     O_NOINHERIT               /* child process doesn't inherit file      */
#endif

/* File seek modes for gbm_io_lseek().
 *
 * Don't use compiler specific defines SEEK_* for portability reasons.
 */
#if defined(__OS2__) || defined(OS2)
  /* The new defines are backward compatible to IBM VAC++ 3.0 to keep
   * programs using GBM.DLL running event though the DLL is compiled
   * with an other compiler (e.g. Open Watcom).
   */
  #define GBM_SEEK_SET        0   /* fseek constants */
  #define GBM_SEEK_CUR        1
  #define GBM_SEEK_END        2

#else
  /* Add backward compatibility mapping of other platforms the DLL has been
   * published in binary format (see OS2 description).
   */

  /* defaults where no portability requirements exist (e.g. static build with same compiler) */
  #define GBM_SEEK_SET        SEEK_SET  /* fseek constants */
  #define GBM_SEEK_CUR        SEEK_CUR
  #define GBM_SEEK_END        SEEK_END
#endif

/* ----------------------------------------------------- */

/* Export as "C" to allow usage from C++ */
#ifdef __cplusplus
  extern "C"
  {
#endif

/* Enforce compatibility to old binary interface by      */
/* aligning data in public structs to 4 byte boundaries. */
/* This allows internal data alignment with optimum size */
/* for best processor specific optimization.             */
#pragma pack(4)

typedef struct
	{
	char *short_name;		/* Eg: "Targa"                       */
	char *long_name;		/* Eg: "Truevision Targa / Vista"    */
	char *extensions;		/* Eg: "TGA VST"                     */
	int flags;			/* What functionality exists         */
	} GBMFT;

typedef struct { byte r, g, b; } GBMRGB;

/* Don't change the size ! Otherwise backward compatibility of DLL is gone !!! */
#define	PRIV_SIZE 2000

typedef struct
	{
	int w, h, bpp;			/* Bitmap dimensions                 */
	byte priv[PRIV_SIZE];		/* Private internal buffer           */
	} GBM;

/* Enable compiler default packing. */
#pragma pack()

/* ----------------------------------------------------- */

#ifndef _GBM_

#if defined(__OS2__) || defined(OS2)
  #define  GBMEXPORT
  #define  GBMENTRY_SYS  _System
  #define  GBMENTRY      _Optlink
  #if defined(__IBMC__) || defined(__IBMCPP__)
    #define GBMENTRYP  * _Optlink
  #else
    #define GBMENTRYP  _Optlink *
  #endif
#elif defined(WIN32)
  #define GBMEXPORT   __declspec(dllexport)
  #define GBMENTRY_SYS
  #define GBMENTRY    __stdcall
  #define GBMENTRYP   *
#elif defined(DOS32)
  #define GBMEXPORT
  #define GBMENTRY_SYS
  #define GBMENTRY __export _cdecl
  #define GBMENTRYP   *
#else
  #define GBMEXPORT
  #define GBMENTRY_SYS
  #define GBMENTRY
  #define GBMENTRYP   *
#endif

/* New interfaces have to use _System on OS/2 to match system compliance
 * and enhance compiler portability. Use GBMENTRY_SYS for these cases.
 */

GBMEXPORT GBM_ERR GBMENTRY gbm_init(void);
GBMEXPORT GBM_ERR GBMENTRY gbm_deinit(void);

GBMEXPORT GBM_ERR GBMENTRY gbm_io_setup(
	int  (GBMENTRYP open  )(const char *fn, int mode),
	int  (GBMENTRYP create)(const char *fn, int mode),
	void (GBMENTRYP close )(int fd),
	long (GBMENTRYP lseek )(int fd, long pos, int whence),
	int  (GBMENTRYP read  )(int fd, void *buf, int len),
	int  (GBMENTRYP write )(int fd, const void *buf, int len)
	);

GBMEXPORT GBM_ERR GBMENTRY_SYS gbm_restore_io_setup(void);

GBMEXPORT int  GBMENTRY gbm_io_open  (const char *fn, int mode);
GBMEXPORT int  GBMENTRY gbm_io_create(const char *fn, int mode);
GBMEXPORT void GBMENTRY gbm_io_close (int fd);
GBMEXPORT long GBMENTRY gbm_io_lseek (int fd, long pos, int whence);
GBMEXPORT int  GBMENTRY gbm_io_read  (int fd, void *buf, int len);
GBMEXPORT int  GBMENTRY gbm_io_write (int fd, const void *buf, int len);

GBMEXPORT GBM_ERR GBMENTRY     gbm_query_n_filetypes(int *n_ft);
GBMEXPORT GBM_ERR GBMENTRY     gbm_query_filetype(int ft, GBMFT *gbmft);
GBMEXPORT GBM_ERR GBMENTRY     gbm_guess_filetype(const char *fn, int *ft);

GBMEXPORT GBM_ERR GBMENTRY_SYS gbm_read_imgcount(const char *fn, int fd, int ft, int *pimgcnt);
GBMEXPORT GBM_ERR GBMENTRY     gbm_read_header(const char *fn, int fd, int ft, GBM *gbm, const char *opt);
GBMEXPORT GBM_ERR GBMENTRY     gbm_read_palette(int fd, int ft, GBM *gbm, GBMRGB *gbmrgb);
GBMEXPORT GBM_ERR GBMENTRY     gbm_read_data(int fd, int ft, GBM *gbm, byte *data);
GBMEXPORT GBM_ERR GBMENTRY     gbm_write(const char *fn, int fd, int ft, const GBM *gbm, const GBMRGB *gbmrgb, const byte *data, const char *opt);

GBMEXPORT const char * GBMENTRY gbm_err(GBM_ERR rc);

GBMEXPORT int GBMENTRY gbm_version(void);

#if defined(__OS2__) || defined(OS2)
/*...s_System entrypoints:0:*/
/* For GBM.DLL to be callable from IBM Smalltalk under OS/2, the entrypoints
   must be of _System calling convention. These veneers help out here.
   I can't just change the usual entrypoints because people depend on them.
   For portability, avoid these entrypoints, use the gbm_ ones. */

GBM_ERR _System Gbm_init(void);
GBM_ERR _System Gbm_deinit(void);
int     _System Gbm_io_open(const char *fn, int mode);
int     _System Gbm_io_create(const char *fn, int mode);
void    _System Gbm_io_close(int fd);
long    _System Gbm_io_lseek(int fd, long pos, int whence);
int     _System Gbm_io_read (int fd, void *buf, int len);
int     _System Gbm_io_write(int fd, const void *buf, int len);
GBM_ERR _System Gbm_query_n_filetypes(int *n_ft);
GBM_ERR _System Gbm_guess_filetype(const char *fn, int *ft);
GBM_ERR _System Gbm_query_filetype(int ft, GBMFT *gbmft);
GBM_ERR _System Gbm_read_header(const char *fn, int fd, int ft, GBM *gbm, const char *opt);
GBM_ERR _System Gbm_read_palette(int fd, int ft, GBM *gbm, GBMRGB *gbmrgb);
GBM_ERR _System Gbm_read_data(int fd, int ft, GBM *gbm, byte *data);
GBM_ERR _System Gbm_write(const char *fn, int fd, int ft, const GBM *gbm, const GBMRGB *gbmrgb, const byte *data, const char *opt);
const char * _System Gbm_err(GBM_ERR rc);
int     _System Gbm_version(void);
/*...e*/
#endif

#endif


#ifdef __cplusplus
  }  /* extern "C" */
#endif

#endif

