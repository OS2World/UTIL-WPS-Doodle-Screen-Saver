.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.9                     *
.*                                                              *
.* Language: General English language                           *
.* Locales : nl_*                                               *
.*                                                              *
.* Author  : MrFawlty                                           *
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
.* General help for the page
.*--------------------------------------------------------------*/
.*
:h1 res=4000.Blokkeren
:p.De eerste pagina van de :hp2.Blokkeren:ehp2. instellingen is de
:hp2.Algemene blokkeringsinstellingen:ehp2. pagina. Dit is de plaats
waar de meest gebruikte instellingen van de scherm beveiliger aangepast kunnen
worden, zoals wachttijd voor het blokkeren en instellingen voor wachtwoord
beveiliging.
:p.
Het wijzigen van de taal van de scherm beveiliger is mogelijk door het
slepen en loslaten van een Locale object op deze pagina. Meer hierover
kan :link reftype=hd res=5000.
hier:elink. gevonden worden.
:p.
Voor een gedetailleerde verklaring van elk veld kunt u uit de onderstaande
lijst selecteren:
:ul compact.
:li.:link reftype=hd res=1000.Algemene instellingen:elink.
:li.:link reftype=hd res=1001.DPMS instellingen:elink.
:li.:link reftype=hd res=2000.Wachtwoord beveiliging:elink.
:li.:link reftype=hd res=3000.Blokkeringsmodules:elink.
:eul.

.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for General settings groupbox
.*
:h2 res=1000.Algemene instellingen
:p.Selecteer :hp2.Blokkeren ingeschakeld:ehp2. om de scherm beveiliging te activeren.
Zolang het geactiveerd is, zal het systeem de gebruikersactiviteit in de gaten houden (muis en
toetsenbord activiteit op de Werkplek), en zal de scherm beveiliging automatisch starten
na een opgegeven tijd van inactiviteit.
:p.Deze tijd kan in minuten opgegeven worden in het invoerveld van :hp2.Start
blokkeren na xxx minu(u)t(en) van inactiviteit:ehp2.
:p.:hp2.Opmerking: :ehp2.
Selectie van de :hp2.Blokkeren:ehp2. menu optie, vanuit het popup menu van
de Werkplek, zal de scherm beveiliging starten ook al is die niet geactiveerd.
:p.Als :hp2.Alleen deblokkeren met toetsenbord:ehp2. geselecteerd is, zal de
scherm beveiliging niet stoppen als het een muis activiteit constateert, maar alleen bij
een toetsenbord activiteit. Dit is handig in de situatie waarin de computer zich in
een niet stabiele omgeving bevindt, of waarin het niet wenselijk is dat de computer
stopt met de scherm beveiliging door een muis activiteit om wat voor reden dan ook.
:p.Als :hp2.VIO hulpvensters tijdens het opslaan uitschakelen:ehp2. geactiveerd is, zal
de scherm beveiliging alle VIO hulpvensters uitschakelen zolang het blokkeren actief is.
Dit betekent dat andere programma's het scherm en de invoerapparatuur niet kunnen overnemen, zelfs
niet de CAD-verwerker of gelijksoortige programma's. Dit kan de gebruiker de mogelijkheid
ontnemen om vastzittende programma's te onderbreken als de scherm beveiliging actief is,
maar het maakt het systeem wel veiliger.

:p.:link reftype=hd res=1001.DPMS instellingen:elink.

.*
.* Help for DPMS settings groupbox
.*
:h2 res=1001.DPMS instellingen
:p.Deze instellingen zijn alleen beschikbaar als zowel het videokaart stuurprogramma DPMS
ondersteunt (momenteel is dat alleen Scitech SNAP) alsook het beeldscherm DPMS geschikt is.
:p.Er zijn vier energie besparingsstanden voor de beeldschermen, conform de DPMS standaard.
Dit zijn de volgende, beginnende bij de minst besparende energie modus:
:ol.
:li.De :hp2.aan stand:ehp2.. Dit is de stand waarin het beeldscherm aan staat, 
en normaal werkt.
:li.De :hp2.waakstand:ehp2.. Het beeldscherm is hier deels uitgeschakeld, maar kan
snel hersteld worden vanuit deze stand.
:li.De :hp2.ruststand:ehp2.. Dit is de stand waarin het beeldscherm bijna volledig uit
staat.
:li.De :hp2.uit stand:ehp2. Het beeldscherm staat uit in deze stand.
:eol.
:p.De scherm beveiliging begint altijd in de eerste stand, en schakelt over naar meer
en meer energie besparende standen naarmate de tijd verstrijkt.
:p.De scherm beveiliging zal alleen die standen gebruiken die hier geselecteerd zijn,
en overgaan naar de volgende stand na het verstrijken van de opgegeven tijd voor de
voorgaande stand.

