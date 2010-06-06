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
// archive_t.cpp
//
// test the archive component

#include "stdcore.h"
#include "archive.h"
#include "test/test.h"
#include "error.h"
#include <stdio.h>

TSS_EXCEPTION(eTestArchiveError, eError);

void TestArchive()
{
    // cMemoryArchive 
    cMemoryArchive memarch;

    memarch.WriteInt32(1);
    memarch.WriteInt32(2);
    memarch.WriteInt32(3);
    memarch.WriteInt32(4);

    TSTRING s = _T("Weenus");
    memarch.WriteString(s);

    memarch.WriteInt64(1234567L);

    memarch.WriteInt16(42);

    memarch.Seek(0, cBidirArchive::BEGINNING);

    int32 i;
    int64 l;
    memarch.ReadInt32(i);
    TEST(i == 1);
    memarch.ReadInt32(i);
    TEST(i == 2);
    memarch.ReadInt32(i);
    TEST(i == 3);
    memarch.ReadInt32(i);
    TEST(i == 4);

    TSTRING s2;
    memarch.ReadString(s2);
    TEST(s2.compare(_T("Weenus")) == 0);

    memarch.ReadInt64(l);
    TEST(l == 1234567L);

    TEST(memarch.ReadBlob(NULL, sizeof(int16)) == sizeof(int16));
    TEST(memarch.ReadBlob(NULL, 1024) == 0);

    try {
        memarch.ReadInt32(i);
		throw eTestArchiveError();
    }
    catch (eArchive& e)
    {
        // Cool we got the right exception
        (void)e;
    }
    catch (eError& e)
    {
        TEST(false);
        (void)e;
    }

    memarch.MapArchive(4 * sizeof(int32) + sizeof(int32) + 6, sizeof(int64));
    TEST(memarch.GetMappedOffset() == 4 * sizeof(int32) + sizeof(int32) + 6);
    TEST(memarch.GetMappedLength() == sizeof(int64));
//    TEST(tw_ntohll(*(int64*)memarch.GetMap()) == 1234567L);


    // cLockedTemporaryFileArchive
    TSTRING lockedFileName = TEMP_DIR;
    lockedFileName += _T("/inaccessable_file.bin");

	cLockedTemporaryFileArchive lockedArch;

    // try to create an archive using a temp file
    lockedArch.OpenReadWrite();
    lockedArch.Close();

    // this should open and lock the file -- shouldn't be able to access it
	lockedArch.OpenReadWrite(lockedFileName.c_str());
	lockedArch.Seek(0, cBidirArchive::BEGINNING);

    // shouldn't be able to see these changes
	lockedArch.WriteInt32(1);
	lockedArch.WriteInt32(2);
	lockedArch.WriteInt32(3);
	lockedArch.WriteInt32(4);
	lockedArch.WriteString(s);
	lockedArch.WriteInt64(1234567L);
    lockedArch.WriteInt16(42);

    // this should delete the file
	lockedArch.Close();

    // cFileArchive 
    TSTRING fileName = TEMP_DIR;
    fileName += _T("/archive_test.bin");

	cFileArchive filearch;
	filearch.OpenReadWrite(fileName.c_str());
	filearch.Seek(0, cBidirArchive::BEGINNING);

	filearch.WriteInt32(1);
	filearch.WriteInt32(2);
	filearch.WriteInt32(3);
	filearch.WriteInt32(4);

	filearch.WriteString(s);
	filearch.WriteInt64(1234567L);

    filearch.WriteInt16(42);
	filearch.Close();

    int32 j;
	int64 k;
	filearch.OpenRead(fileName.c_str());
	filearch.Seek(0, cBidirArchive::BEGINNING);
	filearch.ReadInt32(j);
    TEST(j == 1);
    filearch.ReadInt32(j);
    TEST(j == 2);
    filearch.ReadInt32(j);
    TEST(j == 3);
    filearch.ReadInt32(j);
    TEST(j == 4);

	TSTRING s3;
    filearch.ReadString(s3);
    TEST(s3.compare(_T("Weenus")) == 0);
    filearch.ReadInt64(k);
    TEST(k == 1234567L);

    TEST(filearch.ReadBlob(NULL, sizeof(int16)) == sizeof(int16));
    TEST(filearch.ReadBlob(NULL, 1024) == 0); // should be EOF

    try {
        filearch.ReadInt32(j);
		throw eTestArchiveError();
    }
    catch (eArchive& e)
    {
        // Cool we got the right exception
        (void)e;
    }
    catch (eError& e)
    {
        TEST(false);
        (void)e;
    }
}

