<NOTE: This file is best viewed with a monospaced font, like System Monospaced>

                             Screen Saver v1.8
                             =================


About
-----

This is a new screen saver solution for OS/2, ArcaOS and eComStation systems.
Its main goal was to have an open-source screen saver which can be developed
further by others, if needed, and which can co-operate with third party 
applications, like WarpVision, so the screen saver will not activate itself
while the user watches a movie.

The screen saver integrates itself into the WPS, replacing the old Lockup
facility with an extendable screen saver.


Configuring the screen saver
----------------------------

Open the Desktop->Properties notebook, and you can do all the configuration
in the "Screen Saver" tab.


Problems with DPMS (monitor power states)
-----------------------------------------

Doodle's Screen Saver uses the DPMS functions provided by the Scitech SNAP
video drivers, if they are installed. These work pretty well in most of the
cases. There are some cases, however, where SNAP cannot turn off the monitor.
It can be checked by running the 'gactrl.exe' application of SNAP, and trying
to set Stand-By or Off mode to the monitor, and checking if it really goes
blank. If it doesn't, then you have one of the unsupported monitors.

Nothing is lost in these cases. Starting from Doodle's Screen Saver v1.7.5,
there is a REXX script included in the distribution, which can tell the 
screen saver that the current hardware is a problematic one, and the DPMS
services of the SNAP video driver should not be used, but the monitor should
be blanked by direct programming of the VGA registers. This might be a bit
risky and brute-force method, and it only supports the On and Off states of
the monitor, but it's still more than nothing.

That REXX script can be found in the home folder of the screen saver, and
program objects have also been created for it on the desktop, in the folder
named "Doodle's Screen Saver". These program objects are named as
"Use SNAP for DPMS" and "Use Direct VGA Access for DPMS". All you have to do
is to double click on one of them, and restart the desktop for the changes to
take effect.


Setting the language of the screen saver
----------------------------------------

The screen saver is NLS enabled. It uses the language of the system by default,
and gets it by reading the LANG environmental variable.

It is possible to set a different language by dropping the selected locale 
object to one of the screen saver settings pages. In this case, the screen 
saver will remember this locale, and will not use the LANG environmental 
variable as long as a language files for the selected language exist.

The screen saver language files are located in the 'Lang' folder inside the
home directory of the screen saver. Installing support for new languages is
as easy as copying new language support files into this directory.


Adding new saver modules
------------------------

Adding new modules is easy. All you have to do is to copy the DLL file(s) into
the 'Modules' folder inside the home directory of the screen saver.


Developing new saver modules
----------------------------

This is easy too. If you're interested, please check the homepage of the 
screen saver at http://dssaver.netlabs.org !
Two articles have been posted to EDM/2 (http://www.edm2.com) about this topic,
those can be a good reference, too.

Belive me, it's very easy to create new modules.


Making new NLS versions
-----------------------

If your language is not yet supported by the screen saver, please contact me,
and I'll send you two text files which contains all the text which has to be
translated to your own language.

Once it's translated, and that text file is processed into binary language
support files, it's ready to be used! Copy it to the ($home)\Lang folder, and
drag'n'drop your locale to the settings window to activate it!


Contacting the author
---------------------

If you have any questions, remarks, bugreports, or you only would like to say
hello, I can be reached at

Doodle <doodle@scenergy.dont.spam.please.dfmk.hu>
