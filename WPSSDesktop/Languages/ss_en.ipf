.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v2.5                     *
.*                                                              *
.* Language: General English language                           *
.* Locales : en_*                                               *
.*                                                              *
.* Author  : Doodle                                             *
.* Updates : Dave Yeo, Lewis Rosenthal, Alfredo Fern ndez.      *
.*                                                              *
.* Date (YYYY-MM-DD): 2024-01-02.                               *
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
.* Formerly a dummy H1 panel to keep TOCs tidy (more important when
.* invoking this as part of the help subsystem), now both Overview
.* and landing page when querying the WPS for help on "Lockup now".
.*--------------------------------------------------------------*/
:h1.Screen saver
:p.Doodle's Screen Saver replaces the standard WPS Lockup facility, and it will
be activated through the :hp2.Lockup now:ehp2. menu item of the Desktop context
menu, and any equivalent actions such as the corresponding Toolbar action
button, WarpCenter or XCenter objects.

:p.The screen saver will immediately restrict access to your computer by
locking the keyboard and mouse if you have specified
:link reftype=hd res=2000.a password:elink. in its tab in the Desktop
properties notebook.

:p.There, it can be set to start automatically after a specified amount of
time, :link reftype=hd res=1000.manage power saving in monitors:elink. that
support it, and display a variety of colorful and visually attractive
:link reftype=hd res=3000.modules:elink. that you can enjoy while it is active.

:p.Select a title below for more information&colon.

:ul compact.
:li.:link reftype=hd res=1000.General / Power saving settings:elink.
:li.:link reftype=hd res=2000.Password protection:elink.
:li.:link reftype=hd res=3000.Screen Saver modules:elink.
:eul.

.*
.*--------------------------------------------------------------*\
.* Help for Page 1 of the Screen Saver, General / Power saving
.*--------------------------------------------------------------*/
.*
:h2 res=1000.General / Power saving settings
:p.:hp7.General settings:ehp7.

:p.Select :hp2.Screen saving enabled:ehp2. to enable the screen saver and
access all of its settings. While it is enabled, the system will monitor user
activity (mouse and keyboard activity on the Desktop), and will start screen
saving automatically after a given amount of inactivity.

:p.Specify the timeout interval in minutes in the entry field of :hp2.Start
saver after xxx minute(s) of inactivity:ehp2. using the spinbuttons.

:note.Selecting :hp2.Lockup now:ehp2. from the Desktop context menu will start
the screen saver even if it has not been enabled, provided one of its installed
:link reftype=hd res=3000.modules:elink. is active (selected).

:p.When :hp2.Wake up by keyboard only:ehp2. is selected, the screen saver will
not stop running when it detects mouse activity, but instead only when keyboard
activity has been detected. This is useful for cases where the computer is in a
vibrating environment, or if it is not preferable for the computer to stop the
screen saving if the pointing device is touched for any reason.

:p.When :hp2.Disable VIO popups while saving:ehp2. is checked, the screen saver
will disable all VIO popups while running. In this case, no other applications
will be able to take away the screen and the input devices, including the
CAD-handler or any similar applications. This should enhance system security,
but may preclude activities such as manual process killing while the screen
saver is running.

.* former :h2 res=1001.DPMS settings panel, now 2nd groupbox in the same page

:p.:hp7.Power saving:ehp7.

:p.These settings are available only if both the video driver supports
:link reftype=hd res=1002.DPMS or DPM:elink. (currently supported by SNAP
and Panorama 1.17 and later) on your hardware, and the monitor itself is DPMS
or DPM compliant.

:p.There are four energy saving states for DPMS-compliant monitors, and these
map to just two for DPM-compliant monitors. Beginning with the least
power-saving state, these are&colon.

:dl tsize=12 break=none.
:dt.:hp2.On:ehp2.
:dd.The monitor is on, and no power saving is employed.

:dt.:hp2.Stand-by:ehp2.
:dd.DPMS&colon. The monitor is partially powered down, but can recover very
quickly to full power. Horizontal sync is turned off, while vertical sync
remains on. In this state, a CRT monitor should consume &lt.80&percent. of full
power.
.br
.br
DPM&colon. The monitor is off.

