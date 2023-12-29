.*==============================================================*\
.*          				                        *
.* Doodlen n„yt”ns„„st„j„n v1.4 ohjetiedosto			*
.*                                                              *
.* Kieli: Suomi                           			*
.* Lokaali : fi_* , k„ytetty koodisivu 850                      *
.*                                                              *
.* Tekij„  : Doodle                          			* 
.* Suomentaja : -tapsa-                                         *
.*                                                              *
.* P„iv„ys (YYYY-MM-DD): 2005.03.18.                            *
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
:h1.N„yt”ns„„st„j„
:p.Ensimm„inen sivu :hp2.n„yt”ns„„st„j„n:ehp2. asetuksissa on
:hp2.N„yt”ns„„st„j„n yleisasetukset:ehp2. -sivu. T„„ll„ voidaan 
muuttaa kaikkein yleisimpi„ n„yt”ns„„st„j„n asetuksia, kuten 
n„yt”s„„st„j„n alkamisaikaa ja salasanasuojauksen asetuksia.
:p.
N„yt”ns„„st„j„n kielen vaihtaminen on mahdollista vet„m„ll„ ja pudottamalla Lokaaliobjekti t„lle sivulle. Lis„tietoa kielen vaihtamisesta saat
:link reftype=hd res=5000.
t„„lt„:elink..
:p.
Yksityiskohtaisen selityksen kustakin kent„st„ saat valitsemalla alla olevasta listasta:
:ul compact.
:li.:link reftype=hd res=1000.Yleisasetukset:elink.
:li.:link reftype=hd res=1001.DPMS settings:elink.
:li.:link reftype=hd res=2000.Salasanasuojaus:elink.
:li.:link reftype=hd res=3000.N„yt”ns„„st„j„n moduulit:elink.
:eul.

.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/

.*
.* Help for General settings groupbox
.*
:h2 res=1000.Yleisasetukset
:p.Valitse :hp2.N„yt”ns„„st„j„ k„yt”ss„:ehp2. ottaaksesi n„yt”ns„„st„j„n k„ytt””n. 
Kun n„yt”ns„„st„j„ on k„yt”ss„, systeemi tarkkailee k„ytt„j„n koneen k„ytt”„ (hiiren liikett„ ja n„pp„imist”n k„ytt”„ ty”p”yd„ll„), ja k„ynnist„„ n„yt”ns„„st„j„n automaattisesti annetun koneen k„ytt„m„tt”myystilan j„lkeen.
:p.T„m„ tila voidaan asettaa minuutteina kentt„„n :hp2.K„ynnist„ n„yt”ns„„st„j„ xxx minuutin koneen k„ytt„m„tt”myyden j„lkeen:ehp2..
:note.
Valitsemalla :hp2.Lukitse nyt:ehp2. -vaihtoehto ty”p”yd„n ponnahdusvalikosta k„ynnist„„ n„yt”ns„„st„j„n vaikka sit„ ei olisi aktivoitu k„ytt””n.

:p.:link reftype=hd res=1001.DPMS settings:elink.

.*
.* Help for DPMS settings groupbox
.*
:h2 res=1001.DPMS settings
:p.N„m„ asetukset ovat k„ytett„viss„ vain jos sek„ n„yt”nohjainajuri tukee DPMS&colon.„„ (toistaiseksi tuki on vain Scitechin SNAP-ajureissa), ett„ n„ytt” on DPMS-yhteensopiva.
:p.Monitoreille on nelj„ virrans„„st”tilaa DPMS-standardin mukaisesti.
N„m„ ovat seuraavat, aloittaen v„himmin virtaa s„„st„v„st„ tilasta:
:ol.
:li.:hp2.P„„ll„:ehp2. (on state). T„ss„ tilassa n„ytt” on p„„ll„ ja toimii normaalisti.
:li.:hp2.Stand-by-tila:ehp2. (stand-by state). N„ytt” on osittain pois p„„lt„ mutta voidaan palauttaa t„st„ tilasta nopeasti takaisin p„„lle.
:li.:hp2.Suspend-tila:ehp2. (suspend state). T„ss„ tilassa n„ytt” on melkein kokonaan pois p„„lt„.
:li.:hp2.Pois p„„lt„:ehp2. (off state). N„ytt” on t„ss„ tilassa pois p„„lt„.
:eol.
:p.N„yt”ns„„st„j„ aloittaa aina ensimm„isest„ tilasta ja siirtyy ajan kuluessa yh„ enemm„n virtaa s„„st„viin tiloihin.
:p.N„yt”ns„„st„j„ k„ytt„„ vain niit„ tiloja, jotka on valittu t„„ll„, ja siirtyy seuraavaan tilaan sille annetun ajan kuluttua.

