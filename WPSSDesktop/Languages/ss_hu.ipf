.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.9                     *
.*                                                              *
.* Language: Hungarian language                                 *
.* Locales : hu_*                                               *
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
.* General help for the page
.*--------------------------------------------------------------*/
.*
:h1.K‚perny‹v‚d‹
:p.A :hp2.K‚perny‹v‚d‹:ehp2. be ll¡t sok els‹ oldala az
:hp2.µltal nos k‚perny‹v‚d‹ be ll¡t sok:ehp2. oldal. Ez az a hely,
ahol a k‚perny‹v‚d‹ leg ltal nosabb be ll¡t sait lehet megv ltoztatni,
mint p‚ld ul a bekapcsol shoz szks‚ges id‹, vagy a jelszavas v‚delem
be ll¡t sai.
:p.
A k‚perny‹v‚d‹ nyelv‚t megv ltoztathatja, amennyiben erre az oldalra
dob egy Locale objektumot a Fogd ‚s Vidd m¢dszerrel. Err‹l b‹vebben
:link reftype=hd res=5000.
itt:elink. olvashat.
:p.
Az egyes mez‹k r‚szletes magyar zat hoz v lasszon a k”vetkez‹ list b¢l:
:ul compact.
:li.:link reftype=hd res=1000.µltal nos be ll¡t sok:elink.
:li.:link reftype=hd res=1001.DPMS be ll¡t sok:elink.
:li.:link reftype=hd res=2000.Jelszavas v‚delem:elink.
:li.:link reftype=hd res=3000.K‚perny‹v‚d‹ modulok:elink.
:eul.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for General settings groupbox
.*
:h2 res=1000.µltal nos be ll¡t sok
:p.Jel”lje ki a :hp2.K‚perny‹v‚d‹ enged‚lyezve:ehp2.-t amennyiben enged‚nyezni
szeretn‚ a k‚perny‹v‚d‹t. Am¡g a k‚perny‹v‚d‹ enged‚lyezve van, a rendszer
figyelemmel k¡s‚ri a felhaszn l¢i aktivit st (az eg‚r es a billentyûzet 
haszn lat t az Asztalon), ‚s automatikusan elind¡tja a k‚perny‹v‚d‚st egy adott
id‹nyi inaktivit s ut n.
:p.Ezt az id‹t lehet be ll¡tani a :hp2.K‚perny‹v‚d‹ ind¡t sa xxx percnyi
inaktivit s utan:ehp2. beviteli mez‹n‚l.
:note.
Az Asztal felbukkan¢ menj‚b‹l a :hp2.Z rol s most:ehp2. elemet kiv lasztva
a k‚perny‹v‚d‹ akkor is elindul, ha az itt nincs enged‚lyezve.
:p.Ha a :hp2.Csak a billentyûzetre ‚bredjen fel:ehp2. ki van jel”lve, akkor a
k‚perny‹v‚d‹ nem fogja le ll¡tani a k‚perny‹v‚d‚st ha az eg‚r aktivit s t ‚szleli,
csak akkor, ha a billentyûzet aktivit s t ‚szleli. Ez olyan esetekben lehet hasznos,
ha p‚ld ul a sz m¡t¢g‚p vibr l¢ k”rnyezetben van, vagy ha csak egyszerûen nem
aj nlatos hogy a sz m¡t¢g‚p abbahagyja a k‚perny‹v‚d‚st ha az eg‚r b rmilyen okb¢l
megmozdul.
:p.Amennyiben a :hp2.VIO k‚perny‹re v lt s tilt sa k‚perny‹v‚d‚s alatt:ehp2. ki van jel”lve, akkor a
k‚perny‹v‚d‹ nem fogja enged‚lyezni az £gynevezett VIO popupokat. Ez azt jelenti,
hogy semmilyen m s alkalmaz s nem tudja majd elvenni a k‚perny‹t ‚s a beviteli
eszk”z”ket a k‚perny‹v‚d‹ el‹l, m‚g a CAD-kezel‹ ‚s m s hasonl¢ alkalmaz sok sem.
Ez megakad lyozhatja a felhaszn l¢t abban, hogy egyes lefagyott alkalmaz sokat
kil‹hessen m¡g a k‚perny‹v‚d‹ akt¡v, de sokkal biztons gosabb  is teszi a rendszert.

:p.:link reftype=hd res=1001.DPMS be ll¡t sok:elink.

