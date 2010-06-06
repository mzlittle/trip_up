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
////////////////////////////////////////////////////////////////// 
// unixfsservices.cpp
//
//	Implements cUnixFSServices class in unixfsservices.h
//

#include "core/stdcore.h"
#include "core/corestrings.h"

#if !IS_UNIX //encase this all in an ifdef so it won't cause compile errors
#error Must be unix for unixfsservices
#endif

//=========================================================================
// STANDARD LIBRARY INCLUDES
//=========================================================================

#include <ctype.h>
#include <iostream>

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_USTAT_H
# include <sys/ustat.h>
#endif
#ifdef HAVE_WCHAR_H
# include <wchar.h>
#endif
#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
#include <sys/utsname.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <grp.h>
#include <fcntl.h>
#include <errno.h>

//=========================================================================
// INCLUDES
//=========================================================================

#include "unixfsservices.h"

// commented out definition of _TWNBITSMAJOR because we should use the
// makedev macro in sys/sysmacros.h for portability. 
// except linux has the body of sysmacros.h commented out. why?
// -jeb 7/26/99
// reduced to sys/statfs.h.  Linux is OK and doesn't deserve
// special treatment.  20010317-PH
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif //HAVE_SYS_STATFS_H



//=========================================================================
// DEFINES AND MACROS
//=========================================================================

#define TW_SLASH    _T('/')

//=========================================================================
// OTHER DIRECTIVES
//=========================================================================

using namespace std;

//=========================================================================
// GLOBALS
//=========================================================================

//=========================================================================
// UTIL FUNCTION PROTOTYES
//=========================================================================

static bool util_FileIsExecutable( const TSTRING& );
static bool util_PathFind( TSTRING& strFullPath, const TSTRING& strFilename );
static void util_RemoveLastPathElement( TSTRING& strPath, TSTRING& strElem );
static bool util_GetNextPathElement( const TSTRING& strPathC, TSTRING& strElem, int index );
static void util_RemoveDuplicateSeps( TSTRING& strPath );
static bool util_TrailingSep( TSTRING& str, bool fLeaveSep );
static void util_RemoveTrailingSeps( TSTRING& str );
template< typename T > static inline void util_ZeroMemory( T& obj );

//=========================================================================
// PUBLIC METHOD CODE
//=========================================================================

cUnixFSServices::cUnixFSServices()
{}

cUnixFSServices::~cUnixFSServices()
{}

//=========================================================================
// *** VIRTUAL FUNCTION CODE ***
//=========================================================================

///////////////////////////////////////////////////////////////////////////////
// GetErrString
///////////////////////////////////////////////////////////////////////////////
TSTRING	cUnixFSServices::GetErrString() const
{
	TSTRING ret;
	char* pErrorStr = strerror(errno);
#ifdef _UNICODE
	wchar_t pBuf[1024];
	mbstowcs(pBuf, pErrorStr, 1024);
	ret = pBuf;
#else
	ret = pErrorStr;
#endif
	return ret;
}


///////////////////////////////////////////////////////////////////////////////
// GetHostID
///////////////////////////////////////////////////////////////////////////////
void cUnixFSServices::GetHostID( TSTRING& name ) const
{
    TOSTRINGSTREAM ret;

    ret.setf(ios_base::hex, ios_base::basefield);
#ifdef HAVE_GETHOSTID
    ret << gethostid();
#else
    ret << 999999;
#endif

}

// returns "/" for unix and "\\" for win32
TCHAR  cUnixFSServices::GetPathSeperator() const
{
    return '/';
}


void cUnixFSServices::ReadDir(const TSTRING& strFilename, std::vector<TSTRING> &v, bool bFullPaths) const throw(eFSServices)
{
	//Get all the filenames
	DIR* dp;
	dp = opendir( strFilename.c_str() );

	if (dp == NULL) 
	{
		throw eFSServicesGeneric( strFilename, iFSServices::GetInstance()->GetErrString() );
		return;
	}
	
	struct dirent* d;

	while ((d = readdir(dp)) != NULL)
	{
		if ((strcmp(d->d_name, _T(".")) != 0) && (strcmp(d->d_name, _T("..")) != 0))
		{
			if( bFullPaths )
			{
				//Create the full pathname
				TSTRING strNewName = strFilename;

				// get full path of dir entry
				util_TrailingSep( strNewName, true );
				strNewName += d->d_name;
            
				// save full path name
				v.push_back( strNewName );
			}
			else
				v.push_back( d->d_name );
		}
	}

	//Close the directory
	closedir( dp );
}

