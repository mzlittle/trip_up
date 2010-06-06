//
// The developer of the original code and/or files is Tripwire, Inc.
// Portions created by Tripwire, Inc. are copyright (C) 2000 Tripwire,
// Inc. Tripwire is a registered trademark of Tripwire, Inc.  All rights
// reserved.
// 
// This program is free software.  The contents of this file are subject
// to the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.  You may redistribute it and/or modify it
// only in compliance with the GNU General Public License.
// 
// This program is distributed in the hope that it will be useful.
// However, this program is distributed AS-IS WITHOUT ANY
// WARRANTY; INCLUDING THE IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS
// FOR A PARTICULAR PURPOSE.  Please see the GNU General Public License
// for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.
// 
// Nothing in the GNU General Public License or any other license to use
// the code or files shall permit you to use Tripwire's trademarks,
// service marks, or other intellectual property without Tripwire's
// prior written consent.
// 
// If you have any questions, please contact Tripwire, Inc. at either
// info@tripwire.org or www.tripwire.org.
//
///////////////////////////////////////////////////////////////////////////////
// twutil_t.cpp

#include "stdtw.h"
#include "twutil.h"
#include "test/test.h"
#include <fstream>



#if IS_UNIX
//#include <statbuf.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

std::string WideToNarrow( const TSTRING& strWide );

void TestTWUtil()
{
#pragma message( __FILE__ "(1) : TODO - implement this test file")
#if 0
    // TODO: we should test more than the file exists stuff, but that
    // is all I need to do for right now.
    cDebug d("TestTWUtil");

    /////////////////////////////////////////////////////////////////
    // Test FileExists() and FileWritable()

    d.TraceAlways("Testing FileExists() and FileWritable()\n");

    // assuming the current dir is writable, this test should succeed
    TEST(cTWUtil::FileWritable(_T("afilethatdoesnotexist.tmp")) == true);
    
    TSTRING tmpDir = TEMP_DIR;
    tmpDir += _T("/fileexistdir");
    TSTRING tmpFN = tmpDir;
    tmpFN += _T("/fileexiststest.tmp");

    // make a subdir in the TEMP_DIR
    #if IS_UNIX
    _tmkdir(tmpDir.c_str(), 0700);
    #else
    _tmkdir(tmpDir.c_str());
    #endif

    _tchmod(tmpDir.c_str(), 0700);

    // make sure file is not there
    _tchmod(tmpFN.c_str(), 0777);
    _tunlink(tmpFN.c_str());

    // make sure exists tests false, writable is true
    // and checking writable should not create the file
    TEST(cTWUtil::FileExists(tmpFN) == false);
    TEST(cTWUtil::FileWritable(tmpFN) == true)
    TEST(cTWUtil::FileExists(tmpFN) == false);

    // make the dir read only and make sure write tests false
    #if IS_UNIX // windows fails this test, perhaps because I am an administrator?
    _tchmod(tmpDir.c_str(), 0500);
    TEST(cTWUtil::FileWritable(tmpFN) == false);
    _tchmod(tmpDir.c_str(), 0700);
    #endif

    // create the file
    {
    std::ofstream ostr(WideToNarrow(tmpFN).c_str());
    ostr << "Hey there.\n";
    }

    // test a read only file
    _tchmod(tmpFN.c_str(), 0400);
    TEST(cTWUtil::FileWritable(tmpFN) == false);

    // test a writable file
    _tchmod(tmpFN.c_str(), 0666);
    TEST(cTWUtil::FileWritable(tmpFN) == true);

    // delete the test file and dir
    _tunlink(tmpFN.c_str());
    _tunlink(tmpDir.c_str());
#endif
}

std::string WideToNarrow( const TSTRING& strWide )
{
#ifdef _UNICODE
    std::string strA;
    for( TSTRING::const_iterator i = strWide.begin(); i != strWide.end(); i++ )
    {
        char ach[4];
        ASSERT( MB_CUR_MAX <= 4 );

        int n = wctomb( ach, *i );
        ASSERT( n != -1 );
    
        for( int j = 0; j < n; j++ )
            strA += ach[j];
    }

    return strA;
#else
    return strWide;
#endif 
}

