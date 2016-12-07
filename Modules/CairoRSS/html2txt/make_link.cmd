
g++ -Zdll -Zomf -Zmt -Zmap html2txt.def html2text.o html.o HTMLControl.o HTMLParser.o Area.o format.o sgml.o table.o Properties.o cmp_nocase.o -o html2txt.dll
