.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.6                     *
.*                                                              *
.* Language: General Italian language                           *
.* Locales : it_*                                               *
.*                                                              *
.* Author  : Doodle                                             *
.*                                                              *
.* Date (YYYY-MM-DD): 2005.05.22.                               *
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
.*
:h1 res=4000.Salvaschermo
:p.La prima pagina di configurazione del :hp2.Salvaschermo:ehp2. Š quella delle
:hp2.Impostazioni generali Salvaschermo:ehp2. . Questo Š il posto dove si 
possono cambiare la maggior parte delle impostazioni del Salvaschermo,
come il tempo di attivit… e la password di protezione.
:p.
E' possibile cambiare la lingua del salvaschermo trascinando e rilasciando l'oggetto Locale in questa pagina.
 Altro sull'argomento si pu• trovare
:link reftype=hd res=5000.
qui:elink..
:p.
Per una spiegazione dettagliata di ciascun campo, selezionare dalla lista seguente:
:ul compact.
:li.:link reftype=hd res=1000.Impostazioni generali:elink.
:li.:link reftype=hd res=1001.Impostazioni DPMS:elink.
:li.:link reftype=hd res=2000.Password di protezione:elink.
:li.:link reftype=hd res=3000.Moduli Salvaschermo:elink.
:eul.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Aiuto per la sezione delle impostazioni generali
.*
:h2 res=1000.Impostazioni generali
:p.Selezionare :hp2.Abilita il salvaschermo:ehp2. per abilitare il salvaschermo. In esecuzione
il sistema monitorer… l'utilizzo della tastiera e del mouse e attiver… automaticamente
il salvaschermo dopo il periodo di inattivit… stabilito.
:p.Si pu• impostare questo periodo nel campo :hp2.Avvia il salvaschermo 
dopo xxx minuto(i) di inattivit…:ehp2..
:note.
Selezionando :hp2.Blocca ora:ehp2. dal menu impostazioni della Scrivania si avvier… il salvaschermo anche se questo non Š abilitato.
:p.Se Š abilitato :hp2.Sblocco solo da tastiera :ehp2. , il salvaschermo non si interromper… al movimento del mouse, 
ma solo quando si premer… un tasto qualunque della tastiera. Questo Š il  tipico uso di un PC in ambienti con vibrazioni, 
o se non si preferisce che il computer interrompa il salvaschermo qualora si tocchi il mouse inavvertitamente.
:p.Selezionando :hp2.Disabilita avvisi VIO mentre il salvaschermo Š attivo:ehp2., il salvaschermo disabiliter… tutti gli avvisi VIO quando Š in funzione. 
Questo significa che nessun'altra applicazione sar… in grado di comparire a schermo, input da periferiche comprese, come la pressione dei tasti 
CAD (Ctrl+Alt+Del) o altre simili. Questo evita che l'utente possa  chiudere applicazioni che si fossero bloccate mentre il salvaschermo Š attivo, ma rende 
il sistema pi— sicuro.

:p.:link reftype=hd res=1001.Impostazioni DPMS:elink.

.*
.* Help for DPMS settings groupbox
.*
:h2 res=1001.Impostazioni DPMS
:p.Queste impostazioni sono disponibili solo se il driver video supporta funzioni DPMS (attualmente Š supportato solo nei drivers Scitech SNAP), ed il monitor Š DPMS compatibile.
:p.Ci sono quattro modalit… di risparmio energetico per i monitor, in linea con lo standard DPMS.
Sono i seguenti, iniziando dallo stato di minore risparmio energetico:
:ol.
:li.Lo :hp2.stato attivo:ehp2.. E' il caso in cui il monitor Š acceso e funziona normalmente.
:li.Lo :hp2.stato stand-by:ehp2.. Qui il monitor Š parzialmente spento, ma Š possibile ripristinare molto velocemente lo stato attivo.
:li.Lo :hp2.stato sospensione:ehp2.. Questo Š il caso in cui il monitor Š per lo pi— spento.
:li.Lo :hp2.stato spento:ehp2. Il monitor in questa condizione Š spento del tutto.
:eol.
:p.Il salvaschermo si avvia sempre dal primo stato, e cambia condizioni energetiche in progressione con il trascorrere del tempo.
:p.Il salvaschermo utilizzer… solamente le modalit… di risparmio energetico selezionate in questa scheda e passer… allo stato successivo trascorso il tempo impostato per ognuno di essi.