:p.:link reftype=hd res=1002.Tietoa DPMS&colon.st„:elink.
:p.:link reftype=hd res=1000.Yleisasetukset:elink.


.*
.* Info about DPMS itself
.*
:h3 res=1002.Tietoa DPMS&colon.st„
:p.DPMS on lyhenne sanoista :hp2.Display Power Management Signaling:ehp2.. Se on  VESA-liittym„n standardi, joka m„„rittelee nelj„ virranhallintatilaa n„yt”n lepotilalle: p„„ll„, stand-by, suspend ja pois p„„lt„.

:p.:link reftype=hd res=1001.DPMS settings:elink.
:p.:link reftype=hd res=1000.Yleisasetukset:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Password protection groupbox
.*
:h2 res=2000.Salasanasuojaus
:p.Valitse :hp2.K„yt„ salasanasuojausta:ehp2. ottaaksesi n„yt”ns„„st„j„n salasanasuojauksen k„ytt””n. Jos salasanasuojaus on valittu k„ytt””n, n„yt”ns„„st„j„ pyyt„„ salasanaa ennen n„yt”ns„„st”tilan lopettamista, ja lopettaa sen vain, jos oikea salasana on annettu.
:p.Jos Security/2 on asennettu, n„yt”ns„„st„j„ voidaan asettaa k„ytt„m„„n aktiivisen Security/2-k„ytt„j„n salasanaa. 
Se tarkoittaa, ett„ kun n„yt”ns„„st„j„ pyyt„„ salasanaa n„yt”ns„„st”tilan lopettamiseksi, annettua salasanaa verrataan aktiivisen Security/2-k„ytt„j„n salasanaan. T„m„ voidaan asettaa valitsemalla :hp2.K„yt„ nykyisen Security/2-k„ytt„j„n salasanaa:ehp2. -vaihtoehto. Jos Security/2&colon.sta ei ole asennettu, t„m„ ei ole valittavissa.
:p.Valitsemalla :hp2.K„yt„ t„t„ salasanaa:ehp2. -vihtoehto, n„yt”ns„„st„j„ k„ytt„„ yksityist„ salasanaa n„yt”ns„„st„j„ss„.
T„m„ salasana voidaan asettaa tai vaihtaa antamalla uusi salasana kahteen sy”tt”kentt„„n. Salasana pit„„ sy”tt„„ kahdesti kirjoitusvirheiden est„miseksi. Vaihtaaksesi salasanan sy”tt„m„„si, paina 
:hp2.Vaihda:ehp2. -painiketta.
:p. Jos :hp2.Viive salasanasuojauksessa:ehp2.  on valittu, n„yt”ns„„st„j„ ei kysy salasanaa muutaman n„yt”ns„„st”tilan ensimm„isen minuutin aikana, ja k„ytt„ytyy kuin salasanasuojausta ei olisikaan otettu k„ytt””n. Kuitenkin, jos n„yt”ns„„st”tila kest„„ pitemp„„n kuin :hp2.Kysy salasana vain xxx minuutin n„yt”ns„„st”tilan j„lkeen:ehp2. asetettu aika, salasanaa kysyt„„n ennen n„yt”ns„„st”tilan lopettamista.
:p.Valitse :hp2.K„ynnist„ n„yt”ns„„st„j„ systeemin k„ynnistyksen yhteydess„:ehp2. k„ynnist„„ksesi salasanasuojatun n„yt”ns„„st„j„n automaattisesti, kun systeemi k„ynnistet„„n..
:note.
N„yt”ns„„st„j„n salasanasuojauksessa ei ole viivett„, jos se k„ynnistet„„n k„ytt„j„n pyynn”st„ (valitsemalla :hp2. Lukitse nyt:ehp2. ty”p”yd„n ponnahdusvalikosta), tai josse k„ynnistet„„n systeemin k„ynnistyess„.


