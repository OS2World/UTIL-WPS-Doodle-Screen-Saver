.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v2.x                     *
.*                                                              *
.* Language: Spanish                                            *
.* Locales : es_*                                               *
.*                                                              *
.* Author  : Alfredo Fern†ndez D°az                             *
.*                                                              *
.* Date (YYYY-MM-DD): 2019.07.07.                               *
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
.* Just a dummy H1 panel to keep TOCs tidy -- important when
.* invoking this as part of the help subsystem
.*--------------------------------------------------------------*/
:h1.Protector de pantalla

Cuando se instala correctamente el Protector de pantalla de Doodle, sus
opciones deben establecerse a travÇs del cuaderno de propiedades del
Escritorio, donde quedan integradas. En dicho cuaderno encontrar† una nueva
pesta§a llamada :hp2.Protector de pantalla:ehp2., que consta a su vez de tres
p†ginas.

:p.Seleccione el t°tulo de cualquiera de ellas de la siguiente lista para una
explicaci¢n de las opciones que contiene:

:ul compact.
:li.:link reftype=hd res=1000.Par†metros generales:elink.
:li.:link reftype=hd res=2000.Par†metros DPMS:elink.
:li.:link reftype=hd res=3000.M¢dulos del protector de pantalla:elink.
:eul.

.*
.*--------------------------------------------------------------*\
.* Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h2 res=1000.Par†metros generales

La primera p†gina de opciones del :hp2.Protector de pantalla:ehp2. es
:hp2.Par†metros generales:ehp2.. Aqu° es donde se puede cambiar sus opciones
m†s comunes, como el tiempo de espera de activaci¢n y las opciones de
protecci¢n mediante contrase§a.

:p.Es posible cambiar el idioma del protector de pantalla arrastrando un objeto
ÆEscenarioØ (perfil nacional) hasta aqu°. Para m†s detalles al respecto,
consulte :link reftype=hd res=5000.Configurar el idioma del protector de
pantalla:elink..

:p.Seleccione los campos de la siguiente lista para una explicaci¢n detallada:

:ul compact.
:li.:link reftype=hd res=1001.Par†metros generales:elink.
:li.:link reftype=hd res=1002.Protecci¢n por contrase§a:elink.
:li.:link reftype=hd res=6001.Deshacer:elink.
:li.:link reftype=hd res=6002.Por omisi¢n:elink.
:eul.

.*
.* Help for General settings groupbox
.*
:h3 res=1001.Par†metros generales

Marque :hp2.Habilitar protecci¢n de pantalla:ehp2. para activar el protector
de pantalla. Mientras estÇ habilitado, el sistema vigilar† la actividad del
usuario (eventos de rat¢n y teclado en el Escritorio), e iniciar†
autom†ticamente el protector de pantalla tras un periodo de inactividad dado.

:p.Este periodo puede establecerse en minutos en el campo de entrada
:hp2.Iniciar protecci¢n de pantalla tras <n> minuto(s) de inactividad
del sistema:ehp2..

:note.
Seleccionar la entrada :hp2.Bloquear ahora:ehp2. del men£ emergente del
Escritorio iniciar† el protector de pantalla incluso aunque no estÇ habilitado.

:p.Si se marca :hp2.Desbloquear s¢lo mediante teclado:ehp2., el protector de
pantalla no cesar† su actividad cuando detecte actividad del rat¢n, sino s¢lo
cuando la haya en el teclado. Esto resulta £til en entornos donde el equipo
estÇ sometido a vibraciones, o si es preferible que no se interrumpa la
protecci¢n de pantalla si se toca el rat¢n por cualquier motivo.

:p.Si se marca :hp2.Inhibir sesiones VIO emergentes mientras estÇ activo el
protector:ehp2., ninguna otra aplicaci¢n podr† apropiarse de la pantalla y los
dispositivos de entrada, ni siquiera el Men£ C-A-S u otras aplicaciones
similares. Esto puede impedir al usuario cerrar aplicaciones bloqueadas
mientras el protector de pantalla est† activo, pero hace m†s seguro el sistema.

