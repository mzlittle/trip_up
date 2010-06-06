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
#include "stdcore.h"

#include <string>
#include <iostream>
#include <fstream>

#ifndef __DEBUG_H
#include "debug.h"
#endif

TSTRING test_wost(int, const TSTRING&);
void test_wist(const TSTRING&, cDebug& d);

void TestTCHAR()
{
	cDebug d("TestTCHAR()");

	d.TraceDetail("Entering...\n");

	//Testing TCOUT:
	TCOUT<< _T("Simple test of TSTRING (and TCOUT) :\n\n");
	TCERR<< _T("This should show up on cerr");

    TSTRING pString;
    pString = _T("Hi Mom!");

	d.TraceDetail("%s \n", pString.c_str() );
	d.TraceDetail("Isn't this cool?\n\n");
	
	//Begin fun tests of string streams:

	TSTRINGSTREAM wst;
		//can I declare it?

	TSTRING str;
	str = _T("Kiteman");
	TSTRING test1 = _T("word");

	d.TraceDetail("Testing TOSTRINGSTREAM with TSTRING:\n");
	TOSTRINGSTREAM ost(_T("test up"));
	ost<<test1;
	d.TraceDetail("%s \n", ost.str() );
		//if this gives output, then I'm really baffled...
		//test gets overwritten, yielding "word up"

	TSTRING output;
	output = test_wost(3, str);
	d.TraceDetail("%s \n", output.c_str());
		//A true statement!

	d.TraceDetail("Testing TISTRINGSTREAM with TSTRING:\n");
	TSTRING send = _T("These should appear on seperate lines");
	test_wist(send, d);
		//Did they?

//Testing file streams

	//explict constructors of 'TIFSTREAM' and "TOFSTREAM' take char*
	const char* inputfile = "fun";
	const char* outputfile = "mo'fun";

	//Set up the input file.
	TOFSTREAM out;
	out.open(inputfile, std::ios_base::out);
	out<<"Unicode is fun\n";
	out.close();

	TIFSTREAM from;
	from.open(inputfile, std::ios_base::in);
	if(!from)
		d.TraceDetail("error opening input file\n");

	TOFSTREAM to(outputfile, std::ios_base::trunc);
	if(!to)
		d.TraceDetail("error opening output file\n");

	//Copy contents of input file to output file.
	TCHAR ch;
	while(from.get(ch))
		to.put(ch);
	if(!from.eof() || !to)
		d.TraceDetail("something has gone terribly wrong...\n");

	return;
}

TSTRING test_wost(int n, const TSTRING& inject)
{
	TOSTRINGSTREAM wost(_T("Weird how this doesn't show up! "));
		//It's getting overwritten, why?
	wost<<_T("One out of every ")<<n<<_T(" children loves ")<<inject<<_T("!\n");
	return wost.str();
}

void test_wist(const TSTRING& input, cDebug& d)
{
	TISTRINGSTREAM wist(input);
	TSTRING parse;
	while(wist>>parse)
		d.TraceDetail("%s \n", parse.c_str() );
}

