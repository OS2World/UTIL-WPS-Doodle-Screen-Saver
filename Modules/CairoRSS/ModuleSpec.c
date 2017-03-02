/*
 * Screen Saver - Lockup Desktop replacement for OS/2 and eComStation systems
 * Copyright (C) 2004-2006 Doodle
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
#include <process.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <types.h>
#define INCL_DOS
#define INCL_WIN
#include <os2.h>

#include "ModuleSpec.h"


#define CURL_HIDDEN_SYMBOLS
#define BUILDING_LIBCURL
#define CURL_EXTERN_SYMBOL __declspec(__cdecl)
#include <curl/curl.h>
//#include <curl/types.h>
#include <curl/easy.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "html2txt/html2text.h"

#ifndef M_PI
#define M_PI 3.14159
#endif

/* ------------------------------------------------------------------------*/

/*
 * The next some lines has to be changed according to the module
 * you want to create!
 */

/* Short module name: */
char *pchModuleName = "Cairo RSS";

/* Module description string: */
char *pchModuleDesc =
  "RSS news visualizer.\n"
  "Using Cairo, Curl, libxml2 and html2text.\n";

/* Module version number (Major.Minor format): */
int   iModuleVersionMajor = 2;
int   iModuleVersionMinor = 10;
int   iUniSize;
double  dXAspect, dYAspect;

unsigned long ulQueueCount=0;

/* Internal name generated from module name, to have
 * private window identifier for this kind of windows: */
char *pchSaverWindowClassName = "CairoRSSWindowClass";

char achRSSURL[1024];

/* ------------------------------------------------------------------------*/

/* Some global variables, modified from outside: */
int bPauseDrawing = 0;
int bShutdownDrawing = 0;

typedef struct NewsItem_s
{
  char *pchItemTitle;
  char *pchItemDescription;
  char *pchItemLink;

  struct NewsItem_s *pNext;
} NewsItem_t, *NewsItem_p;

NewsItem_p pNewsListHead;

typedef struct Downloaded_s
{
  char *pBuffer;
  unsigned int uiSize;
} Downloaded_t, *Downloaded_p;


size_t __cdecl WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  Downloaded_p mem = (Downloaded_p) data;

  mem->pBuffer = (char *)realloc(mem->pBuffer, mem->uiSize + realsize + 1);
  if (mem->pBuffer)
  {
    memcpy(&(mem->pBuffer[mem->uiSize]), ptr, realsize);
    mem->uiSize += realsize;
    mem->pBuffer[mem->uiSize] = 0;
  }
  return realsize;
}


void FormatHtml(char **ppchDestination,
                char *pchSource, int iWidth)
{
  char *pchTemp;

  pchTemp = html2text_convert(pchSource, strlen(pchSource), iWidth, 0, 1);
  if (pchTemp)
  {
    int len;
    while (((len=strlen(pchTemp))>0) &&
           ((pchTemp[len-1] == '\r') || (pchTemp[len-1] == '\n'))
          )
      pchTemp[len-1]=0;

    *ppchDestination = strdup(pchTemp);

    html2text_free(pchTemp);
  }
}