.*
.* Help for Password protection groupbox
.*
:h3 res=1002.Protecci¢n por contrase§a

Si marca :hp2.Habilitar protecci¢n con contrase§a:ehp2., el protector de
pantalla solicitar† una clave antes de detenerse y s¢lo lo har† si se introduce
dicha clave correctamente.

:p.Si Security/2 se encuentra instalado, se puede indicar al protector de
pantalla que utilice la contrase§a del usuario actual de Security/2. Esto
significa que cuando el protector de pantalla solicite una contrase§a para
cesar su actividad, la comparar† con la establecida para el usuario actual de
Security/2. Para ello, marque la opci¢n :hp2.Contrase§a del usuario
actual de Security/2:ehp2.. Esta opci¢n se inhabilita si Security/2 no est†
instalado.

:p.Al seleccionar la opci¢n :hp2.Especificar contrase§a:ehp2., el protector de
pantalla solicitar† una contrase§a privada cuando el usuario quiera detenerlo.
Esta contrase§a se puede establecer o cambiar introduciÇndola en los dos campos
de entrada, para evitar erratas. Para establecer como contrase§a actual la
introducida, pulse el bot¢n :hp2.Cambiar:ehp2..

:p.Si se marca :hp2.Retrasar solicitud de contrase§a:ehp2., el protector de
pantalla no pedir† la contrase§a durante los primeros minutos de su actividad,
comport†ndose como si la protecci¢n por contrase§a no se hubiera habilitado.
Sin embargo, si la protecci¢n est† activa m†s tiempo del especificado en
:hp2.Pedir la contrase§a s¢lo tras <n> minuto(s) de actividad:ehp2., se
solicitar† la contrase§a antes de detener el protector de pantalla.

:p.La casilla :hp2.Usar como primer car†cter de la contrase§a la primera tecla
pulsada:ehp2. determina c¢mo se comportan los m¢dulos del protector
de pantalla cuando se va a mostrar el di†logo de protecci¢n por contrase§a. Si
se marca, la pulsaci¢n de tecla que hace aparecer dicho di†logo se utilizar†
como primer car†cter de la contrase§a; si no, esta pulsaci¢n s¢lo se
considerar† un aviso al protector para mostrar el di†logo de solicitud de la
contrase§a. Por favor, tenga en cuenta que esta opci¢n es s¢lo informativa y
algunos m¢dulos del protector de pantalla pueden no tomarla en consideraci¢n.

:p.Marque :hp2.Activar autom†ticamente al inicio del sistema:ehp2. para que la
protecci¢n de pantalla con contrase§a se inicie autom†ticamente al arrancar el
sistema.

:note.
El protector de pantalla no retrasar† la solicitud de contrase§a si inicia su
actividad a petici¢n del usuario (p.e. seleccionando la entrada :hp2.Bloquear
ahora:ehp2. del men£ emergente del Escritorio) o durante la secuencia de inicio
del sistema.

.*
.*--------------------------------------------------------------*\
.* Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h2 res=2000.Par†metros DPMS

La segunda p†gina de opciones del :hp2.Protector de pantalla:ehp2. es
:hp2.Par†metros DPMS:ehp2.. Aqu° puede indicarse al protector de pantalla si
utilizar o no diversos :link reftype=hd res=2002.servicios DPMS:elink., si
est†n disponibles.

:p.Es posible cambiar el idioma del protector de pantalla arrastrando un objeto
ÆEscenarioØ (perfil nacional) hasta aqu°. Para m†s detalles al respecto,
consulte :link reftype=hd res=5000.Configurar el idioma del protector de
pantalla:elink..

:p.Seleccione los campos de la siguiente lista para una explicaci¢n detallada:

