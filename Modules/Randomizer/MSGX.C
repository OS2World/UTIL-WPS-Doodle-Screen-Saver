/*******************************************************************************
	MANGLAIS Message file processing
	¸ 2004 Peter Koller, Maison Anglais. All Rights Reserved
	
	This file is part of the message file system provided as open source to
	anyone who wants to use simple message files in their applications.
	
	Message files like this are extremely handy for supporting multiple languages
	in applications that have used embedded strings in the source code. This
	message file code can directly replace sprintf in apps of this kind.
	
	This code uses message files created by the MANGLAIS MESSAGE COMPILER, which
	is not provided as open source at this time. This code is presently designed
	to work with OS/2, but versions for Windows or other systems may become
	available later.

	LICENCE:-
	You may embed, use, or modify the code msgx.h and msgx.c as you see fit
	providing that you do not remove this header and licence, and providing that
	you do not prevent or hinder others from using this code regardless of whether
	you have modified it. If you make changes to this code, clearly indicate where
	those changes are, and put your name to the changes.

	COMPILER LICENCE:-
	You may use the message compiler to create binary message files.
	You may provide the message compiler to others for the purpose of creating
	binary message files. You may use a compression algorithm such as 'zip' on the
	message compiler in order to store it on a storage or distribution medium.
	You must not disassemble, reverse engineer, or otherwise tamper with the
	message compiler.
*******************************************************************************/
#include    <os2.h>
#include    <string.h>
#include    <stdio.h>
#include    <stdarg.h>

#include	"msgx.h"

#define	FILE_MAGICID	"MESS"

typedef struct	_msgitem
	{
		USHORT	len;
		char	id[MSGID_LEN];
		char	msg[1];
	}	msgItem,	*pmsgItem;

typedef struct	_shortmsgitem
	{
		USHORT	len;
		char	id[MSGID_LEN];
	}	shortmsgItem;

typedef struct	_msgfile
	{
		ULONG	magicid;
		msgItem	msgitem[1];
	}	msgFile,	*pmsgFile;


/***** local functions *****/
void 	memread(void *out, char *mem, ULONG *pcurpos, ULONG size);
BOOL	_findSubstitute(char **fmt, int x);
int		_scrubfmt(char *buffer, char *ret, char *fmt, ULONG *plen, int* twin);
int		_scrubprecent(char *buffer, char *fmt, ULONG *plen);
int 	_isfmtchar(char *ch, int *pw, int* twin);

void	_insertstring(char *buffer, char* inspt, char* data, ULONG len);
void	_deletestring(char *buffer, char* inspt, ULONG len);


/*******************************************************************************
	Open a message file and check it's id
*******************************************************************************/
FILE *fopenMessageFile(char *msgfile)
	{
		FILE 	*hmsg;
		char	magigid[10] = {0};

		hmsg = fopen(msgfile, "rb");
		if(hmsg)
			{
				fread(&magigid, sizeof(ULONG), 1, hmsg);
				if(!strcmp(magigid, FILE_MAGICID))
					{
						rewind(hmsg);
						return hmsg;
					}
				else
					{
						fclose(hmsg);
						hmsg = NULL;
					}
			}
		
		return hmsg;
	}

/*******************************************************************************
	Memory buffer messagefile id check
*******************************************************************************/
BOOL vfyMemMessageFile(char *mem, ULONG memlen)
	{
		if(memlen > sizeof(ULONG))
			{
				if(!strcmp(mem, FILE_MAGICID)) return TRUE;
			}
		return FALSE;
	}

/*******************************************************************************
	Core message proc
*******************************************************************************/
ULONG sprintmsg(char *buffer, FILE *hmsg, char* msgid, ...)
	{
		va_list		arg_ptr;
		int			x, sz, ins, twin;
		ULONG		len;
		char 		*fmt;
		char		tmpfmt[10];
		char		tmpbuf[1024];

		if(!_getMessage(buffer, msgid, &len, hmsg)) return 0;

		va_start(arg_ptr, msgid);

		// max value is 9, substitution can occur out of order, hence %% is done in a 2nd pass
		fmt = buffer;
		for(x = 1; _findSubstitute(&fmt, x); x++)
			{
				twin = 0;
				sz = _scrubfmt(buffer, tmpfmt, fmt, &len, &twin);
				if(twin)
					{
						twin = va_arg(arg_ptr, int);
						if(sz == sizeof(char)) ins = sprintf(tmpbuf, tmpfmt, twin, va_arg(arg_ptr, char));
						else if(sz == sizeof(short int)) ins = sprintf(tmpbuf, tmpfmt, twin, va_arg(arg_ptr, short int));
						else if(sz == sizeof(int)) ins = sprintf(tmpbuf, tmpfmt, twin, va_arg(arg_ptr, int));
						else if(sz == sizeof(double)) ins = sprintf(tmpbuf, tmpfmt, twin, va_arg(arg_ptr, double));
						else if(sz == sizeof(char *)) ins = sprintf(tmpbuf, tmpfmt, twin, va_arg(arg_ptr, char *));
						else if(sz == sizeof(long int)) ins = sprintf(tmpbuf, tmpfmt, twin, va_arg(arg_ptr, long int));
						else if(sz == sizeof(long double)) ins = sprintf(tmpbuf, tmpfmt, twin, va_arg(arg_ptr, long double));
					}
				else
					{
						if(sz == sizeof(char)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, char));
						else if(sz == sizeof(short int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, short int));
						else if(sz == sizeof(int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int));
						else if(sz == sizeof(double)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, double));
						else if(sz == sizeof(char *)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, char *));
						else if(sz == sizeof(long int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, long int));
						else if(sz == sizeof(long double)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, long double));
					}
				_insertstring(buffer, fmt, tmpbuf, ins);
				len += ins;
				fmt = buffer;
			}
		// %% substitution
		fmt = buffer;
		while(_findSubstitute(&fmt, 0))
			{
				sz = _scrubprecent(buffer, fmt, &len);
				if(sz == '%') ins = sprintf(tmpbuf, "%%");
				_insertstring(buffer, fmt, tmpbuf, ins);
				len += ins;
			}

		va_end(arg_ptr);
	return len;
}