:p.:link reftype=hd res=1002.Informatie over DPMS:elink.
:p.:link reftype=hd res=1000.Algemene instellingen:elink.

.*
.* Info about DPMS itself
.*
:h3 res=1002.Informatie over DPMS
:p.DPMS is de afkorting van :hp2.Display Power Management Signaling:ehp2., een VESA
interface standaard dat vier stroom besparende modi definieert voor beeldschermen in
een wacht status: aan, Waakstand, Ruststand en Uit.

:p.:link reftype=hd res=1001.DPMS instellingen:elink.
:p.:link reftype=hd res=1000.Algemene instellingen:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.*
.* Help for Password protection groupbox
.*
:h2 res=2000.Wachtwoord beveiliging
:p.Selecteer :hp2.Gebruik wachtwoord beveiliging:ehp2. om de wachtwoord beveiliging
van de scherm beveiliging te activeren. Als de wachtwoord beveiliging ingeschakeld is,
zal de scherm beveiliging vragen om een wachtwoord voordat het stopt met het blokkeren
van het scherm, en zal het alleen stoppen als het juiste wachtwoord is gegeven.
:p.Als Security/2 ge‹nstalleerd is, kan de scherm beveiliging gevraagd worden om het
wachtwoord van de huidige Security/2 gebruiker te gebruiken. Dit betekent dat als
de scherm beveiliging om een wachtwoord vraagt, om het blokkeren op te heffen, het dit
wachtwoord zal vergelijken met degene die ingesteld is voor de huidige gebruiker volgens
Security/2. Dit kan ingesteld worden via de :hp2.Gebruik wachtwoord van huidige Security/2
gebruiker:ehp2. optie. Als Security/2 niet ge‹nstalleerd is zal deze optie uitgeschakeld
zijn.
:p.Na de selectie van :hp2.Gebruik dit wachtwoord:ehp2., zal de scherm beveiliging
een eigen wachtwoord gebruiken voor het blokkeren. Dit wachtwoord kan ingesteld of
gewijzigd worden door het nieuwe wachtwoord in te voeren bij de twee invoervelden.
Hetzelfde wachtwoord dient in beide velden ingevoerd te worden, om typefouten te
voorkomen. Om het wachtwoord te veranderen in de nieuwe dient op de :hp2.Wijzigen:ehp2. knop
geklikt te worden.
:p.Als de :hp2.Vertraagde wachtwoord beveiliging:ehp2. is geselecteerd, zal de
scherm beveiliging de eerste minuten van blokkeren niet om een wachtwoord vragen,
en zal het reageren alsof de wachtwoord beveiliging niet geactiveerd is. Echter, als
het blokkeren langer duurt dan de ingestelde tijd bij :hp2.Vraag wachtwoord
alleen na xxx minu(u)t(en) van blokkeren:ehp2., zal om het wachtwoord gevraagd
worden voordat gestopt wordt met de blokkering.
:p.De :hp2.Maak de eerst ingetoetste karakter het eerste karakter van het wachtwoord:ehp2.
bepaalt hoe de scherm beveiligingsmodule zal reageren als het wachtwoord beveiligingsvenster
op het punt staat te voorschijn te komen. Als deze optie geselecteerd is, zal de eerste
toetsaanslag die gebruikt is om het wachtwoord beveiligingsvenster te krijgen gelijk
gebruikt worden als het eerste karakter van het wachtwoord. Als deze optie niet geselecteerd
is, zal de eerste toetsaanslag niet beschouwd worden als het eerste karakter van het
wachtwoord, maar als indicatie om het wachtwoord beveiligingsvenster te tonen. Denk er om
dat dit slechts een informatieve instelling is, sommige scherm beveiligingsmodules
kunnen het negeren.
:p.Selecteer :hp2.Blokkeren bij opstarten:ehp2. om de wachtwoord
beveiligde scherm beveiliging automatisch te starten bij het opstarten van het systeem.
:p.:hp2.Opmerking: :ehp2.
De scherm beveiliging zal geen wachttijd voor de wachtwoord beveiliging gebruiken als het
door een gebruikersverzoek gestart is (door selectie van de :hp2.Blokkeren:ehp2.
menu optie vanuit het popup menu van de Werkplek), of als het gestart is in het
systeem opstart proces.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Screen Saver modules groupbox
.*
:h2 res=3000.Blokkeringsmodules
:p.Deze pagina laat de lijst met beschikbare blokkeringsmodules zien,
en laat informatie zien over de momenteel geselecteerde module.
:p.De momenteel geselecteerde module is degene die geselecteerd is vanuit
de lijst met beschikbare modules. Dit is de module die gestart zal worden door
de scherm beveiliging als het tijd is om het scherm te blokkeren.
:p.Een voorbeeld van de huidige module kan bekeken worden na selectie van :hp2.Toon voorbeeld:ehp2..
:p.Aan de rechterkant van de pagina wordt informatie getoond over de huidige module, zoals
de naam van de module, het versie nummer, en laat zien of de module wachtwoord
beveiliging ondersteund.
:p.:hp2.Opmerking: :ehp2.
Als de huidige module geen wachtwoord beveiliging ondersteund, zal de scherm beveiliging
niet wachtwoord beveiligd zijn, ook al is dat ingesteld op de eerste instellingen pagina
van blokkeren!
:p.Sommige modules kunnen ingesteld worden. Als de huidige module ingesteld kan worden,
dan zal de :hp2.Instellen:ehp2. knop geactiveerd zijn. Na het klikken op die knop zal
een module specifiek configuratie scherm te voorschijn komen.
:p.De :hp2.Start nu:ehp2. knop kan gebruikt worden om te zien hoe de huidige module
zich gedraagt met alle huidige instellingen van de scherm beveiliging (inclusief de
instellingen van de andere blokkeringspagina's, zoals :hp2.Vertraagde wachtwoord
beveiliging:ehp2. en anderen).

