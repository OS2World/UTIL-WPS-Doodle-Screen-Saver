/*
 * Screen Saver - Lockup Desktop replacement for OS/2 and eComStation systems
 * Copyright (C) 2004-2008 Doodle
 *
 * Contact: doodle@dont.spam.me.scenergy.dfmk.hu
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#define OUTPUT_FILENAME  "Snow-Images.h"
#define OUTPUT_FILEDEF   "__SNOWIMAGES_H__"
#define NUM_IMAGES    2
char * apchImageNames[NUM_IMAGES] =
{
  "Objects1",
  "Anims1"
};




// Definition of PCX header structure:
#pragma pack(push, 1)
typedef struct PCXHeader_s
{
  unsigned char Manufacturer;
  unsigned char Version;
  unsigned char Encoding;
  unsigned char BitsPerPixel;
  short int Window[4];
  short int HDpi;
  short int VDpi;
  unsigned char Colormap[3*16];
  unsigned char Reserved;
  unsigned char NPlanes;
  short int BytesPerLine;
  short int PaletteInfo;
  short int HscreenSize;
  short int VscreenSize;
  unsigned char Filler[54];
} PCXHeader_t;
#pragma pack(pop)


typedef struct Image_s
{
  unsigned char *pImageData;
  unsigned long ulImageSize;
  unsigned long ulWidth;
  unsigned long ulHeight;
} Image_t, *Image_p;


//---------------------------------------------------------------------
// LoadPCX
//
// Loads a truecolor PCX, and returns an Image_p to the uncompressed
// image. Only for 24bits PCX images!
//---------------------------------------------------------------------
Image_p LoadPCX(char *filename)
{
  Image_p result;
  FILE *PCXFile;
  PCXHeader_t Header;
  long linesize;
  long count;
  unsigned char *linebuf, *imgypos;
  unsigned char *dst;
  long y, i;
  unsigned char b1, b2;
  unsigned char *filedata;
  long VirtFilePos;
  long filesize;

  result = (Image_p) malloc(sizeof(Image_t));

  if (result)
  {
    if ((PCXFile=fopen(filename, "rb"))!=NULL)
    { // If we could open the file:
      // Read the file into memory!
      fseek(PCXFile, 0, SEEK_END);
      filesize=ftell(PCXFile);
      fseek(PCXFile, 0, SEEK_SET);
      
      filedata = (unsigned char *)malloc(filesize);
      if (filedata)
      {
        if (fread(filedata, 1, filesize, PCXFile)!=filesize)
	{ // Error!
          free(filedata);
          free(result); // Error!
          result=NULL;
          fclose(PCXFile);
          return result;
	};
	fclose(PCXFile);

      	// Ok, we have the whole file in memory, so
	// we can process it here!
	VirtFilePos=0;

	memcpy(&Header, filedata, sizeof(Header));
	VirtFilePos+=sizeof(Header);

	result->ulWidth=Header.Window[2]-Header.Window[0]+1;
	result->ulHeight=Header.Window[3]-Header.Window[1]+1;

	linesize=Header.NPlanes*Header.BytesPerLine;

	if ( (!(result->ulWidth)) || (!(result->ulHeight)) ||
	     (Header.BitsPerPixel!=8) || (Header.NPlanes!=3) )
	{ // This is not a 24bits PCX file, so we quit!
          free(filedata);
          free(result); // Error!
          result=NULL;
          return result;
	}
	// Get memory for image data (BGR)
	result->ulImageSize = result->ulWidth * result->ulHeight *3;
	result->pImageData = (unsigned char *) malloc(result->ulImageSize);
	if (!result->pImageData)
	{ // Could not allocate memory!
          free(filedata);
          free(result); // Error!
          result=NULL;
          return result;
	}

	imgypos = result->pImageData;
	linebuf = (unsigned char *)malloc(linesize+4);
	if (!linebuf)
	{
          free(result->pImageData);
          free(filedata);
          free(result); // Error!
          result=NULL;
          return result;
	}
	// Decode PCX planes!
      
	y=0;
	while (y<result->ulHeight)
	{
	  dst=linebuf;
	  count=0;
	  do
	  {
	    b1=filedata[VirtFilePos++]; // Get one byte!
	    if ((b1 & 0xC0) == 0xC0)              // Is it a lot of bytes?
	    {
	      b1=(b1 & 0x3f);             // Get number of bytes
	      b2=filedata[VirtFilePos++]; // and the color
	      memset(dst, b2, b1);
	      dst+=b1;
	      count+=b1;
	    } else
	    {                           // It's just one color value
	      *dst=b1;
	      dst++;
	      count++;
	    }
	  } while (count<linesize);
	  // Now we have one line decoded in linebuf.
	  // Let's store it in image! (in BGR format)

	  for (i=0; i<result->ulWidth; i++)
	  {
	    imgypos[i*3+2] = linebuf[i];                   //R
	    imgypos[i*3+1] = linebuf[(result->ulWidth)+i];   //G
            imgypos[i*3  ] = linebuf[2*(result->ulWidth)+i]; //B
	  }

	  y++;
          imgypos+=linesize;
	}

        free(linebuf);
	free(filedata);
      } else
      {
	free(result); // Error!
        result=NULL;
	fclose(PCXFile);
        return result;
      }
    } else
    { // If we could not open the file:
      free(result); // Error!
      result=NULL;
    }
  }

  return result;
}

void CleanupImage(Image_p Image)
{
  if (!Image) return;

  free(Image->pImageData);
  free(Image);
}



int main(int argc, char *argv[])
{
  FILE *hFile;
  int i;
  unsigned long ulTemp;
  Image_p pImage;
  char achTemp[1024];

  printf("* Creating data file...\n");
  hFile = fopen(OUTPUT_FILENAME, "wt");
  if (!hFile)
  {
    printf("! Error: Could not create output file: [%s]\n", OUTPUT_FILENAME);
    return 1;
  }

  fprintf(hFile, "#ifndef %s\n", OUTPUT_FILEDEF);
  fprintf(hFile, "#define %s\n", OUTPUT_FILEDEF);
  fprintf(hFile, "\n");

  fprintf(hFile, "/*\n");
  fprintf(hFile, " * This file contains the following images:\n");
  for (i=0; i<NUM_IMAGES; i++)
  {
    sprintf(achTemp,"Images\\%s.pcx", apchImageNames[i]);
    fprintf(hFile, " * %s\n", achTemp);
  }
  fprintf(hFile, " */\n");
  fprintf(hFile, "\n");

  for (i=0; i<NUM_IMAGES; i++)
  {
    sprintf(achTemp,"Images\\%s.pcx", apchImageNames[i]);

    printf("* Processing file: %s\n", achTemp);

    pImage = LoadPCX(achTemp);
    if (!pImage)
    {
      printf("! Error: Could not load image file: [%s]\n", achTemp);
      fclose(hFile);
      remove(OUTPUT_FILENAME);
      return 1;
    }

    fprintf(hFile, "/* Image file: %s */\n", achTemp);
    fprintf(hFile, "unsigned long ul%sImageWidth   = %d;\n", apchImageNames[i], pImage->ulWidth);
    fprintf(hFile, "unsigned long ul%sImageHeight  = %d;\n", apchImageNames[i], pImage->ulHeight);
    fprintf(hFile, "unsigned char *pch%sConvertedImageData;\n  ", apchImageNames[i]);
    fprintf(hFile, "unsigned char ach%sImageData[] = {\n  ", apchImageNames[i]);
    for (ulTemp=0; ulTemp<pImage->ulImageSize; ulTemp++)
    {
      if (ulTemp!=0)
      {
        fprintf(hFile, ", ");
        if (ulTemp%16 == 0)
          fprintf(hFile, " /* 0x%08x ... 0x%08x */\n  ", ulTemp-16, ulTemp-1);
      }
      fprintf(hFile, "0x%02x", pImage->pImageData[ulTemp]);
    }

    fprintf(hFile, "\n};\n", apchImageNames[i]);
    fprintf(hFile, "\n");

    CleanupImage(pImage);
  }

  fprintf(hFile, "#endif\n");
  fclose(hFile);

  printf("* All done, bye!\n");

  return 0;
}