/* needs to and with S_IFMT, check EQUALITY with S_*, and return more types
cFSStatArgs::FileType cUnixFSServices::GetFileType(const cFCOName &filename) throw(eFSServices)
{
	cFSStatArgs stat;
	Stat(filename, stat);
	return stat.mFileType;
}
*/

void cUnixFSServices::GetCurrentDir( TSTRING& strCurDir ) const throw(eFSServices)
{
	TCHAR pathname[MAXPATHLEN];
	pathname[0] = '\0';
	TCHAR* ret = getcwd(pathname, sizeof(TCHAR)*MAXPATHLEN);

	if (ret == NULL)
		throw eFSServicesGeneric( strCurDir, iFSServices::GetInstance()->GetErrString() );

    strCurDir = pathname;
}

void cUnixFSServices::ChangeDir( const TSTRING& strDir ) const throw(eFSServices)
{
	if( chdir( strDir.c_str() ) < 0 )
		throw eFSServicesGeneric( strDir, iFSServices::GetInstance()->GetErrString() );
}


TSTRING& cUnixFSServices::MakeTempFilename( TSTRING& strName ) const throw(eFSServices)
{
    char* pchTempFileName;
    char szTemplate[MAXPATHLEN];
    int fd;

#ifdef _UNICODE
    // convert template from wide character to multi-byte string
    char mbBuf[MAXPATHLEN];
    wcstombs( mbBuf, strName.c_str(), strName.length() + 1 );
    strcpy( szTemplate, mbBuf );
#else
    strcpy( szTemplate, strName.c_str() );
#endif

#ifdef HAVE_MKSTEMP
     // create temp filename and check to see if mkstemp failed                 
    if ((fd = mkstemp( szTemplate )) == -1) {                                   
      throw eFSServicesGeneric( strName );                                      
    } else {                                                                    
      close(fd);                                                                
    }                                                                           
    pchTempFileName = szTemplate;
#else
    fd = 0;
    // create temp filename
    pchTempFileName = mktemp( szTemplate );

    //check to see if mktemp failed
    if ( pchTempFileName == NULL || strlen(pchTempFileName) == 0) {
      throw eFSServicesGeneric( strName );
    }
#endif

    // change name so that it has the XXXXXX part filled in
#ifdef _UNICODE
    // convert name from multi-byte to wide character string
    wchar_t wcsbuf[1024];
    mbstowcs( wcsbuf, pchTempFileName, strlen( pchTempFileName ) + 1 ));
    strName = wcsbuf;
#else	
    strName = pchTempFileName;
#endif


    // Linux creates the file!!  Doh!
    // So I'll always attempt to delete it -bam
    FileDelete( strName );

	return( strName );
}

void cUnixFSServices::Mkdir( const TSTRING& strName ) const throw ( eFSServices )
{
	if( 0 != _tmkdir( strName.c_str(), 0777 ) )
    {
        // if mkdir failed because the dir existed, that's OK
        if( errno != EEXIST )
		    throw eFSServicesGeneric( strName, iFSServices::GetInstance()->GetErrString() );
    }
}	

bool cUnixFSServices::Rmdir( const TSTRING& strName ) const
{
	if( 0 == rmdir( strName.c_str() ) )
		return true;

	return false;
}

void cUnixFSServices::GetTempDirName( TSTRING& strName ) const throw(eFSServices)
{
  strName = mTempPath;
}

void cUnixFSServices::SetTempDirName(TSTRING& tmpPath) {

  mTempPath = tmpPath;
}