void WalkXmlTree(xmlNode *pNode,
                 char *pPositionBuffer,
                 unsigned int uiPositionBufferSize,
                 NewsItem_p *ppNewsListHead,
                 NewsItem_p pCurrentNewsItem)
{
  unsigned int uiOldPosition;
  xmlNode *pCurrentNode = NULL;
  NewsItem_p pNewItem = NULL;

  for (pCurrentNode = pNode; pCurrentNode!=NULL; pCurrentNode = pCurrentNode->next)
  {
    uiOldPosition = strlen(pPositionBuffer);
    if (pCurrentNode->name)
    {
      strncat(pPositionBuffer, "/", uiPositionBufferSize);
      strncat(pPositionBuffer, pCurrentNode->name, uiPositionBufferSize);
    }

    if (pCurrentNode->type == XML_ELEMENT_NODE)
    {
      if ((strcmp(pPositionBuffer, "/rss/channel/item")==0) && (!pCurrentNewsItem))
      {
        // One news item starts here!
        pNewItem = malloc(sizeof(NewsItem_t));
        if (pNewItem)
        {
          memset(pNewItem, 0, sizeof(NewsItem_t));
          pCurrentNewsItem = pNewItem;
        }
      }
    }

    if (pCurrentNode->type == XML_TEXT_NODE)
    {
      if ((strcmp(pPositionBuffer, "/rss/channel/item/title/text")==0) &&
          (pCurrentNewsItem))
      {
        if (pCurrentNewsItem->pchItemTitle)
        {
          free(pCurrentNewsItem->pchItemTitle);
          pCurrentNewsItem->pchItemTitle = NULL;
        }
        if (pCurrentNode->content)
          FormatHtml(&(pCurrentNewsItem->pchItemTitle), pCurrentNode->content, 50);
      }

      if ((strcmp(pPositionBuffer, "/rss/channel/item/description/text")==0) &&
          (pCurrentNewsItem))
      {
        if (pCurrentNewsItem->pchItemDescription)
        {
          free(pCurrentNewsItem->pchItemDescription);
          pCurrentNewsItem->pchItemDescription = NULL;
        }
        if (pCurrentNode->content)
          FormatHtml(&(pCurrentNewsItem->pchItemDescription), pCurrentNode->content, 50);
      }

      if ((strcmp(pPositionBuffer, "/rss/channel/item/link/text")==0) &&
          (pCurrentNewsItem))
      {
        if (pCurrentNewsItem->pchItemLink)
        {
          free(pCurrentNewsItem->pchItemLink);
          pCurrentNewsItem->pchItemLink = NULL;
        }
        if (pCurrentNode->content)
          FormatHtml(&(pCurrentNewsItem->pchItemLink), pCurrentNode->content, 80);
      }
    }

    WalkXmlTree(pCurrentNode->children, pPositionBuffer, uiPositionBufferSize, ppNewsListHead, pCurrentNewsItem);

    if (pNewItem)
    {
      NewsItem_p pLastItem = *ppNewsListHead;

      // Add missing stuffs
      if (pNewItem->pchItemLink == NULL)
        pNewItem->pchItemLink = strdup("");

      if (pNewItem->pchItemTitle == NULL)
        pNewItem->pchItemTitle = strdup("");

      if (pNewItem->pchItemDescription == NULL)
        pNewItem->pchItemDescription = strdup("");

      while ((pLastItem) && (pLastItem->pNext))
        pLastItem = pLastItem->pNext;

      if (pLastItem)
      {
        pLastItem->pNext = pNewItem;
      } else
      {
        *ppNewsListHead = pNewItem;
      }

      pLastItem = pNewItem;

      pNewItem = NULL;
      pCurrentNewsItem = NULL;
    }

    pPositionBuffer[uiOldPosition] = 0;
  }

}

void DownloadXML(char *pchURL, NewsItem_p *ppNewsListHead)
{
  Downloaded_t DownloadedXML;
  CURL *curl_handle;
  xmlDocPtr doc;
  int rc;

  *ppNewsListHead = NULL;
  doc = NULL;

  DownloadedXML.uiSize = 0;
  DownloadedXML.pBuffer = NULL;

  curl_global_init(CURL_GLOBAL_ALL);
  curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_URL, pchURL);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &DownloadedXML);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  rc = curl_easy_perform(curl_handle);
  curl_easy_cleanup(curl_handle);

  if (rc==0)
  {
    doc = xmlReadMemory(DownloadedXML.pBuffer, DownloadedXML.uiSize, "noname.xml", NULL, 0);
  }

  if (DownloadedXML.pBuffer)
    free(DownloadedXML.pBuffer);

  if (doc)
  {
    char achPositionBuffer[1024];
    // Parse the XML and build the channel list
    achPositionBuffer[0] = 0;
    WalkXmlTree(xmlDocGetRootElement(doc),
                achPositionBuffer,
                sizeof(achPositionBuffer),
                ppNewsListHead, NULL);
    xmlFreeDoc(doc);
  }

  xmlCleanupParser();

}

void FreeNewsData(NewsItem_p *ppNewsListHead)
{
  NewsItem_p pToDelete;

  while (*ppNewsListHead)
  {
    pToDelete = *ppNewsListHead;
    *ppNewsListHead = (*ppNewsListHead)->pNext;

    if (pToDelete->pchItemTitle)
      free(pToDelete->pchItemTitle);
    if (pToDelete->pchItemDescription)
      free(pToDelete->pchItemDescription);
    if (pToDelete->pchItemLink)
      free(pToDelete->pchItemLink);

    free(pToDelete);
  }
}

