.*==============================================================*\
.*                                                              *
.* Help file for Doodle's Screen Saver v1.6                     *
.*                                                              *
.* Language: Traditional Chinese language                       *
.* Locales : zh_TW_*                                            *
.*                                                              *
.* Author  : Alex Lee                                           *
.*                                                              *
.* Date (YYYY-MM-DD): 2006.02.10.                               *
.*                                                              *
.*                                                              *
.* To create the binary HLP file from this file, use the IPFC   *
.* tool from the toolkit. For more information about it, please *
.* check the IPF Programming Guide and Reference (IPFREF.INF)!  *
.*                                                              *
.*==============================================================*/
.*
:userdoc.
.*
.*--------------------------------------------------------------*\
.* General help for the page
.*--------------------------------------------------------------*/
.*
:h1.螢幕保護程式
:font facename='MINCHO System'.
:p.:color fc=pink.:hp2.螢幕保護程式:ehp2.:color fc=default.的第一頁設定是
:color fc=pink.:hp2.一般螢幕保護程式設定:ehp2.:color fc=default.頁。 
這是變更螢幕保護程式最一般性設定的地方，
.br
像逾時保護和設定密碼保護.
:p.
變更螢幕保護程式的語言可以使用拖曳一個語言環境物件到此頁面。
更多的訊息可以在
:link reftype=hd res=5000.
這裡:elink.找到。
:p.
關於每個欄位的詳細解釋，請從下面列表選擇：
:ul compact.
:li.:link reftype=hd res=1000.一般設定:elink.
:li.:link reftype=hd res=1001.DPMS 設定:elink.
:li.:link reftype=hd res=2000.密碼保護:elink.
:li.:link reftype=hd res=3000.螢幕保護程式模組:elink.
:eul.

.*--------------------------------------------------------------*\
.*  Help for Page 1 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for General settings groupbox
.*
:h2 res=1000.一般設定
:font facename='MINCHO System'.
:p.選取:color fc=pink.:hp2.使用螢幕保護程式:ehp2.:color fc=default.啟用此螢幕保護程式。
當此功能開啟時，系統將監視使用者的動作（桌面滑鼠和鍵盤的動作）， 
.br
並將在設定待機狀態的時間後自動啟動螢幕保護。
:p.這個時間可以以分鐘為單位，可以在此欄位:color fc=pink.:hp2.啟動螢幕保護在 xxx 分鐘怠機之後:ehp2.:color fc=default..
設定時間。
:note.
選取桌面蹦現選擇畫面的:color fc=pink.:hp2.立即鎖定:ehp2.:color fc=default.選單項目，
即啟動螢幕保護程式，無論是否設定保護程式的使用。
:p.如果選取:color fc=pink.:hp2.僅由鍵盤喚醒:ehp2.:color fc=default.，螢幕保護程式
將不會因偵測到滑鼠動作而停止運作，而只會因偵測到鍵盤動作而停止。
.br
當電腦處於震動的環境中，或是因其他原因不適合由碰觸滑鼠停止螢幕保護程式運作時，
.br
此項功能將很有用處。

:p.:link reftype=hd res=1001.DPMS 設定:elink.

.*
.* Help for DPMS settings groupbox
.*
:h2 res=1001.DPMS 設定
:font facename='MINCHO System'.
:p.當顯示驅動程式和螢幕都支援 DPMS (目前僅有 Scitech SNAP 支援) 時，
此設定才能使用。
:p.目前針對螢幕有四種電源保護狀態，都是依據 DPMS 標準。
下列是按照省電最少到最多的次序排列狀態：
:ol.
:li.:color fc=pink.:hp2.開啟狀態:ehp2.:color fc=default.。這是螢幕打開時的狀態， 
一般的工作情形。
:li.:color fc=pink.:hp2.待機狀態:ehp2.:color fc=default.。這時螢幕會暫時關閉，但 
可以非常快速回復。
:li.:color fc=pink.:hp2.暫歇狀態:ehp2.:color fc=default.。這時螢幕幾乎是完全
關閉狀態。
:li.:color fc=pink.:hp2.關閉狀態:ehp2.:color fc=default.。這時螢幕是關閉的狀態。
:eol.
:p.螢幕保護程式總是由第一個狀態，然後一步步隨著時間
進入更多的省電狀態。
:p.螢幕保護程式僅會使用所選取的狀態，並且按著
設定的時間切換至下個狀態。

