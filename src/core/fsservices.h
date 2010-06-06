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
// fsservices.h
//
// iFSServices -- interface to the file system services class
//
// objective:
//    abstract all file system calls, except for reading/writing
//       to elaborate: this class does not handle any file I/O -- we 
//       intend to use streams to abstract out file I/O
//    abstract case sensitivity of underlying file system
//

#ifndef __FSSERVICES_H
#define __FSSERVICES_H

//=========================================================================
// INCLUDES
//=========================================================================

#ifndef __TYPES_H
#include "types.h"
#endif
#ifndef __TCHAR_H
#include "core/tchar.h"
#endif
#ifndef __DEBUG_H
#include "debug.h"
#endif
#ifndef __FILEERROR_H
#include "fileerror.h"
#endif

//=========================================================================
// STANDARD LIBRARY INCLUDES
//=========================================================================

#if IS_UNIX
#include <sys/param.h>
#endif

//=========================================================================
// DEFINES AND MACROS
//=========================================================================

// macros for extracting the major and minor portions of int64's:    
#if !defined(major)
#if !USES_GLIBC
#define major(x) ((int)((x) >> 8) & 0xff)
#define minor(x) ((int)((x) & 0xff))
#else
#ifdef WORDS_BIGENDIAN
#define major(x) (int)(((char*)&(x))[2])
#define minor(x) (int)(((char*)&(x))[3])
#else
#define major(x) (int)(((char*)&(x))[1])
#define minor(x) (int)(((char*)&(x))[0])
#endif
#endif
#endif /* !major */

//=========================================================================
// TYPEDEFS
//=========================================================================

typedef int64  cFSTime;
typedef int64  cFSType;

//=========================================================================
// GLOBALS
//=========================================================================

//=========================================================================
// FORWARD DECLARATIONS
//=========================================================================

//=========================================================================
// DECLARATION OF CLASSES
//=========================================================================

// filesystem access control lists
//     it is the union of MAX(elem) for all the file systems that we support 
class cACLElem {
   // TODO this is just a place holder
   uint32   mUid;
};

// this class is used only to pass arguments to iFSServices
//     it is the union of MAX(elem) for all the file systems that we support 
struct cFSStatArgs {
   enum FileType  {
      TY_INVALID,       // lazy evaluation
      TY_FILE, 
      TY_DIR, 
      TY_BLOCKDEV, 
      TY_CHARDEV,
      TY_SYMLINK,
      TY_FIFO,
      TY_SOCK
   };

   // attr is fs dependent?
   uint64   dev;        // dep
   int64 ino;        // dep
   int64 mode;    // dep
   int64 nlink;      // indep
   int64 uid;        // dep
   int64 gid;        // dep
   uint64   rdev;    // dep
   int64 size;    // indep
   cFSTime  atime;      // indep
   cFSTime  mtime;      // indep
   cFSTime  ctime;      // indep
   int64 blksize;    // indep
   int64 blocks;     // dep
   int64 fstype;     // dep
    TSTRING usid;        // dep
    TSTRING gsid;        // dep
// int64 mFileType;     // Matt's addition...

   FileType mFileType;     // redundant with other information in this struct, but
                     // broken out for convenience

   // access control list should go here, too
   std::list <cACLElem> mACL; // indep
};



//=========================================================================
//
// eFSServices -- exception class
//
//=========================================================================

TSS_FILE_EXCEPTION( eFSServices,         eFileError );
TSS_FILE_EXCEPTION( eFSServicesGeneric,   eFSServices );

//=========================================================================
//
// iFSServices -- abstracts all file system calls, except for opening, reading,
//    and writing, which will be handled by standard c++. 
//    NOTE -- all paths returned from this class will be delimited with a '/'
//
//=========================================================================

class iFSServices 
{
 public:
    
  ///////////////////////////////////////////////////////////////
  // ENUMS
  ///////////////////////////////////////////////////////////////

  ////////////////////////////////////////
  // file creation modes
  ////////////////////////////////////////
  enum Mode
  {
    MODE_DEFAULT        = 0,
    MODE_READ           = 1,
    MODE_WRITE          = 2,
    MODE_SHARE_DELETE      = 4,
    MODE_SHARE_READ        = 8,
    MODE_SHARE_WRITE    = 16,
    MODE_CREATE_EXCL    = 32,
    MODE_CREATE_TRUNCATE   = 64
  };