void cUnixFSServices::Stat( const TSTRING& strName, cFSStatArgs &stat ) const throw(eFSServices)
{
	//local variable for obtaining info on file.
	struct stat statbuf;

	int ret;
	ret = lstat( strName.c_str(), &statbuf );

	cDebug d( "cUnixFSServices::Stat" );
	d.TraceDetail( "Executing on file %s (result=%d)\n", strName.c_str(), ret );

	if( ret < 0 )
		throw eFSServicesGeneric( strName, iFSServices::GetInstance()->GetErrString() );

    // new stuff 7/17/99 - BAM
    // if the file is not a device set rdev to zero by hand (most OSs will
    // do this for us, but some don't)
    if( ! S_ISBLK( statbuf.st_mode ) && ! S_ISCHR( statbuf.st_mode ) )
    {
        // must zero memory instead of '= 0' since we don't know the 
        // actual type of the object -- could be a struct (requiring '= {0}' )
        util_ZeroMemory( statbuf.st_rdev );                                             
    }

	//copy information returned by lstat call into the structure passed in
	stat.gid        = statbuf.st_gid;
	stat.atime		= statbuf.st_atime;
	stat.ctime		= statbuf.st_ctime;
	stat.mtime		= statbuf.st_mtime;
	stat.dev		= statbuf.st_dev;
	stat.rdev		= statbuf.st_rdev;
	stat.ino		= statbuf.st_ino;
	stat.mode		= statbuf.st_mode;
	stat.nlink		= statbuf.st_nlink;
	stat.size		= statbuf.st_size;
	stat.uid		= statbuf.st_uid;
    stat.blksize    = statbuf.st_blksize;
    stat.blocks     = statbuf.st_blocks;

	// set the file type
		 if(S_ISREG(statbuf.st_mode))	stat.mFileType = cFSStatArgs::TY_FILE;
	else if(S_ISDIR(statbuf.st_mode))	stat.mFileType = cFSStatArgs::TY_DIR;
	else if(S_ISLNK(statbuf.st_mode))	stat.mFileType = cFSStatArgs::TY_SYMLINK;
	else if(S_ISBLK(statbuf.st_mode))	stat.mFileType = cFSStatArgs::TY_BLOCKDEV;
	else if(S_ISCHR(statbuf.st_mode))	stat.mFileType = cFSStatArgs::TY_CHARDEV;
	else if(S_ISFIFO(statbuf.st_mode))	stat.mFileType = cFSStatArgs::TY_FIFO;
	else if(S_ISSOCK(statbuf.st_mode))	stat.mFileType = cFSStatArgs::TY_SOCK;
	else								stat.mFileType = cFSStatArgs::TY_INVALID;
}

void cUnixFSServices::GetMachineName( TSTRING& strName ) const throw( eFSServices )
{
	struct utsname namebuf;
	if( uname( &namebuf ) == -1 )
		throw eFSServicesGeneric( strName );
	else
		strName = namebuf.nodename;
}

void cUnixFSServices::GetMachineNameFullyQualified( TSTRING& strName ) const
{
    char buf[256];
    if (gethostname(buf, 256) != 0)
    {
#ifdef SOLARIS_NO_GETHOSTBYNAME
        strName = buf;
        return;
#else
        struct hostent* ret;
        ret = gethostbyname(buf);
        
        if (ret != NULL)
        {
            strName = ret->h_name;
            return;
        }
#endif
    }

    try 
    {
        cUnixFSServices::GetMachineName(strName);
    }
    catch(eFSServices&)
    {
        strName = TSS_GetString(cCore, core::STR_UNKNOWN);
    }
}

bool cUnixFSServices::FileDelete( const TSTRING& strName ) const
{
    return( 0 == remove( strName.c_str() ) );
}

bool cUnixFSServices::GetCurrentUserName( TSTRING& strName ) const
{
    bool fSuccess = false;

    uid_t uid = getuid();
    struct passwd* pp = getpwuid( uid );

    if( pp )
    {
        strName = pp->pw_name;
        fSuccess = true;
    }
    else
		strName = _T("");

    return( fSuccess );
}


