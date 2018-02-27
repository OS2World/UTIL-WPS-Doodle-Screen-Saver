g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN html2text.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN html.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN HTMLControl.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN HTMLParser.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN Area.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN format.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN sgml.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN table.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN Properties.c
g++ -c -O2 -Zomf -march=i686 -DAUTO_PTR_BROKEN cmp_nocase.c

g++ -Zdll -Zomf -Zmap html2txt.def html2text.o html.o HTMLControl.o HTMLParser.o Area.o format.o sgml.o table.o Properties.o cmp_nocase.o -o html2txt.dll
