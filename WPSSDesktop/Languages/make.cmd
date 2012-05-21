/*
 * REXX Make script for Doodle's Screen Saver NLS binaries
 *
 * This scripts creates the binary NLS files for all the currently
 * supported languages. To add new language(s), add new lines to this
 * file!
 *
 */

/* Quiet please! :) */
'@echo off'

/* Tell the IPF Compiler where the IPFC base files are */
'set IPFC=%os2tk%\ipfc'


/* The format of the parameter list of the IPF Compiler:
   IPFC <parameters> <sourcefile>
   Where parameters might contain these (amongst others):
   -d:nnn (nnn = numeric value, meaning the country code)
   -c:nnnn (nnnn = numeric value, meaning the codepage)
   -l:xxx (xxx = alphabetic letters, meaning the language)

   For valid codes and other switches, please check the 
   IPF Programming Guide and Reference (IPFREF.INF in toolkit)!
 */



/* English language */
say "====================================== Creating English NLS files..."
'msgc ss_en.txt'
'%os2tk%\bin\ipfc.exe -d:001 -c:850 -l:ENU ss_en.ipf'

/* Hungarian language */
say "====================================== Creating Hungarian NLS files..."
'msgc ss_hu.txt'
'%os2tk%\bin\ipfc.exe -d:036 -c:852 -l:HUN ss_hu.ipf'

/* Dutch language */
say "====================================== Creating Dutch NLS files..."
'msgc ss_nl.txt'
'%os2tk%\bin\ipfc.exe -d:031 -c:850 -l:NLD ss_nl.ipf'

/* German language support */
say "====================================== Creating German NLS files..."
'msgc ss_de.txt'
'%os2tk%\bin\ipfc.exe -d:049 -c:850 -l:DEU ss_de.ipf'

/* Swedish language support */
say "====================================== Creating Swedish NLS files..."
'msgc ss_sv.txt'
'%os2tk%\bin\ipfc.exe -d:046 -c:850 -l:SVE ss_sv.ipf'

/* Traditional Chinese language support */
say "=============================== Creating Traditional Chinese NLS files..."
'msgc ss_zh_TW.txt'
'%os2tk%\bin\ipfc.exe -d:088 -c:950 -l:TWN ss_zh_TW.ipf'

/* French language support */
say "====================================== Creating French files..."
'msgc ss_fr.txt'
'%os2tk%\bin\ipfc.exe -d:033 -c:850 -l:FRA ss_fr.ipf'

/* Spanish language support */
say "====================================== Creating Spanish files..."
'msgc ss_es.txt'
'%os2tk%\bin\ipfc.exe -d:034 -c:850 -L:ESP ss_es.ipf'

/* Russian language support */
say "====================================== Creating Russian files..."
'msgc ss_ru.txt'
'%os2tk%\bin\ipfc.exe -d:007 -c:866 -l:RUS ss_ru.ipf'

/* Finnish language support */
say "====================================== Creating Finnish files..."
'msgc ss_fi.txt'
'%os2tk%\bin\ipfc.exe -d:358 -c:850 -l:FIN ss_fi.ipf'

/* Italian language support */
say "====================================== Creating Italian files..."
'msgc ss_it.txt'
'%os2tk%\bin\ipfc.exe -d:039 -c:850 -l:ITA ss_it.ipf'

say "====================================== All the NLS files have been made."
