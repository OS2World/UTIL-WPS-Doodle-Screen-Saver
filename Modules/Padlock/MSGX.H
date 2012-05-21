/*******************************************************************************
    Message file extractor
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
#ifndef  MANGLAIS_MESSAGEFILES
#define MANGLAIS_MESSAGEFILES

#define	MSGID_LEN		8
typedef char*			MSGTOKEN;

#define msgtokencpy(out, in) memmove(((PVOID)out), ((PVOID)in), 9)

/* file stream version */
FILE*		fopenMessageFile(char *msgfile);
ULONG 		sprintmsg(char *buffer, FILE *hmsg, char* msgid, ...);
//use fclose() to close the message file when done

/* memory buffer version */
BOOL		vfyMemMessageFile(char *mem, ULONG memlen);
ULONG		sprintmemmsg(char *buffer, char *mem, ULONG memlen, char* msgid, ...);

/* used where sprintmsg is too complex */
BOOL 	_getMessage(char *buffer, char* msgid, ULONG *plen, FILE *hmsg);
BOOL 	_getMemMessage(char *buffer, char* msgid, ULONG *plen, char *mem, ULONG memlen);

#endif
