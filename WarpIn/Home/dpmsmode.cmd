/* dpmsmode.cmd - Set the DPMS handling mode of Doodle's Screen Saver  */
/*                                                                     */
/* If you want DSSaver to use SNAP's services to set the DPMS modes of */
/* the monitor, set it to the default value, to '0'.                   */
/* However, if you have problems with the DPMS modes, like that SNAP   */
/* cannot reliably set them (and you confirmed it by using the         */
/* gactrl.exe utility of SNAP), try to set it to the value '1'.        */
/*                                                                     */
/* This value determines if DSSaver should force using direct VGA      */
/* register access without using SNAP (value '1'), or if SNAP should   */
/* be used for DPMS settings (value '0', the default).                 */

call RxFuncAdd "SysIni", "RexxUtil", "SysIni"

parse arg arguments

val = subword(arguments, 1, 1)
dowait = subword(arguments, 2, 1)

if (val == "") then
  val = '0'

inifile='USER'
app='SSaver'
key='DPMS_ForceDirectVGAAccess'

call SysIni inifile, app, key, val

if (val == '0') then
  say "DSSaver will use the SNAP API to manage DPMS modes of the monitor."
else
  say "DSSaver will use direct VGA access to manage DPMS modes of the monitor."

say "The desktop has to be restarted for the change to take effect!"

if (dowait \= "") then
  "@pause"

exit 