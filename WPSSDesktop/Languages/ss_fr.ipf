.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.4                     *
.*                                                              *
.* Language: General French language                            *
.* Locales : fr_*                                               *
.*                                                              *
.* Author  : Guillaume Gay                                      *
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
:h1.conomiseur d'‚cran (page 1)
:p.La premiŠre page des paramŠtres de l':hp2.‚conomiseur 
d'‚cran:ehp2. est la page des :hp2.paramŠtres g‚n‚raux:ehp2.&per. C'est 
ici que les paramŠtres les plus communs de l'‚conomiseur d'‚cran peuvent 
ˆtre modifi‚s, comme le d‚lai pour l'‚conomie, et la protection par mot 
de passe. 
:p.La modification de la langue de l'‚conomiseur d'‚cran est possible 
en glissant et d‚posant un objet de r‚gion sur cette page. Pour plus 
d'informations, veuillez cliquer 
:link reftype=hd res=5000.ici:elink.&per. 
:p.
Pour une explication d‚taill‚e de chaque option, veuillez s‚lectionner 
l'un des ‚l‚ments de la liste ci-dessous &colon. 
:ul compact.
:li.:link reftype=hd res=1000.ParamŠtres g‚n‚raux:elink.
:li.:link reftype=hd res=1001.ParamŠtres DPMS:elink.
:li.:link reftype=hd res=2000.Protection par mot de passe:elink.
:li.:link reftype=hd res=3000.Modules d'‚conomie d'‚cran:elink.
:eul.
:p.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for General settings groupbox
.*
:h2 res=1000.ParamŠtres g‚n‚raux
:p.S‚lectionnez :hp2.Activer l'‚conomiseur d'‚cran:ehp2. pour mettre en
fonction l'‚conomiseur d'‚cran. Une fois activ‚, l'‚conomiseur 
surveillera l'activit‚ utilisateur (activit‚ du clavier et de la souris 
sur le bureau), et lancera automatiquement l'‚conomie d'‚cran aprŠs la 
p‚riode donn‚e d'inactivit‚. 
:p.Cette dur‚e peut ˆtre indiqu‚e, en minutes, dans le champ d'entr‚e 
:hp2.Lancement aprŠs xxx minute(s) d'inactivit‚:ehp2.&per. 
:note.la s‚lection de l'‚l‚ment de menu :hp2.Verrouillage imm‚diat:ehp2.
dans le menu contextuel du bureau lancera l'‚conomiseur d'‚cran, mˆme 
s'il n'est pas activ‚. 
:p.Si l'option :hp2.Sortie de veille au ~clavier uniquement:ehp2. est 
s‚lectionn‚e, l'‚conomiseur ne sortira pas de la veille s'il d‚tecte une
activit‚ de la souris, uniquement lorsqu'il d‚tecte une activit‚ clavier. 
Cela peut ˆtre utile dans les cas o— l'environnement de l'ordinateur 
provoque des vibrations, ou s'il est pr‚f‚rable que l'ordinateur ne sorte 
pas de la veille si la souris a ‚t‚ boug‚e pour n'importe qu'elle raison. 
:p.Une fois l'option :hp2.D‚sactiver les fenˆtres VIO lors de la
veille:ehp2. activ‚e, l'‚conomiseur d'‚cran d‚sactivera toute fenˆtre
VIO (en mode texte) en incrustation lorsque la veille est active. Cela
signifie qu'aucune autre application ne sera capable de d‚tourner l'‚cran
et les p‚riph‚riques d'entr‚e pendant la veille, pas mˆme CAD-handler ou
autre application similaire. Certes, cela empˆche … l'utilisateur
de mettre fin brutalement … des applications qui plantent lorsque l'‚cran
est en ‚conomie, mais cela rend cependant le systŠme plus s‚curis‚. 
:p.

:p.:link reftype=hd res=1001.ParamŠtres DPMS:elink.

.*
.* Help for DPMS settings groupbox
.*
:h2 res=1001.ParamŠtres DPMS
:p.Ces options de param‚trage ne sont disponibles que si … la fois le 
pilote video (Scitech SNAP uniquement pour le moment) et le moniteur 
prennent en charge le standard DPMS. 
:p.Il y a quatre ‚tats d'‚conomie d'‚nergie pour les moniteurs, selon le
standard DPMS. Ce sont les suivants, en comman‡ant par le moins 
‚conome &colon. 
:ol.
:li.L'‚tat :hp2.en fonction:ehp2.&per. C'est l'‚tat dans lequel le 
moniteur se trouve lorsqu'il est allum‚ et fonctionne normalement. 
:li.L'‚tat d':hp2.attente:ehp2.&per. Le moniteur n'est ici que 
partiellement mis hors fonction, et peut revenir trŠs rapidement … 
l'‚tat en fonction. 
:li.L'‚tat :hp2.suspendu:ehp2.&per. C'est l'‚tat dans lequel le 
moniteur est mis hors fonction … peu prŠs complŠtement. 
:li.L'‚tat :hp2.hors tension:ehp2.&per. Le moniteur est mis hors tension 
dans cet ‚tat. 
:eol.
:p.L'‚conomiseur d'‚cran d‚marre toujours depuis le premier ‚tat, et 
passe dans un ‚tat d'‚conomie plus avanc‚e au fur et … mesure que le 
temps passe. 
:p.L'‚conomiseur d'‚cran n'utilisera que les ‚tats qui sont s‚lectionn‚s 
ici et passera au prochain ‚tat aprŠs le d‚lai donn‚ pour cet ‚tat. 
:p.