:p.:link reftype=hd res=3001.Modules:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.Modules
:p.Een blokkeringsmodule is/zijn ‚‚n of meer speciale DLL bestand(en),
in de :hp3.Modules:ehp3. map binnen de basis directory van de scherm
beveiliging. Het wordt gestart als de scherm beveiliging gestart zou
moeten worden.

:p.:link reftype=hd res=3000.Blokkeringsmodules:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h2 res=5000.Het instellen van de taal van de scherm beveiliging
:p.Het is mogelijk om de taal van de blokkeren instellingen pagina's
te wijzigen en de taal van sommige van de blokkeringsmodules (die NLS
ondersteunen) door het slepen en loslaten van een :hp2.Locale object:ehp2. vanuit
de :hp2.Landinstellingen:ehp2..
:p.De scherm beveiliging probeert standaard de taal van het systeem te gebruiken.
Dat krijgt het door de :hp2.LANG:ehp2. omgevingsvariabele te controleren. Als er
geen taal ondersteuningsbestanden voor die taal ge‹nstalleerd zijn, zal de
scherm beveiliging terug vallen op de Engelse taal.
:p.Echter, andere talen kunnen ook ingesteld worden, het is niet nodig om de taal van
het systeem te gebruiken voor de scherm beveiliging. Elk van de :hp2.Locale objecten:ehp2.
vanuit de :hp2.Landinstellingen:ehp2. (in Configuratie) kan gesleept en losgelaten
worden op het notitieblok met de blokkeren pagina's.
:p.Als een :hp2.Locale object:ehp2. op ‚‚n van de instellingen pagina's losgelaten wordt,
zal de scherm beveiliging controleren of ondersteuning voor die taal ge‹nstalleerd is of
niet. Als het niet ge‹nstalleerd is zal het terugvallen op de standaard methode, om de
:hp2.LANG:ehp2. omgevingsvariabele te gebruiken. Echter, als de taal ondersteund is,
zal het wijzigen naar die taal, en zal het verder die taal gebruiken.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.Beginwaarde
:p.Selecteer :hp2.Beginwaarde:ehp2. om de instellingen te veranderen in die welke
actief waren voordat dit venster getoond werd.
.*
.* Help for the Default button
.*
:h3 res=6002.Standaard
:p.Selecteer :hp2.Standaard:ehp2. om de instellingen te veranderen in die welke
actief waren toen het systeem ge‹nstalleerd werd.
:euserdoc.
