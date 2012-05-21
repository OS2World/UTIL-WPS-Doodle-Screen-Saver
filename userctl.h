/*
 * Copyright (C) 2002-2003 nickk
 * All rights reserved.
 *
 * This code is a part of Security/2 project.
 * The use, changing and distribution out of Security/2 project is not allowed.
 */

#ifndef _USERCTL_H_
#define _USERCTL_H_

#include <usertype.h>

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------*
 * End user API                            *
 *-----------------------------------------*/

/* 
 * Add new user 
 */
APIRET APIENTRY DosUserAdd(PUSERADD);

/* 
 * Update existing user info
 * User is identified by uid (PUSERADD->lUid), if lUid >= 0, or by username (PUSERADD->pszUser), if lUid < 0.
 * Only !NULL fields willbe updated.
 */
APIRET APIENTRY DosUserChange(PUSERCHANGE);

/* 
 * Remove user from database 
 * User is identified by uid (lUid), if lUid >= 0, or by username (pszUser), if lUid < 0.
 * If deleted user is already registered at this time, the function will return
 * ERROR_ACCESS_DENIED
 */
APIRET APIENTRY DosUserDel(PSZ pszUser, LONG lUid);

/* 
 * Query info about user
 * User is identified by uid (*PUSERADD->plUid), if *plUid >= 0, or by username (PUSERADD->pszUser), if lUid < 0.
 * The *PUSERADD->plUid and PUSERADD->pszUser will be updated from the current values from userbase.
 */
APIRET APIENTRY DosUserQuery(PUSERQUERY);

/*
 * Login (Register) user. If pid = -1 in USERREGISTER or ulPids = 0 in USERREGISTER2
 * the user will not be registered in the system, only given login info will be
 * checked.
 */
APIRET APIENTRY DosUserRegister(PUSERREGISTER);

/* 
 * Update info for logged in user by the user database 
 * User is identified by uid (lUid), if lUid >= 0, or by username (pszUser), if lUid < 0.
 */
APIRET APIENTRY DosUserUpdate(PSZ pszUser, LONG lUid);

/*
 * List all users in userbase. On output the bBuffer will contain array of USERLIST structure.
 */
APIRET APIENTRY DosUserList(PVOID pBuffer, PULONG pcbBuffer);

/*
 * Query user assigned to current process.
 * Returns both username to pszUser and user uid to *plUid.
 */
APIRET APIENTRY DosUserQueryCurrent(PSZ pszUser, ULONG cbUser, PLONG plUid);

/*
 * Query user assigned to process with given pid.
 * If pid = -1, function queries user assigned to current process, which is 
 * identical to DosUserQueryCurrent. To query user of arbitrary process, 
 * current user must have read right to <user> (see acls definitions).
 * Returns both username to pszUser and user uid to *plUid.
 */
APIRET APIENTRY DosUserQueryByPid(PID pid, PSZ pszUser, ULONG cbUser, PLONG plUid);

/*
 * Query pids for current user assigned to current process to the pPids buffer
 * of size contained in pcbPids.
 * On return pcbPids contains the occupied size of pPids buffer (the number of 
 * returned user pids = *pcbPids / sizeof(PID)).
 * If the pMainPid is not null, on return the *pMainPid will contain the pid of
 * the first user process (the process the user registered by DosUserRegister with).
 * If pPids buffer = NULL, function will return required buffer size in pcbPids.
 * If pPids buffer length is not enough to hold the data, ERROR_BUFFER_OVERFLOW 
 * will be returned and pcbPids will contain the required buffer size
 */
APIRET APIENTRY DosUserQueryPids(PPID pPids, PULONG pcbPids, PPID pMainPid);

/*
 * Check if requested action can be performed
 * pid - the pid of checking user, ppid - parent pid of checking user, 
 * ulAction - one or more predefined actions.
 */
APIRET APIENTRY DosUserCheckAction(PID pid, PID ppid, ULONG ulAction, PSZ pszObjectName);

/*
 * Check if requested action can be performed or return available actions for 
 * given object.
 * lUid - the uid of checking user, pszName - name of checking user
 * either lUid or pszName should be set, if both are set, user searched by lUid.
 * pulAction - container where the available action returns. If *pulAction != 0
 * Security/2 checks *pulAction action and returns ERROR_ACCESS_DENIED if it is
 * prohibited and do not return available actions to pulAction.
 */
APIRET APIENTRY DosUserGetAction(LONG lUid, PSZ pszName, PULONG pulAction, PSZ pszObjectName);

/* 
 * Check if process can be killed (i.e, there is enough rights)
 */
APIRET APIENTRY DosUserCheckKill(PID pid, PID ppid, PID pidtokill);

/*
 * Brutally kills the specified process (if called user has enough rights).
 */
APIRET APIENTRY DosUserKillProcess(PID pid);

/* 
 * Set ACL's. 
 * ulSectionType is the type of section to be set, pszSectionName is its name
 * pBuffer should contain the cbBuffer bytes array of USERACL elements
 * If section of given type and name exists, it will be replaced, if pBuffer is
 * NULL, it will be deleted.
 */
APIRET APIENTRY DosUserAclSet(ULONG ulSectionType, PSZ pszSectionName, PVOID pBuffer, ULONG cbBuffer);

/* 
 * Add ACL's. 
 * If section of given type and name exists, it will be replenished by given ACLs, 
 * if this section does not exist, error will be returned.
 */
APIRET APIENTRY DosUserAclAdd(ULONG ulSectionType, PSZ pszSectionName, PVOID pBuffer, ULONG cbBuffer);

/*
 * Get ACL's
 * If pBuffer is null, function just returns required buffer size in pcbBuffer.
 * If pBuffer length is not enough to hold the data, ERROR_BUFFER_OVERFLOW will
 * be returned and pcbBuffer will contain the required buffer size
 */
APIRET APIENTRY DosUserAclQuery(ULONG ulSectionType, PSZ pszSectionName, PVOID pBuffer, PULONG pcbBuffer);

/*
 * List ACL's sections
 * On exit pBuffer will contain the array of USERACLLIST elements.
 * If pBuffer is null, function just returns required buffer size in pcbBuffer.
 * If pBuffer length is not enough to hold the data, ERROR_BUFFER_OVERFLOW will
 * be returned and pcbBuffer will contain the required buffer size
 */
APIRET APIENTRY DosUserAclList(ULONG ulSectionType, PVOID pBuffer, PULONG pcbBuffer);

/*
 * Update (reloads user acls into SSES driver) all users whose ACL's 
 * include given ACL's section.
 * If pszSectionName is NULL, users with all ACL's section of given type will be updated.
 */
APIRET APIENTRY DosUserAclUpdate(ULONG ulSectionType, PSZ pszSectionName);

/*
 * Returns version string of Security/2 into pszVersion buffer. The buffer must
 * be et least 6 bytes in length.
 */
APIRET APIENTRY DosUserQueryVersion(PSZ pszVersion);

#ifdef __cplusplus
}
#endif

#endif /* _USERCTL_H_ */