:dt.:hp2.Suspend:ehp2.
:dd.DPMS&colon. The monitor is almost fully powered down. Horizontal sync is
turned on, while vertical sync is turned off. In this state, a CRT monitor
should consume &lt.30W.
.br
.br
DPM&colon. The monitor is off.

:dt.:hp2.Off:ehp2.
:dd.DPMS &amp. DPM&colon. The monitor is fully powered down. Both horizontal and
vertical sync are turned off. In this state, a CRT monitor should consume
&lt.8W, with the only power used for LED indicators.
:edl.

:p.Screen saving usually starts from the :hp2.On:ehp2. state. More states may be
available, and if the monitor supports one or more of them, the amount of time
allowed before switching to subsequent states may be configured.

:p.:link reftype=hd res=1002.Information about DPMS &amp. DPM:elink.

.*
.* Info about DPMS itself
.*
:h3 res=1002.Information about DPMS &amp. DPM
:p.DPMS (:hp2.Display Power Management Signaling:ehp2.) is a VESA interface
standard which defines four power management modes, or states, for
monitors&colon. On, Stand-by, Suspend, and Off.

:p.DPMS 1.0 was issued by the VESA Consorium in 1993, based in part on the
earlier work defined in the US EPA's Energy Star power management
specifications.

:p.DPM (:hp2.Display Power Management:ehp2.) is a later VESA standard, which
supersedes DPMS and defines just two power states&colon. On and Off. DPMS
Standby and Suspend states are mapped to the Off state for DPM-compliant
monitors.

:note.Most modern LCD panels should be DPM-compliant.

:p.
:p.The ACPI (:hp2.Advanced Configuration and Power Interface:ehp2.)
specification also describes power states for display devices beyond the
original analog CRTs for which DPMS was designed. In its Display Device Class
specifications, ACPI relates the four DPMS modes to power states D0 (On) through
D3 (Off), further defining which states are :hp2.Required:ehp2. and which are
:hp2.Optional:ehp2. for various types of monitors.

:p.Different types of displays support varying numbers of power management
states in different ways and recover at different rates. Examples include&colon.

:p.:hp7.CRT monitors:ehp7.
:p.
:dl tsize=12 break=none.
:dt.:hp2.On:ehp2.
:dd.The monitor is on, and no power saving is employed. This state (D0) is
:hp2.Required:ehp2. for ACPI compliance.

:dt.:hp2.Stand-by:ehp2.
:dd.The monitor is partially powered down, but can recover very quickly to full
power. Horizontal sync is turned off, while vertical sync remains on. In this
state, the monitor should consume &lt.80&percent. of full power. Recovery time
must be &lt.5 seconds. This state (D1) is :hp2.Optional:ehp2. for ACPI
compliance.

:dt.:hp2.Suspend:ehp2.
:dd.The monitor is almost fully powered down. Horizontal sync is turned on,
while vertical sync is turned off. In this state, the monitor should consume
&lt.30W. Recovery time must be &lt.10 seconds. This state (D2) is required for
ACPI compliance.

:dt.:hp2.Off:ehp2.
:dd.The monitor is fully powered down. Both horizontal and vertical sync are
turned off. In this state, the monitor should consume &lt.8W, with the only
power used for LED indicators. Recovery time should be within &tilde.20 seconds.
This state (D3) is :hp2.Required:ehp2. for ACPI compliance.
:edl.

:p.
:p.:hp7.LCD monitors (analog):ehp7.
:p.
:dl tsize=12 break=none.
:dt.:hp2.On:ehp2.
:dd.The monitor is on, and no power saving is employed. This state (D0) is
:hp2.Required:ehp2. for ACPI compliance.

:dt.:hp2.Stand-by:ehp2.
:dd.The monitor is partially powered down, but can recover very quickly to full
power. Recovery time must be &lt.500ms. This state (D1) is :hp2.Optional:ehp2.
for ACPI compliance, and may be equivalent to :hp2.Off:ehp2. (D3).

