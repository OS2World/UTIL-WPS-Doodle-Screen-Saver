.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.9                     *
.*                                                              *
.* Language   : Swedish language                                         *
.* Locales    : sv_*                                            *
.*                                                              *
.* Author     : Doodle
.*                                                              *
.* Date (YYYY-MM-DD): 2008.04.25.                               *
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
:h1.Sk„rmsl„ckare
:p.Den f”rsta sidan hos :hp2.Sk„rmsl„ckarens:ehp2. inst„llningar „r
sidan f”r :hp2.Allm„nna inst„llningar:ehp2.. Detta „r platsen
d„r de vanligaste inst„llningarna hos sk„rmsl„ckaren kan „ndras, som
t.ex. timeout f”r att spara, och inst„llningar f”r l”senordskydd.
:p.
ndra spr†k hos sk„rmsl„ckaren „r m”jligt genom att dra och sl„ppa ett
Lokalobjekt p† den h„r sidan. Mer om detta kan hittas 
:link reftype=hd res=5000.
h„r:elink..
:p.
F”r en detaljerad f”rklaring av varje f„lt, v„lj fr†n listan nedanf”r:
:ul compact.
:li.:link reftype=hd res=1000.Allm„nna inst„llningar:elink.
:li.:link reftype=hd res=1001.DPMS inst„llningar:elink.
:li.:link reftype=hd res=2000.L”senordsskydd:elink.
:li.:link reftype=hd res=3000.Sk„rmsl„ckarmoduler:elink.
:eul.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for General settings groupbox
.*
:h2 res=1000.Allm„nna inst„llningar
:p.V„lj :hp2.Sk„rmsl„ckare aktiverad:ehp2. f”r att aktivera sk„rmsl„ckaren. N„r den
„r aktiverad, monitorerar systemet anv„ndaraktiviteten 
(mus och tangentbordsaktivitet p† Skrivbordet),
och kommer att starta sk„rmsl„ckaren automatiskt efter en given tidsrymd av inaktivitet.
:p.Tidsrymden kan st„llas in i minuter p† inmatningsf„ltet hos :hp2.Starta sk„rmsl„ckaren efter xxx minut(er)
av inaktivitet:ehp2..
:note.
Val av menyalternativet :hp2.L†s nu:ehp2. fr†n popupmenyn hos Skrivbordet kommer att starta sk„rmsl„ckaren „ven 
om den inte „r aktiverad.
:p.Om :hp2.Vakna upp enbart via tangentbordet:ehp2. „r valt, kommer 
sk„rmsl„ckaren inte att stoppa sk„rmsl„ckningen n„r den uppt„cker
aktivitet hos musen, utan enbart n„r den k„nner av aktivitet hos tangentbordet. Detta „r anv„ndbart n„r en dator st†r i en omgivning
som ger vibrationer, eller om det inte „r ”nskv„rt att datorn 
skall stoppa sk„rmsl„ckningen av n†got sk„l.
:p.N„r :hp2.Avaktivera VIO popups under sparande:ehp2. „r f”rbockat, kommer sk„rmsl„ckaren
att avaktivera alla VIO popups n„r sk„rmsl„ckningen „r aktiv. Det betyder att inga andra
applikationer kommer att kunna ta ”ver sk„rm och inmatningsenheter, inte
ens CAD-hanteraren eller andra liknade applikationer. Detta kan eventuellt hindra anv„ndaren
fr†n att d”da n†gra h„ngande applikationer medan sk„rmsl„ckaren „r ig†ng, men det
g”r systemet s„krare.

:p.:link reftype=hd res=1001.DPMS inst„llningar:elink.

.*
.* Help for DPMS settings groupbox
.*
:h2 res=1001.DPMS inst„llningar
:p.Dessa inst„llningar „r endast tillg„ngliga om videodrivrutinen st”der DPMS (f”r n„rvarande „r det endast
tillg„ngligt med Scitech SNAP), och monitorn kan hantera DPMS.
:p.Det finns fyra energisparl„gen f”r monitorer, i enlighet med DPMS standarden. Dessa „r f”ljande,
startande fr†n de minst energisparande l„gena:
:ol.
:li.L„get :hp2.P†:ehp2.. Detta „r l„get d„r monitorn „r p†slagen, 
och fungerar normalt.
:li.L„get :hp2.Stand-by:ehp2.. Monitorn „r delvis avslagen h„r, men 
kan v„ldigt snabbt †terh„mtas fr†n detta l„ge.
:li.L„get :hp2.Suspendera:ehp2.. I detta l„ge „r monitorn n„stan 
helt avslagen.
:li.L„get :hp2.Av:ehp2.. Monitorn „r avslagen i detta l„ge.
:eol.
:p.Sk„rmsl„ckaren startar alltid fr†n det f”rsta l„get, och g†r till mer och mer energisparande l„gen i takt
med att tiden g†r.
:p.Sk„rmsl„ckaren kommer endast att anv„nda de l„gen som „r valda h„r, och v„xla till n„sta l„ge efter den tid som
angivits f”r detta l„ge.

