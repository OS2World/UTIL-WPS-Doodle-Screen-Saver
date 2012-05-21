.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.9                     *
.*                                                              *
.* Language: General English language                           *
.* Locales : en_*                                               *
.*                                                              *
.* Author  : Doodle                                             *
.*                                                              *
.* Date (YYYY-MM-DD): 2008.04.22.                               *
.*                                                              *
.*                                                              *
.* To create the binary HLP file from this file, use the IPFC   *
.* tool from the toolkit. For more information about it, please *
.* check the IPF Programming Guide and Reference (IPFREF.INF)!  *
.*                                                              *
.*==============================================================*/
:userdoc.
.*
.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h1 res=1000.Screen Saver
:p.The first page of the :hp2.Screen Saver:ehp2. settings is the
:hp2.General Screen Saver settings:ehp2. page. This is the place
where the most common settings of the screen saver can be changed,
like timeout for the saving, and settings of password protection.
:p.
Changing the language of the screen saver is possible by dragging and
dropping a Locale object to this page. More about it can be found
:link reftype=hd res=5000.
here:elink..
:p.
For a detailed explanation of each field, select from the list below:
:ul compact.
:li.:link reftype=hd res=1001.General settings:elink.
:li.:link reftype=hd res=1002.Password protection:elink.
:li.:link reftype=hd res=6001.Undo:elink.
:li.:link reftype=hd res=6002.Default:elink.
:eul.
.*
.* Help for General settings groupbox
.*
:h1 res=1001.General settings
:p.Select :hp2.Screen saving enabled:ehp2. to enable the screen saver. While
it is enabled, the system will monitor user activity (mouse and keyboard 
activity on the Desktop), and will start the screen saving automatically after
a given amount of inactivity.
:p.This amount can be set in minutes at the entry field of :hp2.Start saver 
after xxx minute(s) of inactivity:ehp2..
:note.
Selecting the :hp2.Lockup now:ehp2. menu item from the popup menu of
the Desktop will start the screen saver even if it is not enabled.
:p.If the :hp2.Wake up by keyboard only:ehp2. is selected, the screen saver will
not stop the screen saving when it detects mouse activity, only when it detects
keyboard activity. This is useful for cases where the computer is in a vibrating
environment, or if it is not preferable for the computer to stop the screen saving 
if the mouse is touched for any reason.
:p.Once :hp2.Disable VIO popups while saving:ehp2. is checked, the screen saver will
disable all VIO popups while the screen saving is active. It means that no other
applications will be able to take away the screen and the input devices, not
even the CAD-handler or any similar applications. This may prevent the user from
killing some hanging applications while the screen saver is running, but makes
the system more secure.
.*
.* Help for Password protection groupbox
.*
:h1 res=1002.Password Protection
:p.Select :hp2.Use password protection:ehp2. to enable the password protection for
the screen saver. If the password protection is turned on, then the screen saver will
ask for a password before stopping the screen saving, and will stop it only if the right
password was entered.
:p.If Security/2 is installed, the screen saver can be instructed to use the password
of the current Security/2 user. It means that when the screen saver asks for a password
to stop the screen saving, it will compare this password to the one set for the current
user in Security/2. This can be set by selecting the :hp2.Use password of current
Security/2 user:ehp2. option. If Security/2 is not installed, this option is disabled.
:p.Selecting the :hp2.Use this password:ehp2. option, the screen saver will use a private
password for screen saving. This password can be set or changed by entering the new 
password into the two entry fields. The same password has to be entered into both fields, 
to avoid typos. To change the password to the one that is entered, press the 
:hp2.Change:ehp2. button.
:p.If the :hp2.Delay password protection:ehp2. is selected, the screen saver will not
ask for the password in the first some minutes of the screen saving, and will behave as
if the password protection would not be turned on. However, if the screen saving will 
last longer than the time set at :hp2.Ask password only after xxx minute(s) of 
saving:ehp2., the password will be asked before stopping the screen saving.
:p.The :hp2.Make the first keypress the first key of the password:ehp2. determines how
the screen saver modules behave when the password protection window is about to be shown.
If this option is selected, the keypress that caused the password protection window to
appear will already be used as the first character of the password. If it is not selected,
then the keypress will not be treated as the first password letter, but will be treated 
only as a notification to show the password protection window. Please note that this is
an informational setting only, some screen saver modules may not honor it.
:p.Select :hp2.Start screen saver on system startup:ehp2. to automatically start the
password protected screen saver when the system starts.
:note.
The screen saver will not delay the password protection if it is started by the request 
of the user (by selecting the :hp2.Lockup now:ehp2. menu item from the popup menu of the
Desktop), or if it's started at the system startup sequence.
.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h1 res=2000.Screen Saver
:p.The second page of the :hp2.Screen Saver:ehp2. settings is the
:hp2.DPMS settings:ehp2. page. This is the place where it can be 
told to the screen saver if it should use different 
:link reftype=hd res=2002.
DPMS:elink. services or not, if they are available.
:p.
Changing the language of the screen saver is possible by dragging and
dropping a Locale object to this page. More about it can be found
:link reftype=hd res=5000.
here:elink..
:p.
For a detailed explanation of each field, select from the list below:
:ul compact.
:li.:link reftype=hd res=2001.DPMS settings:elink.
:li.:link reftype=hd res=6001.Undo:elink.
:li.:link reftype=hd res=6002.Default:elink.
:eul.
.*
.* Help for DPMS settings groupbox
.*
:h1 res=2001.DPMS settings
:p.These settings are available only if both the video driver supports DPMS (currently
it is only supported in Scitech SNAP), and the monitor is DPMS capable.
:p.There are four energy saving states for the monitors, according to the DPMS standard.
These are the followings, starting from the less power saving state:
:ol.
:li.The :hp2.on state:ehp2.. This is the state in which the monitor is turned on, 
and working as normal.
:li.The :hp2.stand-by state:ehp2.. The monitor is partially turned off here, but can 
recover very quickly from this state.
:li.The :hp2.suspend state:ehp2.. This is the state in which the monitor is almost fully
turned off.
:li.The :hp2.off state:ehp2. The monitor is turned off in this state.
:eol.
:p.The screen saver always starts from the first state, and goes into more and more 
power saving states as the time passes.
:p.The screen saver will use only those states that are selected here, and switch to
the next state after the given time for that state.
.*
.* Info about DPMS itself
.*
:h1 res=2002.Information about DPMS
:p.DPMS is the abbreviation for :hp2.Display Power Management Signaling:ehp2., a VESA 
interface standard that defines four power management modes for monitors in idle state: 
on, stand-by, suspend and off.
.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h1 res=3000.Screen Saver
:p.The third page of the :hp2.Screen Saver:ehp2. settings is the
:hp2.Screen Saver modules:ehp2. page. This is the place where the list
of available screen saver
:link reftype=hd res=3002.
modules:elink. can be seen, these modules can be configured,
and a module can be selected for being the current screen saver module.
:p.
Changing the language of the screen saver is possible by dragging and
dropping a Locale object to this page. More about it can be found
:link reftype=hd res=5000.
here:elink..
:p.
For a detailed explanation of each field, select from the list below:
:ul compact.
:li.:link reftype=hd res=3001.Screen Saver modules:elink.
:li.:link reftype=hd res=6001.Undo:elink.
:li.:link reftype=hd res=6002.Default:elink.
:eul.
.*
.* Help for Screen Saver modules groupbox
.*
:h1 res=3001.Screen Saver modules
:p.This page shows the list of available screen saver modules, and shows
information about the currently selected module.
:p.The currently selected module is the one which is selected from the list
of available modules. This is the module which will be started by the screen saver
when it's time to save the screen.
:p.A preview of the current module can be seen by selecting :hp2.Show preview:ehp2..
:p.The right side of the page shows information about the current module, like the
name of the module, its version number, and shows if the module supports password 
protection.
:note.
If the current module does not support password protection, then the screen saving will
not be password protected, even if it's set to be at the first settings page of the
screen saver!
:p.Some of the modules can be configured. If the current module can be configured, then
the :hp2.Configure:ehp2. button is enabled. Pressing that button will result in a
module specific configuration dialog.
:p.The :hp2.Start now:ehp2. button can be used to see how the current module would behave 
with all the current settings of the screen saver (including the settings of other screen
saver pages, like :hp2.Delayed password protection:ehp2. and others).
.*
.* Help about Screen Saver modules
.*
:h1 res=3002.Modules
:p.A screen saver module is one or more special DLL file(s), located in the 
:hp3.Modules:ehp3. folder inside the home directory of the screen saver. It 
gets started when the screen saving should be started.
.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h1 res=5000.Setting the language of the screen saver
:p.It is possible to change the language of the screen saver settings pages and
the language of some of the screen saver modules (which support NLS) by dragging
and dropping a :hp2.Locale object:ehp2. from the :hp2.Country palette:ehp2..
:p.The screen saver tries to use the language of the system by default. It gets it
by checking the :hp2.LANG:ehp2. environmental variable. If there are no language
support files installed for that language, the screen saver falls back to the English
language.
:p.However, other languages can also be set, it's not necessary to use the language of
the system for the screen saver. Any of the :hp2.Locale objects:ehp2. from the 
:hp2.Country palette:ehp2. (in System Setup) can be dragged and dropped to the screen 
saver settings pages.
:p.When a :hp2.Locale object:ehp2. is dropped to any of the settings pages, the screen 
saver will check if support for that language is installed or not. If it's not 
installed, it will fall back to the default method, to use the :hp2.LANG:ehp2. 
environmental variable. However, if the language is supported, it will change to that 
language, and will use that language after it.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h1 res=6001.Undo
:p.Select :hp2.Undo:ehp2. to change the settings to those that were
active before this window was displayed.
.*
.* Help for the Default button
.*
:h1 res=6002.Default
:p.Select :hp2.Default:ehp2. to change the settings to those that were
active when you installed the system.
:euserdoc.