:ul compact.
:li.:link reftype=hd res=2001.Par†metros DPMS para el monitor:elink.
:li.:link reftype=hd res=6001.Deshacer:elink.
:li.:link reftype=hd res=6002.Por omisi¢n:elink.
:eul.

.*
.* Help for DPMS settings groupbox
.*
:h3 res=2001.Par†metros DPMS para el monitor

Estas opciones s¢lo est†n disponibles si el monitor tiene capacidades DPMS
y el controlador de v°deo lo soporta (en este momento s¢lo Scitech SNAP lo
hace).

:p.De acuerdo con el est†ndar DPMS, para los monitores se definen cuatro
estados de ahorro de energ°a. Son los siguientes, en orden creciente de ahorro
energÇtico:

:ol.
:li.:hp2.Encendido:ehp2.. Es el estado de funcionamiento normal del
monitor.
:li.:hp2.En espera:ehp2.. El monitor est† parcialmente apagado, pero puede
recuperarse muy r†pidamente desde este estado.
:li.:hp2.Suspensi¢n:ehp2.. En este estado el monitor est† casi completamente
apagado.
:li.:hp2.Apagado:ehp2.. El monitor est† apagado.
:eol.

:p.El protector de pantalla siempre parte del primer estado y va pasando a
estados de mayor ahorro seg£n transcurre el tiempo.

:p.El protector de pantalla s¢lo utilizar† los estados seleccionados aqu°, y
pasar† al siguiente estado tras el periodo de tiempo dado para cada uno.

.*
.* Info about DPMS itself
.*
:h3 res=2002.Informaci¢n sobre DPMS

El acr¢nimo DPMS corresponde a :hp2.Display Power Management Signaling:ehp2.
(se§alizaci¢n de gesti¢n de energ°a para pantallas), una interfaz del est†ndar
VESA que define cuatro modos de gesti¢n de la energ°a para monitores inactivos:
encendido, en espera, suspensi¢n y apagado.

.*
.*--------------------------------------------------------------*\
.* Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h2 res=3000.M¢dulos del protector de pantalla

La tercera p†gina de opciones del :hp2.Protector de pantalla:ehp2. es
:hp2.M¢dulos del protector de pantalla:ehp2.. Aqu° puede verse la
lista de :link reftype=hd res=3002.m¢dulos del protector de pantalla:elink.,
configurar dichos m¢dulos si lo necesitan y seleccionar cu†l de ellos ser† el
que active actualmente el protector de pantalla.

:p.Es posible cambiar el idioma del protector de pantalla arrastrando un objeto
ÆEscenarioØ (perfil nacional) hasta aqu°. Para m†s detalles al respecto,
consulte :link reftype=hd res=5000.Configurar el idioma del protector de
pantalla:elink..

:p.Seleccione los campos de la siguiente lista para una explicaci¢n detallada:

:ul compact.
:li.:link reftype=hd res=3001.M¢dulos del protector de pantalla:elink.
:li.:link reftype=hd res=6001.Deshacer:elink.
:li.:link reftype=hd res=6002.Por omisi¢n:elink.
:eul.

.*
.* Help for Screen Saver modules groupbox
.*
:h3 res=3001.M¢dulos del protector de pantalla

Esta p†gina muestra la lista de m¢dulos del protector de pantalla disponibles,
as° como informaci¢n sobre el seleccionado actualmente.

:p.El m¢dulo seleccionado actualmente es el resaltado en la lista de m¢dulos
disponibles y ser† el que se inicie al activarse la protecci¢n de pantalla.

:p.Se puede tener una vista previa del m¢dulo actual marcando la casilla
:hp2.Mostrar vista previa:ehp2..

:p.La parte derecha de la p†gina muestra informaci¢n sobre el m¢dulo, como su
nombre, n£mero de versi¢n y si admite la protecci¢n por contrase§a.