.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Screen Saver modules groupbox
.*
:h2 res=3000.N„yt”ns„„st„j„n moduulit
:p.T„m„ sivu n„ytt„„ luettelon k„ytett„viss„ olevista n„yt”ns„„st„j„n moduuleista ja n„ytt„„ tietoa kustakin valitusta moduulista.
:p.Valittu moduuli on moduuli, joka on valittu k„ytett„viss„ olevien moduulien luettelosta. T„m„ moduuli k„ynnistet„„n n„yt”ns„„st”tilassa.
:p.Valittua moduulia voidaan esikatsella valitsemalla :hp2.Esikatselu:ehp2..
:p.Sivulla oikealla n„ytet„„n tieto valitusta moduulista, kuten moduulin nimi ja versionumero sek„ n„ytet„„n, jos moduuli tukee salasanasuojausta.
:note.
Jos valittu moduuli ei tue salasanasuojausta, n„yt”ns„„st„j„ ei ole salasanasuojattu vaikka suojaus olisi valittu k„ytt””n ensimm„isell„ sivulla!
:p.Joitakin moduuleita voidaan muokata. Jos valittua moduulia voidaan muokata,
the :hp2.Muokkaa:ehp2. painike on k„ytett„viss„. Painikeen painaminen avaa yksityiskohtaisemman asetusikkunan.
:p.:hp2.Aloita:ehp2. -painiketta voidaan k„ytt„„ n„ytt„m„„n kuinka valittu moduuli k„ytt„ytyy valituilla n„yt”ns„„st„j„n asetuksilla (mukaan lukien my”s muiden n„yt”ns„„st„j„n sivujen asetukset kuten :hp2.Viive salasanasuojauksessa:ehp2. ym.).

:p.:link reftype=hd res=3001.Moduulit:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.Moduulit
:p.N„yt”ns„„st„j„n moduuli on yksi tai useampi erityisi„ DLL-tiedostoja, jotka sijaitsevat n„yt”nohjaimen hakemiston
:hp3.Modules:ehp3. -alihakemistossa (\TOOLS\DSSaver\Modules). Se k„ynnistyy n„yt”ns„„st”tilaan siirrytt„ess„.

:p.:link reftype=hd res=3000.N„yt”ns„„st„j„n moduulit:elink.


.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h2 res=5000.N„yt”nohjaimen kielen valinta
:p.N„yt”ns„„st„j„n asetukset sivujen kieli ja joidenkin n„yt”ns„„st„j„n moduulien (jotka tukevat NLS&colon.„„) kieli on mahdollista vaihtaa vet„m„ll„ ja pudottamalla :hp2.Lokaaliobjekti:ehp2. :hp2.Maa-paletista:ehp2. (System Setup -> Locale -> Country Palette).
:p.N„yt”ns„„st„j„ yritt„„ k„ytt„„ systeemin kielt„ oletuksena, jonka se saa tarkastelemalla :hp2.LANG:ehp2.-ymp„rist”muuttujaa. Jos j„rjestelm„n kielelle ei ole kielitukea, n„yt”ns„„st„j„ k„ytt„„ englantia.
:p.Kuitenkin my”s muita kieli„ voidaan k„ytt„„ eik„ n„yt”ns„„st„j„n tarvitse v„ltt„m„tt„ k„ytt„„ j„rjestelm„n oletuskielt„. Mik„ tahansa :hp2.Lokaaliobjekteista:ehp2. :hp2.Maa-paletista:ehp2. (System Setup -> Locale -> Country Palette -> valittu lokaaliobjekti) voidaan vet„„ ja pudottaa n„yt”ns„„st„j„n sivuille.
:p.Kun :hp2.Lokaaliobjekti:ehp2. on pudotettu mille tahansa n„yt”ns„„st„j„n asetussivulle, n„yt”ns„„st„j„ tarkistaa, jos kielelle on asennettu kielituki. Jos sit„ ei ole asennettu, k„ytet„„n oletusmenetelm„„  :hp2.LANG:ehp2.-ymp„rist”muuttujan mukaisesti. Kuitenkin jos kieli on tuettu, vaihdetaan siihen kieleen, ja k„ytet„„n siit„ siit„ eteenp„in.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.Peru
:p.Valitse :hp2.Peru:ehp2. vaihtaaksesi takaisin asetuksiin, jotka olivat aktiivisina ennen t„m„n ikkunan n„ytt„mist„.
.*
.* Help for the Default button
.*
:h3 res=6002.Oletus
:p.Valitse :hp2.Oletus:ehp2. vaihtaaksesi takaisin j„rjestelm„n oletusasetuksiin.
:euserdoc.