// returns IP address in network byte order
bool cUnixFSServices::GetIPAddress( uint32& uiIPAddress )
{
    bool    fGotAddress = false;    
    cDebug  d( _T("cUnixFSServices::GetIPAddress") );

    struct utsname utsnameBuf;    
    if( EFAULT != uname( &utsnameBuf) )
    {
        d.TraceDetail( "uname returned nodename: %s\n", utsnameBuf.nodename );

        struct hostent* phostent = gethostbyname( utsnameBuf.nodename );

        if( phostent )
        {
            ASSERT( AF_INET == phostent->h_addrtype );
            ASSERT( sizeof(int32) == phostent->h_length );

            if( phostent->h_length )
            {
                if( phostent->h_addr_list[0] )
                {
                    int32* pAddress = reinterpret_cast<int32*>( phostent->h_addr_list[0] );
                    uiIPAddress = *pAddress;
                    fGotAddress = true;
                }
                else
                    d.TraceError( _T("phostent->h_addr_list[0] was zero") ); 
            }
            else
                d.TraceError( _T("phostent->h_length was zero") ); 
        }
        else
            d.TraceError( _T("gethostbyname failed") );    
    }
    else
        d.TraceError( _T("uname failed") );

    return( fGotAddress );
}

bool cUnixFSServices::IsCaseSensitive() const
{
	return true;
}


bool cUnixFSServices::GetOwnerForFile( const TSTRING& tstrFilename, TSTRING& tstrUser ) const 
{
    bool fSuccess = true;
	struct stat statbuf;

    int ret = lstat(tstrFilename.c_str(), &statbuf);	
	if(ret < 0)
    {
        fSuccess = false;
    }

    if( fSuccess )
    {
        struct passwd* pp = getpwuid( statbuf.st_uid );
        //ASSERT( pp );
		// We shouldn't assert this, because it might be the case that a file
		// is associated with some old user that no longer exists... we should
		// not fail this case.  Instead, the method will just return false per
		// the test below.
	    if( pp == NULL )
        {
            fSuccess = false;
        }
        else
            tstrUser = pp->pw_name;
    }

    return( fSuccess );

}


bool cUnixFSServices::GetGroupForFile( const TSTRING& tstrFilename, TSTRING& tstrGroup ) const 
{    
    bool fSuccess = true;
	struct stat statbuf;

    int ret = lstat(tstrFilename.c_str(), &statbuf);	
	if(ret < 0)
    {
        fSuccess = false;
    }

    if( fSuccess )
    {
        struct group* pg = getgrgid( statbuf.st_gid );
        //ASSERT( pg ); this assert stops everything in debug mode if we can't lookup a groupid
    
	    if( pg == NULL )
        {
            fSuccess = false;
            tstrGroup = TSS_GetString(cCore, core::STR_UNKNOWN);
        }
        else
            tstrGroup = pg->gr_name;
    }	

    return( fSuccess );
}

////////////////////////////////////////////////////////////////////////
// Function name	: cUnixFSServices::ConvertModeToString
// Description	    : takes a TSTRING and fills it with an "ls -l" representation
//                    of the object's permission bits ( e.g. "drwxr-x--x" ).  
//
// Returns          : void -- no errors are reported
//
// Argument         : uint64 perm -- st_mode from "stat"
// Argument         : TSTRING& tstrPerm -- converted permissions, ls -l style
//
void cUnixFSServices::ConvertModeToString( uint64 perm, TSTRING& tstrPerm ) const
{   
    TCHAR szPerm[11]; //10 permission bits plus the NULL
	_tcscpy( szPerm, _T("----------") );

	ASSERT( sizeof(unsigned short) <= sizeof(uint32) );
	// We do this in case an "unsigned short" is ever larger than the
	// value we are switching on, since the size of the mode parameter
	// will be unsigned short (whatever that means, for the given platform...)

    // check file type
	switch ((uint32)perm & S_IFMT)	//some versions of Unix don't like to switch on
									//64 bit values.
    {
	    case S_IFDIR:
		    szPerm[0] = _T('d');
        break;
	    case S_IFCHR:
		    szPerm[0] = _T('c');
		break;
	    case S_IFBLK:
		    szPerm[0] = _T('b');
		break;
	    case S_IFIFO:
		    szPerm[0] = _T('p');
		break;
	    case S_IFLNK:
		    szPerm[0] = _T('l');
        break;
	}

    // check owner read and write
	if (perm & S_IREAD)
		szPerm[1] = _T('r');
	if (perm & S_IWRITE)
		szPerm[2] = _T('w');

    // check owner execute
	if (perm & S_ISUID && perm & S_IEXEC)
		szPerm[3] = _T('s');
	else if (perm & S_IEXEC)
		szPerm[3] = _T('x');
	else if (perm & S_ISUID)
		szPerm[3] = _T('S');

    // check group read and write
	if (perm & S_IRGRP)
		szPerm[4] = _T('r');
	if (perm & S_IWGRP)
		szPerm[5] = _T('w');

    // check group execute
	if (perm & S_ISGID && perm & S_IXGRP)
		szPerm[6] = _T('s');
	else if (perm & S_IXGRP)
		szPerm[6] = _T('x');
	else if (perm & S_ISGID)
		szPerm[6] = _T('l');

    // check other read and write
	if (perm & S_IROTH)
		szPerm[7] = _T('r');
	if (perm & S_IWOTH)
		szPerm[8] = _T('w');

    // check other execute
	if (perm & S_ISVTX && perm & S_IXOTH)
		szPerm[9] = _T('t');
	else if (perm & S_IXOTH)
		szPerm[9] = _T('x');
	else if (perm & S_ISVTX)
		szPerm[9] = _T('T');

    tstrPerm = szPerm;

	return;
}

