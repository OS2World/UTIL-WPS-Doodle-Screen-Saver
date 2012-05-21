/*
 * Copyright (C) 2002-2003 nickk
 * All rights reserved.
 *
 * This code is a part of Security/2 project.
 * The use, changing and distribution out of Security/2 project is not allowed.
 */

#ifndef _USERTYPE_H_
#define _USERTYPE_H_

#define MAXUSERNAMELEN    80
#define MAXPASSLEN        80
#define MAXSECTIONNAMELEN 80
#define MAXGROUPSLEN      2048
#define MAXHOMELEN        CCHMAXPATH
#define MAXSHELLLEN       (2 * CCHMAXPATH)
#define MAXCOMMENTLEN     2048

#ifndef ACT_ALL
#define ACT_READ   0x00000001
#define ACT_WRITE  0x00000002
#define ACT_CREATE 0x00000004
#define ACT_DELETE 0x00000008
#define ACT_EXEC   0x00000010
#define ACT_SESS   0x00000020
#define ACT_ALL (ACT_READ | ACT_WRITE | ACT_CREATE | ACT_DELETE | ACT_EXEC | ACT_SESS)
#endif

#ifndef TYPE_ALL
#define TYPE_ALL	0
#define TYPE_FIRST	1
#define TYPE_LAST	2
#define TYPE_USER	3
#define TYPE_GROUP	4
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

/*
 * ACLs container
 */
typedef struct
{
    ULONG ulRight;                 /* The set of allowed actions */
    CHAR  szMask[CCHMAXPATH + 1];  /* The mask for which this actions allowed */
}
USERACL;

typedef USERACL* PUSERACL;

/*
 * ACLs section container
 */
typedef struct USERACLSET 
{
    LONG     lSectionId;            /* Type of acls section, dependant on user base manager */
    ULONG    ulUnused;              /* Does not matter for you */
    ULONG    ulAcls;                /* Number of acls records in acls buffer */
    PUSERACL acls;                  /* acls buffer */
} 
USERACLSET;

typedef USERACLSET* PUSERACLSET;

/*
 * DosUserList result
 */
typedef struct
{
    LONG lUid;                       /* User UID, UID = 0 is reserved for root user */
    CHAR szUser[MAXUSERNAMELEN + 1]; /* User name */
}
USERLIST;

typedef USERLIST* PUSERLIST;

/*
 * DosUserAclList result
 */
typedef struct
{
    ULONG ulType;                          /* Type of structure, dependant on user base manager */
    CHAR szSection[MAXSECTIONNAMELEN + 1]; /* Section name */
}
USERACLLIST;

typedef USERACLLIST* PUSERACLLIST;

/*
 * user_check request packet
 */

typedef struct
{
    ULONG  cbSize;       /* Size of structure */
    PSZ    pszUser;      /* User name with length <= MAXUSERNAMELEN */
    PSZ    pszPass;      /* Password with length <= MAXPASSLEN */
    PLONG  plUid;        /* Destination buffer to hold user UID */
    PSZ    pszShell;     /* Destination buffer to hold user shell */
    ULONG  cbShell;      /* Length of user shell buffer */
    PSZ    pszHome;      /* Destination buffer to hold user home dir */
    ULONG  cbHome;       /* Length of user home dir buffer */
    PULONG pulMaxProc;   /* Destination buffer to max number of processes allowed to user */
    PSZ    pszRemShell;  /* Destination buffer to hold user remote shell */
    ULONG  cbRemShell;   /* Length of user remote shell buffer */
}
USERCHECK;

typedef USERCHECK* PUSERCHECK;

/*
 * DosUserAdd && user_add request packet
 */

typedef struct
{
    ULONG cbSize;       /* Size of structure */
    PSZ   pszUser;      /* User name with length <= MAXUSERNAMELEN */
    PSZ   pszPass;      /* Password with length <= MAXPASSLEN */
    PSZ   pszGroups;    /* User groups separated by ; with total length <= MAXGROUPSLEN */
    PSZ   pszShell;     /* User shell with length <= MAXSHELLLEN */
    PSZ   pszHome;      /* User home dir length <= MAXHOMELEN */
    ULONG ulMaxProc;    /* Max number of processes allowed to user */
    PSZ   pszRemShell;  /* User remote shell with length <= MAXSHELLLEN */
    PSZ   pszComment;   /* Comment with length <= MAXCOMMENTLEN */
}
USERADD;

typedef USERADD* PUSERADD;

/*
 * DosUserChange && user_change request packet
 */