:p.:link reftype=hd res=1002.關於 DPMS 的資訊:elink.
:p.:link reftype=hd res=1000.一般設定:elink.

.*
.* Info about DPMS itself
.*
:h3 res=1002.關於 DPMS 的資訊
:font facename='MINCHO System'.
:p.DPMS 是 :color fc=pink.:hp2.Display Power Management Signaling:ehp2.:color fc=default. 的縮寫，一種 VESA 介面標準，
.br
可定義四種螢幕怠機狀態電源管理模式： 開啟、待機、暫歇和關閉。

:p.:link reftype=hd res=1001.DPMS 設定:elink.
:p.:link reftype=hd res=1000.一般設定:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for Page 2 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Password protection groupbox
.*
:h2 res=2000.密碼保護
:font facename='MINCHO System'.
:p.選取:color fc=pink.:hp2.使用密碼保護:ehp2.:color fc=default.以啟動螢幕保護程式的密碼保護。
.br
如果密碼保護功能開啟，螢幕保護程式就會在螢幕保護作用終止前詢問密碼，
.br
若輸入的密碼正確，螢幕保護作用就會終止。
:p.如果有 Security/2 安裝，螢幕保護程式會被告之使用目前 Security/2 的使用者密碼。
.br
也就是說，當螢幕保護程式詢問密碼以終止螢幕保護時，它會比較此密碼和設定在目前 Security/2 的使用者密碼。
.br
這個設定可選取:color fc=pink.:hp2.使用目前 Security/2 使用者密碼:ehp2.:color fc=default.選項。如果未安裝 Security/2，此選項不能作用。
:p.選取:color fc=pink.:hp2.使用此密碼:ehp2.:color fc=default.選項，螢幕保護程式會使用個人的螢幕保護作用密碼。此密碼可以設定或變更，
.br
藉著輸入新密碼到這兩個欄位中。這兩欄位中的密碼需要相同，以防發生錯誤。
.br
要變更已輸入的密碼，可按下
:color fc=pink.:hp2.變更:ehp2.:color fc=default.按鈕。
:p.如果選取:color fc=pink.:hp2.延遲密碼保護:ehp2.:color fc=default.，螢幕保護程式將不會
在螢幕保護作用開始時詢問密碼，就如同密碼保護作用沒有被開啟一樣。
.br
另一方面，螢幕保護將在大於設定:color fc=pink.:hp2.詢問密碼僅在 xxx 分鐘保護作用 
時間之後:ehp2.:color fc=default.，將會在終止螢幕保護作用之前詢問密碼。
:p.:color fc=pink.:hp2.以第一個按鍵為密碼的第一個 Key:ehp2.:color fc=default. 會決定
當此密碼保護視窗將有的顯示，螢幕保護模組會如何運作。
.br
如果選取這個選項，密碼保護視窗會顯示這個已經使用的按鍵為密碼的第一個字元。
.br
如果沒有選取，那麼這個按鍵就不會被當作是第一個密碼的字元，而只會被當成用來
.br
顯示密碼保護視窗的一個通知信息。請注意，這只是一個設定資訊，一些螢幕保護
.br
模組並不會理睬它。
:p.選取:color fc=pink.:hp2.開機時啟動螢幕保護模組:ehp2.:color fc=default.會在系統開機時
自動啟動有設密碼的螢幕保護程式。
:note.
當螢幕保護程式依照使者的要求啟動（從桌面的主磞現選擇畫面選取:color fc=pink.:hp2.立即鎖定:ehp2.:color fc=default.
選單項目），
.br
或是由系統啟動順序開啟時，就不會延遲密碼保護。



