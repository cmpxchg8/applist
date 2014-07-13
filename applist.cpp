/**
 *  The MIT License:
 *
 *  Copyright (c) 2014 Kevin Devine
 *
 *  Permission is hereby granted,  free of charge,  to any person obtaining a 
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction,  including without limitation 
 *  the rights to use,  copy,  modify,  merge,  publish,  distribute,  
 *  sublicense,  and/or sell copies of the Software,  and to permit persons to 
 *  whom the Software is furnished to do so,  subject to the following 
 *  conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS",  WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED,  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,  DAMAGES OR OTHER
 *  LIABILITY,  WHETHER IN AN ACTION OF CONTRACT,  TORT OR OTHERWISE,  
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
 *  OTHER DEALINGS IN THE SOFTWARE.
 */
 
// cl applist.cpp

#define UNICODE
#include <windows.h>
#include <Shlwapi.h>

#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Shlwapi.lib")

// just to format
DWORD maxname=10, maxpub=10, maxver=10;

// structure for each application found
struct ProductEntry {
  std::wstring name;
  std::wstring publisher;
  std::wstring version;
  ULONGLONG    size;
};

void xstrerror (const wchar_t fmt[], ...) 
{
  wchar_t *err;
  va_list arglist;
  wchar_t buf[2048];
  
  va_start (arglist, fmt);
  _vsnwprintf (buf, sizeof (buf) / sizeof (wchar_t), fmt, arglist);
  va_end (arglist);
  
  FormatMessage (
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, GetLastError (), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
      (LPWSTR)&err, 0, NULL);

  wprintf (L"[ %s : %s\n", buf, err);
  LocalFree (err);
}

BOOL listentry (HKEY hAppKey, std::wstring regKey, ProductEntry &entry)
{
  DWORD   dwError, dwValue, dwSize, dwResult;
  HKEY    hEntry;
  wchar_t buf[MAX_PATH];
  BOOL    bResult=FALSE;
      
  // try open the application as just itself
  dwError = RegOpenKeyEx (hAppKey, regKey.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hEntry);
  if (dwError != ERROR_SUCCESS) {
    // assume it's MSI entry - should be able to remove this later
    dwError = RegOpenKeyEx (hAppKey, (regKey + L"\\InstallProperties").c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hEntry);
    if (dwError != ERROR_SUCCESS) return bResult;
  }
  
  // exclude system components
  dwValue  = 0;
  dwSize   = sizeof (DWORD);
  dwResult = RegQueryValueEx (hEntry, L"SystemComponent", NULL, NULL, (LPBYTE)&dwValue, &dwSize);

  // if not a system component
  if (dwValue == 0) {
    // check if child application
    dwSize=0;
    dwError = RegQueryValueEx (hEntry, L"ParentDisplayName", NULL, NULL, NULL, &dwSize);
    if (dwError != ERROR_SUCCESS) 
    {
      // we need a displayname atleast
      dwSize = MAX_PATH;

      dwError = RegQueryValueEx (hEntry, L"DisplayName", NULL, NULL, (LPBYTE)buf, &dwSize);
      if (dwError == ERROR_SUCCESS) 
      {
        entry.name.clear();
        entry.publisher.clear();
        entry.version.clear();
        entry.size=0;
        
        bResult=TRUE;
        entry.name = buf;
        maxname = max (maxname, dwSize/sizeof(wchar_t));

        // query the version - not compulsory
        dwSize = MAX_PATH;

        dwError = RegQueryValueEx (hEntry, L"DisplayVersion", NULL, NULL, (LPBYTE)buf, &dwSize);
        if (dwError == ERROR_SUCCESS) {
          entry.version = buf;
          maxver = max (maxver, dwSize/sizeof(wchar_t));
        }
        // query the publisher - not compulsory
        dwSize = MAX_PATH;

        dwError=RegQueryValueEx (hEntry, L"Publisher", NULL, NULL, (LPBYTE)buf, &dwSize);
        if (dwError == ERROR_SUCCESS) {
          entry.publisher = buf;
          maxpub = max (maxpub, dwSize/sizeof(wchar_t));
        }
        
        // query the size - not compulsory
        dwSize  = sizeof (dwSize);
        dwValue = 0;
        dwError=RegQueryValueEx (hEntry, L"EstimatedSize", NULL, NULL, (LPBYTE)&dwValue, &dwSize);
        if (dwError == ERROR_SUCCESS) {
          ULONGLONG x=dwValue;
          entry.size=x*1024;
        }
      }
    }
  }
  RegCloseKey (hEntry);
  return bResult;
}

// default sort by name
int sort=1;