typedef struct
{
    ULONG cbSize;       /* Size of structure */
    LONG  lUid;         /* User uid */
    PSZ   pszUser;      /* User name with length <= MAXUSERNAMELEN */
    PSZ   pszPass;      /* Password with length <= MAXPASSLEN */
    PSZ   pszGroups;    /* User groups separated by ; with total length <= MAXGROUPSLEN */
    PSZ   pszShell;     /* User shell with length <= MAXSHELLLEN */
    PSZ   pszHome;      /* User home dir length <= MAXHOMELEN */
    ULONG ulMaxProc;    /* Max number of processes allowed to user */
    PSZ   pszRemShell;  /* User remote shell with length <= MAXSHELLLEN */
    PSZ   pszComment;   /* Comment with length <= MAXCOMMENTLEN */
}
USERCHANGE;

typedef USERCHANGE* PUSERCHANGE;

/*
 * DosUserQuery && user_query request packet
 */

typedef struct
{
    ULONG  cbSize;       /* Size of structure */
    PSZ    pszUser;      /* Destination buffer to hold user name */
    ULONG  cbUser;       /* Length of user name buffer */
    PLONG  plUid;        /* Destination buffer to hold user UID */
    PSZ    pszGroups;    /* Destination buffer to hold user groups separated by ; of total length < MAXGROUPSLEN */
    ULONG  cbGroups;     /* Length of user groups buffer */
    PSZ    pszShell;     /* Destination buffer to hold user shell */
    ULONG  cbShell;      /* Length of user shell buffer */
    PSZ    pszHome;      /* Destination buffer to hold user home dir */
    ULONG  cbHome;       /* Length of user home dir buffer */
    PULONG pulMaxProc;   /* Destination buffer to max number of processes allowed to user */
    PSZ    pszRemShell;  /* Destination buffer to hold user remote shell */
    ULONG  cbRemShell;   /* Length of user remote shell buffer */
    PSZ    pszComment;   /* Destination buffer to hold comment */
    ULONG  cbComment;    /* Length of comment buffer */
}
USERQUERY;

typedef USERQUERY* PUSERQUERY;

/*
 * DosUserRegister request packet
 */

typedef struct
{
    ULONG cbSize;       /* Size of structure */
    PID   pid;          /* The PID of user root process (i.e. user shell) */
    PSZ   pszUser;      /* User name with length <= MAXUSERNAMELEN */
    PSZ   pszPass;      /* Password with length <= MAXPASSLEN */
    PLONG plUid;        /* Destination buffer to hold user UID */
    PSZ   pszShell;     /* Destination buffer to hold user shell */
    ULONG cbShell;      /* Length of user shell buffer */
    PSZ   pszHome;      /* Destination buffer to hold user home dir */
    ULONG cbHome;       /* Length of user home dir buffer */
    PSZ   pszRemShell;  /* Destination buffer to hold user remote shell */
    ULONG cbRemShell;   /* Length of user remote shell buffer */
}
USERREGISTER;

typedef USERREGISTER* PUSERREGISTER;

typedef struct
{
    ULONG cbSize;       /* Size of structure */
    PPID  pids;         /* The PIDS of user processes (i.e. user shell) */
    PSZ   pszUser;      /* User name with length <= MAXUSERNAMELEN */
    PSZ   pszPass;      /* Password with length <= MAXPASSLEN */
    PLONG plUid;        /* Destination buffer to hold user UID */
    PSZ   pszShell;     /* Destination buffer to hold user shell */
    ULONG cbShell;      /* Length of user shell buffer */
    PSZ   pszHome;      /* Destination buffer to hold user home dir */
    ULONG cbHome;       /* Length of user home dir buffer */
    PSZ   pszRemShell;  /* Destination buffer to hold user remote shell */
    ULONG cbRemShell;   /* Length of user remote shell buffer */
    ULONG ulPids;	/* The number of pids in pids array. Max is 128 */
}
USERREGISTER2;

typedef USERREGISTER2* PUSERREGISTER2;


/*
 * user_aclquery request packet
 */

typedef struct
{
    ULONG  cbSize;      /* Size of structure */
    LONG   lUid;        /* User uid */
    PSZ    pszUser;     /* User name with length <= MAXUSERNAMELEN */
    PVOID* pAclSet;     /* Pointer to buffer containing user acls (USERACLSET) */
    PULONG pcbAclSet;   /* Length of returned acls buffer in bytes */
    APIRET (* APIENTRY memAlloc)(PVOID pAddr, ULONG cbSize); /* The callback to allocate memory for acls buffers */
}
USERACLQUERY;

typedef USERACLQUERY* PUSERACLQUERY;

/*
 * user_updatecallback request packet
 */

typedef struct
{
    ULONG cbSize;       /* Size of structure */
    PSZ   pszUser;      /* User name with length <= MAXUSERNAMELEN */
    LONG  lUid;         /* User uid */
    ULONG ulMaxProc;    /* Max number of processes allowed to user */
    PVOID pAclSet;      /* buffer containing user acls (USERACLSET) */
    ULONG cbAclSet;     /* size of acls buffer in bytes */
}
USERUPDATE;

typedef USERUPDATE* PUSERUPDATE;

#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* _USERTYPE_H_ */