.*
.* Help for DPMS settings groupbox
.*
:h2 res=1001.DPMS be ll¡t sok
:p.Ezek a be ll¡t sok csak akkor el‚rhet‹k, ha a videok rtya meghajt¢programja 
is t mogatja a DPMS-t (jelenleg csak a Scitech SNAP ilyen), ‚s a monitor is 
t mogatja azt.
:p.A monitoroknak n‚gy energiatakar‚kos zemm¢da l‚tezik a DPMS standard szerint.
Ezek a k”vetkez‹k (a legkev‚sb‚ energiatakar‚kossal kezdve):
:ol.
:li.Az :hp2.on  llapot:ehp2.. Ez az az  llapot, amelyben a monitor be van
kapcsolva, es a megszokott m¢don mûk”dik.
:li.A :hp2.stand-by  llapot:ehp2.. Ilyenkor a monitor r‚szlegesen ki van kapcsolva,
de ebb‹l az  llapotb¢l m‚g gyorsan vissza tud t‚rni a bekapcsolt  llapot ba.
:li.A :hp2.suspend  llapot:ehp2.. Ebben az  llapot ban a monitor m r majdnem
teljesen ki van kapcsolva.
:li.Az :hp2.off  llapot:ehp2. A monitor ilyenkor ki van kapcsolva.
:eol.
:p.A k‚perny‹v‚d‹ mindig a legels‹  llapotb¢l indul, ‚s az id‹ el‹rehaladt val
egyre takar‚kosabb  llapotokba kapcsol.
:p.Csak azokat az  llapotokat fogja felhaszn lni, amelyek itt ki vannak jel”lve,
‚s az adott  llapotn l megadott id‹ letelte ut n kapcsol  t a k”vetkez‹  llapotba.

:p.:link reftype=hd res=1002.A DPMS-r‹l:elink.
:p.:link reftype=hd res=1000.µltal nos be ll¡t sok:elink.

.*
.* Info about DPMS itself
.*
:h3 res=1002.A DPMS-r‹l
:p.A DPMS a :hp2.Display Power Management Signaling:ehp2. r”vid¡t‚se, amely
egy VESA standard, mely n‚gy energiatakar‚kos monitor-zemm¢dot defini l a 
monitorok sz m ra: on, stand-by, suspend ‚s off.

