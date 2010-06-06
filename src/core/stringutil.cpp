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
/*
* Name....: stringutil.cpp
* Date....: 09/02/99
* Creator.: 
*
* This module implements the cStringUtil class and helpers. This class,
* primarily, is used to convert between various character formats. The
* supported formats include:
*
*       NTWCS - using 32-bit wchar_t
*       NTWCS - using 16-bit wchar_t
*       NTMBS - using integral char's
*
* PLEASE NOTE:
*   Refrain from using the StrToTStr, TStrToStr, WStrToTStr, and
*   etc calls and instead use the type dispatched member:
*
*       DestT::const_iterator Convert( DestT&, const SrcT& );
*
*   It can be used like so:
*
*       TSTRING tstr;
*       TCOUT << cStringUtil::Convert( tstr, sUnknownType );
*
*   This call makes our code more portable and extensible by not embedding
*   type selection in the names of interfaces. For example:
*
*       std::string s1 = cStringUtil::TStrToStr( TSTRING( "Huh" ) );
*
*   Can be rewritten as:
*
*       std::string s1;
*       cStringUtil::Convert( s1, TSTRING( "Huh" ) );
*/

/// Requirements

#include "stdcore.h"            // for: pch

#include "stringutil.h"         // for: These Declarations
#include "debug.h"              // for: Debugging Interface
#include "codeconvert.h"        // for: iconv abstractions
#include "ntmbs.h"              // for: eCharacterEncoding
#include "hashtable.h"



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// String Cache
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// TODO:BAM -- use these for mb -> UCS2 conversions as well?

class tss_hash_key_convert
{
public:
    const byte* operator()( const wc16_string& s, int* const pcbKeyLen )
    {
        *pcbKeyLen = sizeof(WCHAR16) * s.length();
	    return (byte*)s.c_str();
    }
};

class tss_hash_key_compare
{
public:
    bool operator()( const wc16_string& lhs, const wc16_string& rhs )
    {
	    return ( lhs.compare( rhs ) == 0 );
    }
};

namespace /* Module Local */
{
    typedef
        cHashTable< wc16_string,
                    TSTRING, 
                    tss_hash_key_compare, 
                    tss_hash_key_convert >
        hashtable_t;
 
    hashtable_t& tss_GetHashTable()
    {
        static hashtable_t s_table;
        return s_table;
    }
}//Anon.
 

inline void tss_insert_in_hash( const wc16_string& lhs, const TSTRING& rhs )
{
    tss_GetHashTable().Insert( lhs, rhs );
}

inline bool tss_find_in_hash( const wc16_string& lhs, TSTRING& rhs )
{    
    return( tss_GetHashTable().Lookup( lhs, rhs ) );
}

#ifdef _UNICODE

inline bool tss_find_in_hash( const wc16_string& lhs, std::string& rhs )
{
    return false; // don't have a hash table for this!
}

inline void tss_insert_in_hash( const wc16_string& lhs, const std::string& rhs )
{
    return; // don't have a hash table for this!
}

#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Type Dispatched Conversions
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*static*/
std::string::const_iterator
cStringUtil::Convert( std::string& nbs, const wc16_string& dbs )
{
    cDebug d("cStringUtil::Convert( char, w16 )");

    ASSERT( (void*)nbs.c_str() != (void*)dbs.c_str() );

    if ( dbs.empty() )
        nbs.erase();
    else
    {
        if( tss_find_in_hash( dbs, nbs ) )
        {
            d.TraceNever( _T("found string in hash table\n") );
            return nbs.begin();
        }
        else
        {
            d.TraceNever( _T("didn't find string in hash table\n") );
        }

        /*
        d.TraceDebug( _T("converting: \n") );
        for( int i = 0; i < dbs.size(); i++ )
            d.TraceDebug( _T("0x%04X\n"), dbs[i] );
        d.TraceDebug( _T("----\n") );
        */

        nbs.resize( dbs.size() * MB_CUR_MAX );

        int nWrote = 
            iCodeConverter::GetInstance()->Convert( 
                (char*)nbs.c_str(), nbs.size(),
                dbs.c_str(), dbs.size() ); // c_str() because must be null terminated

        d.TraceDetail( _T("iCodeConverter returned %d\n"), nWrote );

        if ( nWrote == -1 )
            throw eCharacterEncoding( TSS_GetString( cCore, core::STR_ERR_BADCHAR ) );

        nbs.resize( nWrote ); 
        
        tss_insert_in_hash( dbs, nbs );
    }


    return nbs.begin();
}