  ////////////////////////////////////////
  // maximum path length on platform
  ////////////////////////////////////////
  enum
  {        
#if IS_UNIX
    TW_MAX_PATH = MAXPATHLEN
#endif
  };

  ///////////////////////////////////////////////////////////////
  // MEMBER FUNCTIONS
  ///////////////////////////////////////////////////////////////

    
  ////////////////////////////////////////
  // platform specific functions
  ////////////////////////////////////////
  virtual bool    IsCaseSensitive() const = 0;
  // returns true if the file system is case sensitive
  virtual TCHAR       GetPathSeperator() const = 0;
  // returns "/" for unix and "\\" for win32
  virtual TCHAR*      GetStandardBackupExtension() const = 0;
  // returns normal string to append to backup files for this os.
    
  ////////////////////////////////////////
  // process functions
  ////////////////////////////////////////
  virtual void        Sleep( int nSeconds ) const = 0;
  // makes the current process sleep for the specified number of seconds

    
  ////////////////////////////////////////
  // major filesystem functions
  ////////////////////////////////////////
  virtual void       Stat( const TSTRING& strFileName, cFSStatArgs& pStat ) const throw( eFSServices ) = 0;
  // fills out the cFSStatArgs structure with the stat info for the named file
  virtual void       GetTempDirName( TSTRING& strName ) const throw( eFSServices ) = 0;
  // makes directory if it doesn't exist already.  Dirname will end with a delimiter ( '/' )
    
  virtual void       SetTempDirName( TSTRING& tmpName ) = 0;
  
  virtual TSTRING&    MakeTempFilename( TSTRING& strName ) const throw( eFSServices ) = 0;
  // create temporary file
  //      TSTRING must have the form ("baseXXXXXX"), where the X's are replaced with 
  //      characters to make it a unique file.  There must be at least 6 Xs.

  // TODO: remove this function
  // Matt theorized that this is no longer used - dmb Aug 23 1999
  //    virtual int         CreateLockedTemporaryFile( const TCHAR* szFilename, int perm ) const = 0;
  // creates a temporary file to which only the current process has read or write access.  
  // returns an open C file descriptor on success and -1 on error.  Returns error if filename already exists on the filesystem.
  // the file will automatically be deleted when the file descriptor is closed.
  // perm should be zero or more of:  
  //      O_WRONLY: create with read write only permission
  //      O_RDWR: create with read and write permission


  ////////////////////////////////////////
  // minor filesystem functions
  ////////////////////////////////////////
  virtual void        GetHostID( TSTRING& name ) const = 0;

  virtual void    GetMachineName( TSTRING& name ) const throw(eFSServices) = 0;

  virtual void    GetMachineNameFullyQualified( TSTRING& name ) const = 0;

  virtual bool        GetCurrentUserName( TSTRING& tstrName ) const = 0;

  virtual bool        GetIPAddress( uint32& uiIPAddress ) = 0;


  ////////////////////////////////////////
  // directory specific functions
  ////////////////////////////////////////
  virtual void        ReadDir( const TSTRING& strName, std::vector<TSTRING> &vDirContents, bool bFullPaths = true ) const throw( eFSServices ) = 0;
  // puts the contents of the specified directory, except for . and .., into the supplied vector. 
  // if bFullPaths is true, then the vector contains fully qualified path names; otherwise, it only contains the 
  // short names.
  virtual void        GetCurrentDir( TSTRING& strCurDir ) const throw( eFSServices ) = 0;
  // returns the current working directory
  virtual void        ChangeDir( const TSTRING& strName ) const throw( eFSServices ) = 0;
  // sets the current working directory
  virtual void    Mkdir( const TSTRING& strName ) const throw( eFSServices ) = 0;

  virtual bool        Rmdir( const TSTRING& strName ) const = 0;


  ////////////////////////////////////////
  // file specific functions
  ////////////////////////////////////////
  virtual bool        FileDelete( const TSTRING& name ) const = 0;

    
  ////////////////////////////////////////
  // directory and file functions
  ////////////////////////////////////////
  virtual bool    Rename( const TSTRING& strOldName, const TSTRING& strNewName, bool fOverWrite = true ) const = 0;
  // rename a file
  virtual bool        GetOwnerForFile( const TSTRING& tstrFilename, TSTRING& tstrUser ) const = 0;

  virtual bool        GetGroupForFile( const TSTRING& tstrFilename, TSTRING& tstrGroup ) const = 0;