:note.
Si el m¢dulo actual no soporta protecci¢n por contrase§a, entonces la
protecci¢n de pantalla no contar† con la protecci¢n por contrase§a incluso si
se ha habilitado esta opci¢n en la primera p†gina de propiedades del protector
de pantalla.

:p.Algunos de los m¢dulos se pueden configurar. Cuando es el caso del m¢dulo
actual, se habilita el bot¢n :hp2.Configurar:ehp2.. Al pulsar este bot¢n se
abre el di†logo de configuraci¢n espec°fico del m¢dulo.

:p.El bot¢n :hp2.Iniciar ahora:ehp2. se puede usar para ver c¢mo se comportar°a
el m¢dulo actual con todas las opciones actuales del protector de pantalla
(incluidas las opciones de las dem†s p†ginas de propiedades del protector de
pantalla, como :hp2.Retrasar solicitud de contrase§a:ehp2. y otras).

.*
.* Help about Screen Saver modules
.*
:h3 res=3002.M¢dulos

Un m¢dulo del protector de pantalla consiste en uno o m†s archivos DLL
especiales situados en el directorio :hp3.Modules:ehp3. dentro del directorio
de instalaci¢n del protector de pantalla. Se inicia cuando deba iniciarse la
protecci¢n de pantalla.

.*--------------------------------------------------------------*\
.* Just a dummy H2 panel to keep TOCs tidier by lumping together
.* all common options...
.*--------------------------------------------------------------*/

:h2.Opciones comunes

êstas son opciones comunes a todas las p†ginas de propiedades del protector de
pantalla. Seleccione cualquiera de ellas de la siguiente lista para una
explicaci¢n detallada:

:ul compact.
:li.:link reftype=hd res=5000.Configuraci¢n de idioma:elink.
:li.:link reftype=hd res=6001.Deshacer:elink.
:li.:link reftype=hd res=6002.Por omisi¢n:elink.
:eul.

.*
.*--------------------------------------------------------------*\
.* Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h3 res=5000.Configurar el idioma del protector de pantalla

Es posible cambiar el idioma de las p†ginas de opciones del protector de
pantalla y de algunos de sus m¢dulos (los que tienen soporte de idiomas)
arrastrando hasta cualquiera de ellas un objeto :hp2.Escenario:ehp2. (perfil
nacional) desde la :hp2.Paleta de pa°s:ehp2. (o ÆPerfiles nacionalesØ).

:p.Por omisi¢n, el protector de pantalla intenta usar el idioma del sistema,
leyÇndolo de la variable de entorno :hp2.LANG:ehp2.. Si no hay instalados
archivos de soporte de idioma para Çl, el protector de pantalla recurre al
inglÇs.

:p.Sin embargo, no es forzoso utilizar para el protector de pantalla el mismo
idioma del sistema. Se puede arrastrar hasta sus p†ginas de propiedades
cualquiera de los :hp2.Escenarios:ehp2. (perfiles nacionales) de la :hp2.Paleta
de pa°s:ehp2. (en Configuraci¢n del sistema).

:p.Al soltar sobre cualquiera de las p†ginas de propiedades un objeto
:hp2.Escenario:ehp2., el protector de pantalla comprobar† si tiene instalado o
no soporte para ese idioma. Si no, recurrir† al mÇtodo por omisi¢n, utilizar la
variable de entorno :hp2.LANG:ehp2.. Sin embargo, si est† soportado, se
cambiar† al idioma del perfil nacional y se utilizar† desde ese momento.

.*
.*--------------------------------------------------------------*\
.* Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.Deshacer

Pulse :hp2.Deshacer:ehp2. para restablecer las opciones que estaban activas
cuando se mostr¢ esta ventana.

.*
.* Help for the Default button
.*
:h3 res=6002.Por omisi¢n

Pulse :hp2.Por omisi¢n:ehp2. para cambiar las opciones a las activas en el
momento de instalar el sistema.

:euserdoc.
