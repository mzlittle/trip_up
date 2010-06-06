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
// fsdatasourceiter.cpp
//

//=========================================================================
// INCLUDES
//=========================================================================

#include "stdfs.h"

#include "fsdatasourceiter.h"
#include "fco/fcodatasourceiter.h"
#include "fsobject.h"
#include "core/fsservices.h"
#include "core/errorbucket.h"
#include "core/corestrings.h"
#include "core/usernotify.h"
#include "fco/twfactory.h"
#include "fco/fconametranslator.h"
#include "fco/fconameinfo.h"
#include "fsstrings.h"

//=========================================================================
// METHOD CODE
//=========================================================================

cFSDataSourceIter::cFSDataSourceIter()
{
	// set the case sensitiveness of the parent...
	//
    mDev = 0;
	mParentName.SetCaseSensitive( iTWFactory::GetInstance()->GetNameInfo()->IsCaseSensitive() );
}

cFSDataSourceIter::~cFSDataSourceIter()
{
}

cFSDataSourceIter::cFSDataSourceIter( const cFSDataSourceIter& rhs )
{
	// set the case sensitiveness of the parent...
	//
	mParentName.SetCaseSensitive( iTWFactory::GetInstance()->GetNameInfo()->IsCaseSensitive() );
	*this = rhs;
}

///////////////////////////////////////////////////////////////////////////////
// operator =
///////////////////////////////////////////////////////////////////////////////
cFSDataSourceIter& cFSDataSourceIter::operator=( const cFSDataSourceIter& rhs )
{
    if( this == &rhs ) 
        return *this;

    // copy base
    cFCODataSourceIterImpl::operator=(rhs);
    // copy derived
	mDev = rhs.mDev;

	return *this;
}

static bool gCrossFileSystems = false;

// Call this to set the property where cFSDataSourceIter does not automatically recurse
// across file system boundaries.  Currently this is by default is set to false.
/*static*/ void cFSDataSourceIter::SetFileSystemCrossing(bool crossFS)
{
    gCrossFileSystems = crossFS;
}

///////////////////////////////////////////////////////////////////////////////
// CreateCopy
///////////////////////////////////////////////////////////////////////////////
iFCODataSourceIter* cFSDataSourceIter::CreateCopy() const 
{
	return (new cFSDataSourceIter(*this));
}


///////////////////////////////////////////////////////////////////////////////
// CreateObject -- creates the named object and fills out its properties
//		appropriately. Returns NULL if any errors occur and fills up the 
//		error queue.
//		
//		This does not create the object if dev > 0 and the object's mDev is not
//		equal to it (thus preventing the crossing of file systems)
//
//		TODO -- in the future, this should become some kind of lazy evaluation so
//			that we don't have to get all the stats() we don't need to.
///////////////////////////////////////////////////////////////////////////////
iFCO * cFSDataSourceIter::CreateObject(const cFCOName& name, bool bCreatePeers )
{
    cFSObject* pNewObj = new cFSObject(name);

	if( ! bCreatePeers )
	{
		// when bCreatePeers is false, it means we should set mDev to the 
		// device number of the current object (ie -- it is a new "start point")
		// If we don't do this here, InitializeTypeInfo() will reject creating the
		// node.
		// -- 9 June 99 mdb
		//
		mDev = 0;
		if( ! InitializeTypeInfo( pNewObj ) )
		{
			pNewObj->Release();
			return 0;
		}
		mDev = pNewObj->GetFSPropSet().GetDev();
	}

    return pNewObj;
}

void cFSDataSourceIter::GetChildrenNames( const TSTRING& strParentName, std::vector<TSTRING>& vChildrenNames )
{
    try
    {
        iFSServices::GetInstance()->ReadDir( strParentName, vChildrenNames, false );
    }
	catch( eFSServices& e )
	{
		cDebug d("cFSDataSourceIter::GeneratePeers");
		d.TraceError("**** ReadDir failed for %s\n", strParentName.c_str() );

		if( mpErrorBucket )
        {
			eFSDataSourceIterReadDir eReadDir(e.GetMsg(), eError::NON_FATAL);
			mpErrorBucket->AddError( eReadDir );
        }
	}
}

