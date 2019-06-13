//this file is part of notepad++
//Copyright (C)2003 Don HO <donho@altern.org>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "menuCmdID.h"

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// cpnfig variable
//
BOOL bCopySelected = FALSE;
// set flags according conf
#define VIZMENUFLAGS (bCopySelected?SCFIND_WHOLEWORD:0)

// inicializace fci pro zmenu checku v menu konfig (funkce prehazuje 1/0)
unsigned miCopySelected; void doUpdateCopySelected() { bCopySelected = AlterMenuCheck(miCopySelected, '!'); }

struct SelPos {
	size_t selBeg;
	size_t selEnd;
};

SelPos selPos;

//
// DEFINE part
//
#define SCDS_COPYRECTANGULAR 0x4 /* Mark the text as from a rectangular selection according to the Scintilla standard */
#define SCDS_COPYAPPEND 0x8 /* Reads the old clipboard (depends on flags) and appends the new text to it */

// JD: Basic kod prevezmut z NFX
#define INT_CURRENTEDIT int currentEdit
#define GET_CURRENTEDIT SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit)
#define SENDMSGTOCED(whichEdit,mesg,wpm,lpm) SendMessage(((whichEdit)?nppData._scintillaSecondHandle:nppData._scintillaMainHandle),mesg,(WPARAM)(wpm),(LPARAM)(lpm))

CString tmpMsg; // for MessageBoxExtra format

const char eoltypes[3][4] = { ("\r\n"), ("\r"), ("\n") };
#ifndef NELEM
#define NELEM(xyzzy) (sizeof(xyzzy)/sizeof(xyzzy[0]))
#endif

UINT cfColumnSelect;

//
// JD own functions
//

/// realloc safe procedure - msg box when error
void * reallocsafe(void *bf, size_t ct)
{
	void * bf2;
	if (bf == NULL) {
		bf2 = (void *)malloc(ct);
	}
	else {
		bf2 = (void *)realloc(bf, ct);
	}
	if (bf2 == NULL) {
		MessageBox(nppData._nppHandle, _T("Err reallocsafe !!!"), _T(PLUGIN_NAME), MB_OK | MB_ICONERROR);
		return (NULL);
	}
	else {
		return(bf2);
	}
}

#define mallocsafe(ct) reallocsafe(NULL,ct)

/// Concatenate new strings with resize
bool strcatX(char**(orig), const char* concatenateText)
{
	size_t newLen = strlen(concatenateText);
	char * newArr = (char *)reallocsafe(*orig, strlen(*orig) + newLen + 1);
	if (newArr == NULL) {
		MessageBox(nppData._nppHandle, _T("Err strcatX !!!"), _T(PLUGIN_NAME), MB_OK | MB_ICONERROR);
		return false;
	}
	else {
		strcat(newArr, concatenateText);
		*orig = newArr;
	}
	return true;
}


