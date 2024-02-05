/*
   Kill stored Desktop Settings view:

   Look for a stored Desktop settings view; if present, offer to delete it.
*/
if RxFuncQuery('SysLoadFuncs') > 0 then do
  call RxFuncAdd 'SysLoadFuncs','RexxUtil','SysLoadFuncs'
  call SysLoadFuncs
 end

handle = SysIni('', 'PM_Workplace:Location', '<WP_DESKTOP>')
if handle = 'ERROR:' then do
  call lineout stderr,'Error getting <WP_DESKTOP> handle.'
  exit 1
 end

target = c2d(reverse(handle))||'@20'
DVS = SysIni('', 'PM_Workplace:FolderPos', target)
If DVS = 'ERROR:' then do
  call lineout stderr,'<WP_DESKTOP> Settings view not found (?)'
  exit 0
 end

say 'A Desktop settings view is stored in your system INI file.'
call charout stdout,'Get rid of it [y/n]? '
parse value SysCurPos() with scry scrx
answer = ''
do while pos(answer,'YN') = 0
  answer = translate(SysGetKey())
  call SysCurPos scry, scrx
 end
say ''

if answer = 'N' then do
  call lineout stderr,'Stored <WP_DESKTOP> settings view spared.'
  exit 0
 end

call SysIni '', 'PM_Workplace:FolderPos', target, 'DELETE:'
chk = SysIni('', 'PM_Workplace:FolderPos', target)
if chk <> 'ERROR:' then do
  call lineout stderr,'Stored <WP_DESKTOP> settings could not be deleted.'
  exit 1
 end

exit 0