:p.:link reftype=hd res=1002.Informations … propos du standard DPMS:elink.
:p.:link reftype=hd res=1000.ParamŠtres g‚n‚raux:elink.

.*
.* Info about DPMS itself
.*
:h3 res=1002.Informations … propos du standard DPMS
:p.DPMS est l'abbr‚viation de :hp2.Display Power Management 
Signaling:ehp2. (Signalisation pour la Gestion de l'Alimentation de 
l'Affichage), un standard d'interfa‡age VESA d‚finissant quatre modes de 
gestion d'alimentation pour les moniteurs : en fonction, en attente, 
suspendu et hors tension. 
:p.

:p.:link reftype=hd res=1001.ParamŠtres DPMS:elink.
:p.:link reftype=hd res=1000.ParamŠtres g‚n‚raux:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Password protection groupbox
.*
:h2 res=2000.Protection par mot de passe
:p.S‚lectionnez :hp2.Activer la protection par mot de passe:ehp2. pour 
permettre la protection par mot de passe de l'‚conomiseur d'‚cran. 
Si la protection par mot de passe est en fonction, l'‚conomiseur 
demandera un mot de passe avant de stopper l'‚conomie d'‚cran, et ne 
l'arrˆtera uniquement que si le bon mot de passe a ‚t‚ saisi. 
:p.Si Security/2 est install‚, l'‚conomiseur d'‚cran peut se charger 
de prendre en compte le mot de passe de l'utilisateur Security/2 en 
cours. Cela signifie que lorsque l'‚conomiseur d'‚cran demande un mot de 
passe pour sortir l'‚cran de l'‚conomie, il comparera ce mot de passe … 
celui paramŠtr‚ pour l'utilisateur en cours dans Security/2. Cela peut 
ˆtre d‚fini en s‚lectionnant l'option :hp2.Utilisation du mot de 
passe Security/2 en cours:ehp2.&per. Si Security/2 n'est pas install‚, 
cette option est d‚sactiv‚e. 
:p.En s‚lectionnant l'option :hp2.Utilisation de ce mot de 
passe:ehp2.&comma. l'‚conomiseur d'‚cran utilisera un mot de passe priv‚ 
pour la protection. Ce mot de passe peut ˆtre paramŠtr‚ ou modifi‚ en 
l'entrant dans chacun des deux champs d'entr‚e. Le mˆme mot de passe 
devra ˆtre tap‚ dans les deux champs, de maniŠre … s'assurer qu'aucune
faute de frappe n'a ‚t‚ faite. Pour modifier le mot de passe pour celui 
qui a ‚t‚ tap‚, appuyez sur le bouton :hp2.Modifier:ehp2.&per. 
:p.Si l'option :hp2.Protection par mot de passe diff‚r‚e:ehp2. est 
s‚lectionn‚e, l'‚conomiseur d'‚cran ne demandera pas de mot de passe si
on essaie de sortir l'‚cran de l'‚conomie lors des premiŠre minutes, et 
se comportera comme si la protection par mot de passe n'avait pas ‚t‚ 
activ‚e. Cependant, si l'‚conomie d'‚cran dure plus longtemps que le 
d‚lai indiqu‚ par l'option :hp2.Demander le mot de passe seulement aprŠs 
xxx minute(s) de passage en ‚conomie:ehp2.&comma. le mot de passe sera 
alors demand‚ pour en sortir. 
:p.L'option :hp2.La premiŠre touche press‚e correspond au 1er caractŠre 
du mot de passe:ehp2. d‚termine comment le module d'‚conomie se comporte 
lorsque la fenˆtre de protection par mot de passe est sur le point d'ˆtre 
affich‚e. Si l'option est s‚lectionn‚e, la touche appuy‚e qui a provoqu‚ 
l'apparition d'une fenˆtre de protection par mot de passe sera d‚j… 
utilis‚e comme premier caractŠre du mot de passe. Si l'option n'est pas 
activ‚e, l'appui sur la touche ne sera alors pas trait‚ comme premier 
caractŠre de mot de passe, mais juste comme notification pour pour 
l'affichage la fenˆtre de protection. Veuillez noter qu'il s'agit d'un 
paramŠtre purement informationnel, certains modules d'‚conomie peuvent 
ne pas l'honorer. 
:p.S‚lectionnez :hp2.Passage en ‚conomie au lancement du systŠme:ehp2. 
pour d‚marrer automatiquement l'‚conomie d'‚cran prot‚g‚e par mot de 
passe au lancement du systŠme. 
:note.l'‚conomiseur d'‚cran ne diffŠrera pas la protection par mot de 
passe si il est lanc‚ … la demande de l'utilisateur (en s‚lectionnant 
l'‚l‚ment de menu :hp2.Verrouillage imm‚diat:ehp2. dans le menu 
contextuel du bureau) ou s'il est lanc‚ lors de la s‚quence d‚marrage du 
systŠme. 
:p.