void ShowInfo(char *pchText, char *pchText2,
              HWND hwndClientWindow,
              cairo_surface_t *pCairoSurface,
              cairo_t         *pCairoHandle)
{
  cairo_text_extents_t CairoTextExtents;
  cairo_text_extents_t CairoTextExtents2;
  cairo_pattern_t *pattern1;

  cairo_set_operator(pCairoHandle, CAIRO_OPERATOR_OVER);

  // Set random line width
  cairo_set_line_width(pCairoHandle, 0.03);

  cairo_select_font_face(pCairoHandle, "Sans",
                         0, 0);
  cairo_set_font_size(pCairoHandle,
                      0.05);

  // Get how big the text will be!
  cairo_text_extents(pCairoHandle,
                     pchText,
                     &CairoTextExtents);
  cairo_text_extents(pCairoHandle,
                     pchText2,
                     &CairoTextExtents2);

  // Draw a frame
  pattern1 = cairo_pattern_create_linear(0.3,0,0.7,1);
  cairo_pattern_add_color_stop_rgba(pattern1, 0,      0,   0,   0,     1);
  cairo_pattern_add_color_stop_rgba(pattern1, 0.5,    0, 0.5,   0,     1);
  cairo_pattern_add_color_stop_rgba(pattern1, 1,    0.3,   1, 0.2,     1);

  //cairo_set_source_rgba(pCairoHandle, 0.2, 0.8 ,0, 0.9);
  cairo_set_source(pCairoHandle, pattern1);
  cairo_rectangle(pCairoHandle,
                  0.05 * dXAspect,
                  0.25*dYAspect,
                  0.90 * dXAspect,
                  0.5*dYAspect);
  cairo_fill_preserve(pCairoHandle);
  cairo_set_source_rgba(pCairoHandle,
                        0.9, 0.5, 0.2,
                        0.7);
  cairo_stroke(pCairoHandle);
  cairo_pattern_destroy(pattern1);

  pattern1 = cairo_pattern_create_linear(0,0,1,1);
  cairo_pattern_add_color_stop_rgba(pattern1, 0,    0.1, 0.7, 0.3,   0.4);
  cairo_pattern_add_color_stop_rgba(pattern1, 0.2,  0.3, 0.2, 0.4,   0.6);
  cairo_pattern_add_color_stop_rgba(pattern1, 0.4,  0.1, 0.1, 0.8,   0.3);
  cairo_pattern_add_color_stop_rgba(pattern1, 0.6,    0, 0.5, 0.7,   0.4);
  cairo_pattern_add_color_stop_rgba(pattern1, 0.8,  0.4, 0.5, 0.2,   0.6);
  cairo_pattern_add_color_stop_rgba(pattern1, 1,    0.1,   1, 0.2,   0.3);

  //cairo_set_source_rgba(pCairoHandle, 0.2, 0.8 ,0, 0.9);
  cairo_set_source(pCairoHandle, pattern1);
  cairo_rectangle(pCairoHandle,
                  0.05 * dXAspect,
                  0.25*dYAspect,
                  0.90 * dXAspect,
                  0.5*dYAspect);
  cairo_fill(pCairoHandle);
  cairo_pattern_destroy(pattern1);

  // Print the text, centered!
  cairo_set_source_rgba(pCairoHandle,
                        1, 1, 1,
                        0.8);

  cairo_move_to(pCairoHandle,
                (0.5*dXAspect - CairoTextExtents.width/2),
                (0.5*dYAspect - CairoTextExtents.height));
  cairo_show_text(pCairoHandle, pchText);
  cairo_stroke (pCairoHandle);

  // Print the text, centered!
  cairo_move_to(pCairoHandle,
                0.1 * dXAspect,
                (0.5*dYAspect + CairoTextExtents2.height));
  cairo_scale(pCairoHandle, 0.8*dXAspect/CairoTextExtents2.width, 1);
  cairo_show_text(pCairoHandle, pchText2);
  cairo_stroke (pCairoHandle);
}

void QueryMultilineTextExtents(cairo_t *pCairoHandle, char *pchMLText,
                               double *pdWidth, double *pdHeight)
{
  char *pchText;
  cairo_text_extents_t CairoTextExtents;

  *pdWidth = 0;
  *pdHeight = 0;

  pchText = malloc(strlen(pchMLText)+1);
  if (pchText)
  {
    char *pchStart;
    char *pchEnd;
    int bIsEnd;

    strcpy(pchText, pchMLText);
    pchStart = pchEnd = pchText;

    while (*pchEnd)
    {
      while ((*pchEnd) && (*pchEnd!='\n'))
        pchEnd++;

      bIsEnd = (*pchEnd==NULL);
      *pchEnd = 0;

      cairo_text_extents(pCairoHandle,
                         pchStart,
                         &CairoTextExtents);

      if (*pdWidth < CairoTextExtents.width)
        *pdWidth = CairoTextExtents.width;

      *pdHeight += CairoTextExtents.height + 0.01;

      pchStart = pchEnd+1;
      if (!bIsEnd)
        pchEnd++;
    }

    free(pchText);
  }
}

