.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.9                     *
.*                                                              *
.* Language: Espa¤ol                                            *
.* Locales : es_*                                               *
.*                                                              *
.* Author  : Guillermo G. Llorens                               *
.*                                                              *
.* Date (YYYY-MM-DD): 2008.05.08.                               *
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
:h1 res=1000.Salvapantalla
:p.La primer p gina de par metros de :hp2.Salvapantalla:ehp2. es la p gina de :hp2.
Par metros Generales del Salvapantalla:ehp2. .Este es el lugar donde los par metros
 mas comunes del Salvapantalla pueden modificarse, como el retardo para activarlo,
 y la configuraci¢n para protecci¢n con contrase¤a.
:p.
Cambiar el idioma del Salvapantalla es posible arrastrando y soltando
un objeto Locale a esta p gina. Mas acerca de ello puede hallarse
:link reftype=hd res=5000.
aqu¡:elink..
:p.
Para una explicaci¢n detallada de cada campo elija de la siguiente lista:
:ul compact.
:li.:link reftype=hd res=1001.Par metros generales:elink.
:li.:link reftype=hd res=1002.Protecci¢n por contrase¤a:elink.
:li.:link reftype=hd res=6001.Deshacer:elink.
:li.:link reftype=hd res=6002.Por defecto:elink.
:eul.
.*
.* Help for General settings groupbox
.*
:h1 res=1001.Par metros generales
:p.Seleccionar :hp2.Salvapantalla activado:ehp2. para activar el Salvapantalla.
Mientras est‚ activado, el sistema monitorear  la actividad del usuario (actividad de rat¢n
y teclado en el Escritorio), y arrancar  el funcionamiento del Salvapantalla
autom ticamente tras un dado per¡odo de inactividad.
:p.Esta cantidad puede configurarse en minutos en el campo :hp2.Comenzar Salvapantalla
tras xxx minuto(s) de inactividad:ehp2..
:note.
Seleccionando el item :hp2.Bloquear ahora:ehp2. en el menu desplegado del
Escritorio arrancar  el Salvapantalla a£n si no estuviera habilitado.
:p.Si :hp2.Activar por teclado solamente:ehp2. es seleccionado, el Salvapantalla
no detendr  su funcionamiento cuando se detecte actividad del rat¢n, pero solo 
cuando haya actividad del teclado. Esto es £til en casos en que la computadora
 se encuentra en un entorno vibrante, o si no se prefiere que se detenga el 
funcionamiento del Salvapantallas si por alguna raz¢n el rat¢n es tocado.
:p.Una vez :hp2.Inhabilitar apariciones VIO al guardar:ehp2. se marca, el Salvapantallas 
inhabilitar  todas las apariciones VIO mientras este se halle activo. Esto significa que 
ninguna otra aplicaci¢n ser  capaz de tomar los dispositivos de pantalla y de entrada, ni 
siquiera el manejador de Ctrl-Alt-Del u otra aplicaci¢n similar. Esto puede prevenir que el 
usuario termine algunas aplicaciones que no responden, mientras el Salvapantallas se est  
ejecutando, pero hace que el sistema sea m s seguro.
.*
.* Help for Password protection groupbox
.*
:h1 res=1002.Protecci¢n por Contrase¤a
:p.Seleccione :hp2.Use protecci¢n por contrase¤a:ehp2. para habilitar la protecci¢n
por contrase¤a para el Salvapantalla. Si la Protecci¢n por Contrase¤a se activa, 
entonces el Salvapantalla pedir  el ingreso de una contrase¤a antes de detener 
su funcionamiento, y s¢lo se detendr  si se ingres¢ la contrase¤a correcta.
:p.Si Security/2 est  instalado, el Salvapantalla puede ser configurado para que
use la contrase¤a del usuario actual de Security/2. Esto significa que cuando
el Salvapantalla pide la contrase¤a para detener su funcionamiento, comparar  la
contrase¤a contra aquella configurada en Security/2. Esto puede configurarse
seleccionando la opci¢n :hp2.Usar contrase¤a del usuario actual Security/2:ehp2. .
Si Security/2 no ha sido instalado, esta opci¢n no est  disponible.
:p.Seleccionando la opci¢n :hp2.Usar esta contrase¤a:ehp2. , el Salvapantalla
usar  una contrase¤a propia. Esta contrase¤a puede ser configurada o cambiada
ingresando la nueva contrase¤a en ambos campos de entrada. La misma contrase¤a
debe ser ingresada en ambos, a fin de evitar errores de tipograf¡a.
Para activar el cambio de contrase¤a a la nueva que se ha ingresado, pulsar el
bot¢n :hp2.Cambiar:ehp2. .
:p.Si la opci¢n :hp2.Retrasar protecci¢n por contrase¤a:ehp2. es seleccionada, 
el Salvapantalla no pedir  ingresar la contrase¤a en los primeros minutos
tras haberse activado, y se comportar  como si la protecci¢n por contrase¤a
no estuviera activa. Sin embargo, si el tiempo de funcionamiento del Salvapantalla 
fuera mayor que el configurado en :hp2.Pedir contrase¤a solo despu‚s de xxx
 minuto(s) de activado:ehp2., la contrase¤a ser  pedida antes de detener el 