:p.:link reftype=hd res=1001.DPMS be ll¡t sok:elink.
:p.:link reftype=hd res=1000.µltal nos be ll¡t sok:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.*
.* Help for Password protection groupbox
.*
:h2 res=2000.Jelszavas v‚delem
:p.Jel”lje ki a :hp2.Jelszavas v‚delem enged‚lyezve:ehp2.-t amennyiben jelszavas
v‚delmet szeretne a k‚perny‹v‚d‹h”z. Ha a jelszavas v‚delem be van kapcsolva,
akkor a k‚perny‹v‚d‹ egy jelsz¢t fog k‚rni miel‹tt le ll¡tani a k‚perny‹v‚d‚st,
es csak akkor  ll¡tja le azt, ha a megfelel‹ jelsz¢ kerlt beg‚pel‚sre.
:p.Ha a Security/2 telep¡tve van, akkor a k‚perny‹v‚d‹t utas¡tani lehet arra, hogy
az ‚ppen aktu lis Security/2 felhaszn l¢ jelszav t haszn lja. Ez azt jelenti, hogy
amikor a k‚perny‹v‚d‹ jelsz¢t k‚r a k‚perny‹v‚d‚s le ll¡t s hoz, akkor a megadott
jelsz¢t az aktu lis Security/2 felhaszn l¢ jelszav hoz fogja hasonl¡tani. Ezt a
lehet‹s‚get a :hp2.Haszn lja az aktu lis Security/2 felhaszn l¢ jelszav t:ehp2. 
kiv laszt s val ‚rheti el. Amennyiben a Security/2 nincs telep¡tve, ez a lehet‹s‚g
le van tiltva.
:p.A :hp2.Haszn lja a k”vetkez‹ jelsz¢t:ehp2. kiv laszt s val a k‚perny‹v‚d‹ egy 
saj t jelsz¢t fog haszn lni a jelszavas k‚perny‹v‚d‚shez. Ezt a jelsz¢t lehet be ll¡tani 
illetve megv ltoztatni, ha az £j jelsz¢t beg‚peli a k‚t beviteli mez‹be. Ugyanazon 
jelsz¢t kell megadni mindk‚t mez‹ben, ezzel lehet kiz rni az elg‚pel‚st. A jelsz¢ 
megv ltoztat s hoz a beg‚pel‚s ut n nyomja meg a :hp2.V ltoztat:ehp2. gombot!
:p.Ha a :hp2.K‚sleltetett jelszavas v‚delem:ehp2. ki van jel”lve, akkor a
k‚perny‹v‚d‹ nem fog jelsz¢t k‚rni a k‚perny‹v‚d‚s els‹ n‚h ny perc‚ben, ‚s 
£gy fog mûk”dni, mintha a jelszavas v‚delem nem lenne bekapcsolva. Ha viszont
a k‚perny‹v‚d‚s tov bb tart mint ah ny perc be van  ll¡tva az :hp2.A jelsz¢t
csak xxx perccel k‚s‹bb k‚rje:ehp2.-n‚l, akkor jelsz¢t fog k‚rni a 
k‚perny‹v‚d‚s le ll¡t sa el‹tt.
:p.:hp2.Az els‹ billentyûlet‚s m r a jelsz¢ r‚sze legyen:ehp2. hat rozza meg azt, hogy
hogyan viselkedjenek a k‚perny‹v‚d‹ modulok amikor a jelsz¢k‚r‹ ablakot meg kell jelen¡tenik.
Ha ez ki van jel”lve, akkor az a billentyûlet‚s amely a jelsz¢k‚r‹ ablak megjelen‚s‚t okozta
m r a jelsz¢ els‹ karakterek‚nt lesz ‚rtelmezve. Ha ez nincs kijel”lve, akkor a billentyûlet‚s
nem lesz jelsz¢k‚nt ‚rtelmezve, csak mint jelz‚s a jelsz¢k‚r‹ ablak megjelen¡t‚s‚re. K‚rem,
vegye figyelembe, hogy ez a be ll¡t s csak inform lis jellegû, megeshet, hogy n‚h ny k‚perny‹v‚d‹
modul a be ll¡t st figyelmen k¡vl hagyja!
:p.Amennyiben a :hp2.K‚perny‹v‚d‹ ind¡t sa rendszerind¡t skor:ehp2. ki van 
jel”lve, akkor a jelszavas k‚perny‹v‚d‹ automatikusan el fog indulni 
rendszerind¡t skor.
:note.
A k‚perny‹v‚d‹ nem fogja k‚sleltetni a jelszavas v‚delmet ha az a felhaszn l¢
k‚r‚s‚re indult el (az Asztal felbukkan¢ menj‚b‹l a :hp2.Z rol s most:ehp2. 
elemet kiv lasztva), vagy ha az a rendszer ind¡t sa miatt automatikusan indult
el!


.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Screen Saver modules groupbox
.*
:h2 res=3000.K‚perny‹v‚d‹ modulok
:p.Ezen az oldalon a rendelkez‚sre  ll¢ k‚perny‹v‚d‹ modulok list j t l thatja,
‚s inform ci¢kat az aktu lis modulr¢l.
:p.Az aktu lis modul az a modul, amelyik ‚ppen ki van v lasztva a rendelkez‚sre
 ll¢ modulok list j ban. Ez az a modul, amelyet a k‚perny‹v‚d‹ el fog ind¡tani
ha elj”n az ideje a k‚perny‹v‚d‚snek.
:p.Az aktu lis modul el‹n‚zeti k‚p‚t akkor l thatja, ha kijel”li az
:hp2.El‹n‚zeti k‚p:ehp2.-et.
:p.Az jobb oldalon tal lhat olyan inform ci¢kat az aktu lis modulr¢l, mint
p‚ld ul a modul neve, verzi¢sz ma, ‚s inform ci¢ arr¢l, hogy a modul t mogatja-e
a jelszavas v‚delmet.
:note.
Amennyiben az aktu lis modul nem t mogatja a jelszavas v‚delmet, akkor a
k‚perny‹v‚d‚s nem lesz jelsz¢val v‚dett, m‚g akkor sem, ha azt ™n a k‚perny‹v‚d‹
be ll¡t sainak els‹ oldal n k‚rte!
:p.N‚h ny k‚perny‹v‚d‹ modul konfigur lhat¢. Ha az aktu lis modul konfigur lhat¢,
akkor a :hp2.Konfigur ci¢:ehp2. nyom¢gomb enged‚lyezve van. Ezt a gombot 
megnyomva egy, az aktu lis modult¢l fgg‹ konfigur ci¢s ablakhoz juthat el.
:p.Az :hp2.Ind¡t s:ehp2. gombot akkor haszn lhatja, amikor arra k¡v ncsi, hogy
az aktu lis modul hogyan viselkedik a k‚perny‹v‚d‹ ”sszes jelenlegi be ll¡t sa
mellett (figyelembe v‚ve a k‚perny‹v‚d‹ be ll¡t sainak m s oldalain szerepl‹
be ll¡t sokat is, mint p‚ld ul a :hp2.K‚sleltetett jelszavas v‚delem:ehp2., ‚s
egyebek).