  ////////////////////////////////////////
  // miscellaneous utility functions
  ////////////////////////////////////////
  virtual void        ConvertModeToString( uint64 perm, TSTRING& tstrPerm ) const = 0;
  // takes a int64 permission (from stat) and changes it to look like UNIX's 'ls -l' (e.g. drwxrwxrwx)
  virtual bool    FullPath( TSTRING& fullPath, const TSTRING& relPath, const TSTRING& pathRelFrom = _T("") ) const = 0;
  // converts relPath into a fully qualified path, storing it in FullPath. If this 
  // fails, false is returned.  if the path to which relPath is relative is not CWD, put it in pathRelFrom.
  // TODO: In some places we have depended on the behaviour that if relPath.empty() == true then we
  // fail or return an empty string.  Should we add this behaviour to the interface?
  virtual bool        GetExecutableFilename( TSTRING& strFullPath, const TSTRING& strFilename ) const = 0;
  // get the path to the current executable file
  virtual bool        IsRoot( const TSTRING& strPath ) const = 0;
  // returns true if strPath denotes a root path


  ////////////////////////////////////////
  // error functions
  ////////////////////////////////////////
  virtual TSTRING     GetErrString() const = 0;
  // Returns an error string that is appropriate for the system, (e.g. errorstr(errno) 
  // on UNIX and FormatError( ..., GetLastError(), ...) in Win32
  // Call this immediately after a failed system call to get a string
  // representation of the error event.


  ////////////////////////////////////////
  // singleton manipulation
  ////////////////////////////////////////
  static iFSServices*         GetInstance();
  static void                 SetInstance( iFSServices* pInst );

    
  ///////////////////////////////////////////////////////////////
  // PRIVATE DATA
  ///////////////////////////////////////////////////////////////
 private:
  static iFSServices* mpInstance;
    
  //virtual cFSStatArgs::FileType GetFileType(const cFCOName &filename) throw(eFSServices) = 0;
  // returns the type of the file specified by filename. Separated from Stat() for convenience
  // and potential efficiency (if a file system doesn't need to do a stat to get the file type)
    
  //virtual char*&   MakeTempFile( char*& name) throw(eFSServices) = 0;
  //virtual wchar_t*&   MakeTempFile( wchar_t*& name) throw(eFSServices) = 0;

  //
  // file ops
  //
  // these are commented out for timeliness reasons; we should uncomment and implement as needed...
  //     -- mdb
  /*
    virtual int         Create(cFSName &name, Mode mode = MODE_DEFAULT) = 0;
    virtual bool     Mkdir(cFSName &name, Mode mode = MODE_DEFAULT) = 0;
    virtual bool     Delete(cFSName &name) = 0;
    virtual bool     Link(cFSName &src, cFSName &dest) = 0; 
   
    virtual bool     Chmod(cFSName &name, Mode mode) = 0;
    virtual Mode     Umask(Mode mode) = 0;
    // file system types (e.g., "FAT", "NTFS", "UFS", "NFS") 
    virtual bool     FSTypeAsString(cFSName &name, TSTRING &str) = 0;
    // file type (e.g., "file", "symbolic link", "block device")
    // TODO -- this should either be static or in another utility class, since it does not rely on the
    //      instance of the class (there is a global enumeration of file types; this fn should take a
    //      cFSStatArgs::FileType instead of a name.
    virtual bool     FileTypeAsString(cFSName &filename, TSTRING &str) = 0; 
    // TODO -- does this beling here? Is it always true that st_dev defines the file system? If not, then the
    //      Stat() call should add some extra piece of data that identifies the file system.
    virtual bool     IsSameFileSystem(cFSStatArgs &file1, cFSStatArgs &file2) = 0;
    // to determine whether to recurse - musn't traverse mount points

    // capabilities
    virtual bool     IsUnicodeFilename(cFSName &name) = 0;
    virtual bool     IsACLCapable(cFSName &name) = 0;
    virtual bool     Is8bitFilename(cFSName &name) = 0;
  */
};


//=========================================================================
// INLINE FUNCTIONS
//=========================================================================
inline iFSServices* iFSServices::GetInstance()
{
   ASSERT(mpInstance != 0);

   return mpInstance;
}

inline void iFSServices::SetInstance( iFSServices* pInst )
{
   mpInstance = pInst;
}


#endif