void ShowMultilineText(cairo_t *pCairoHandle, char *pchMLText,
                       double dXPos,
                       double dYPos)
{
  char *pchText;
  cairo_text_extents_t CairoTextExtents;

  pchText = malloc(strlen(pchMLText)+1);
  if (pchText)
  {
    char *pchStart;
    char *pchEnd;
    int bIsEnd;

    strcpy(pchText, pchMLText);
    pchStart = pchEnd = pchText;

    while (*pchEnd)
    {
      while ((*pchEnd) && (*pchEnd!='\n'))
        pchEnd++;

      bIsEnd = (*pchEnd==NULL);
      *pchEnd = 0;

      cairo_text_extents(pCairoHandle,
                         pchStart,
                         &CairoTextExtents);

      cairo_move_to(pCairoHandle,
                    dXPos,
                    dYPos);
      dYPos += CairoTextExtents.height + 0.01;
      cairo_show_text(pCairoHandle, pchStart);

      pchStart = pchEnd+1;
      if (!bIsEnd)
        pchEnd++;
    }

    free(pchText);
  }
}

void VisualizeXML(NewsItem_p pNewsListHead,
                  cairo_t         *pCairoHandle)
{
  double dLinkWidth, dLinkHeight;
  double dWidth, dHeight;
  double dTitleWidth, dTitleHeight;
  // TODO

  cairo_set_operator(pCairoHandle, CAIRO_OPERATOR_OVER);

  // Query item link width
  cairo_select_font_face(pCairoHandle, "Helv",
                         1, 0.2);
  cairo_set_font_size(pCairoHandle,
                      0.03);
  cairo_set_source_rgba(pCairoHandle,
                        0.6, 0.6, 0.9,
                        0.8);

  QueryMultilineTextExtents(pCairoHandle,
                            pNewsListHead->pchItemLink,
                            &dLinkWidth, &dLinkHeight);


  // Show the multiline text, centered!
  cairo_select_font_face(pCairoHandle, "Helv",
                         0, 0);
  cairo_set_font_size(pCairoHandle,
                      0.05);
  cairo_set_source_rgba(pCairoHandle,
                        1, 1, 1,
                        1);

  QueryMultilineTextExtents(pCairoHandle,
                            pNewsListHead->pchItemDescription,
                            &dWidth, &dHeight);
  if (dWidth<dLinkWidth)
    dWidth = dLinkWidth;
  ShowMultilineText(pCairoHandle,
                    pNewsListHead->pchItemDescription,
                    (1*dXAspect-dWidth)/2, (1*dYAspect-dHeight)/2 );

  cairo_stroke (pCairoHandle);

  // Show the item title above it!
  cairo_select_font_face(pCairoHandle, "Helv",
                         1, 1);
  cairo_set_font_size(pCairoHandle,
                      0.055);
  cairo_set_source_rgba(pCairoHandle,
                        1, 1, 0.2,
                        0.9);
  QueryMultilineTextExtents(pCairoHandle,
                            pNewsListHead->pchItemTitle,
                            &dTitleWidth, &dTitleHeight);
  ShowMultilineText(pCairoHandle,
                    pNewsListHead->pchItemTitle,
                    (1*dXAspect-dTitleWidth)/2, (1*dYAspect-dHeight)/2 - 0.02 - dTitleHeight);

  cairo_stroke (pCairoHandle);

  // Show the item link under it!
  cairo_select_font_face(pCairoHandle, "Helv",
                         1, 0.2);
  cairo_set_font_size(pCairoHandle,
                      0.03);
  cairo_set_source_rgba(pCairoHandle,
                        0.6, 0.6, 0.9,
                        0.8);

  QueryMultilineTextExtents(pCairoHandle,
                            pNewsListHead->pchItemLink,
                            &dLinkWidth, &dLinkHeight);

  ShowMultilineText(pCairoHandle,
                    pNewsListHead->pchItemLink,
                    (1*dXAspect-dWidth)/2, (1*dYAspect-dHeight)/2 + 0.02 + dHeight);

  cairo_stroke (pCairoHandle);

}