:p.:link reftype=hd res=3001.Modulok:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.Modulok
:p.A k‚perny‹v‚d‹ modul egy vagy t”bb speci lis DLL file, mely a k‚perny‹v‚d‹
"home" k”nyvt r nak :hp3.Modules:ehp3. alk”nyvt r ban van (a "home" k”nyvt r
itt azt a k”nyvt rat jelenti, ahov  a k‚perny‹v‚d‹ telep¡t‚sre kerlt). A modul
az, amely elindul amikor a k‚perny‹v‚d‚st meg kell kezdeni.

:p.:link reftype=hd res=3000.K‚perny‹v‚d‹ modulok:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h2 res=5000.A k‚perny‹v‚d‹ nyelv‚nek megv ltoztat sa
:p.Lehet‹s‚g van arra, hogy megv ltoztassa a k‚perny‹v‚d‹ nyelv‚t. A be ll¡tott
nyelvet haszn lj k a k‚perny‹v‚d‹ be ll¡t s ra szolg l¢ oldalak,
illetve azok a k‚perny‹v‚d‹ modulok is, amelyek ezt t mogatj k. A nyelv 
megv ltoztat s hoz annyit kell tennie, hogy egy :hp2.Locale:ehp2. objektumot
dob a Fogd ‚s Vidd m¢dszer haszn lat val a :hp2.Country palette:ehp2.-b‹l 
a k‚perny‹v‚d‹ be ll¡t sainak b rmelyik oldal ra.
:p.A k‚perny‹v‚d‹ alapesetben a rendszer nyelv‚t pr¢b lja meg haszn lni. Ezt
a :hp2.LANG:ehp2. k”rnyezeti v ltoz¢b¢l tudja meg. Amennyiben az a nyelv nem
t mogatott, az Angol nyelvet v lasztja a k‚perny‹v‚d‹.
:p.Mindazon ltal m s nyelveket is be lehet  ll¡tani, nem k”telez‹ a rendszer
nyelv‚t haszn lni a k‚perny‹v‚d‹h”z is. A System Setup-ban tal lhat¢
:hp2.Country palette:ehp2. b rmelyik :hp2.Locale objektuma:ehp2. r dobhat¢ a
k‚perny‹v‚d‹ be ll¡t sainak b rmelyik oldal ra a Fogd ‚s Vidd m¢dszerrel.
:p.Amikor a felhaszn l¢ egy :hp2.Locale objektumot:ehp2. dob a k‚perny‹v‚d‹
be ll¡t sainak b rmelyik oldal ra, a k‚perny‹v‚d‹ megviszg lja, hogy az adott
nyelvhez van-e nyelvi t mogat s telep¡tve. Amennyiben nincs, akkor a fentebb
le¡rt alapvet‹ m¢dszert alkalmazza, azaz a :hp2.LANG:ehp2. k”rnyezeti v ltoz¢
alapj n  ll¡tja be a nyelvet. Ha azonban az adott nyelv t mogatott, akkor arra
a nyelvre fog  tv ltani, ‚s ett‹l fogva azt a nyelvet fogja haszn lni.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.Visszavon
:p.V lassza a :hp2.Visszavon:ehp2.-t, ha a be ll¡t sokat arra akarja 
visszav ltoztatni, amelyek akkor voltak akt¡vak, miel‹tt ez az ablak megjelent!
.*
.* Help for the Default button
.*
:h3 res=6002.Alapbe ll¡t s
:p.V lassza az :hp2.Alapbe ll¡t s:ehp2.-t, ha a be ll¡t sokat arra akarja
visszav ltoztatni, amelyek akkor voltak akt¡vak, amikor a rendszert telep¡tette!
:euserdoc.