:p.:link reftype=hd res=1002.Information om DPMS:elink.
:p.:link reftype=hd res=1000.Allm„nna inst„llningar:elink.

.*
.* Info about DPMS itself
.*

:h3 res=1002.Information om DPMS
:p.DPMS „r en f”rkortning f”r :hp2.Display Power Management Signaling:ehp2., en VESA interfacestandard som definierar
fyra energisparl„gen f”r monitorer som „r vilande: p†, stand-by, suspendera och av.

:p.:link reftype=hd res=1001.DPMS inst„llningar:elink.
:p.:link reftype=hd res=1000.Allm„nna inst„llningar:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Password protection groupbox
.*
:h2 res=2000.L”senordsskydd
:p.V„lj :hp2.Anv„nd l”senordsskydd:ehp2. f”r att aktivera l”senordsskydd hos sk„rmsl„ckaren. Om
l”senordsskyddet „r aktiverat, kommer sk„rmsl„ckaren att fr†ga efter ett l”senord innan
sk„rmsl„ckningen avbryts, och kommer endast 
att avbrytas om r„tt l”senord anges.
:p.Om Security/2 „r installerad, kan sk„rmsl„ckaren instrueras att anv„nda l”senordet hos den
aktuella Security/2 anv„ndaren. Det betyder att n„r sk„rmsl„ckaren fr†gar efter ett l”senord f”r
att avbryta sk„rmsl„ckningen. kommer den att j„mf”ra detta l”senord med det som „r inst„llt f”r den
aktuella anv„ndaren i Security/2. Detta kan st„llas in genom att v„lja alternativet :hp2.Anv„nd l”senord hos aktuell
Security/2 anv„ndare:ehp2.. Om Security/2 inte „r installerad, kommer alternativet att vara avaktiverat.
:p.Genom att v„lja alternativet :hp2.Anv„nd detta l”senord:ehp2., kommer sk„rmsl„ckaren att v„lja ett privat
l”senord f”r sk„rmsl„ckning. Detta l”senord kan st„llas 
in eller „ndras genom att ange ett nytt l”senord i de tv† 
inmatningsf„lten. Samma l”senord m†ste anges i b†da f„lten, f”r att undvika skrivfel. F”r att „ndra ett l”senord till det
som angivits, tryck knappen :hp2.ndra:ehp2..
:p.Om :hp2.F”rdr”j l”senordsskydd:ehp2. „r valt, kommer sk„rmsl„ckaren inte att fr†ga efter l”senordet
under de f”rsta minuterna av sk„rmsl„ckningen, och kommer att bete sig som om l”senordsskydd inte vore
aktiverat. Emellertid, om sk„rmsl„ckningen kommer att vara l„ngre „n den 
tid som st„llts in hos :hp2.Fr†ga efter l”senord endast efter xxx minut(er) av 
sl„ckning:ehp2., kommer l”senordet att efterfr†gas innan sk„rmsl„ckaren st„ngs av.
:p.:hp2.G”r den f”rsta tangentryckningen med f”rsta tecknet i l”senordet:ehp2. best„mmer hur
sk„rmsl„ckarmodulerna beter sig f”nstret med l”senordskydd „r p† v„g att visas.
Om detta alternativ „r valt, kommer tangentryckningen som orsakade att f”nstret med l”senordsskydd
upptr„dde att anv„ndas som f”rsta tecken i l”senordet. Om detta inte „r valt,
kommer tangentryckningen inte att anv„ndas som f”rsta tecken i l”senordet, utan endast betraktas 
som en notifikation f”r att visa f”nstret med l”senordsskydd. Var v„nlig och notera att detta
endast „r en informell inst„llning, vissa sk„rmsl„ckarmoduler kommer kanske inte att framh„va det.
:p.V„lj :hp2.Starta sk„rmsl„ckaren vid systemstart:ehp2. f”r att automatiskt starta den l”senordsskyddade
sk„rmsl„ckaren n„r systemet startas.
:note.
Sk„rmsl„ckaren kommer inte att f”rdr”ja l”senordsskydd om den startas p† beg„ran av anv„ndaren (genom
att v„lja menyalternativet :hp2.L†s nu:ehp2. fr†n popupmenyn hos Skrivbordet), eller om den startas under
systemets startsekvens.