/* And the real drawing loop: */
void CairoDrawLoop(HWND             hwndClientWindow,
                   int              iWidth,
                   int              iHeight,
                   cairo_surface_t *pCairoSurface,
                   cairo_t         *pCairoHandle)
{
  double dRandMax;
  unsigned int uiLoopCounter = 0;
  NewsItem_p pCurrItem;

  // Initialize random number generator
  srand(clock());
  dRandMax = RAND_MAX;
  // Clear background with black
  cairo_set_source_rgb (pCairoHandle, 0, 0 ,0);
  cairo_rectangle (pCairoHandle,
                   0, 0,
                   iWidth, iHeight);
  cairo_fill(pCairoHandle);

  // Calculate screen aspect ratio
  if (iWidth<iHeight)
  {
    iUniSize = iWidth;
    dYAspect = iHeight;
    dYAspect /= iWidth;
    dXAspect = 1;
  }
  else
  {
    iUniSize = iHeight;
    dXAspect = iWidth;
    dXAspect /= iHeight;
    dYAspect = 1;
  }

  // --------------------------------
  // Show info that we're downloading the feed

  cairo_save(pCairoHandle);
  cairo_scale(pCairoHandle, iUniSize, iUniSize);


  ShowInfo("Downloading",
           achRSSURL,
           hwndClientWindow,
           pCairoSurface, pCairoHandle);

  cairo_restore(pCairoHandle);

  WinInvalidateRect(hwndClientWindow, NULL, TRUE);

  // --------------------------------
  // Download the URL and parse it with libXML
  pNewsListHead = NULL;
  DownloadXML(achRSSURL, &pNewsListHead);
  pCurrItem = pNewsListHead;
  // --------------------------------

  // Do the main drawing loop as long as needed!
  while (!bShutdownDrawing)
  {
    if (bPauseDrawing)
    {
      // Do not draw anything if we're paused!
      DosSleep(250);
    }
    else
    {
      // Otherwise draw something!

      if (!pNewsListHead)
      {
        if (uiLoopCounter==0)
        {
          // Save cairo canvas state
          cairo_save(pCairoHandle);
  
          // Scale canvas so we'll have a
          // normalized coordinate system of (0;0) -> (1;1)
          cairo_scale(pCairoHandle, iUniSize, iUniSize);
  
          // Clear background with black
          cairo_set_source_rgb(pCairoHandle, 0, 0 ,0);
          cairo_rectangle(pCairoHandle,
                          0, 0,
                          dXAspect, dYAspect);
          cairo_fill(pCairoHandle);
  

          ShowInfo("Error downloading",
                   achRSSURL,
                   hwndClientWindow,
                   pCairoSurface, pCairoHandle);

          // Restore canvas state to original one
          cairo_restore(pCairoHandle);
          WinInvalidateRect(hwndClientWindow, NULL, TRUE);
        } else
        if (uiLoopCounter==10)
        {
          // Save cairo canvas state
          cairo_save(pCairoHandle);
  
          // Scale canvas so we'll have a
          // normalized coordinate system of (0;0) -> (1;1)
          cairo_scale(pCairoHandle, iUniSize, iUniSize);
  
          // Clear background with black
          cairo_set_source_rgb(pCairoHandle, 0, 0 ,0);
          cairo_rectangle(pCairoHandle,
                          0, 0,
                          dXAspect, dYAspect);
          cairo_fill(pCairoHandle);

          cairo_os2_surface_paint_window(pCairoSurface,
                                         NULL,
                                         NULL,
                                         1);


          // Restore canvas state to original one
          cairo_restore(pCairoHandle);
          WinInvalidateRect(hwndClientWindow, NULL, TRUE);
        }

        if (uiLoopCounter<20)
          uiLoopCounter++;
        else
          uiLoopCounter = 0;

      } else
      {
        if (uiLoopCounter % 150 == 0)
        {
          // Save cairo canvas state
          cairo_save(pCairoHandle);

          // Scale canvas so we'll have a
          // normalized coordinate system of (0;0) -> (1;1)
          cairo_scale(pCairoHandle, iUniSize, iUniSize);

          // Clear background with black
          cairo_set_source_rgb(pCairoHandle, 0, 0 ,0);
          cairo_rectangle(pCairoHandle,
                          0, 0,
                          dXAspect, dYAspect);
          cairo_fill(pCairoHandle);

          if (pCurrItem)
            VisualizeXML(pCurrItem,
                         pCairoHandle);

          // Restore canvas state to original one
          cairo_restore(pCairoHandle);
          WinInvalidateRect(hwndClientWindow, NULL, TRUE);

          // Go for next news item then.
          if (pCurrItem)
          {
            pCurrItem = pCurrItem->pNext;
            if (!pCurrItem)
              pCurrItem = pNewsListHead;
          }
        }

        uiLoopCounter++;
      }
    }


    DosSleep(30); // Wait some
  }
  // Got shutdown event, so stopped drawing. That's all.

  // Clean up
  FreeNewsData(&pNewsListHead);
}

/* ------------------------------------------------------------------------*/
/* End of file */
