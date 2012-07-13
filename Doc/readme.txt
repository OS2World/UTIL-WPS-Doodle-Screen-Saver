
Building Doodle's Screen Saver
------------------------------

You'll need the OpenWatcom C compiler, the OS/2 Developer's Toolkit,
the OS2TK variable pointing to the root of the toolkit.
( e.g.: SET OS2TK=C:\OS2TK45 )

You'll also need the Scitech SNAP SDK to be able to build the SSDPMS.DLL file.


Before building parts of SSaver, you should note that the RC files might 
contain absolute paths inside, and you might replace those to fit your
directory structure.
The reason for this is that the Universal Resource Editor (URE) always puts
absolute paths into the RC files it creates. Sorry for that.


Building parts of SSaver:

- SSCore.dll:
  Enter 'wmake' in the root SSaver folder

- SSDPMS.dll:
  Make sure you initialize the Scitech SNAP SDK environment
  (This is usually done by running the 'start-sdk.cmd' file of your Scitech
   SNAP SDK folder, usually at \Scitech\start-sdk.cmd)
  Enter 'wmake' in the SSDPMS folder

- WPSSDesk.dll:
  Enter 'somenv' in WPSSDesktop folder to setup environment variables for SOM
  Enter 'make' to make WPSSDesk.dll

- National Language Support binary files:
  Enter 'make' in the WPSSDesktop\Languages folder

- Modules:
  Enter 'wmake' in each of the folders of the modules

- Tester:
  Enter 'wmake' in the Tester folder

- Creating an install package:
  Go to 'WarpIn' folder
  Edit the 'create.cmd' file this way:
    - Replace 'c:\Utils\warpin' string with the place where you have WarpIn installed
  Edit the 'dssaver_wic_params.txt' file this way:
    - Replace 'dssaver_v15' string with the name you want to give to the WarpIn archive
  Make sure the install script fits your needs
  Make sure that the core DLL files in 'WarpIn\Core' are there and up to date
  Make sure that the document files in 'WarpIn\Docs' are there and up to date
  Make sure that the saver modules in 'WarpIn\Modules' are there and up to date
  Make sure that the language support files in 'WarpIn\Languages' are there and up to date
  Start 'create.cmd' to create the install package!


The Tester.EXE is a quick hack application, which can be used by saver module
developers to test their saver modules easier. It somehow simulates the
SSCore environment, and shows a preview window (not perfect, for DIVE modules
it might need a window refresh to take the changes).
It can be used to test preview mode, fullscreen mode, password protection,
and configuration of a saver module.


Creating new language support packs
-----------------------------------

To create new language support packs, one has to translate two
files to the destination language.
These files are in the WPSSDesktop\Languages folder.


First one is the file which has every text which appears in the settings
windows of the screen saver. It also has some texts which is sent to the
saver modules (only to the ones supporting it), so the modules can ask the
password in the language set for the whole saver.

This file is the ss_*.txt file, and after compilation, it will be ss_*.msg.

The other file is the one which contains the help for the screen saver.
This is the ss_*.ipf file, and after compilation, it will be ss_*.hlp.


Please note that you have to give it a good name, to work properly.
Let's see how to name the language files through an example!

There are quite a lot of Locales and Locale names. The simpliest ones are
like:
en_US
en_UK
hu_HU
nl_NL
nl_NL_EURO
...

As you can see, the first letters tell the language, and the next letters tell
the geographical location. Also, there are some locales which has some 
variations, like the Dutch, which has Euro'd version and non-Euro'd version.

In the Screen Saver, to make language support flexible, the following
process is used:

- First, a language support file is searched for the full locale name.
  For example, ss_nl_NL_EURO.msg for nl_NL_EURO locale.
- If not found, then characters are cut from locale name until the next
  underscore ( _ ) character, and tried with that locale name, too, until
  either the language support file is found, or we've cut all the locale name.
  In our example: ss_nl_NL.msg, then ss_nl.msg
- If still not found, fall back to default language.

This has the advantage that one can create general language support,
like ss_en.msg for english messages, but, if a given locale requires some
modification (for example, people in UK say something in a different way),
then ss_en_UK.msg can be created, which will be used only for en_UK locale.


Once the new txt and ipf files are ready, modify the create.cmd file in
that folder (see instructions about it in that file), and you're done!


Starting from v1.5, the saver modules have been also made NLS-enabled. This
means that the saver modules will look for their own .msg files that contain
the translated texts for the configuration window and the module description.
You should also translate those .txt files if you want a full translation!
See 'Languages' subdirectories in the directories containing the source code
of the screen saver modules!


Creating new Saver modules
--------------------------

First of all, please read the Programmer's documentation in Doc folder!
After that, you should have a slight overview of the Screen Saver Modules API.

Now, you can start developing your own module. The easiest way is to make a
new folder in Modules, taking an old module as a template, and modifying it.

Just make sure to give it a unique filename and module name (NAME line in .lnk
linker file), and change the module description in the .c file, inside the
SSModule_GetModuleDesc() API.

When you've ready, you can test your DLL with tester.exe. If it works in there,
you can try to "install" it by copying it to the Modules folder of the home
directory of the installed screen saver, and trying it with the real saver.

Also, there is a tutorial about how to develop new modules, at 
EDM/2 ( http://www.edm2.com ).


That's all!

Doodle <doodle@dont.spam.please.scenergy.dfmk.hu>