.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Screen Saver modules groupbox
.*
:h2 res=3000.Sk„rmsl„ckarmoduler
:p.Den h„r sidan visar en lista ”ver tillg„ngliga sk„rmsl„ckarmoduler, och ger information
om den f”r n„rvarande valda modulen.
:p.Den aktuella modulen „r den som „r vald fr†n listan ”ver tillg„ngliga 
moduler. Detta „r den modul som kommer att startas av sk„rmsl„ckaren 
n„r det „r dags att sl„cka sk„rmen.
:p.En f”rhandsvisning av den aktuella modulen kan ses genom att v„lja :hp2.Visa f”rhandsvisning:ehp2..
:p.Den h”gra delen av sidan visar information om 
den aktuella modulen, som namnet hos modulen, dess
versionsnummer, och visar om modulen st”der l”senordsskydd.
:note.
Om den aktuella modulen inte st”der l”senordsskydd, kommer sk„rmsl„ckaren inte att bli l”senordsskyddad,
„ven om den „r inst„lld p† att vara det p† f”rsta sidan 
hos sk„rmsl„ckaren!
:p.Vissa av modulerna kan konfigureras. Om den aktuella modulen kan konfigureras, kommer knappen :hp2.Konfigurera:ehp2.
att bli aktiverad. Tryck p† knappen kommer att resultera i en 
modulspecifik konfigurationsdialog.
:p.Knappen :hp2.Starta nu:ehp2. kan anv„ndas f”r att se om den aktuella modulen skulle bete sig med alla
aktuella inst„llningar hos sk„rmsl„ckaren (inkluderande inst„llningarna p† de andra sk„rmsl„ckarsidorna,
som :hp2.F”rdr”jt l”senordsskydd:ehp2. och andra).

:p.:link reftype=hd res=3001.Moduler:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.Moduler
:p.En sk„rmsl„ckarmodul best†r av en eller flera special DLL fil(er), 
i katalogen :hp3.Modules:ehp3. under
hemkatalogen hos sk„rmsl„ckaren. Den startas n„r sk„rmsl„ckaren skall startas.

:p.:link reftype=hd res=3000.Sk„rmsl„ckarmoduler:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h2 res=5000.St„lla in spr†ket hos sk„rmsl„ckaren
:p.Det „r m”jligt att „ndra spr†ket hos sk„rmsl„ckarens inst„llningssidor och spr†ket hos vissa
sk„rmsl„ckarmoduler (vilka st”der NLS) genom att dra och 
sl„ppa ett :hp2.Lokalobjekt:ehp2. fr†n:hp2.Spr†kpaletten:ehp2..
:p.Sk„rmsl„ckaren f”rs”ker att anv„nda spr†ket som „r standard p† systemet. 
Den hittar det genom att kontrollera milj”variabeln :hp2.LANG:ehp2.. Om det 
inte finns n†gra spr†kst”dsfiler f”r det spr†ket, kommer sk„rmsl„ckaren
att falla tilllbaka p† det Engelska spr†ket.
:p.Emellertid, „ven andra spr†k kan st„llas in, det 
„r inte n”dv„ndigt att anv„nda spr†ket hos systemet f”r
sk„rmsl„ckaren. Vilket som helst av :hp2.Lokalobjekten:ehp2. fr†n :hp2.Spr†kpaletten:ehp2. (i Systemkonfiguration)
kan dras och sl„ppas ”ver sk„rmsl„ckarens inst„llningssidor.
:p.N„r ett :hp2.Lokalobjekt:ehp2. sl„pps ”ver n†gon av inst„llningssidorna, kommer sk„rmsl„ckaren att kontrollera om
st”d f”r det spr†ket „r installerat eller inte. 
Om det inte finns installerat, kommer den att falla tillbaka p†
standardmetoden, f”r att anv„nda milj”variabeln :hp2.LANG:ehp2.. Emellertid, om spr†ket har st”d, kommer den att „ndra
till det spr†ket, och anv„nda spr†ket efter detta.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.ngra
:p.V„lj :hp2.ngra:ehp2. f”r att „ndra inst„llningarna till de 
som var aktiva innan detta f”nster visades.
.*
.* Help for the Default button
.*
:h3 res=6002.Standard
:p.V„lj :hp2.Standard:ehp2. f”r att „ndra inst„llningarna 
till de som var aktiva n„r du installerade systemet.
:euserdoc.