.*
.*--------------------------------------------------------------*\
.*  Help for Page 3 of the Screen Saver
.*--------------------------------------------------------------*/
.*
.* Help for Screen Saver modules groupbox
.*
:h2 res=3000.螢幕保護程式模組
:font facename='MINCHO System'.
:p.此頁面顯示可使用的螢幕保護程式模組列表，
和顯示關於目前所選取模組的資訊。
:p.目前選取的模組，是在可使用的模組
列表中。當螢幕保護的時間到時，
此模組會由螢幕保護程式啟動。
:p.當選取:color fc=pink.:hp2.顯示預覽:ehp2.:color fc=default.時，可以預視目前的模組。
:p.右方的頁面顯示此模組的資訊，如
模組名稱、版本和顯示此模組是否支援密碼保護。
:note.
如果目前的模組並不支援密碼保護，那麼螢幕就
不會有密碼保護作用，即便在第一頁面有設定
使用密碼保護！
:p.當中一些模組可以架構。如果目前的模組可以架構，那麼
:color fc=pink.:hp2.架構:ehp2.:color fc=default.按鈕就可以作用。
.br
按下此按鈕將呈現此模組的特殊架構對話框。
:p.:color fc=pink.:hp2.立即啟用:ehp2.:color fc=default.按鈕可以用使觀看目前模組的動作， 
有關此螢幕保護程式目前的設定 (包括其他螢幕保護程式頁面的
設定，
.br
如:color fc=pink.:hp2.延遲密碼保護:ehp2.:color fc=default.和其他的)。

:p.:link reftype=hd res=3001.模組:elink.

.*
.* Help about Screen Saver modules
.*
:h3 res=3001.模組
:font facename='MINCHO System'.
:p.螢幕保護模組是一個或多個特殊的 DLL 檔，置放於 
螢幕保護程式主目錄中的 :color fc=pink.:hp3.Modules:ehp3.:color fc=default. 資料夾。
.br
當螢幕保護程式啟動時，模組就會開啟。

:p.:link reftype=hd res=3000.螢幕保護程式模組:elink.

.*
.*--------------------------------------------------------------*\
.*  Help for setting the language of the screen saver
.*--------------------------------------------------------------*/
.*
.* Help for setting the language
.*
:h2 res=5000.設定此螢幕保護程式的語言
:font facename='MINCHO System'.
:p.你可以變更螢幕保護程式設定頁的語系，和某些螢幕保護程式模組的語系（支援 NLS ）, 從
:color fc=pink.:hp2.國別選用區:ehp2.:color fc=default.使用拖拉
.br
某個:color fc=pink.:hp2.語言環境物件:ehp2.:color fc=default.。
:p.螢幕保護模組會嘗試使用系統預設的語言。它會
偵測 :color fc=pink.:hp2.LANG:ehp2.:color fc=default. 環境變數。 如果該語言支援檔沒有安裝，
.br
螢幕保護程式會回到英語模式。
:p.但是，也可以設定其他語言，不一定要讓螢幕保護程式
使用系統的語言。
 任何:color fc=pink.:hp2.國別選用區:ehp2.:color fc=default.（在「系統設定」裡）的
.br
:color fc=pink.:hp2.語言環境物件:ehp2.:color fc=default.
都可以拖曳至螢幕保護程式設定通。
:p.當一個:color fc=pink.:hp2.語言環境物件:ehp2.:color fc=default.被拖曳至設定通，螢幕
保護程式會偵測此語言的支援是否已安裝。 如果沒有安裝，程式會回到
.br
預設值，使用 :color fc=pink.:hp2.LANG:ehp2.:color fc=default. 的環境變數。
但是，如果此語言有被支援，就會變更為該語言並即可使用。

.*
.*--------------------------------------------------------------*\
.*  Help for common buttons like Undo and Default
.*--------------------------------------------------------------*/
.*
.* Help for the Undo button
.*
:h3 res=6001.還原
:font facename='MINCHO System'.
:p.選取:hp2.「還原」:ehp2.以變更在此視窗顯示之前
已生效的設定。
.*
.* Help for the Default button
.*
:h3 res=6002.預設值
:font facename='MINCHO System'.
:p.選取:hp2.「預設值」:ehp2.以變更當你安裝在此系統時
已生效的設定。
:euserdoc.