////////////////////////////////////////////////////////////////////////
// Function name	: cUnixFSServices::Rename
// Description	    : Rename a file.  Overwrites newname if it exists.and overwrite is true
//
// Returns          : false if failure, true on success
bool cUnixFSServices::Rename(const TSTRING& strOldName, const TSTRING& strNewName, bool overwrite) const
{
#ifdef _UNICODE
#error UNICODE Rename not implemented
#endif

    // delete new file if overwriting
    if ( overwrite )
        if ( access( strNewName.c_str(), F_OK ) == 0 && remove( strNewName.c_str() ) != 0 )
            return false;
    
    if ( rename( strOldName.c_str(), strNewName.c_str() ) == 0 )
        return true;

    // Note: errno will be set
    return false;
}


bool cUnixFSServices::GetExecutableFilename( TSTRING& strFullPath, const TSTRING& strFilename ) const
{
    bool fGotName = false;

    if( strFilename.empty() )
        return false;

    // if there is a slash in the filename, it's absolute or relative to cwd
    if( TSTRING::npos != strFilename.find( _T('/') ) )
    {
        // if absolute path
        if( strFilename[0] == _T('/') )
        {
            strFullPath = strFilename;
            fGotName = true;
        }
        else // is relative path; find path from cwd
        {
            fGotName = FullPath( strFullPath, strFilename );
        }
    }
    else // it's just a filename: should be found in path
    {
        fGotName = util_PathFind( strFullPath, strFilename );

        TSTRING strFP;
        if( fGotName && FullPath( strFP, strFullPath ) )
            strFullPath = strFP;
    }

    return( fGotName );
}


