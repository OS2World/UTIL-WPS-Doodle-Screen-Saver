/*
 * REXX Make script for the PrettyClock saver module NLS binaries
 *
 * This scripts creates the binary NLS files for all the currently
 * supported languages. To add new language(s), add new lines to this
 * file!
 *
 */

/* Quiet please! :) */
'@echo off'

/* English language */
say "====================================== Creating English NLS files..."
'msgc en.txt'

/* Hungarian language */
say "====================================== Creating Hungarian NLS files..."
'msgc hu.txt'

/* Dutch language */
say "====================================== Creating Dutch NLS files..."
'msgc nl.txt'

/* German language support */
say "====================================== Creating German NLS files..."
'msgc de.txt'

/* Swedish language support */
say "====================================== Creating Swedish NLS files..."
'msgc sv.txt'

/* Traditional Chinese language support */
say "=============================== Creating Traditional Chinese NLS files..."
'msgc zh_TW.txt'

/* French language support */
say "====================================== Creating French files..."
'msgc fr.txt'

/* Spanish language support */
say "====================================== Creating Spanish files..."
'msgc es.txt'

/* Russian language support */
say "====================================== Creating Russian files..."
'msgc ru.txt'

/* Finnish language support */
say "====================================== Creating Finnish files..."
'msgc fi.txt'

/* Italian language support */
say "====================================== Creating Italian files..."
'msgc it.txt'

say "====================================== All the NLS files have been made."
