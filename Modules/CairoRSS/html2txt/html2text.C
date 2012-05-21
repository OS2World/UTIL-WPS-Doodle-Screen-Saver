
 /***************************************************************************/

/*
 * Portions Copyright (c) 1999 GMRS Software GmbH
 * Carl-von-Linde-Str. 38, D-85716 Unterschleissheim, http://www.gmrs.de
 * All rights reserved.
 *
 * Author: Arno Unkrig <arno@unkrig.de>
 */
 
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License in the file COPYING for more details.
 */

 /***************************************************************************/

/*
 * Changes to version 1.2.2 were made by Martin Bayer <mbayer@zedat.fu-berlin.de>
 * Dates and reasons of modifications:
 * Fre Jun  8 19:00:26 CEST 2001: new image handling
 * Thu Oct  4 21:42:24 CEST 2001: ported to g++ 3.0, bugfix for '-' as synonym for STDIN
 * Mon Jul 22 13:48:26 CEST 2002: Made finaly reading from STDIN work.
 * Sat Sep 14 15:04:09 CEST 2002: Added plain ASCII output patch by Bela Lubkin
 * Wed Jul  2 22:08:45 CEST 2003: ported to g++ 3.3
 */
  
 /***************************************************************************/


#include <iostream>
#include <strstream>
#include <string>
#include <stdlib.h>

#include <emx/startup.h>

#include "html.h"
#include "HTMLControl.h"
#include "format.h"
#include "html2text.h"

/* ------------------------------------------------------------------------- */

int use_iso8859 = 1;

class LocalParser : public HTMLControl {

public:
    LocalParser(std::istream &is_, std::ostream &os_, int width_) :
        HTMLControl(is_, false, false),
        os(os_),
        width(width_)
    {}

private:
  /*virtual*/ void yyerror(char *);
  /*virtual*/ void process(const Document &);

  ostream &os;
  int     width;
};

/*virtual*/ void
LocalParser::yyerror(char *p)
{
}

/*virtual*/ void
LocalParser::process(const Document &document)
{
    document.format(/*indent_left*/ 0, width, Area::LEFT, os);
}



__declspec(dllexport) char * _System html2text_convert(char *input, int length, int width,
                                                       int use_backspaces_, int use_iso8859_)
{
    std::strstream input_stream(input, length, std::ios_base::in | std::ios_base::out | std::ios_base::app);
    std::strstream output_stream;

    Area::use_backspaces = use_backspaces_;
    use_iso8859 = use_iso8859_;

    LocalParser parser(input_stream, output_stream, width);
    if (parser.yyparse()!=0)
        return NULL;

    output_stream << "\0";

    char *result = (char *) malloc(output_stream.pcount()+1);
    if (result)
    {
      memset(result, 0, output_stream.pcount()+1);
      strncpy(result, output_stream.str(), output_stream.pcount());
    }
    return result;
}

__declspec(dllexport) void   _System html2text_free(char *output)
{
    if (output)
        free(output);
}


#if 0

extern "C" {

unsigned long _System
_DLL_InitTerm (unsigned long hModule,
               unsigned long termination)
{
  if (termination)
  {
    /* Unloading the DLL */

    /* Uninitialize RTL of GCC */
    __ctordtorTerm ();
    _CRT_term ();

    return 1;
  } else
  {
    /* Loading the DLL */

    /* Initialize RTL of GCC */
    if (_CRT_init () != 0)
      return 0;
    __ctordtorInit ();

    return 1;
  }
}

}

#endif

