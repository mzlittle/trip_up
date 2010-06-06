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
// fcocompare_t.cpp -- the compare object's test driver
#include "stdfco.h"
#include "fcocompare.h"
#include "core/debug.h"
#include "fs/fsobject.h"
#include "fs/fspropcalc.h"
#include "test/test.h"

#include <fstream>

///////////////////////////////////////////////////////////////////////////////
// PrintProps -- prints out all the valid property names and values as pairs...
///////////////////////////////////////////////////////////////////////////////
/*
static void PrintProps(const iFCO* pFCO)
{
	cDebug d("PrintProps");
	const iFCOPropSet* pSet = pFCO->GetPropSet();
	const cFCOPropVector& v = pSet->GetValidVector();
	
	for(int i=0; i<pSet->GetNumProps(); i++)
	{
		if(v.ContainsItem(i))
		{
			d.TraceDebug(_T("[%d] %s\t%s\n"), i, pSet->GetPropName(i).c_str(), pSet->GetPropAt(i)->AsString().c_str());
		}
	}
}
*/

void TestFCOCompare()
{
#pragma message( __FILE__ "(1) : TODO - implement this test file")
#if 0

	const TCHAR* FILE_NAME = TEMP_DIR _T("/dog.txt");
    const char*  FILE_NAME_N = TEMP_DIR_N "/dog.txt";

	cDebug d("TestFCOCompare");
	d.TraceDebug("Entering...\n");

	// first, create an fco to compare with...
	
	TOFSTREAM fstr(FILE_NAME_N);
	if(fstr.bad())
	{
		d.TraceError("Unable to create test file %s!\n", FILE_NAME);
        TEST(false);
		return;
	}

    fstr << "Bark! Bark! Bark!" << std::endl;
	fstr.close();

	// create the test FCO
	cFSDataSource ds;
	iFCO* pFCO = ds.CreateFCO(cFCOName(FILE_NAME), 0);
	TEST(pFCO);

	// measure a couple of properties, some of which will change...
	cFSPropCalc propCalc;
	cFCOPropVector v(pFCO->GetPropSet()->GetValidVector().GetSize());
	v.AddItem(cFSPropSet::PROP_DEV);
	v.AddItem(cFSPropSet::PROP_CTIME);
	v.AddItem(cFSPropSet::PROP_SIZE);
	v.AddItem(cFSPropSet::PROP_MTIME);
	v.AddItem(cFSPropSet::PROP_FILETYPE);
	v.AddItem(cFSPropSet::PROP_GROWING_FILE);
	propCalc.SetPropVector(v);
	pFCO->AcceptVisitor(&propCalc);
	d.TraceDebug("First FCO's properties:\n");
	PrintProps(pFCO);

	// first, try comparing it to itself...
	cFCOCompare					comp;
	cFCOCompare::CompareResult	result;
	comp.SetPropsToCmp(v);
	comp.Compare(pFCO, pFCO, result);
	d.TraceDebug("Compare to itself is (expect true) %s\n", result.mResult == cFCOCompare::EQUAL? "true" : "false");
	TEST(result.mResult == cFCOCompare::EQUAL);

	// change the file...
	d.TraceDebug("Changing the file...\n");
	fstr.open(FILE_NAME_N);
	if(fstr.bad())
	{
		d.TraceError("Unable to reopen %s!\n", FILE_NAME_N);
        TEST(false);
		return;
	}
    fstr << "Meow! Meow! Meow! Meow!" << std::endl;
	fstr.close();

	iFCO* pFCO2 = ds.CreateFCO(cFCOName(FILE_NAME), 0);
	ASSERT(pFCO2);
	pFCO2->AcceptVisitor(&propCalc);
	d.TraceDebug("Second FCO's properties:\n");
	PrintProps(pFCO2);

	comp.Compare(pFCO, pFCO2, result);
	d.TraceDebug("Compare to new object is (expect false) %s\n", result.mResult == cFCOCompare::EQUAL? "true" : "false");
	TEST(result.mResult == cFCOCompare::UNEQUAL);
	d.TraceDebug("Properties that differ are:\n");
	result.mPropVector.TraceContents();

	// try testing properties that weren't calculated...
	d.TraceDebug("Comparing FCOs with different properties calculated\n");
	iFCO* pFCO3 = ds.CreateFCO(cFCOName(FILE_NAME), 0);
	v = propCalc.GetPropVector();
	v.AddItem(cFSPropSet::PROP_MD5);
	propCalc.SetPropVector(v);
	pFCO3->AcceptVisitor(&propCalc);
	// do the compare
	comp.SetPropsToCmp(v);
	comp.Compare(pFCO2, pFCO3, result);
	TEST(result.mResult == cFCOCompare::PROPS_NOT_ALL_VALID);
	d.TraceDebug("Properties not valid are (should be %d):\n", cFSPropSet::PROP_MD5);
	result.mPropVector.TraceContents();

	// release the fcos
	pFCO3->Release();
	pFCO2->Release();
	pFCO->Release();
#endif
	return;
}