Salvapantalla.
:p.La opci¢n :hp2.La primer tecla pulsada se toma como el primer car cter de la 
contrase¤a:ehp2., determina como los m¢dulos del Salvapantalla se comportar n 
cuando la ventana de contrase¤a est  por ser mostrada. Si esta opci¢n se 
selecciona, la tecla que causa que aparezca la ventana de contrase¤a har  que 
esta tecla se use asimismo como primer caracter de la misma. Si no es 
seleccionada, entonces la primer tecla no ser  usada como la primer letra de 
la contrase¤a, pero s¡ como aviso para que se muestre la ventana de 
contrase¤a. N¢tese que esto es solo con prop¢sito informativo puesto que 
algunos salvapantallas no se comportan asi.

:p.Seleccionar :hp2.Activar Salvapantalla al iniciar el sistema:ehp2. para 
autom ticamente activar el Salvapantalla protegido por contrase¤a al inicio del
sistema.
:note.
El Salvapantalla no retrasar  la protecci¢n por contrase¤a si es requerido por 
 el usuario (seleccionando el item de men£ :hp2.Bloquear ahora:ehp2. desde el men£ 
desplegable del Escritorio), o si es activado al inicio del sistema.
.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h1 res=2000.Salvapantalla
:p.La segunda p gina de par metros de :hp2.Salvapantalla:ehp2. es la de
:hp2.Par metros DPMS:ehp2. .Aqu¡ es donde se le puede indicar al Salvapantalla 
si debe usar diferentes servicios
:link reftype=hd res=2002.
DPMS:elink. o no, si est n disponibles.
:p.
Cambiar el idioma del Salvapantalla es posible arrastrando y soltando
un objeto Locale a esta p gina. Mas acerca de ello puede hallarse
:link reftype=hd res=5000.
aqu¡:elink..
:p.
Para una explicaci¢n detallada de cada campo elija de la siguiente lista:
:ul compact.
:li.:link reftype=hd res=2001.Par metros DPMS:elink.
:li.:link reftype=hd res=6001.Deshacer:elink.
:li.:link reftype=hd res=6002.Por defecto:elink.
:eul.
.*
.* Help for DPMS settings groupbox
.*
:h1 res=2001.Par metros DPMS
:p.Estos par metros est n disponibles s¢lo si el controlador de video soporta DPMS 
(actualmente solo es soportado por el controlador SNAP de Scitech), y el monitor 
tiene capacidad DPMS.
:p.Existen cuatro estados de ahorro de energ¡a para monitores, de acuerdo con el 
standard DPMS.
Estos son los siguientes, comenzando por el estado de menor ahorro:
:ol.
:li.El :hp2.estado encendido:ehp2.. Este es el estado en el cual el monitor se enciende, y
trabaja normalmente.
:li.El :hp2.estado stand-by:ehp2.. El monitor est   parcialmente apagado aqu¡, pero
puede recuperarse r pidamente de este estado.
:li.El :hp2.estado suspender:ehp2.. Este es el estado en el cual el monitor est  casi totalmente
apagado.
:li.El :hp2.estado apagado:ehp2. El monitor se apaga en este estado.
:eol.
:p.El Salvapantalla siempre comienza en el primer estado, y avanza hacia los subsiguientes estados
de ahorro de energ¡a a medida que el tiempo pasa.
:p.El Salvapantalla s¢lo usar  aquellos estados que sean seleccionados aqu¡, y
cambiar  al siguiente estado despu‚s del tiempo especificado para ese estado.
.*
.* Info about DPMS itself
.*
:h1 res=2002.Informaci¢n acerca de DPMS
:p.DPMS es la abreviatura de :hp2.Display Power Management Signaling:ehp2., (Se¤alizaci¢n para el 
manejo de energ¡a de la Pantalla), una interfase del standard VESA que define cuatro
 modos de manejo de energ¡a para monitores: 