ULONG sprintmemmsg(char *buffer, char *mem, ULONG memlen, char* msgid, ...)
	{
		va_list		arg_ptr;
		int			x, sz, ins, twin;
		ULONG		len;
		char 		*fmt;
		char		tmpfmt[10];
		char		tmpbuf[1024];

		if(!_getMemMessage(buffer, msgid, &len, mem, memlen)) return 0;

		va_start(arg_ptr, msgid);

		// max value is 9, substitution can occur out of order, hence %% is done in a 2nd pass
		fmt = buffer;
		for(x = 1; _findSubstitute(&fmt, x); x++)
			{
				twin= 0;
				sz = _scrubfmt(buffer, tmpfmt, fmt, &len, &twin);
				if(twin)
					{
						if(sz == sizeof(char)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int), va_arg(arg_ptr, char));
						else if(sz == sizeof(short int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int), va_arg(arg_ptr, short int));
						else if(sz == sizeof(int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int), va_arg(arg_ptr, int));
						else if(sz == sizeof(double)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int), va_arg(arg_ptr, double));
						else if(sz == sizeof(char *)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int), va_arg(arg_ptr, char *));
						else if(sz == sizeof(long int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int), va_arg(arg_ptr, long int));
						else if(sz == sizeof(long double)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int), va_arg(arg_ptr, long double));
					}
				else
					{
						if(sz == sizeof(char)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, char));
						else if(sz == sizeof(short int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, short int));
						else if(sz == sizeof(int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, int));
						else if(sz == sizeof(double)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, double));
						else if(sz == sizeof(char *)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, char *));
						else if(sz == sizeof(long int)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, long int));
						else if(sz == sizeof(long double)) ins = sprintf(tmpbuf, tmpfmt, va_arg(arg_ptr, long double));
					}
				_insertstring(buffer, fmt, tmpbuf, ins);
				len += ins;
				fmt = buffer;
			}
		// %% substitution
		fmt = buffer;
		while(_findSubstitute(&fmt, 0))
			{
				sz = _scrubprecent(buffer, fmt, &len);
				if(sz == '%') ins = sprintf(tmpbuf, "%%");
				_insertstring(buffer, fmt, tmpbuf, ins);
				len += ins;
			}

		va_end(arg_ptr);
	return len;
}

/*******************************************************************************
	Finds message id in file
*******************************************************************************/
BOOL _getMessage(char *buffer, char* msgid, ULONG *plen, FILE *hmsg)
	{
		shortmsgItem 	shmi;
		BOOL			failed = FALSE;
		rewind(hmsg);
		fseek(hmsg, sizeof(ULONG), SEEK_SET);
		do	{
				if(!fread(&shmi, sizeof(shortmsgItem), 1, hmsg)) return FALSE;
				if(!strnicmp(msgid, shmi.id, MSGID_LEN)) break;
				fseek(hmsg, shmi.len, SEEK_CUR);
			}	while(!(failed = feof(hmsg)));
		if(failed) return FALSE;
		if(buffer)
			{
				fread(buffer, 1, shmi.len, hmsg);
				*(buffer + shmi.len) = '\0';
			}
		if(plen) *plen = shmi.len;
		return TRUE;
	}

/*******************************************************************************
	Finds message id in memory buffer
*******************************************************************************/
BOOL _getMemMessage(char *buffer, char* msgid, ULONG *plen, char *mem, ULONG memlen)
	{
		shortmsgItem 	shmi;
		BOOL			failed = FALSE;
		ULONG	curpos = sizeof(ULONG);
		do	{
				if((memlen - curpos) <= sizeof(shortmsgItem)) return FALSE;
				memread(&shmi, mem, &curpos, sizeof(shortmsgItem));
				if(!strnicmp(msgid, shmi.id, MSGID_LEN)) break;
				curpos += shmi.len;
			}	while(!(failed = !(curpos < memlen)));
		if(failed) return FALSE;
		if(buffer)
			{
				memread(buffer, mem, &curpos, shmi.len);
				*(buffer + shmi.len) = '\0';
			}
		if(plen) *plen = shmi.len;
		return TRUE;
	}