:p.:link reftype=hd res=1002.Informazioni su DPMS:elink.
:p.:link reftype=hd res=1000.Impostazioni generali:elink.

.*
.* Info about DPMS itself
.*
:h3 res=1002.Informazioni su DPMS
:p.DPMS Š l'abbreviazione di :hp2.Display Power Management Signaling:ehp2., una interfaccia VESA 
standard che definisce quattro modalit… di gestione del risparmio energetico quando i monitor sono inattivi: 
acceso, stand-by, sospensione e spento.

:p.:link reftype=hd res=1001.Impostazioni DPMS:elink.
:p.:link reftype=hd res=1000.Impostazioni generali:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.*
.* Help for Password protection groupbox
.*
:h2 res=2000.Password di protezione
:p.Selezionare :hp2.Usa una password di protezione:ehp2. per abilitare la protezione con password del salvaschermo. Se la password di protezione Š attivata, il salvaschermo chieder… l'inserimento della password prima di interrompersi, e si fermer… solo se si inserir… la password esatta.
:p.Se Š installato Security/2, il salvaschermo pu• essere configurato perchŠ utilizzi la stessa password dell'utente  autorizzato all'accesso con Security/2. 
Quando il salvaschermo chieder… l'inserimento della password per interrompersi, confronter… la password immessa con quella dell'utente attualmente autorizzato all'accesso al sistema da Security/2. 
Abilitare questa  funzione selezionando l'opzione :hp2.Usa la stessa password di Security/2 :ehp2.. Se Security/2 non Š installato, questa opzione  Š disabilitata.
:p.Selezionare l'opzione :hp2.Usa questa password:ehp2. per, usare una password diversa per sbloccare il salvaschermo. Questa password si pu• abilitare o cambiare inserendone una nuova nei due campi di testo vuoti. 
Si deve inserire la stessa password in entrambi i campi di testo, o lasciare vuoto per invalidare. Per cambiare la password con quella inserita, premere il pulsante 
:hp2.Applica:ehp2. .
:p.Se Š selezionato :hp2.Ritardo protezione password:ehp2. , il salvaschermo non richieder… la password se non dopo il periodo di tempo impostato, e rester… visibile in attesa fino ad inserimento. Se in questa condizione il salvaschermo rester… attivo 
per il tempo impostato in :hp2.Richiedi la password solo dopo xxx minuto(i) di attivit…:ehp2., verr… richiesta la password prima di interrompere il salvaschermo.
:p.L'opzione :hp2.Considera il primo tasto premuto la prima chiave della password:ehp2. determina il comportamento del modulo salvaschermo quando deve mostrare la finestra di immissione password di protezione.
Se si seleziona questa opzione, la pressione di un tasto, che causa la comparsa della finestra di richiesta password di protezione, sar… considerata anche come il primo carattere della password da immettere. Se l'opzione non Š selezionata,
allora la pressione accidentale di un tasto non verr… considerata come primo carattere della password, ma causer… solamente la comparsa della finestra di immissione password di protezione. Si noti che questa Š solamente una impostazione di notifica, 
alcuni moduli salvaschermo potrebbero non supportarla.
:p.Selezionare :hp2.Attiva il salvaschermo all'avvio del sistema:ehp2. per avviare automaticamente il salvaschermo con password all'avvio del sistema.
:note.
Il salvaschermo non user… il Ritardo protezione password se avviato a richiesta dell'utente (selezionando :hp2.Blocca ora  :ehp2. dal menu impostazioni della scrivania), o se eseguito  automaticamente all'avvio del sistema.


