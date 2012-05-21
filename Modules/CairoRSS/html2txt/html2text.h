
#ifndef __HTML2TEXT_H__
#define __HTML2TEXT_H__

char * _System html2text_convert(char *input, int length, int width,
                                 int use_backspaces_, int use_iso8859_);
void   _System html2text_free(char *output);

#endif // __HTML2TEXT_H__