encendido (on), stand-by, suspender (suspend) y apagado (off).
.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* General help for the page
.*
:h1 res=3000.Salvapantalla
:p.La tercer p gina de par metros del :hp2.Salvapantalla:ehp2. es la de
:hp2.M¢dulos del Salvapantalla:ehp2. . Aqu¡ es donde la lista de los 
:link reftype=hd res=3002.
m¢dulos:elink. de Salvapantalla disponibles se pueden ver, uno de estos m¢dulos 
puede ser configurado y seleccionado para su uso.
:p.
Cambiar el idioma del Salvapantalla es posible arrastrando y soltando
un objeto Locale a esta p gina. Mas acerca de ello puede hallarse
:link reftype=hd res=5000.
aqu¡:elink..
:p.
Para una explicaci¢n detallada de cada campo elija de la siguiente lista:
:ul compact.
:li.:link reftype=hd res=3001.M¢dulos del Salvapantalla:elink.
:li.:link reftype=hd res=6001.Deshacer:elink.
:li.:link reftype=hd res=6002.Por defecto:elink.
:eul.
.*
.* Help for Screen Saver modules groupbox
.*
:h1 res=3001.M¢dulos del Salvapantalla
:p.Esta p gina muestra la lista de los m¢dulos disponibles, y alguna informaci¢n
acerca del m¢dulo elegido.
:p.El m¢dulo en uso ser  aquel que sea seleccionado de la lista de m¢dulos disponibles.
 Este es el m¢dulo que ser  utilizado por el Salvapantalla cuando sea tiempo de activarse.
:p.Una vista previa del m¢dulo seleccionado puede verse eligiendo :hp2.Vista previa:ehp2..
:p.La parte derecha de la p gina muestra informaci¢n acerca del m¢dulo en uso, as¡ como 
el nombre del mismo, el n£mero de versi¢n, y si el mismo soporta protecci¢n por
contrase¤a.
:note.
Si el m¢dulo actual no soporta protecci¢n por contrase¤a, entonces la protecci¢n de pantalla
no estar  protegida por contrase¤a, a£n si hubiese sido configurado as¡ en la primera
p gina de par metros del Salvapantalla!
:p.Algunos de los m¢dulos pueden ser configurados. Si el m¢dulo en uso puede ser configurado, 
entonces el bot¢n :hp2.Configurar:ehp2. estar  habilitado. Pulsando ese bot¢n dar  lugar
a un di logo de configuraci¢n para ese m¢dulo.
:p.El bot¢n :hp2.Arrancar ahora:ehp2. puede utilizarse para ver el comportamiento del m¢dulo en cuesti¢n 
incluyendo los otros par metros del Salvapantalla, como :hp2.Retrasar protecci¢n por contrase¤a:ehp2. y otros.
.*
.* Help about Screen Saver modules
.*
:h1 res=3002.M¢dulos
:p.Un m¢dulo Salvapantalla consiste en uno o mas archivos DLL, ubicados en la carpeta 
:hp3.M¢dulos:ehp3. en el directorio principal del Salvapantalla. Es arrancado en el momento
que el Salvapantalla ha de ser iniciado.
.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h1 res=5000.Configurando el idioma del Salvapantalla
:p.Es posible cambiar el idioma de las p ginas de par metros del Salvapantalla y
el idioma de algunos de los m¢dulos que este utiliza (el que soporte NLS) arrastrando
 y soltando un :hp2.objeto Locale:ehp2. desde la :hp2.paleta Pa¡s:ehp2..
:p.El Salvapantalla intenta usar el idioma del sistema por defecto, el cual obtiene
de la variable de entorno :hp2.LANG:ehp2. . Si no hay archivos de soporte instalados para
ese idioma, el Salvapantalla utilizar  el idioma Ingl‚s.
:p.Sin embargo, otros idiomas pueden tambi‚n ser elegidos, no es necesario usar el idioma
 del sistema para el Salvapantalla. Cualquiera de los :hp2.objetos Locale:ehp2. de la 
:hp2.paleta Pa¡s:ehp2. (en Configuraci¢n del Sistema) puede ser arrastrado y soltado
a las p ginas de configuraci¢n del Salvapantalla.
:p.Cuando un :hp2.objeto Locale:ehp2. es soltado en alguna de las p ginas de configuraci¢n, 
el Salvapantalla revisar  si hay soporte instalado para ese idioma o no. Si no ha sido 
instalado, revertir  al m‚todo por defecto, para usar la variable de entorno :hp2.LANG:ehp2.. Si el idioma est  soportado, cambiar  a ese idioma y lo utilizar .
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h1 res=6001.Deshacer
:p.Seleccionar el bot¢n :hp2.Deshacer:ehp2. para cambiar la configuraci¢n a aquella que
 estaba activa antes de que esa ventana se muestre.
.*
.* Help for the Default button
.*
:h1 res=6002.Por defecto
:p.Seleccionar :hp2.Por defecto:ehp2. para cambiar la configuraci¢n a aquella que 
 estaba activa cuando se instal¢ el sistema.
:euserdoc.