#if 0
int main(int argc, char **argv)
{
    char *output;
    char *input = \
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\">\n" \
"<html>\n" \
"<head>\n" \
"<title>About</title>\n" \
"<meta http-equiv=Content-Type content=\"text/html; charset=iso-8859-1\">\n" \
"</head>\n" \
"<body lang=\"EN-US\">\n" \
"<h2>About This Content</h2>\n" \
" \n" \
"<p>17 August, 2004</p>\n" \
"<h3>License</h3>\n" \
"<p>Eclipse.org makes available all content in this plug-in (&quot;Content&quot;).  With the exception of the files listed below, all Content should be defined as the &quot;SWT&quot; and\n" \
"is provided to you under the terms and conditions of the Common Public License Version 1.0 (&quot;CPL&quot;).\n" \
"A copy of the CPL is available at <a href=\"http://www.eclipse.org/legal/cpl-v10.html\">http://www.eclipse.org/legal/cpl-v10.html</a>.\n" \
"For purposes of the CPL, &quot;Program&quot; shall mean the SWT.</p>\n" \
" \n" \
"<h3>Third Party Content</h3>\n" \
" \n" \
"<h4>GTK+ Binding</h4>\n" \
" \n" \
"<p>The following files shall be defined as the &quot;GTK+ Binding&quot;:\n" \
"<ul>\n" \
"        <li>os/linux/x86/libswt-atk-gtk-3106.so</li>\n" \
"        <li>os/linux/x86/libswt-pi-gtk-3106.so</li>\n" \
"        <li>ws/gtk/swt-pi.jar</li>\n" \
"        <li>ws/gtk/swt-pisrc.zip</li>\n" \
"</ul>\n" \
" \n" \
"<p>The GTK+ Binding contains portions of GTK+ (&quot;Library&quot;).  GTK+ is made available by The Free Software Foundation.  Use of the Library is governed by the terms and\n" \
"conditions of the GNU Lesser General Public License Version 2.1 (&quot;LGPL&quot;).  Use of the GTK+ Binding on a standalone\n" \
"basis, is also governed by the terms and conditions of the LGPL.  A copy of the LGPL is provided with the Content and is also available at\n" \
"<a href=\"http://www.gnu.org/licenses/lgpl.html\">http://www.gnu.org/licenses/lgpl.html</a>.</p>\n" \
" \n" \
"<p>In accordance with Section 6 of the LGPL, you may combine or link a \"work that uses the Library\" (e.g. the SWT) with the Library to produce a work\n" \
"containing portions of the Library (e.g. the GTK+ Binding) and distribute that work under the terms of your choice (e.g. the CPL) provided you comply with all\n" \
"other terms and conditions of Section 6 as well as other Sections of the LGPL.  Please note, if you modify the GTK+ Binding such modifications shall be\n" \
"governed by the terms and conditions of the LGPL.  Also note, the terms of the CPL permit you to modify the combined work and the source code of the combined\n" \
"work is provided for debugging purposes so there is no need to reverse engineer the combined work.</p>\n" \
" \n" \
"<h4>Mozilla Binding</h4>\n" \
" \n" \
"<p>The following files shall be defined as the &quot;Mozilla Binding&quot;:\n" \
"<ul>\n" \
"        <li>os/linux/x86/libswt-mozilla-gtk-3106.so</li>\n" \
"        <li>ws/gtk/swt-mozilla.jar</li>\n" \
"        <li>ws/gtk/swt-mozillasrc.zip</li>\n" \
"</ul>\n" \
" \n" \
"<p>The Mozilla Binding contains portions of Mozilla (&quot;Mozilla&quot;).  Mozilla is made available by Mozilla.org.  Use of Mozilla is governed by the terms and\n" \
"conditions of the Mozilla Public License Version 1.1 (&quot;MPL&quot;).  A copy of the MPL is provided with the Content and is also available at\n" \
"<a href=\"http://www.mozilla.org/MPL/MPL-1.1.html\">http://www.mozilla.org/MPL/MPL-1.1.html</a>.</p>\n" \
" \n" \
"<h3>Contributions</h3>\n" \
" \n" \
"<p>If you wish to provide Contributions related to the SWT, such Contributions shall be made under the terms of the CPL.  If you wish to make\n" \
"Contributions related to the GTK+ Binding such Contributions shall be made under the terms of the LGPL and the CPL (with respect to portions\n" \
"of the contribution for which you are the copyright holder).  If you wish to make Contributions related to the Mozilla Binding such\n" \
"Contributions shall be made under the terms of the MPL.</p>\n" \
"</body>\n" \
"</html>\n";

        printf("Input:\n%s\n\n", input);

        output = html2text_convert(input, strlen(input), 80, false, false);
        if (output)
        {
            printf("Output:\n%s\n\n", output);
            html2text_free(output);
        }

        printf("Done.\n");
        return 0;
}

#endif

/* ------------------------------------------------------------------------- */