.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Screen Saver modules groupbox
.*
:h2 res=3000.Modules d'‚conomie d'‚cran
:p.Cette page affiche la liste des modules d'‚conomie d'‚cran disponibles 
et donne des informations sur le module en cours de s‚lection. 
:p.Le module en cours de s‚lection est celui en surbrillance dans la 
liste des modules disponibles. C'est le module qui sera lanc‚ par 
l'‚conomiseur d'‚cran le moment venu. 
:p.Un aper‡u du module en cours peut ˆtre affich‚ en cliquant sur 
:hp2.Aper‡u:ehp2.&per. 
:p.Sur la partie droite de la page sont affich‚es les informations au 
sujet du module en cours, tels que ses nom et num‚ro de version, et 
affiche si le module prend en charge la protection par mot de passe. 
:note.si le module en cours ne prend pas en charge la protection par mot 
de passe, l'‚conomie d'‚cran ne sera alors pas prot‚g‚e par mot de passe, 
mˆme si l'option est paramŠtr‚e dans la premiŠre page des paramŠtres de 
l'‚conomiseur d'‚cran ! 
:p.Certains des modules peuvent ˆtre configur‚s. Si tel est le cas, le 
bouton :hp2.Configuration:ehp2. est alors activ‚. L'appui sur ce bouton 
fera apparaŒtre une boŒte de dialogue de configuration sp‚cifique au 
module. 
:p.Le bouton :hp2.Lancement imm‚diat:ehp2. peut ˆtre utilis‚ pour voir 
comment le module se comporterait avec tous les paramŠtres de 
l'‚conomiseur d'‚cran en cours (y compris les paramŠtres d'autres pages 
de paramŠtres comme la :hp2.protection par mot de passe diff‚r‚e:ehp2. et 
autres). 
:p.

:p.:link reftype=hd res=3001.Modules:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.Modules
:p.Un module est un (ou plus) fichier .DLL particulier, situ‚ dans le 
dossier :hp3.Modules:ehp3. du r‚pertoire d'installation de base de 
l'‚conomiseur d'‚cran. Il est lanc‚ lorsque l'‚conomie d'‚cran doit ˆtre 
d‚marr‚e. 
:p.

:p.:link reftype=hd res=3000.Modules d'‚conomie d'‚cran:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h2 res=5000.Param‚trage de la langue de l'‚conomiseur
:p.Il est possible de modifier la langue des pages de paramŠtres de 
l'‚conomiseur d'‚cran, ainsi que celle de certains des modules (prenant 
en charge plusieurs langues), en glissant et d‚posant un :hp2.objet de 
r‚gion:ehp2. depuis la :hp2.palette de r‚gions:ehp2.&per. 
:p.L'‚conomiseur d'‚cran essaie par d‚faut d'utiliser la langue du systŠme. 
Il l'obtient en v‚rifiant la variable d'environnment :hp2.LANG:ehp2.&per. 
S'il n'y a pas de fichier de support de langue install‚ pour la langue 
s‚lectionn‚e, l'‚conomiseur d'‚cran se rabat sur l'anglais. 
:p.Cependant, d'autres langues peuvent ˆtre utilis‚es, il n'est pas 
obligatoire pour l'‚conomiseur d'‚cran d'utiliser la langue du systŠme. 
N'importe quel :hp2.objet de r‚gion:ehp2. de la :hp2.palette de 
r‚gions:ehp2. (dans le dossier de configuration du systŠme) peut ˆtre 
gliss‚ et d‚pos‚ sur l'une des pages de paramŠtres de l'‚conomiseur 
d'‚cran. 
:p.Lorsqu'un :hp2.objet de r‚gion:ehp2. est d‚pos‚ sur l'une des pages de 
paramŠtres, l'‚conomiseur d'‚cran v‚rifiera si le support pour la langue 
s‚lectionn‚e est install‚ ou pas. S'il ne l'est pas, il se rabattra sur 
la m‚thode par d‚faut en utilisant la variable d'environnement 
:hp2.LANG:ehp2.&per. Autrement, si la langue est support‚e, il 
l'utilisera aprŠs l'op‚ration de Glisser/D‚poser. 
:p.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.D‚faire
:p.S‚lectionnez :hp2.D‚faire:ehp2. pour retourner aux valeurs des 
paramŠtres qui ‚taient d‚finies avant d'ouvrir la fenˆtre. 
:p.
.*
.* Help for the Default button
.*
:h3 res=6002.Par d‚faut
:p.S‚lectionnez :hp2.Par d‚faut:ehp2. pour retourner aux valeurs des 
paramŠtres qui ‚taient d‚finies … l'installation du systŠme. 
:p.
:euserdoc.
