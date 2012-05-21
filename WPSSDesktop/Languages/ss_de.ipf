.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.6                     *
.*                                                              *
.* Language: German language                                    *
.* Locales : de_*                                               *
.*                                                              *
.* Author  : Christian Hennecke                                 *
.* Modified: Robert Henschel 2005.05.24                         *
.* Modified: Christian Hennecke 2006.02.10                      *
.* Modified: Christian Hennecke 2008.04.22                      *
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
:h1 res=1000.Bildschirmschoner
:p.Die erste Seite der :hp2.Bildschirmschoner:ehp2.-Einstellungen ist die
Seite :hp2.Allgemein:ehp2.. Hier k�nnen die allgemeinen Einstellungen des
Bildschirmschoners, wie etwa die Zeitspanne bis zum Aktivieren des Schonbetriebs
und Optionen f�r den Kennwortschutz, angepa�t werden.
:p.
Die Sprachversion des Bildschirmschoners l��t sich durch Ziehen und �bergeben
eines Objektes der Landeseinstellungen an diese Seite �ndern. Mehr dazu
:link reftype=hd res=5000.
hier:elink..
:p.
Ausf�hrliche Erl�uterungen f�r jedes Feld stehen durch Auswahl aus nachstehender
Liste zur Verf�gung:
:ul compact.
:li.:link reftype=hd res=1001.Allgemein:elink.
:li.:link reftype=hd res=1002.Kennwortschutz:elink.
:li.:link reftype=hd res=6001.Widerrufen:elink.
:li.:link reftype=hd res=6002.Standard:elink.
:eul.
.*
.* Help for General settings groupbox
.*
:h1 res=1001.Allgemein
:p.Zum Aktivieren des Bildschirmschoners w�hlen Sie :hp2.Bildschirmschoner aktivieren:ehp2..
Ist diese Einstellungen aktiv, �berwacht das System die Benutzeraktivit�t in Form
der Bedienung von Maus und Tastatur auf der Arbeitsoberfl�che und schaltet
nach einer festgelegten Zeitspanne der Inaktivit�t automatisch in den Schonbetrieb um.
:p.Diese Spanne kann mittels des Drehreglers :hp2.Schoner einschalten nach
xxx Minute(n) Inaktivit�t:ehp2. angepa�t werden.
:note text='Hinweis'.
Durch Auswahl des Men�eintrags :hp2.Sperren:ehp2. aus dem Kontextmen� der Arbeitsoberfl�che
wird in den Schonbetrieb umgeschaltet, auch wenn der Bildschirmschoner nicht aktiviert ist.
:p.Wenn :hp2.Aufwachen nur durch Tastatur:ehp2. ausgew�hlt ist, kann der Bildschirmschoner nur
durch einen Tastendruck abgebrochen werden. Mausaktivit�t wird ignoriert. Diese Einstellung
ist besonders hilfreich, wenn sich die Maus in einer wackeligen Umgebung befindet oder
der Bildschirmschoner nicht durch kleine Mausbewegungen unterbrochen werden soll.
:p.Ist :hp2.VIO-Popups im Schonbetrieb unterdr�cken:ehp2. aktiviert, unterdr�ckt der
Bildschirmschoner w�hrend des Schonbetriebs die Anzeige s�mtlicher sich �ffnender VIO-Fenster.
Infolgedessen sind andere Anwendungen nicht mehr in der Lage, die Ger�te f�r Anzeige und Eingabe
zu �bernehmen, auch nicht der CAD-Handler und �hnliche Anwendungen. Zwar kann dann
der Benutzer h�ngende Anwendungen eventuell nicht mehr beenden, w�hrend der Bildschirmschoner
l�uft, jedoch erh�ht dies die Systemsicherheit.
.*
.* Help for password protection groupbox
.*
:h1 res=1002.Kennwortschutz
:p.Um den Bildschirmschoner �ber ein Kennwort abzusichern, markieren Sie :hp2.Kennwortschutz aktivieren:ehp2..
Ist der Kennwortschutz eingeschaltet, erfolgt vor Beendung des Schonbetriebs eine Kennwortabfrage.
Der Schonbetrieb wird nur beendet, wenn das korrekte Kennwort eingegeben wurde.
:p.Ist Security/2 installiert, besteht die M�glichkeit, das Kennwort des gerade angemeldeten
Security/2-Benutzers zu �bernehmen. Das bei der Kennwortabfrage zum Beenden des Schonbetriebs
eingegebene Kennwort wird dann mit dem Kennwort des gerade angemeldeten Security/2-Benutzers
verglichen. Um dieses Verfahren zu verwenden, markieren Sie die Option :hp2.Kennwort des aktiven
Security/2-Benutzers verwenden:ehp2.. Diese Option ist deaktiviert, wenn Security/2 nicht installiert
ist.
:p.Durch Auswahl der Option :hp2.Dieses Kennwort verwenden:ehp2. kann f�r den Schonbetrieb
ein eigenes Kennwort verwendet werden.
Dieses kann durch Eingabe eines neuen Kennwortes in beide Eingabefelder
gesetzt oder ge�ndert werden. Zur Vermeidung von Tippfehlern ist dasselbe Kennwort
in beide Felder einzutragen. Um das eingegebene Kennwort zu �bernehmen, dr�cken Sie
:hp2.�ndern:ehp2..
:p.Ist :hp2.Verz�gerter Kennwortschutz:ehp2. ausgew�hlt, so erfolgt nach den ersten Minuten
Schonbetrieb noch keine Kennwortabfrage und der Bildschirmschoner verh�lt sich so, als ob
der Kennwortschutz nicht aktiviert w�re. Dauert der Schonbetrieb jedoch l�nger an, als
mittels des Drehreglers :hp2.Kennwort erst nach xxx Minute(n) Schonbetrieb abfragen:ehp2.
festgelegt, wird das Kennwort vor Beendung des Schonbetriebs abgefragt.
:p.Die Option :hp2.Ersten Tastendruck als ersten Buchstaben des Kennworts �bernehmen:ehp2. beeinflu�t
das Verhalten der Bildschirmschonermodule beim Aufruf des Dialogs f�r den Kennwortschutz. Ist die Option
aktiviert, so wird die Taste, durch deren Bet�tigung der Aufruf des Dialogs ausgel�st wurde, bereits
als erster Buchstabe des Kennworts interpretiert. Ist die Option nicht aktiviert, wird dieser Tastendruck
dagegen nur als Aufforderung zum Anzeigen des Kennwortschutzdialogs angesehen. Beachten Sie, da� diese
Einstellung eventuell nicht von allen Bildschirmschonermodulen befolgt wird.
:p.W�hlen Sie :hp2.Bildschirmschoner bei Systemstart aktivieren:ehp2., um den Kennwortgesch�tzten
Bildschirmschoner automatisch beim Systemstart zu aktivieren.
:note text='Hinweis'.
Der Kennwortschutz wird nicht verz�gert, wenn der Bildschirmschoner durch den Anwender direkt
(durch Auswahl des Men�eintrags :hp2.Sperren:ehp2. aus dem Kontextmen� der
Arbeitsoberfl�che) oder beim Systemstart aktiviert wird.
.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h1 res=2000.Bildschirmschoner
:p.Die zweite Seite der :hp2.Bildschirmschoner:ehp2.-Einstellungen ist
die Seite :hp2.DPMS:ehp2.. Hier kann festgelegt werden, ob der Bildschirmschoner
verschiedene
:link reftype=hd res=2002.
DPMS:elink.-Dienste verwenden soll, sofern diese verf�gbar sind.
:p.
Die Sprachversion des Bildschirmschoners l��t sich durch Ziehen und �bergeben
eines Objektes mit L�nderspezifischen Angaben an diese Seite �ndern. Mehr dazu
:link reftype=hd res=5000.
hier:elink..
:p.
Ausf�hrliche Erl�uterungen f�r jedes Feld stehen durch Auswahl aus nachstehender
Liste zur Verf�gung:
:ul compact.
:li.:link reftype=hd res=2001.DPMS:elink.
:li.:link reftype=hd res=6001.Widerrufen:elink.
:li.:link reftype=hd res=6002.Standard:elink.
:eul.
.*
.* Help for DPMS settings groupbox
.*
:h1 res=2001.DPMS
:p.Diese Einstellungen stehen nur zur Verf�gung, wenn sowohl der Grafiktreiber DPMS
unterst�tzt als auch der Monitor DPMS-f�hig ist (derzeit nur bei Scitech SNAP der Fall).
:p.Entsprechend dem DPMS-Standard gibt es vier Energiesparmodi f�r Monitore.
Beginnend mit dem am wenigsten energiesparenden Modus sind dies:
:ol.
:li.Der :hp2.On-Modus:ehp2.. In diesem Modus ist der Monitor angeschaltet und arbeitet normal.
:li.Der :hp2.Stand-by-Modus:ehp2.. Hier ist der Monitor teilweise abgeschaltet, kann jedoch
schnell wieder zur�ckgeschaltet werden.
:li.Der :hp2.Suspend-Modus:ehp2.. In diesem Modus ist der Monitor fast vollst�ndig abgeschaltet.
:li.Der :hp2.Off-Modus:ehp2. In diesem Modus ist der Monitor ausgeschaltet.
:eol.
:p.Der Bildschirmschoner beginnt stets beim ersten Modus und schaltet mit der Zeit in mehr
und mehr energiesparende Modi um.
:p.Es werden nur die Modi verwendet, die hier markiert sind, und die Umschaltung erfolgt
jeweils nach der festgelegten Zeit.
.*
.* Info about DPMS itself
.*
:h1 res=2002.Information �ber DPMS
:p.DPMS ist eine Abk�rzung f�r :hp2.Display Power Management Signaling:ehp2., einen
VESA-Schnittstellenstandard, der vier Energieverwaltungsmodi f�r gerade nicht genutzte
Monitore definiert: On, Stand-by, Suspend und Off.
.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h1 res=3000.Bildschirmschoner
:p.Die dritte Seite der :hp2.Bildschirmschoner:ehp2.-Einstellungen ist die
Seite :hp2.Module:ehp2.. Hier wird eine Liste verf�gbarer
:link reftype=hd res=3002.
Module:elink. f�r den Bildschirmschoner angezeigt, die
konfiguriert und zur Verwendung ausgew�hlt werden k�nnen.
:p.
Die Sprachversion des Bildschirmschoners l��t sich durch Ziehen und �bergeben
eines Objektes der Landeseinstellungen an diese Seite �ndern. Mehr dazu
:link reftype=hd res=5000.
hier:elink..
:p.
Ausf�hrliche Erl�uterungen f�r jedes Feld stehen durch Auswahl aus nachstehender
Liste zur Verf�gung:
:ul compact.
:li.:link reftype=hd res=3001.Module:elink.
:li.:link reftype=hd res=6001.Widerrufen:elink.
:li.:link reftype=hd res=6002.Standard:elink.
:eul.
.*
.* Help for Screen Saver modules groupbox
.*
:h1 res=3001.Module
:p.Auf dieser Seite werden eine Liste der verf�gbaren Bildschirmschonermodule
sowie Informationen �ber das derzeit ausgew�hlte Modul angezeigt.
:p.Das derzeit ausgew�hlte ist das Modul, das gerade in der Liste der verf�gbaren
Module hervorgehoben ist. Dieses Modul wird vom Bildschirmschoner verwendet, wenn
in den Schonbetrieb umgeschaltet wird.
:p.Durch Markieren von :hp2.Vorschau anzeigen:ehp2. l��t sich eine Vorschau der
Funktion des derzeit ausgew�hlten Moduls anzeigen.
:p.Im rechten Teil der Notizbuchseite werden Informationen �ber das derzeit ausgew�hlte
Modul angezeigt, wie etwa der Name des Moduls, die Version und ob das Modul den Mechanismus
f�r den Kennwortschutz unterst�tzt.
:note text='Hinweis'.
Unterst�tzt das derzeit ausgew�hlte Modul keinen Kennwortschutz, wird der Schonbetrieb
nicht �ber ein Kennwort abgesichert, auch wenn dies auf der ersten Einstellungsseite
des Bildschirmschoners ausgew�hlt ist!
:p.Manche Module lassen sich konfigurieren. Ist dies beim derzeit ausgew�hlten Modul der Fall,
ist die Schaltfl�che :hp2.Konfigurieren:ehp2. aktiv. Durch Bet�tigen der Schaltfl�che wird
ein modulspezifischer Konfigurationsdialog aufgerufen.
:p.Mit Hilfe der Schaltfl�che :hp2.Test:ehp2. l��t sich feststellen, wie sich das derzeit ausgew�hlte
Modul mit allen aktuellen Einstellungen des Bildschirmschoners (einschlie�lich der Einstellungen
auf anderen Notizbuchseiten, wie :hp2.Kennwortschutz verz�gern:ehp2.) verh�lt.
.*
.* Help about Screen Saver modules
.*
:h1 res=3002.Module
:p.Bei Bildschirmschonermodulen handelt es sich um spezielle DLL-Dateien, die
sich im Unterverzeichnis :hp3.Modules:ehp3. des Heimverzeichnisses des Bildschirmschoners
befinden. Sie werden beim Umschalten in den Schonbetrieb ausgef�hrt.
.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h1 res=5000.Festlegen der Sprachversion des Bildschirmschoners
:p.Die Sprache der Einstellungsseiten des Bildschirmschoners sowie mancher
Module (die Sprachunterst�tzung bieten) l��t sich durch Ziehen und �bergeben
von :hp2.L�nderspezifischen Angaben:ehp2.-Objekten aus der :hp2.L�nderpalette:ehp2.
festlegen.
:p.Standardm��ig versucht der Bildschirmschoner die Systemsprache zu verwenden.
Sie wird aus der Umgebungsvariable :hp2.LANG:ehp2. ermittelt. Sind f�r diese Sprache
keine Sprachunterst�tzungsdateien installiert, verwendet der Bildschirmschoner
Englisch.
:p.Es ist jedoch m�glich, auch andere Sprachen festzulegen. F�r den Bildschirmschoner
mu� nicht die Systemsprache verwendet werden. Es kann jedes beliebige
:hp2.L�nderspezifische Angaben:ehp2.-Objekt aus der
:hp2.L�nderpalette:ehp2. (im Ordner Systemkonfiguration) gezogen und an die
Einstellungsseiten des Bildschirmschoners �bergeben werden.
:p.Wird ein :hp2.L�nderspezifische Angaben:ehp2.-Objekt an eine der Einstellungsseiten
�bergeben, wird gepr�ft, ob Unterst�tzung f�r diese Sprache installiert ist. Ist dies
nicht der Fall, erfolgt dies Auswahl nach der Standardmethode �ber die Umgebungsvariable
:hp2.LANG:ehp2.. Wird die Sprache jedoch unterst�tzt, so wird dauerhaft auf sie umgeschaltet.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h1 res=6001.Widerrufen
:p.Dr�cken Sie :hp2.Widerrufen:ehp2., um die Einstellungen auf den Stand
vor Anzeige dieser Notizbuchseite zur�ckzusetzen.
.*
.* Help for the Default button
.*
:h1 res=6002.Standard
:p.Dr�cken Sie :hp2.Standard:ehp2., um die Einstellungen auf den Installationsstand
zur�ckzusetzen.
:euserdoc.