///////////////////////////////////////////////////////////////////////////////
// InitializeTypeInfo
///////////////////////////////////////////////////////////////////////////////
bool cFSDataSourceIter::InitializeTypeInfo(iFCO* pFCO)
{
	cFSObject* pObj = (cFSObject*)pFCO;
	iFCONameTranslator* pTrans = iTWFactory::GetInstance()->GetNameTranslator();
	
	if( pObj->GetFSPropSet().GetValidVector().ContainsItem( cFSPropSet::PROP_FILETYPE) )
		return true;

	//
	// assume invalid by default...
	//
	cFSPropSet& propSet = pObj->GetFSPropSet();
	propSet.SetFileType(cFSPropSet::FT_INVALID);

	cFSStatArgs statArgs;
	try
	{
		iFSServices::GetInstance()->Stat( pTrans->ToStringAPI( pObj->GetName() ), statArgs);
	}
	catch(eFSServices& e)
	{
		cDebug d("CreateObject");
		d.TraceError( "*** Stat of %s failed!!!\n", pObj->GetName().AsString().c_str() );
		if( mpErrorBucket )
        {
            e.SetFatality( false );
			mpErrorBucket->AddError( e );
        }
		return false;
	}
	
	//
	// don't create the object if it is on a different file system...
	//
	if( gCrossFileSystems == false && (mDev != 0) && (statArgs.dev != mDev) )
	{
        TW_NOTIFY_NORMAL(	TSS_GetString( cFS, fs::STR_DIFFERENT_FILESYSTEM ).c_str(),
							pTrans->ToStringDisplay( pObj->GetName() ).c_str() );
		return false;
	}

	//
	// fill out all of the appropriate properties....
	//
	propSet.SetDev			(statArgs.dev);
    propSet.SetRDev			(statArgs.rdev);
	propSet.SetInode		(statArgs.ino);
	propSet.SetMode			(statArgs.mode);
	propSet.SetNLink		(statArgs.nlink);
	propSet.SetUID			(statArgs.uid);
	propSet.SetGID			(statArgs.gid);
	propSet.SetSize			(statArgs.size);
	propSet.SetAccessTime	(statArgs.atime);
	propSet.SetModifyTime	(statArgs.mtime);
	propSet.SetCreateTime	(statArgs.ctime);
	propSet.SetBlockSize	(statArgs.blksize);
	propSet.SetBlocks		(statArgs.blocks);
	propSet.SetGrowingFile	(statArgs.size);

	// set the file type
	switch(statArgs.mFileType)
	{
	case cFSStatArgs::TY_FILE:
		propSet.SetFileType(cFSPropSet::FT_FILE);
		break;
	case cFSStatArgs::TY_DIR:
		propSet.SetFileType(cFSPropSet::FT_DIR);
		break;
	case cFSStatArgs::TY_BLOCKDEV:
		propSet.SetFileType(cFSPropSet::FT_BLOCKDEV);
		break;
	case cFSStatArgs::TY_CHARDEV:
		propSet.SetFileType(cFSPropSet::FT_CHARDEV);
		break;
	case cFSStatArgs::TY_SYMLINK:
		propSet.SetFileType(cFSPropSet::FT_SYMLINK);
		break;
	case cFSStatArgs::TY_FIFO:
		propSet.SetFileType(cFSPropSet::FT_FIFO);
		break;
	case cFSStatArgs::TY_SOCK:
		propSet.SetFileType(cFSPropSet::FT_SOCK);
		break;
	default:
		// set it to invalid
		propSet.SetFileType(cFSPropSet::FT_INVALID);
	}

	return true;
	
}

