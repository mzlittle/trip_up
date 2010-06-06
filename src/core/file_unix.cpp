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
// file_unix.cpp : Specific implementation of file operations for Unix.

#include "core/stdcore.h"

#if !IS_UNIX
#error Need to be unix to use unixfsservices
#endif

#include "core/file.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "core/debug.h"
#include "core/corestrings.h"
#include "core/fsservices.h"
#include "core/errorutil.h"

///////////////////////////////////////////////////////////////////////////////
// util_GetErrnoString -- return the result of strerror(errno) as a tstring
///////////////////////////////////////////////////////////////////////////////
static TSTRING util_GetErrnoString()
{
	TSTRING ret;
	char* pErrorStr = strerror(errno);
#ifdef _UNICODE
#error	We dont currently support unicode on unix
#else
	ret = pErrorStr;
#endif
	return ret;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
// cFile_i : Insulated implementation for cFile objects.
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
struct cFile_i
{
	cFile_i();
	~cFile_i();

	FILE* mpCurrStream;	//currently defined file stream
	TSTRING mFileName;	//the name of the file we are currently referencing.
};

//Ctor
cFile_i::cFile_i() :
	mpCurrStream(NULL)
{}

//Dtor
cFile_i::~cFile_i()
{
	if (mpCurrStream != NULL)
		fclose( mpCurrStream );
	mpCurrStream = NULL;

	mFileName.empty();
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
// cFile () -- Implements file operations
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

cFile::cFile() :
	mpData(NULL), isWritable(false)
{
	mpData = new cFile_i;
}

cFile::~cFile()
{
	if( mpData != NULL)
	{
		delete mpData;
		mpData = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Open
///////////////////////////////////////////////////////////////////////////////
void cFile::Open( const TSTRING& sFileName, uint32 flags )
{
	mode_t openmode = 0664;
	if ( mpData->mpCurrStream != NULL )
		Close();
	
	//
	// set up the sopen permissions
	//
	int perm = 0;

	TSTRING mode;

	if( flags & OPEN_WRITE )
	{
		perm		|= O_RDWR;
		isWritable	= true;
		mode		= _T("rb");
		if( flags & OPEN_TRUNCATE )
		{
			perm |= O_TRUNC;
			perm |= O_CREAT;
			mode = _T("w+b");
		}
		else
			mode = _T("r+b");
	}
	else
	{
		perm		|= O_RDONLY;
		isWritable	= false;
		mode		= _T("rb");
	}

	if ( flags & OPEN_EXCLUSIVE ) {
		perm |= O_CREAT | O_EXCL;
		openmode = (mode_t) 0600; // Make sure only root can read the file
	}

	if ( flags & OPEN_CREATE )
		perm |= O_CREAT;

	//
	// actually open the file
	//
	int fh = _topen( sFileName.c_str(), perm, openmode );
	if( fh == -1 )
	{
		throw( eFileOpen( sFileName, iFSServices::GetInstance()->GetErrString() ) );
	}
	if( flags & OPEN_LOCKED_TEMP )
	{
		// unlink this file 
        if( 0 != unlink( sFileName.c_str() ) )
        {
            // we weren't able to unlink file, so close handle and fail
            close( fh );
			throw( eFileOpen( sFileName, iFSServices::GetInstance()->GetErrString() ) );
        }
	}

	//
	// turn the file handle into a FILE*
	//
	mpData->mpCurrStream = _tfdopen(fh, mode.c_str());

	mpData->mFileName = sFileName;	//Set mFileName to the newly opened file.
	
	cFile::Rewind();
}


///////////////////////////////////////////////////////////////////////////
// Close -- Closes mpCurrStream and sets the pointer to NULL
///////////////////////////////////////////////////////////////////////////
void cFile::Close()	//throw(eFile)
{
	if(mpData->mpCurrStream != NULL)
	{
		fclose( mpData->mpCurrStream );
		mpData->mpCurrStream = NULL;
	}
	mpData->mFileName.empty();
}

bool cFile::IsOpen( void ) const
{
	return( mpData->mpCurrStream != NULL );
}

///////////////////////////////////////////////////////////////////////////
// Seek -- Positions the read/write offset in mpCurrStream.  Returns the
//		current offset upon completion.  Returns 0 if no stream is defined.
///////////////////////////////////////////////////////////////////////////
cFile::File_t cFile::Seek( File_t offset, SeekFrom From) const //throw(eFile)
{
	//Check to see if a file as been opened yet...
	ASSERT( mpData->mpCurrStream != 0);

    int apiFrom;

	switch( From )
	{
	case cFile::SEEK_BEGIN:
        apiFrom = SEEK_SET;
		break;
	case cFile::SEEK_CURRENT:
        apiFrom = SEEK_CUR;
		break;
	case cFile::SEEK_EOF:
        apiFrom = SEEK_END;
		break;
	default:
		//An invalid SeekFrom parameter was passed.
		throw( eInternal( _T("file_unix") ) );
	}

    // this is a hack to simulate running out of disk space
    #if 0
    static int blowupCount = 1;
    if (++blowupCount == 1075)
    {
        fputs("***** faking seek failure!\n", stderr);
        //throw eFileSeek();
        throw std::bad_alloc();
    }
    fprintf(stderr, "%d\n", blowupCount);
    #endif

	if (fseek( mpData->mpCurrStream, offset, apiFrom ) != 0)
    {
        #ifdef _DEBUG
        cDebug d("cFile::Seek");
        d.TraceDebug("Seek failed!\n");
        #endif
        throw eFileSeek();
    }

	return ftell( mpData->mpCurrStream );
}

///////////////////////////////////////////////////////////////////////////
// Read -- Returns the actual bytes read from mpCurrStream.  Returns 0 if
//		mpCurrStream is undefined.
///////////////////////////////////////////////////////////////////////////
cFile::File_t cFile::Read( void* buffer, File_t nBytes ) const //throw(eFile)
{
	File_t iBytesRead;

	// Has a file been opened?
	ASSERT( mpData->mpCurrStream != NULL );

	// Is the nBytes parameter 0?  If so, return without touching buffer:
	if( nBytes == 0 )
		return 0;

	iBytesRead = fread( buffer, sizeof(byte), nBytes, mpData->mpCurrStream );
	
	if( ferror( mpData->mpCurrStream ) != 0 )
		throw eFileRead( mpData->mFileName, iFSServices::GetInstance()->GetErrString() ) ;
	else
		return iBytesRead;
}

///////////////////////////////////////////////////////////////////////////
// Write -- Returns the actual number of bytes written to mpCurrStream
//		Returns 0 if no file has been opened.
///////////////////////////////////////////////////////////////////////////
cFile::File_t cFile::Write( const void* buffer, File_t nBytes )		//throw(eFile)
{
	File_t actual_count = 0;

	// Has a file been opened? Is it writable?
	ASSERT( mpData->mpCurrStream != NULL );
	ASSERT( isWritable );

	if( ( actual_count = fwrite( buffer, sizeof(byte), nBytes, mpData->mpCurrStream ) ) < nBytes )
		throw eFileWrite( mpData->mFileName, iFSServices::GetInstance()->GetErrString() );
	else
		return actual_count;
}

///////////////////////////////////////////////////////////////////////////
// Tell -- Returns the current file offset.  Returns 0 if no file has been
//		opened.
///////////////////////////////////////////////////////////////////////////
cFile::File_t cFile::Tell() const
{
	ASSERT( mpData->mpCurrStream != 0);

	return ftell( mpData->mpCurrStream );
}

///////////////////////////////////////////////////////////////////////////
// Flush -- Flushes the current stream.
///////////////////////////////////////////////////////////////////////////
bool cFile::Flush()	//throw(eFile)
{
	if ( mpData->mpCurrStream == NULL )
		throw eFileFlush( mpData->mFileName, iFSServices::GetInstance()->GetErrString() );
		
	return ( fflush( mpData->mpCurrStream) == 0 );
}
///////////////////////////////////////////////////////////////////////////
// Rewind -- Sets the offset to the beginning of the file.  If mpCurrStream
//		is NULL, this method returns false. If the rewind operation fails,
//		an exception is thrown.
///////////////////////////////////////////////////////////////////////////
void cFile::Rewind() const	//throw(eFile)
{
	ASSERT( mpData->mpCurrStream != 0);

	rewind( mpData->mpCurrStream );
	if( ftell( mpData->mpCurrStream ) != 0 )
		throw( eFileRewind( mpData->mFileName, iFSServices::GetInstance()->GetErrString() ) );
}

///////////////////////////////////////////////////////////////////////////
// GetSize -- Returns the size of the current stream, if one has been
//		opened. If no stream has been opened, returns -1.
///////////////////////////////////////////////////////////////////////////
cFile::File_t cFile::GetSize() const
{
	File_t vCurrentOffset = Tell();	//for saving the current offset
	File_t ret;

	//Has a file been opened? If not, return -1
	if( mpData->mpCurrStream == NULL )
		return -1;

	ret = Seek( 0, cFile::SEEK_EOF );
	Seek( vCurrentOffset, cFile::SEEK_BEGIN );
		//return the offset to it's position prior to GetSize call.

	return ret;
}

/////////////////////////////////////////////////////////////////////////
// Truncate
/////////////////////////////////////////////////////////////////////////
void cFile::Truncate( File_t offset ) // throw(eFile) 
{
	ASSERT( mpData->mpCurrStream != 0);
	ASSERT( isWritable );

	ftruncate( fileno(mpData->mpCurrStream), offset );
	if( GetSize() != offset )
		throw( eFileTrunc( mpData->mFileName, iFSServices::GetInstance()->GetErrString() ) );
}

