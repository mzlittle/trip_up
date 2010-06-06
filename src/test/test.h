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
// test.h -- This header file is used included by all component test modules.
//      It provides an ASSERT() like macro that causes a breakpoint to occur
//      even in release mode.
//      It also provides some values that are platform dependant such as a
//      well defined temporary directory
//

#ifndef __TEST_H
#define __TEST_H

#ifndef __TCHAR_H
#include "core/tchar.h"
#endif

#ifndef __DEBUG_H
#include "core/debug.h"
#endif

#if IS_UNIX
#include <iostream>
using namespace std;
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// Dependencies
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#include "core/package.h"


TSS_BeginPackage( cTest )

public:
    cTest();

TSS_EndPackage( cTest )

///////////////////////////////////////////////////////////////////////////////
// TEST() -- Works like ASSERT() but it also breaks during release mode

#if IS_UNIX

#define TEST(exp)   if (!(exp)) \
                    { \
                        std::cerr<<"TEST(" << #exp << ") failure, file " << __FILE__ << " line " << __LINE__ << std::endl; \
                        exit(-1); \
                    } 
#endif

///////////////////////////////////////////////////////////////////////////////
// Platform dependancies

#if IS_UNIX
#define TEMP_DIR   _T("/tmp")
#define TEMP_DIR_N "/tmp"
#endif

#endif // __TEST_H