.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Screen Saver modules groupbox
.*
:h2 res=3000.Moduli Salvaschermo
:p.Questa pagina mostra la lista dei moduli salvaschermo disponibili e le informazioni sul modulo attualmente selezionato.
:p.Il modulo attualmente selezionato Š quello che si seleziona dalla lista dei moduli disponibili. Questo Š il modulo che sar…  avviato dal salvaschermo al momento prestabilito.
:p.E' possibile vedere un'anteprima del modulo prescelto selezionando :hp2.Mostra anteprima:ehp2..
:p.La parte destra della pagina mostra informazioni sul modulo corrente, come il nome del modulo, il suo numero di versione e se supporta o meno password di protezione.
:note.
Se il modulo corrente non supporta password di protezione, non sar… possibile proteggere il salvaschermo con password, anche se sar… stata selezionata l'opzione prevista nella prima pagina delle impostazioni del salvaschermo!
:p.Alcuni moduli possono essere configurati. Se il modulo selezionato pu• essere configurato, allora il pulsante :hp2.Configura:ehp2. Š disponibile. Premendo il pulsante si acceder… ad una finestra  specifica per configurare il modulo  dettagliatamente.
:p.Il pulsante :hp2.Avvia adesso:ehp2. pu• essere usato per vedere come si presenter… il modulo salvaschermo in esecuzione con le opzioni specifiche previste (considerando anche quelle previste  dalle altre pagine di configurazione del salvaschermo, come per esempio :hp2.Ritardo protezione password:ehp2. ed altre).

:p.:link reftype=hd res=3001.Moduli:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.Moduli
:p.Un modulo salvaschermo consiste di uno o pi— file DLL speciali, localizzati nella cartella :hp3.Modules:ehp3. contenuta nella directory principale del salvaschermo. Vengono eseguiti quando il salvaschermo si attiva.

:p.:link reftype=hd res=3000.Moduli Salvaschermo:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h2 res=5000.Configurare la lingua del salvaschermo
:p.E' possibile cambiare la lingua delle pagine di impostazione del salvaschermo e la lingua di alcuni moduli salvaschermo (se supportano la nazionalizzazione) trascinando e rilasciando l'oggetto :hp2.Locale:ehp2. dalla :hp2.Tavolozza paesi:ehp2..
:p.Il salvaschermo prova ad usare la lingua di sistema automaticamente servendosi della variabile di ambiente :hp2.LANG:ehp2. . Se non sono disponibili i files di supporto per la lingua individuata dalla variabile, il salvaschermo utilizzer… automaticamente la lingua Inglese.
:p.Si possono comunque selezionare altre lingue, non Š necessario utilizzare la lingua di sistema per il salvaschermo. E' possibile trascinare e rilasciare sulle pagine di impostazione del salvaschermo qualunque oggetto :hp2.Locale:ehp2. dalla  
:hp2.Tavolozza paesi:ehp2. (contenuta nella cartella Impostazioni di sistema) .
:p.Quando si rilascia un' oggetto :hp2.Locale:ehp2. su qualsiasi pagina delle impostazioni del salvaschermo, il salvaschermo verificher… se c'Š la disponibilit… del supporto per la lingua specifica. Se non presente, verr… applicato il  metodo di lettura della variabile di ambiente :hp2.LANG:ehp2. .
Se la lingua Š supportata, le pagine di impostazione del salvaschermo, dei moduli e delle loro eventuali finestre specifiche e l'aiuto in linea, cambieranno automaticamente lingua dopo il rilascio.
.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.Regredire
:p.Seleziona :hp2.Regredire:ehp2. per ripristinare le impostazioni che erano attive prima della visualizzazione della finestra.
.*
.* Help for the Default button
.*
:h3 res=6002.Valori assunti
:p.Seleziona :hp2.Valori assunti:ehp2. per ripristinare le impostazioni che erano attive quando Š stato installato il sistema.
:euserdoc.