:dt.:hp2.Suspend:ehp2.
:dd.The monitor is almost fully powered down. Recovery time must be &lt.500ms.
This state (D2) is :hp2.Optional:ehp2. for ACPI compliance, and may be
equivalent to :hp2.Off:ehp2. (D3).

:dt.:hp2.Off:ehp2.
:dd.The monitor is fully powered down. Recovery time should be &lt.500ms. This
state (D3) is required for ACPI compliance.
:edl.

:p.
:p.:hp7.LCD monitors (digital/DVI):ehp7.
:p.
:dl tsize=12 break=none.
:dt.:hp2.On:ehp2.
:dd.The monitor is on, and no power saving is employed. This state (D0) is
required for ACPI compliance.

:dt.:hp2.Stand-by:ehp2.
:dd.The monitor is partially powered down, but can recover very quickly to full
power. Recovery time must be &lt.250ms. This state (D1) is :hp2.Optional:ehp2.
for ACPI compliance, and may be equivalent to :hp2.Off:ehp2. (D3).

:dt.:hp2.Suspend:ehp2.
:dd.The monitor is almost fully powered down. Recovery time must be &lt.250ms.
This state (D2) is :hp2.Optional:ehp2. for ACPI compliance, and may be
equivalent to :hp2.Off:ehp2. (D3).

:dt.:hp2.Off:ehp2.
:dd.The monitor is fully powered down. Recovery time should be &lt.250ms. This
state (D3) is :hp2.Required:ehp2. for ACPI compliance.
:edl.

:p.
:p.:hp7.Standard TVs, analog HDTVs, projectors:ehp7.
:p.
:dl tsize=12 break=none.
:dt.:hp2.On:ehp2.
:dd.The monitor is on, and no power saving is employed. This state (D0) is
:hp2.Required:ehp2. for ACPI compliance.

:dt.:hp2.Stand-by:ehp2.
:dd.The monitor is partially powered down, but can recover very quickly to full
power. Recovery time must be &lt.100ms. This state (D1) is :hp2.Optional:ehp2.
for ACPI compliance, and may be equivalent to :hp2.Off:ehp2. (D3).

:dt.:hp2.Suspend:ehp2.
:dd.The monitor is almost fully powered down. Recovery time must be &lt.100ms.
This state (D2) is :hp2.Optional:ehp2. for ACPI compliance, and may be
equivalent to :hp2.Off:ehp2. (D3).

:dt.:hp2.Off:ehp2.
:dd.The monitor is fully powered down. Recovery time should be &lt.100ms. This
state (D3) is :hp2.Required:ehp2. for ACPI compliance.
:edl.

.* :p.:link reftype=hd res=1001.DPMS settings:elink.
:p.:link reftype=hd res=1000.General Screen Saver settings:elink.

.*
.*--------------------------------------------------------------*\
.* Help for Page 2 of the Screen Saver, Password protection
.*--------------------------------------------------------------*/
.*
:h2 res=2000.Password Protection
:p.Select :hp2.Use password protection:ehp2. to enable rudimentary security for
the screen saver. If password protection is on, the screen saver will prompt
for a password before stopping.

:note.An unlimited number of password attempts are allowed.

:p.If Security/2 is installed, the screen saver can be instructed to use the
password of the current Security/2 user. To use this feature, select the
:hp2.Use password of current Security/2 user:ehp2. option. If Security/2 is not
installed, this option is disabled.

:p.The :hp2.Use this password:ehp2. option (selected by default when Security/2
has not been installed), enables the password and verification entry fields. To
set a new password, complete both fields and press the :hp2.Change:ehp2.
button.

:p.When :hp2.Delay password protection:ehp2. is selected, the screen saver will
not prompt for the password during the first minutes of the screen saving,
settable with the :hp2.Ask password only after xxx minute(s) of saving:ehp2.
spinbuttons.

:p.The :hp2.Make the first keypress the first key of the password:ehp2.
checkbox determines how the active screen saver module should react when the
password prompt is to be shown. If selected, the keypress which raised the
prompt will be used as the first character of the password. Otherwise, the
keypress will only raise the prompt.