/*static*/
wc16_string::const_iterator
cStringUtil::Convert( wc16_string& dbs, const std::string& nbs )
{
    cDebug d("cStringUtil::Convert( w16, char )");

    if (nbs.size() != 0)
    {
        dbs.resize( nbs.size() );
    
        // d.TraceDebug( "converting <%s>\n", nbs.c_str() );

        int nWrote = 
            iCodeConverter::GetInstance()->Convert( 
                (WCHAR16*)dbs.c_str(), dbs.length(),
                nbs.c_str(), nbs.size() ); // c_str() because must be null terminated
    
        d.TraceDetail( _T("iCodeConverter returned %d\n"), nWrote );
    
        if ( nWrote == -1 )
        {
            throw eCharacterEncoding( TSS_GetString( cCore, core::STR_ERR_BADCHAR ) );
        }

        dbs.resize( nWrote ); 
    
        /*
        d.TraceDebug( _T("converted to: \n") );
        for( int i = 0; i < dbs.size(); i++ )
            d.TraceDebug( _T("0x%04X\n"), dbs[i] );
        d.TraceDebug( _T("----\n") );
        */
    }
    else
        dbs.resize(0);

    return dbs.begin();
}

#if SUPPORTS_WCHART && WCHAR_REP_IS_UCS2

/*static*/
std::wstring::const_iterator
cStringUtil::Convert( std::wstring& wcs, const wc16_string& dbs )
{
    if (dbs.size() != 0)
    {
        if( tss_find_in_hash( dbs, wcs ) )
            return wcs.begin();

        wcs.resize( dbs.size() );
        std::copy( dbs.begin(), dbs.end(), wcs.begin() );
        
        tss_insert_in_hash( dbs, wcs );
    }
    else
        wcs.erase();

    return wcs.begin();
}

/*static*/
wc16_string::const_iterator
cStringUtil::Convert( wc16_string& dbs, const std::wstring& wcs )
{
    if (wcs.size() != 0)
    {
        dbs.resize( wcs.size() );
        std::copy( wcs.begin(), wcs.end(), dbs.begin() );
    }
    else
        dbs.resize(0);

    return dbs.begin();
}

/*static*/
std::wstring::const_iterator
cStringUtil::Convert( std::wstring& wcs, const std::string& nbs )
{
    if (nbs.size() != 0)
    {
        wcs.resize( nbs.size() );
    
        size_t nWrote = ::mbstowcs( (wchar_t*)wcs.c_str(), nbs.c_str(), wcs.size() + 1 );
        if ( nWrote == (size_t)-1 )
            throw eCharacterEncoding( TSS_GetString( cCore, core::STR_ERR_BADCHAR ) );
    
        wcs.resize( nWrote );
    }
    else
        wcs.erase();

    return wcs.begin();
}

/*static*/
std::string::const_iterator
cStringUtil::Convert( std::string& nbs, const std::wstring& wcs )
{
    if (wcs.size() != 0)
    {
        if( tss_find_in_hash( wc16_string( wcs.c_str() ), nbs ) )
            return nbs.begin();

        nbs.resize( MB_CUR_MAX * wcs.size() ); 
        size_t nWrote = ::wcstombs( (char*)nbs.c_str(), wcs.c_str(), nbs.size() + 1 );
        if ( nWrote == (size_t)-1 )
            throw eCharacterEncoding( TSS_GetString( cCore, core::STR_ERR_BADCHAR ) );
    
        nbs.resize( nWrote );
        
        tss_insert_in_hash( wc16_string( wcs.c_str() ), nbs );
    }
    else
        nbs.erase();

    return nbs.begin();
}

#endif // SUPPORTS_WCHART && WCHAR_REP_IS_UCS2


/// TSTRING and Type Knowledge Required

///////////////////////////////////////////////////////////////////////////////
// TstrToStr
///////////////////////////////////////////////////////////////////////////////

std::string
cStringUtil::TstrToStr( const TSTRING& tstr )
{
    std::string s;
    cStringUtil::Convert( s, tstr );
    return s;
}

///////////////////////////////////////////////////////////////////////////////
// StrToTstr
///////////////////////////////////////////////////////////////////////////////
TSTRING cStringUtil::StrToTstr( const std::string& rhs )
{
    TSTRING lhs;
    cStringUtil::Convert( lhs, rhs );
    return lhs;
}

///////////////////////////////////////////////////////////////////////////////
// WstrToTstr
///////////////////////////////////////////////////////////////////////////////
TSTRING cStringUtil::WstrToTstr(const wc16_string& rhs )
{ 
    TSTRING lhs;
    cStringUtil::Convert( lhs, rhs );
    return lhs;
}   