void memread(void *out, char *mem, ULONG *pcurpos, ULONG size)
	{
		memmove(out, mem + *pcurpos, size);
		*pcurpos += size;
	}

/******************************************************************************
    Insert string segment
******************************************************************************/
void	_insertstring(char *buffer, char* inspt, char* data, ULONG len)
	{
        ULONG       start;
        ULONG       end;
        size_t      count;
        PVOID       mvdest = (PVOID)((ULONG)inspt + len);

        start = (ULONG)inspt;
        end = (ULONG)buffer + strlen(buffer) + 1;
        if(start < end)
            {
                count = end - start;
            }
        else count = 0;
        if(count) memmove(mvdest, inspt, count);
        if(data) memmove(inspt, data, len);

	}

/******************************************************************************
    Delete string segment
******************************************************************************/
void	_deletestring(char *buffer, char* delpt, ULONG len)
	{
        ULONG       start;
        ULONG       end;
        size_t      count;
        PVOID       mvsrc = (PVOID)((ULONG)delpt + len);

        start = (ULONG)mvsrc;
        end = (ULONG)buffer + strlen(buffer) + 1;

        if(start < end)
            {
                count = end - start;
            }
        else count = 0;
        if(count) memmove(delpt, mvsrc, count);
	}

/*******************************************************************************
	get format string out of buffer - substitution numbers are limited 1 to 9
*******************************************************************************/
int _scrubfmt(char *buffer, char *ret, char *fmt, ULONG *plen, int* twin)
	{
		int	x, w;
		*ret = *fmt;
		*twin = 0;
		for(x = 1;; x++)
			{
				*(ret + x) = *(fmt + (x + 1));
				if(_isfmtchar(fmt + (x + 1), &w, twin))
					{
						x++;
						break;
					}
			}
		*(ret + x) = *(fmt + (x + 1));
		x += (*twin);
		*(ret + x) = '\0';
		
		_deletestring(buffer, fmt, x + 1);
		
		if(plen) *plen -= x + 1;
		return w;
	}

/*******************************************************************************
	get format string out of buffer - substitution for %0
*******************************************************************************/
int _scrubprecent(char *buffer, char *fmt, ULONG *plen)
	{
		int	w;
		if(*(fmt + 1) == '0') w = '%';
		if(*(fmt + 1) == '%') w = '%';
		_deletestring(buffer, fmt, 2);
		
		if(plen) *plen -= 2;
		return w;
	}

/*******************************************************************************
	Gets format specifier
*******************************************************************************/
int _isfmtchar(char *ch, int *pw, int* twin)
	{

		switch(*ch)
			{
				case '%':
					if(pw) *pw = '%';
					return 1;
				case 'd':
				case 'i':
				case 'u':
				case 'o':
				case 'x':
				case 'X':
					if(pw) *pw = sizeof(int);
					return 1;
				case 's':
				case 'n':
				case 'p':
					if(pw) *pw = sizeof(char*);
					return 1;
				case 'f':
				case 'F':
				case 'e':
				case 'E':
				case 'g':
				case 'G':
					if(pw) *pw = sizeof(double);
					return 1;				
				case 'c':		
					if(pw) *pw = sizeof(char);
					return 1;
					
				case '*':
					(*twin)++;
					return _isfmtchar(ch + 1, pw, twin);

				case 'h':
					switch(*(ch + 1))
						{
							case 'd':
							case 'i':
							case 'u':
							case 'o':
							case 'x':
							case 'X':
								if(pw) *pw = sizeof(short int);
								return 1;
							default:
								return _isfmtchar(ch + 1, pw, twin);
						}
				case 'l':
					switch(*(ch + 1))
						{
							case 'd':
							case 'i':
							case 'u':
							case 'o':
							case 'x':
							case 'X':
								if(pw) *pw = sizeof(long int);
								return 1;
							default:
								return _isfmtchar(ch + 1, pw, twin);
						}
				case 'L':
					switch(*(ch + 1))
						{
							case 'f':
							case 'F':
							case 'e':
							case 'E':
							case 'g':
							case 'G':
								if(pw) *pw = sizeof(long double);
								return 1;
							default:
								return _isfmtchar(ch + 1, pw, twin);
						}
 			}
		return 0;
	}

/*******************************************************************************
	Finds a substitution item from 0 to 9
*******************************************************************************/
BOOL _findSubstitute(char **fmt, int x)
	{		
		while( NULL != (*fmt = strchr(*fmt, '%')))
			{
				if(!x && (*((*fmt) + 1) == '%')) return TRUE;
				if(*((*fmt) + 1) == (x + '0')) return TRUE;
				(*fmt)++;
			}
		return FALSE;
	}