/* =========================================== INIT BEGIN =========================================== */
//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE /*hModule*/)
{
	cfColumnSelect = RegisterClipboardFormat(_T("MSDEVColumnSelect"));

	selPos.selBeg = 0; // Set 0 as current position on init to avoid auto copy selected text on N++ start
	selPos.selEnd = 0;
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{
	//--------------------------------------------//
	//-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
	//--------------------------------------------//
	// with function :
	// setCommand(int index,                      // zero based number to indicate the order of command
	//            TCHAR *commandName,             // the command name that you want to see in plugin menu
	//            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
	//            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
	//            bool check0nInit                // optional. Make this menu item be checked visually
	//            );
	setCommand(0, TEXT("Auto copy selection to clipboard"), doUpdateCopySelected, NULL, bCopySelected != 0);
	setCommand(1, TEXT("About"), doAboutDlg, NULL, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
	IniSaveSettings(true);
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit)
{
	if (index >= nbFunc)
		return false;

	if (!pFunc)
		return false;

	lstrcpy(funcItem[index]._itemName, cmdName);
	funcItem[index]._pFunc = pFunc;
	funcItem[index]._init2Check = check0nInit;
	funcItem[index]._pShKey = sk;

	return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//

/// ulozi text do schranky
void InsertTextToClipboard(const char * strInput, unsigned flags)
{
	INT_CURRENTEDIT;
	GET_CURRENTEDIT;
	CString TextToClip = strInput;

	if (strInput != NULL) {
		// When not Codepage ANSI (codepage != CP_ACP), convert to widechar
		int intCodePage = (int)SENDMSGTOCED(currentEdit, SCI_GETCODEPAGE, 0, 0);
		if (intCodePage != CP_ACP) {
			wchar_t* bufWchar;
			int kolik = MultiByteToWideChar(CP_UTF8, 0, &strInput[0], -1, NULL, 0);
			bufWchar = new wchar_t[kolik];
			MultiByteToWideChar(CP_UTF8, 0, &strInput[0], -1, &bufWchar[0], kolik);
			TextToClip = bufWchar;
			delete[] bufWchar;
		}

		if (OpenClipboard(NULL)) {
			EmptyClipboard();

			HGLOBAL hClipboardData;
			size_t size = (TextToClip.GetLength() + 1) * sizeof(TCHAR);
			hClipboardData = GlobalAlloc(NULL, size);
			TCHAR* pchData = (TCHAR*)GlobalLock(hClipboardData);
			memcpy(pchData, LPCTSTR(TextToClip.GetString()), size);
			SetClipboardData(CF_UNICODETEXT, hClipboardData);
			if (flags&SCDS_COPYRECTANGULAR) {
				SetClipboardData(cfColumnSelect, 0);
			}
			GlobalUnlock(hClipboardData);
			CloseClipboard();
		}
	}
}

void doCopySelection()
{
	INT_CURRENTEDIT;
	GET_CURRENTEDIT;

	size_t p1 = (size_t)SENDMSGTOCED(currentEdit, SCI_GETSELECTIONSTART, 0, 0);
	size_t p2 = (size_t)SENDMSGTOCED(currentEdit, SCI_GETSELECTIONEND, 0, 0);

	if((selPos.selBeg != p1 || selPos.selEnd != p2) && p2 > p1 && selPos.selEnd != 0) CopyRoutine();
	selPos.selBeg = p1;
	selPos.selEnd = p2;
}

/// Set CString to Clipboard
bool SetClipboard(CString textToclipboard)
{
	bool success = true;

	if (OpenClipboard(NULL)) {
		EmptyClipboard();
		HGLOBAL hClipboardData;
		size_t size = (textToclipboard.GetLength() + 1) * sizeof(TCHAR);
		hClipboardData = GlobalAlloc(NULL, size);
		TCHAR* pchData = (TCHAR*)GlobalLock(hClipboardData);
		memcpy(pchData, LPCTSTR(textToclipboard.GetString()), size);
		SetClipboardData(CF_UNICODETEXT, hClipboardData);
		//SetClipboardData(cfColumnSelect, 0); // pri vkladani RECT
		GlobalUnlock(hClipboardData);
		CloseClipboard();
	}
	return success;
}

/// Set CString to Clipboard
bool SetClipboard(char* textToclipboard)
{
	CString txtToClp;
	txtToClp.Format(_T("%s"), textToclipboard);
	return SetClipboard(txtToClp);
}


/* ============= COPY selected part ============= */

/// Make Cupy/Cut with text
void CopyRoutine()
{
	INT_CURRENTEDIT;
	GET_CURRENTEDIT;

	unsigned flags;
	unsigned *lps = NULL, *lpe = NULL;
	char * charBuf = NULL;
	size_t buflen;
	bool isError = false; // to check whether an error appears

	unsigned lplen;
	long p1 = (long)SENDMSGTOCED(currentEdit, SCI_GETSELECTIONSTART, 0, 0);
	long p2 = (long)SENDMSGTOCED(currentEdit, SCI_GETSELECTIONEND, 0, 0);
	if (p1 < p2) {
		unsigned eoltype = (unsigned)SENDMSGTOCED(currentEdit, SCI_GETEOLMODE, 0, 0);
		int addEOL = 1;
		if (eoltype >= NELEM(eoltypes))
			eoltype = NELEM(eoltypes) - 1;

		if (SENDMSGTOCED(currentEdit, SCI_SELECTIONISRECTANGLE, 0, 0)) {
			flags |= SCDS_COPYRECTANGULAR;
			addEOL = 0;
		}
		else {
			flags &= ~SCDS_COPYRECTANGULAR;
		}
		unsigned blockstart = (unsigned)SENDMSGTOCED(currentEdit, SCI_LINEFROMPOSITION, p1, 0);
		unsigned blocklines = (unsigned)SENDMSGTOCED(currentEdit, SCI_LINEFROMPOSITION, p2, 0) - blockstart + 1;
		unsigned ln; for (lplen = ln = 0; ln < blocklines; ln++) {
			{
				unsigned ls; if ((unsigned)INVALID_POSITION == (ls = (long)SENDMSGTOCED(currentEdit, SCI_GETLINESELSTARTPOSITION, (blockstart + ln), 0)))
					continue;
				unsigned le; if ((unsigned)INVALID_POSITION == (le = (long)SENDMSGTOCED(currentEdit, SCI_GETLINESELENDPOSITION, (blockstart + ln), 0)))
					continue;

				if (addEOL && ln + 1 < blocklines) {  // If not RECT and not last line adding EOL
					// counting len of EOL
					le += (unsigned)SENDMSGTOCED(currentEdit, SCI_POSITIONFROMLINE, (blockstart + ln), 0) + (unsigned)SENDMSGTOCED(currentEdit, SCI_LINELENGTH, (blockstart + ln), 0) - le;
				}

				if (!lplen || lpe[lplen - 1] != ls) {
					lps = (unsigned *)reallocsafe(lps, sizeof(*lps) * (lplen + 1));
					if (!lps) {
						//MessageBox(nppData._nppHandle, _T("Prusvih lps !!!"), _T(PLUGIN_NAME), MB_OK | MB_ICONERROR);
						isError = true;
						break;
					}

					lpe = (unsigned *)reallocsafe(lpe, sizeof(*lpe) * (lplen + 1));
					if (!lpe) {
						//MessageBox(nppData._nppHandle, _T("Prusvih lpe !!!"), _T(PLUGIN_NAME), MB_OK | MB_ICONERROR);
						isError = true;
						break;
					}

					lps[lplen] = ls;
					lpe[lplen] = le;
					lplen++;
				}
				else {
					lpe[lplen - 1] = le;
				}
			}
		}

		if (!isError) {
			for (buflen = 0, ln = 0; ln < lplen; ln++) {
				struct TextRange tr;
				tr.chrg.cpMin = lps[ln];
				tr.chrg.cpMax = lpe[ln];

				charBuf = (char *)reallocsafe(charBuf, buflen + tr.chrg.cpMax - tr.chrg.cpMin + 1);
				if (!charBuf) {
					//MessageBox(nppData._nppHandle, _T("Prusvih buf !!!"), _T(PLUGIN_NAME), MB_OK | MB_ICONERROR);
					isError = true;
					break;
				}

				tr.lpstrText = charBuf + buflen;
				buflen += (long)SENDMSGTOCED(currentEdit, SCI_GETTEXTRANGE, 0, &tr);

				if (flags&SCDS_COPYRECTANGULAR && ln + 1 < lplen) { // for RECT adding EOL
					if (strcatX(&charBuf, eoltypes[eoltype])) {
						buflen += strlen(eoltypes[eoltype]);
					}
					else {
						//MessageBox(nppData._nppHandle, _T("Prusvih RECT EOL !!!"), _T(PLUGIN_NAME), MB_OK | MB_ICONERROR);
						isError = true;
						break;
					}
				}
			}

			InsertTextToClipboard(charBuf, flags | (SENDMSGTOCED(currentEdit, SCI_SELECTIONISRECTANGLE, 0, 0) ? SCDS_COPYRECTANGULAR : 0));
		}

		delete[] charBuf;
		delete[] lps;
		delete[] lpe;

	}
	else {
		//MessageBox(nppData._nppHandle, _T("Nothing is selected"), _T(PLUGIN_NAME), MB_OK | MB_ICONINFORMATION);
	}
}


/* ============= INI config part ============= */

unsigned FindMenuItem(PFUNCPLUGINCMD _pFunc)
{
	unsigned itemno, items = NELEM(funcItem);
	for (itemno = 0; itemno < items; itemno++) {
		if (_pFunc == funcItem[itemno]._pFunc) {
			return(itemno);
		}
	}
	return(0);
}

void IniSaveSettings(bool save)
{
	const TCHAR sectionSelect[] = TEXT("SelectToClipboard");
	const TCHAR keyCopySelected[] = TEXT("CopySelected");
	const TCHAR configFileName[] = TEXT("SelectToClipboard.ini");
	TCHAR iniFilePath[MAX_PATH];

	// initialize Mmenu Item holder
	miCopySelected = FindMenuItem(doUpdateCopySelected);

	// get path of plugin configuration
	SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)iniFilePath);

	// if config path doesn't exist, we create it
	if (PathFileExists(iniFilePath) == FALSE) {
		CreateDirectory(iniFilePath, NULL);
	}

	PathAppend(iniFilePath, configFileName);

	if (!save) {
		bCopySelected = AlterMenuCheck(miCopySelected, (GetPrivateProfileInt(sectionSelect, keyCopySelected, 0, iniFilePath) != 0) ? '1' : '0');

		VIZMENUFLAGS;
	}
	else {
		WritePrivateProfileString(sectionSelect, keyCopySelected, bCopySelected ? TEXT("1") : TEXT("0"), iniFilePath);
	}
}


/// Check settings menu item
bool AlterMenuCheck(int itemno, char action)
{
	switch (action)
	{
	case '1': // set
		funcItem[itemno]._init2Check = TRUE;
		break;
	case '0': // clear
		funcItem[itemno]._init2Check = FALSE;
		break;
	case '!': // invert
		funcItem[itemno]._init2Check = !funcItem[itemno]._init2Check;
		break;
	case '-': // change nothing but apply the current check value
		break;
	}
	CheckMenuItem(GetMenu(nppData._nppHandle), funcItem[itemno]._cmdID, MF_BYCOMMAND | ((funcItem[itemno]._init2Check) ? MF_CHECKED : MF_UNCHECKED));

	return(funcItem[itemno]._init2Check);
}

/* ============= About dialog ============= */

void doAboutDlg()
{
	CString aboutMsg = _T("Version: ");
	aboutMsg += _T(PLUGIN_VERSION);
	aboutMsg += _T("\n\nLicense: GPL\n\n");
	aboutMsg += _T("Author: Jakub Dvorak <dvorak.jakub@outlook.com>\n");
	MessageBox(nppData._nppHandle, aboutMsg, _T(PLUGIN_NAME), MB_OK);
}