///////////////////////////////////////////////////////////////////////////////
// TstrToWstr   
///////////////////////////////////////////////////////////////////////////////
wc16_string cStringUtil::TstrToWstr( const TSTRING& rhs )   
{  
    wc16_string lhs;
    cStringUtil::Convert( lhs, rhs );
    return lhs;
}

///////////////////////////////////////////////////////////////////////////////
// StringIsInteger -- returns true if the string is a length of at least one 
//      and contains nothing other than [0-9].
//      Used to verify a string is acceptable to pass to atoi()
///////////////////////////////////////////////////////////////////////////////
#ifdef OLDCODE
bool cStringUtil::StringIsInteger(TCHAR* pStr)
{
    int len = _tcslen(pStr);

    if (len <= 0)
        return false;

    for (int i = 0; i < len; ++i)
    {
        if (pStr[i] < _T('0') || pStr[i] > _T('9'))
            return false;
    }

    return true;
}
#endif//OLDCODE

// eof: stringutil.cpp

/*


typedef wchar_t dbchar_t;
typedef char    mbchar_t;

class cMBUCS2Cache
{
public:
    cMBUCS2Cache();
    ~cMBUCS2Cache();

    bool Convert(dbchar_t& ret, mbchar_t* pMBChar); 
        // returns false if conversion could not be done
    void Add(mbchar_t* pMBchar, int mblen);
        // add a mb char def to the chache

protected:
    struct Element
    {
        union 
        {
            Element* mpNext; // points to another Element[256]
            dbchar_t mUCS2;
        };

        enum ElementDefinition { UNDEFINED, UCS2, CONTINUES };
        short mElementDefinition;  

        Element() { mElementDefinition = UNDEFINED; }
        ~Element() { if (mElementDefinition == CONTINUES) delete mpNext; }
    };

    Element mTop[256];
};


cMBUCS2Cache::cMBUCS2Cache()
{
}

cMBUCS2Cache::~cMBUCS2Cache()
{
}

// returns false if conversion could not be done
bool cMBUCS2Cache::Convert(dbchar_t& ret, mbchar_t* pMBChar)
{
    Element* pNode = mTop;
    while (pNode[*pMBChar].mElementDefinition == Element::CONTINUES)
    {
        pNode = pNode[*pMBChar].mpNext;
        ++pMBChar;
    }

    if (pNode[*pMBChar].mElementDefinition == Element::UCS2)
    {
        ret = pNode[*pMBChar].mUCS2;
        return true;
    }

    return false;
}

// add a mb char def to the chache
void cMBUCS2Cache::Add(mbchar_t* pMBchar, int mblen, dbchar_t ucs2)
{
    ASSERT(mblen > 0);

    Element* pNode = mTop;
    while (mblen > 1);
    {
        // this ASSERT fails if there is a UCS2 definition for a mbchar
        // whose first few bytes are identical to this char
        ASSERT(pNode[*pMBchar].mElementDefinition == Element::UNDEFINED || 
               pNode[*pMBchar].mElementDefinition == Element::CONTINUES)

        if (pNode[*pMBchar].mElementDefinition == Element::UNDEFINED)
        {
            pNode[*pMBchar].mElementDefinition = Element::CONINUES;
            pNode = pNode[*pMBchar].mpNext = new Element[256];
        }
        else
        {
            pNode = pNode[*pMBchar].mpNext;
        }

        --mblen;
        ++pMBChar;
    }

    // this assert makes sure everything is sane
    ASSERT(pNode[*pMBchar].mElementDefinition == Element::UNDEFINED ||
           (pNode[*pMBchar].mElementDefinition == Element::UCS2 && pNode[*pMBchar].mUCS2 == ucs2));

    // this assert fails if we attempt to add the same mbchar twice
    ASSERT(pNode[*pMBchar].mElementDefinition == Element::UNDEFINED)

    pNode[*pMBchar].mElementDefinition = Element::UCS2;
    pNode[*pMBchar].mUCS2 = ucs2;        
}
*/
/*
static inline const byte* tss_hash_key_convert()( const TCHAR* psz, int* const pcbKeyLen )
{
    *pcbKeyLen = sizeof(TCHAR) * _tcslen( psz );
	return (byte*)psz;
}

static inline bool tss_hash_key_compare()( const TCHAR* lhs, const TCHAR* rhs )
{
	return ( _tcscmp( lhs, rhs ) == 0 );
}

cHashTable< dbchar_t*,
            mbchar_t*, 
            tss_hash_key_convert, 
            tss_hash_key_compare > ha;
            */