bool SortFunction (ProductEntry rpStart, ProductEntry rpEnd)
{
  if (sort==1) {        // by name
    return (lstrcmpi (rpStart.name.c_str(), rpEnd.name.c_str()) < 0);
  } else if (sort==2) { // by publisher
    return (lstrcmpi (rpStart.publisher.c_str(), rpEnd.publisher.c_str()) < 0);
  } else if (sort==3) { // by size
    return rpStart.size > rpEnd.size;
  }
}

std::vector<ProductEntry> products;

DWORD listapps (HKEY hRootKey, wchar_t appkey[])
{
  HKEY         hAppKey;
  wchar_t      buf[MAX_PATH];
  DWORD        dwSize, dwCount=0, dwIdx=0;
  ProductEntry entry;
  
  DWORD dwError=RegOpenKeyEx (hRootKey, appkey, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hAppKey);
  if (dwError!=ERROR_SUCCESS) return 0;
  
  while (1) {
    DWORD dwSize=MAX_PATH;
    dwError=RegEnumKeyEx (hAppKey, dwIdx++, buf, &dwSize, NULL, NULL, NULL, NULL);
    if (dwError==ERROR_NO_MORE_ITEMS) break;
    
    if (listentry (hAppKey, buf, entry)) {
      products.push_back (entry);
    }
  }  
  RegCloseKey (hAppKey);
  return dwCount;
}

VOID ConsoleSetBufferWidth (SHORT X) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  
  GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &csbi);
  
  if (X <= csbi.dwSize.X) return;
  csbi.dwSize.X  = X;
  SetConsoleScreenBufferSize (GetStdHandle (STD_OUTPUT_HANDLE), csbi.dwSize);  
}

void usage (wchar_t *argv[])
{
  wprintf (L"usage: applist [options]\n");
  wprintf (L"  -s#\tsort entries : s1 = by name (default), s2 = by publisher, s3 = by size\n\n");
  exit (0);
}

int wmain (int argc, wchar_t *argv[])
{
  DWORD dwCount=0;
  ULONGLONG total_size=0;
  wchar_t buf[MAX_PATH];
  int n;
  
  puts ("\nAppList v0.1 - List installed applications"
       "\nCopyright (c) 2014 Kevin Devine\n");
  
  for (int i=1; i<argc; i++)
  {
    if (argv[i][0]==L'/'||argv[i][0]==L'-') {
      switch (argv[i][1]) {
        case L's' : {
          n=_wtoi(&argv[i][2]);
          if (n > 0 && n < 4) {
            sort=n;
          } else {
            wprintf (L"\nInvalid sort option %i, valid values are 1 (name), 2 (publisher), 3 (size)\n", n);
            exit (0);
          }
        }
        break;
        case L'?' :
        case L'h' :
        default:
          usage (argv);
          break;
      }
    } else {
      usage (argv);
    }
  }
  
  // list the system applications
  listapps (HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
  listapps (HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
  
  // now for current user
  // would be nice to list apps for all profiles but that would require loading user.dat
  // into memory before checking. It's doable but not implemented right now.
  listapps (HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
  
  std::sort (products.begin(), products.end(), SortFunction);
  
  // set console buffer width according to maximum len of strings
  ConsoleSetBufferWidth (maxname+maxver+maxpub+32);
  
  // print header
  wprintf (L"\n%-*s\t%-*s\t%-*s\tSize", 
    maxname, L"Name", 
    maxver, L"Version", 
    maxpub, L"Publisher");
    
  wprintf (L"\n%s\t%s\t%s\t%s", 
    std::wstring(maxname, L'=').c_str(), 
    std::wstring(maxver,  L'=').c_str(), 
    std::wstring(maxpub,  L'=').c_str(),
    std::wstring(10,      L'=').c_str());
  
  // print each app entry
  for (std::vector<ProductEntry>::size_type i=0; i<products.size(); i++) {
    StrFormatByteSizeEx (products[i].size, SFBS_FLAGS_TRUNCATE_UNDISPLAYED_DECIMAL_DIGITS, buf, MAX_PATH);
    wprintf (L"\n%-*s\t%-*s\t%-*s\t%10s",
        maxname,
        products[i].name.c_str(), 
        maxver,
        products[i].version.c_str(), 
        maxpub,
        products[i].publisher.c_str(), buf);
        
    total_size += products[i].size;
    dwCount++;
  }
  StrFormatByteSizeEx (total_size, SFBS_FLAGS_TRUNCATE_UNDISPLAYED_DECIMAL_DIGITS, buf, MAX_PATH);
  wprintf (L"\n\n%i programs installed\nTotal size: %s\n", dwCount, buf);
  return 0;
}