:nt text='CAUTION:'.Some screen saver modules may not honor this setting.:ent.

:p.Select :hp2.Start screen saver on system startup:ehp2. to automatically
start the password protected screen saver when the Desktop initializes.

:note.Password protection is enforced without delay if the screen saver is
started at user request (by selecting :hp2.Lockup now:ehp2. from the Desktop
context menu) or when started at Desktop initialization.

.*
.*--------------------------------------------------------------*\
.* Help for Page 3 of the Screen Saver, Modules
.*--------------------------------------------------------------*/
.*
:h2 res=3000.Screen Saver modules
By default, several different visuals, or :hp1.modules:ehp1., are installed
along Doodle's Screen Saver. The installed modules are listed in the upper
left, where any of them can be selected. When this happens, the module in
question will become the one displayed by DSS whenever it is started.

:p.When a module is thus selected as the active one, information about it is
displayed in the rest of the page: selecting :hp2.Show preview:ehp2. will
display on the right a miniaturized, full motion example of the selected module
as currently configured, and a groupbox below, labelled with its name and
version number, displays whatever information the module author decided to
provide.

:nt text='CAUTION:'.If the active module does not support password protection,
screen saving will not be password protected, even if
:hp2.Use password protection:ehp2. has been set on the
:link reftype=hd res=2000.Password protection:elink. page.:ent.

:p.The :hp2.Start now:ehp2. button may be used to immediately begin screen
saving with all currently selected options.

:p.Some of the modules support configuration. If the current module can be
configured, the :hp2.Configure:ehp2. button will be enabled. Clicking it will
present a module-specific configuration dialog.

:p.:link reftype=hd res=3001.Modules:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.Modules
:p.A screen saver module is comprised of one or more DLLs, located in the
:hp3.Modules:ehp3. subdirectory of the screen saver program directory.

:p.DSS's main goal is to have an open-source screen saver which can be
developed further by others, if needed, and which can co-operate with third
party applications, so the screen saver will not activate itself e.g. while the
user watches a movie.

:p.Writing modules for DSS is both easy and fun!

:p.:link reftype=hd res=3000.Screen Saver modules:elink.

.*
.*--------------------------------------------------------------*\
.* Another dummy panel to keep TOCs tidier by lumping together
.* all common options...
.*--------------------------------------------------------------*/
.*
:h2.Common features and controls
:p.These are features and controls shared among all of the Screen Saver properties
pages. Select any of them in the list below for a detailed explanation:

:ul compact.
:li.:link reftype=hd res=5000.Setting the language of the screen saver:elink.
:li.:link reftype=hd res=6001.Undo:elink.
:li.:link reftype=hd res=6002.Default:elink.
:eul.

.*
.*--------------------------------------------------------------*\
.* Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
:h3 res=5000.Setting the language of the screen saver
:p.The screen saver tries to use the language of the system by default. It
determines this by checking the :hp2.LANG:ehp2. environmental variable. If there
are no language support files installed for that language, the screen saver will
fall back to English.

:p.It is also possible to change the language of the screen saver settings pages
and the language of some of the screen saver modules (which support NLS) by
dragging and dropping a :hp2.Locale object:ehp2. from the
:hp2.Country palette:ehp2. onto any of the screen saver configuration pages.

:p.When such a :hp2.Locale object:ehp2. has been dropped onto any of the
screen saver's configuration pages, the screen saver will check whether or not
support for that language is available. If installed, it will change to that
language. Otherwise, it will fall back to the default method, to use the
:hp2.LANG:ehp2. environmental variable or, ultimately, English.

.*
.*--------------------------------------------------------------*\
.* Help for common buttons -- Undo and Default
.*--------------------------------------------------------------*/
.*
:h3 res=6001.Undo
:p.Select :hp2.Undo:ehp2. to revert the settings on the current page to those
which were active before the last change.

:h3 res=6002.Default
:p.Select :hp2.Default:ehp2. to change the settings on the current page to
installation default values.

:euserdoc.