///////////////////////////////////////////////////////////////////////////////
// Function name	: cUnixFSServices::FullPath
// Description	    : 
//
// Return type		: bool 
// Argument         :  TSTRING& strFullPath
// Argument         : const TSTRING& strRelPathC
// Argument         : const TSTRING& pathRelFromC
//
// TODO -- is throwing an exception the more appropriate alternative to returning
//		a bool? I think it is ... mdb
///////////////////////////////////////////////////////////////////////////////
bool cUnixFSServices::FullPath( TSTRING& strFullPath, const TSTRING& strRelPathC, const TSTRING& pathRelFromC ) const
{    
    // don't do anything with an empty path
    if( strRelPathC.empty() )
        return false;

    TSTRING strRelPath = strRelPathC; // make non-const temp var

    //
    // get base name (where strRelPath will be relative to), which will either be;
    //  1. the root directory if strRelPath is an absolute path
    //  2. pathRelFrom if it's not empty
    //  3. otherwise ( not abs path AND no rel path ) the current working directory
    //

    if( strRelPath[0] == TW_SLASH ) // if is absolute path
	{
        if( IsRoot( strRelPath ) ) // if it's root, don't monkey with it, just return it.
        {
            strFullPath = strRelPath;
            return true;
        }
        else
        {
		    strFullPath = _T(""); // push root, then add path elements from strRelPathC
                                  // one by one (in while loop below)
        }
	}
    else // is a relative path, so check pathRelFromC
    {
        if( pathRelFromC.empty() ) // if we're relative to CWD...
        {
	        //
	        // get the current working directory
	        //
	        try
	        {
		        GetCurrentDir( strFullPath );
                util_TrailingSep( strFullPath, false );
	        }
	        catch( eFSServices& )
	        {
		        return false;
	        }
        }
        else // we're relative to a given dir
        {
            strFullPath = pathRelFromC;
            util_RemoveDuplicateSeps( strFullPath );
            util_TrailingSep( strFullPath, false );
        }
    }

    //
    // start adding path elements from strRelPath to the base name 
    // ( which already has an absolute starting point.  see above. )
    //

    TSTRING strElem;
    int index = 0;
    while( util_GetNextPathElement( strRelPath, strElem, index++ ) )
    {
        if( 0 == strElem.compare( _T(".") ) )
        {
            // ignore it
        }
        else if( 0 == strElem.compare( _T("..") ) )
        {
            // go up a dir ( the function takes care of root dir case )
            TSTRING strDummy;
            util_RemoveLastPathElement( strFullPath, strDummy );
        }
        else // just a regular old path element
        {
            strFullPath += TW_SLASH;
            strFullPath += strElem;
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// GetStandardBackupExtension() 
//
// Returns normal string to append to backup files for this os.
// (e.g. "~" for unix and ".bak" for winos)
///////////////////////////////////////////////////////////////////////////////
TCHAR* cUnixFSServices::GetStandardBackupExtension() const
{
    return _T(".bak");
}


    // TODO: remove this function
    // Matt theorized that this is no longer used - dmb Aug 23 1999
/*
int cUnixFSServices::CreateLockedTemporaryFile( const TCHAR* szFilename, int perm ) const
{    
    // make sure perm is AT LEAST one of: O_RDWR, O_WRONLY
    ASSERT( 0 != ( perm & ( O_RDWR | O_WRONLY ) ) );
    // make sure perm is ONLY composed of: O_RDWR, O_WRONLY
    ASSERT( 0 == ( perm & ~( O_RDWR | O_WRONLY ) ) );
    // get rid of any unsupported bits caller may have supplied
    perm &= ( O_RDWR | O_WRONLY );

    // set flags
    int oflags = perm | 
                 O_CREAT | O_EXCL;    // only create a new file -- error if it exists already

    // create file
	int fh = _topen( szFilename, oflags, 0666 );    
	if( fh >= 0 )
    {
        // file was created.  Now unlink it
        if( 0 != unlink( szFilename ) )
        {
            // we weren't able to unlink file, so close handle and fail
            close( fh );
            fh = -1;
        }
    }
        
    return( fh );
}
*/

void cUnixFSServices::Sleep( int nSeconds ) const
{
    sleep( nSeconds );
}


////////////////////////////////////////////////////////////////////////////////
// Function name	: IsRoot
// Description	    : A root path is all '/'s
//
// Return type		: bool 
// Argument         : const TSTRING& strPath
///////////////////////////////////////////////////////////////////////////////
bool cUnixFSServices::IsRoot( const TSTRING& strPath ) const
{
    // and empty path is NOT the root path
    if( strPath.empty() )
        return false;

    // check to see if all characters are a slash
    for( TSTRING::const_iterator iter = strPath.begin(); iter != strPath.end(); iter++ )
    {
        // if we've found a char that's not '/', then it's not the root path
        if( *iter != TW_SLASH )
            return false;
    }

    return true;
}

//*************************************************************************
//*************************************************************************
//                      UTIL FUNCTION CODE
//*************************************************************************
//*************************************************************************


///////////////////////////////////////////////////////////////////////////////
// Function name	: util_PathFind
// Description	    : 
//      takes single-element executible filename and looks in path env var for it
//      assumes path is colon-delimited string of directories.
//
// Return type		: bool 
// Argument         :  TSTRING& strFullPath
// Argument         : const TSTRING& strFilename
///////////////////////////////////////////////////////////////////////////////
bool util_PathFind( TSTRING& strFullPath, const TSTRING& strFilename )
{
    bool fFoundFile = false;
    
    if( strFilename.empty() )
        return false;

    //
    // get the path environment variable
    //
    TCHAR* pszPathVar = _tgetenv("PATH");
    if( pszPathVar != NULL )
    {
        //
        // cycle over characters in path looking for the ':'
        // 
        TSTRING strCurPath;
        TCHAR* pchTemp = pszPathVar;        
        bool fMorePaths = true;
        do // while still more paths and haven't found file
        {
            //
            // are we at the ':'?
            //             
            if( *pchTemp && *pchTemp != _T(':') ) // if we're not at the end of the path
            {
                strCurPath += *pchTemp;
            }
            else // we have found the ':'
            {
                //
                // expand current path into a fully qualified path
                // if it's empty, use current directory
                //
                TSTRING strFP;
                if( strCurPath.empty() )
                    strCurPath = _T(".");
                if( iFSServices::GetInstance()->FullPath( strFP, strCurPath ) )
                    strCurPath = strFP;

                //
                // put the file together with the path dir
                //
                TSTRING strFullName = strCurPath;
                util_TrailingSep( strFullName, true );
                strFullName += strFilename;

                //
                // the file must exist and be executable
                //
                if( util_FileIsExecutable( strFullName ) )
                {
                    strFullPath = strFullName;
                    fFoundFile = true;
                }
                else
                    strCurPath.erase(); // start over
            }

            //
            // keep searching if we're not at the end of the path string
            //

            if( *pchTemp )
                pchTemp++;
            else
                fMorePaths = false;
        }
        while( !fFoundFile && fMorePaths );
    }

    return( fFoundFile );
}


///////////////////////////////////////////////////////////////////////////////
// Function name	: util_FileIsExecutable
// Description	    : file ( or file a link points to ) must be a regular 
//                    file and executable by someone
//
// Return type		: bool 
// Argument         : const TSTRING& strFile
///////////////////////////////////////////////////////////////////////////////
bool util_FileIsExecutable( const TSTRING& strFile )
{
    if( strFile.empty() )
        return false;
    
    struct stat s;
	if( stat( strFile.c_str(), &s ) < 0 ) // this call handles links
        return false;

    return( S_ISREG( s.st_mode ) && ( s.st_mode & ( S_IXUSR | S_IXGRP | S_IXOTH ) ) ); // can someone execute it?
}


////////////////////////////////////////////////////////////////////////////////
// Function name	: util_RemoveDuplicateSeps
// Description	    : 
// takes all adjacent slashes and replaces them with a single slash
//      ///root//foo -> /root/foo
//      rel//foo///  -> rel/foo/
//
// Return type		: void 
// Argument         : TSTRING& strPath
///////////////////////////////////////////////////////////////////////////////
void util_RemoveDuplicateSeps( TSTRING& strPath )
{
    bool fLastCharWasSep = false;
    TSTRING::iterator iter = strPath.begin(); 
    while( iter != strPath.end() )
    {
        bool fErasedChar = false;
        // if we've found a char that's not '/', then it's not the root
        if( *iter == TW_SLASH )
        {
            // if this char is a duplicate sep, erase it
            if( fLastCharWasSep )
            {
                iter = strPath.erase( iter );
                fErasedChar = true;
            }

            fLastCharWasSep = true;
        }
        else
        {
            fLastCharWasSep = false;
        }

        // don't go past end of string (could happen with erase)
        if( ! fErasedChar )
            iter++;
    }
}


//////////////////////////////////////////////////////////////////////////////////
// Function name	: util_RemoveLastPathElement
// Description	    : 
//      effectively pops off a path element from the end, except for the root dir, where it does nothing
//      it removes any slashes before and after the element
//      ///root//foo/    -> leaves "///root" ("foo"  is strElem)
//      ///root          -> leaves ""        ("root" is strElem)
//      //               -> leaves ""        (""     is strElem)
//
// Return type		: void 
// Argument         :  TSTRING& strPath
// Argument         : TSTRING& strElem
/////////////////////////////////////////////////////////////////////////////////
void util_RemoveLastPathElement( TSTRING& strPath, TSTRING& strElem )
{
    
    // remove all trailing separators
    util_RemoveTrailingSeps( strPath );

    // find the last separator
    TSTRING::size_type lastSep = strPath.rfind( TW_SLASH );

    // if separator was found, take all chars after it
    if( lastSep != TSTRING::npos )
    {
        strElem = strPath.substr( lastSep + 1 );
        strPath.resize( lastSep + 1 );
    }
    else // no seps in name, take whole string
    {
        // last element
        strElem = strPath;
        strPath.erase();
    }
    
    // remove all trailing separators
    util_RemoveTrailingSeps( strPath );
}


////////////////////////////////////////////////////////////////////////////////////
// Function name	: util_GetNextPathElement
// Description	    : 
//      starting from the left side of the path string, returns the index'th path element
//      returns true if the element exists, false if there aren't <index + 1> many elements
//
//      index is ZERO BASED
//
//      2rd element of   ABC/DEF/GH -> GH
//      1st element of //ABC/DEF/GH -> DEF
//
// Return type		: bool : got path element? ( i.e. was there index path elements? )
// Argument         : const TSTRING& strPathC
// Argument         : TSTRING& strElem
// Argument         : int index
/////////////////////////////////////////////////////////////////////////////////
bool util_GetNextPathElement( const TSTRING& strPathC, TSTRING& strElem, int index )
{

    // don't do anything if root or empty
    if( strPathC.empty() || iFSServices::GetInstance()->IsRoot( strPathC ) )
        return false;

    TSTRING strPath = strPathC; // writable local version
    
    bool fMoreSeps = true;
    TSTRING::size_type firstSep, nextSep, nextNonSep;
    firstSep = nextSep = nextNonSep = (TSTRING::size_type)-1;
    for( int i = 0; i <= index && fMoreSeps; i++ )
    {        
        // go past leading separators
        nextNonSep = strPath.find_first_not_of( TW_SLASH, nextSep + 1 );

        if( nextNonSep != TSTRING::npos )
        {
            // find index'th slash (start of index'th element)
            nextSep = strPath.find( TW_SLASH, nextNonSep );

            // if we're at the end and we haven't found the index'th element
            // left, then tell the caller that there aren't that many elemnts
            if( nextSep == TSTRING::npos && i < index )
                fMoreSeps = false;
        
        }
        else
            fMoreSeps = false;
    }

    // get the element and remove it from the path
    if( fMoreSeps )
        strElem = strPath.substr( nextNonSep, nextSep - nextNonSep );

    return( fMoreSeps );
}

/////////////////////////////////////////////////////////////////////////
// Function name	: util_TrailingSep
// Description	    : ensure that a path ( fLeaveSep ? "has" : "does not have" ) a trailing slash
//
// Return type		: bool : was there a trailing slash?
// Argument         : TSTRING& str
// Argument         : bool fLeaveSep
/////////////////////////////////////////////////////////////////////////////////
bool util_TrailingSep( TSTRING& str, bool fLeaveSep )
{
    bool fWasSep = false;

    // if there's a trailing sep
    if( 
        ! str.empty()
        &&
        str[ str.size() - 1 ] == TW_SLASH
      )
    {
        if( ! fLeaveSep )
            str.resize( str.size() - 1 );
        fWasSep = true;
    }
    else // else no trailing sep
    {
        if( fLeaveSep )
            str += TW_SLASH;
        fWasSep = false;
    }

    return( fWasSep );
}

/////////////////////////////////////////////////////////////////////////
// Function name	: util_RemoveTrailingSeps
// Description	    : removes all trailing separators
//
// Return type		: void 
// Argument         : TSTRING& str
/////////////////////////////////////////////////////////////////////////////////
void util_RemoveTrailingSeps( TSTRING& str )
{
    while( util_TrailingSep( str, false ) )
    {}
}

template< typename T > static inline void util_ZeroMemory( T& obj )
{
    memset( &obj, 0, sizeof( obj ) );
}

