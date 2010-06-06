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
// twcmdlineutil.cpp
#include "stdtripwire.h"

#include "twcmdlineutil.h"

#include "fco/fcospeclist.h"
#include "twparser/policyparser.h"
#include "twparser/parserhelper.h"     // used in TripPropsFromSpecs below
#include "fco/parsergenreutil.h"
#include "fco/fcopropvector.h"
#include "core/fsservices.h"
#include "core/usernotify.h"
#include "tw/fcoreport.h"
#include "twcrypto/keyfile.h"
#include "tw/twutil.h"
#include "tw/textreportviewer.h"
#include "core/archive.h"
#include <fstream>            // used to open plain text policy file
#include "mailmessage.h"         // used for email reporting
#include "fco/twfactory.h"
#include "tripwirestrings.h"
#include "twcmdline.h"
#include "fco/fcospecutil.h"
#include "integritycheck.h"
#include "fco/genreswitcher.h"
#include "tw/fcodatabasefile.h"
#include "core/errorgeneral.h"
#include "tw/twerrors.h"
#include "fco/fconametranslator.h"
#include "fco/iterproxy.h"
#include "core/twlocale.h"
#include "core/stringutil.h"
#include "tw/twstrings.h"
#include "tw/filemanipulator.h"
#include "core/displayencoder.h"

#include <memory>

#ifdef GMMS

// local helper functions for GMMS notification
static TSTRING FormatGmmsReportSummary(int32 date, int violationCount, int maxSeverity);
static TSTRING FormatGmmsViolationSummary(int32 date, const TSTRING& rulename, const TSTRING& filename, int severity);
static void    SendGmmsAlert(const TSTRING& gmmsProg, const TSTRING& gmmsOptions, const TSTRING& alerts); // throw (eGmmsError)

#endif


///////////////////////////////////////////////////////////////////////////////
// ParsePolicyFile
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::ParsePolicyFile(cGenreSpecListVector& genreSpecList, const TSTRING& fileName, TSTRING& strSiteKeyFile, cErrorQueue* pQueue)
{
    // get site public key
   cKeyFile siteKeyfile;
    cTWUtil::OpenKeyFile( siteKeyfile, strSiteKeyFile );

    // read in and unencrypt policy file
    std::string strPolicyText;

    iUserNotify::GetInstance()->Notify( iUserNotify::V_VERBOSE, _T("%s%s\n"),
                              TSS_GetString( cTW, tw::STR_OPEN_POLICY_FILE).c_str(), 
                                        cDisplayEncoder::EncodeInline( fileName ).c_str() );

    // warn user if policy file is unencrypted
    {
        cFileManipulator policyManipulator(fileName.c_str());
        policyManipulator.Init();
        if (policyManipulator.GetEncoding() != cFileHeader::ASYM_ENCRYPTION)
            cTWUtil::PrintErrorMsg(eTWPolUnencrypted(_T(""), eError::NON_FATAL|eError::SUPRESS_THIRD_MSG));
    }
    
    cTWUtil::ReadPolicyText( fileName.c_str(), strPolicyText, siteKeyfile.GetPublicKey() );

   iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, _T("%s%s\n"), 
                              TSS_GetString( cTW, tw::STR_PARSING_POLICY).c_str(),
                              cDisplayEncoder::EncodeInline( fileName ).c_str());
    
   // set up parser and parser policy file
    std::istringstream in( strPolicyText );
    cPolicyParser parser( in );
    parser.Execute( genreSpecList, pQueue );
}

///////////////////////////////////////////////////////////////////////////////
// ParseTextPolicyFile
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::ParseTextPolicyFile(cGenreSpecListVector& genreSpecList, const TSTRING& fileName, TSTRING& strSiteKeyFile, cErrorQueue* pQueue)
{
   std::ifstream in;
   in.open(cStringUtil::TstrToStr(fileName).c_str());    
   if(in.fail())
   {
      throw eOpen( fileName );
   }

   iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, _T("%s%s\n"), 
                              TSS_GetString( cTW, tw::STR_PARSING_POLICY).c_str(),
                              cDisplayEncoder::EncodeInline( fileName ).c_str());

   cPolicyParser parser(in);
   parser.Execute(genreSpecList, pQueue);
}


///////////////////////////////////////////////////////////////////////////////
// util_NextPropToken
//
// gets the next token in the property string. Tokens are delimited by , or : and
// iff : is the delimiter, bGenreName is set to true. Returns an empty string when
// the end of the string is reached
///////////////////////////////////////////////////////////////////////////////
static TSTRING util_NextPropToken(TSTRING::const_iterator& i, const TSTRING& src, bool& bGenreName)
{
   TSTRING ret;
   TSTRING::const_iterator begin = i;
   for(; i != src.end(); i++ )
   {
      if( (*i == _T(':')) || (*i == _T(',')) )
         break;
   }
   if( (i == src.end()) || (*i != _T(':')) )
      bGenreName = false;
   else
      bGenreName = true;
   ret.assign( begin, i );
   //
   // move the iter to the beginning of the next token
   //
   if( i != src.end() )
      i++;
   return ret;
}

///////////////////////////////////////////////////////////////////////////////
// TrimPropsFromSpecs
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::TrimPropsFromSpecs(cFCOSpecList& specList, const TSTRING& propsToIgnore)
{
   cDebug d("cTWCmdLineUtil::TrimPropsFromSpecs");
   d.TraceDebug("Entering...\n");
   cFCOPropVector v;
    iParserGenreUtil* pHelper = iTWFactory::GetInstance()->CreateParserGenreUtil();
   //
   // trim only the properties appropriate for the current genre...
   //
   TSTRING propStr, ignoreStr;
   cGenre::Genre sysGenre = cGenreSwitcher::GetInstance()->CurrentGenre();
    cGenre::Genre curGenre = cGenreSwitcher::GetInstance()->GetDefaultGenre();

    iParserGenreUtil* pGU = iTWFactory::GetInstance()->CreateParserGenreUtil();
   TSTRING::const_iterator i = propsToIgnore.begin();
   TSTRING curToken;
   bool  bGenreName;
   //
   // the main token-getting loop
   //
   while( true )
   {
      curToken = util_NextPropToken( i, propsToIgnore, bGenreName );
      if( curToken.empty() )
         break;   // all done
      //
      // handle the genre switching token...
      //
      if( bGenreName )
      {
         curGenre = cGenreSwitcher::GetInstance()->StringToGenre( curToken.c_str() );
         if( curGenre == cGenre::GENRE_INVALID )
         {                
              throw eTWUnknownSectionName( curToken );
         }
      }
      else
      {
         if( curGenre == sysGenre )
         {
                // put together a string to pass to parsing code.
                // Single letter properties can be simply concatenated, where
                // long property names must have a '&' between them.
                if( pGU->HasSingleLetterProps() )
                    propStr += curToken;
                else
                    propStr += _T("&") + curToken;

            //
            // this string is used to tell the user what we are ignoring
            //
            if( ignoreStr.empty() )
               ignoreStr = curToken;
            else
               ignoreStr += _T(", ") + curToken;
         }
      }
   }
   //
   // if there is nothing to remove, just return...
   //
   if( propStr.empty() )
      return;

   try
   {
        cParserUtil::CreatePropVector( propStr, v, pHelper );
        delete pHelper;
   }
   catch(eParserHelper&)
   {
        delete pHelper;
      d.TraceError("CreatePropVector failed!!\n");
      throw eTWBadPropsToIgnore( propsToIgnore );
   }

   d.TraceDebug("We are ignoring these properties:\n");
   v.TraceContents();

   iUserNotify::GetInstance()->Notify(1, _T("%s %s\n"), TSS_GetString( cTripwire, tripwire::STR_IGNORE_PROPS).c_str(),
                                           cDisplayEncoder::EncodeInline( ignoreStr ).c_str());
   

   // okay, now let's change the specs!
   cFCOSpecListAddedIter iter(specList);
   for(iter.SeekBegin(); ! iter.Done(); iter.Next())
   {
      // TODO -- when we have multiple masks, we will need to iterate through all of the masks.
      cFCOPropVector specVector     = iter.Spec()->GetPropVector(iFCOSpecMask::GetDefaultMask());
      cFCOPropVector newSpecVector  = (specVector & v) ^ specVector;
      iter.Spec()->SetPropVector(iFCOSpecMask::GetDefaultMask(), newSpecVector);
   
      d.TraceDebug("Spec's prop set\n-----before:\n");
      specVector.TraceContents();
      d.TraceDebug("-----after:\n");
      newSpecVector.TraceContents();
   }
}

///////////////////////////////////////////////////////////////////////////////
// TrimSpecsByName
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::TrimSpecsByName(cFCOSpecList& specList, const TSTRING specName)
{
   iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, 
                              _T("%s%s\n"), 
                              TSS_GetString( cTripwire, tripwire::STR_CHECKING_SPECS_NAMED).c_str(),
                              cDisplayEncoder::EncodeInline( specName ).c_str());

   cFCOSpecListAddedIter iter(specList);
   for(iter.SeekBegin(); ! iter.Done(); )
   {
      //if(iter.Attr()->GetName().compare(specName) == 0)
      // right now, names are stored in the spec, but soon they will be in the 
      // attributes (see commented out code above)
      if(iter.Spec()->GetName().compare(specName) == 0)
      {
         // found one!
         iter.Next();
      }
      else
      {
         iter.Remove();
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
// TrimSpecsBySeverity
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::TrimSpecsBySeverity(cFCOSpecList& specList, int severity)
{
   cDebug d("cTWCmdLineUtil::TrimSpecsBySeverity");
   d.TraceDebug("Entering; severity=%d\n", severity);
   iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, 
                              _T("%s%d\n"), 
                              TSS_GetString( cTripwire, tripwire::STR_IC_SEVERITY_LEVEL).c_str(),
                              severity);
   
   cFCOSpecListAddedIter iter(specList);
   for(iter.SeekBegin(); ! iter.Done(); )
   {
      if(iter.Attr()->GetSeverity() < severity)
      {
         d.TraceDetail("Removing spec %s (severity=%d)\n",  iter.Spec()->GetStartPoint().AsString().c_str(), 
                                                iter.Attr()->GetSeverity());

         TW_NOTIFY_VERBOSE(   TSS_GetString( cTripwire, tripwire::STR_IC_IGNORING_SEVERITY).c_str(), 
                        iTWFactory::GetInstance()->GetNameTranslator()->ToStringDisplay
                           ( iter.Spec()->GetStartPoint() ).c_str(),
                        iter.Attr()->GetSeverity());
         
         iter.Remove();
      }
      else
         iter.Next();
   }
}

///////////////////////////////////////////////////////////////////////////////
// TrimSpecsBySeverityName
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::TrimSpecsBySeverityName(cFCOSpecList& specList, const TSTRING& severity)
{
   // for now, the names are hard coded, but they might not be in the future...
   if(_tcsicmp(severity.c_str(), _T("low")) == 0)
      TrimSpecsBySeverity(specList, 33);
   else if(_tcsicmp(severity.c_str(), _T("medium")) == 0)
      TrimSpecsBySeverity(specList, 66);
   else if(_tcsicmp(severity.c_str(), _T("high")) == 0)
      TrimSpecsBySeverity(specList, 100);
   else
   {
      throw eTWBadSeverityName( severity );
   }
}

///////////////////////////////////////////////////////////////////////////////
// ReportContainsFCO
///////////////////////////////////////////////////////////////////////////////
bool cTWCmdLineUtil::ReportContainsFCO(const cFCOReport& report)
{
    cFCOReportGenreIter genreIter( report ); 
    for(genreIter.SeekBegin(); ! genreIter.Done(); genreIter.Next())
    {
        cFCOReportSpecIter specIter( genreIter );
       for(specIter.SeekBegin(); ! specIter.Done(); specIter.Next())
       {
          if(  (! specIter.GetAddedSet()->IsEmpty())  || 
             (! specIter.GetRemovedSet()->IsEmpty())  ||
             (  specIter.GetNumChanged() != 0))
             return true;        
       }
    }

   return false;
}

///////////////////////////////////////////////////////////////////////////////
// GetICReturnValue
///////////////////////////////////////////////////////////////////////////////
int cTWCmdLineUtil::GetICReturnValue(const cFCOReport& report )
{
   enum { ADDED = 1, REMOVED = 2, CHANGED = 4 };

   int rtn = 0;
    cFCOReportGenreIter genreIter( report ); 
    for(genreIter.SeekBegin(); ! genreIter.Done(); genreIter.Next())
    {
        cFCOReportSpecIter specIter(report, genreIter.GetGenre());
       for(specIter.SeekBegin(); ! specIter.Done(); specIter.Next())
       {
          if(! specIter.GetAddedSet()->IsEmpty())
             rtn |= ADDED;
          if(! specIter.GetRemovedSet()->IsEmpty())   
             rtn |= REMOVED;
          if(specIter.GetNumChanged() != 0)
             rtn |= CHANGED;        
       }
    }
   return rtn;
}

///////////////////////////////////////////////////////////////////////////////
// VerifySpecs
//
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::VerifySpecs( const cFCOSpecList& parsedList, const cFCOSpecList& dbList )
{
    cFCOSpecListCanonicalIter policyItr   (parsedList);
    cFCOSpecListCanonicalIter dbItr    (dbList);

   if( parsedList.Size() != dbList.Size() )
   {
      throw eICBadPol();
   }

   for ( policyItr.SeekBegin(), dbItr.SeekBegin(); ! policyItr.Done(); policyItr.Next(), dbItr.Next() )
    {
      if( ! iFCOSpecUtil::FCOSpecEqual(*policyItr.Spec(), *dbItr.Spec()) )
      {
         throw eICBadPol();
      }
      // make sure the vectors are the same...
      // TODO -- this won't work when we get spec masks
      //
      if( policyItr.Spec()->GetPropVector( iFCOSpecMask::GetDefaultMask() ) != 
         dbItr.Spec()->GetPropVector( iFCOSpecMask::GetDefaultMask() ) )
      {
         throw eICBadPol();
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
// VerifyPolicy
///////////////////////////////////////////////////////////////////////////////
void cTWCmdLineUtil::VerifyPolicy( cGenreSpecListVector& genreSpecList, cFCODatabaseFile& dbFile )
{
   cGenreSpecListVector::iterator   genreIter;
   cFCODatabaseFile::iterator    dbIter( dbFile );

   // make sure the number of genres in each is the same.
   //
   if( genreSpecList.size() != (size_t)dbIter.Size() )
      throw eICBadPol();
   
   // iterate over all of the genres...
   //
   for (genreIter = genreSpecList.begin(); genreIter != genreSpecList.end(); ++genreIter)
   {
      dbIter.SeekToGenre( genreIter->GetGenre() );
      if( dbIter.Done() )
      {  
         throw eICBadPol();
      }
      // now, compare the two property sets...
      //
      VerifySpecs( genreIter->GetSpecList(), dbIter.GetSpecList());

   }
}


///////////////////////////////////////////////////////////////////////////////
//
// GetEmailRecipients - produce a list of all email addresses referred to in
// the report.
//
////////////////////////////////////////////////////////////////////////////////
static void GetEmailRecipients(std::vector<TSTRING> &vstrRecipients, 
                               const cFCOReport& report,
                               const cTWModeCommon *modeCommon) {

  // loop through all generes
  cFCOReportGenreIter genreIter(report);
  for (genreIter.SeekBegin(); !genreIter.Done(); genreIter.Next()) {

    // loop through all specs
    cFCOReportSpecIter specIter(report, genreIter.GetGenre());

    for (specIter.SeekBegin(); ! specIter.Done(); specIter.Next()) {

      // loop through all email addresses for this spec
      cFCOSpecAttrEmailIter emailIter(*specIter.GetAttr());

      for(emailIter.SeekBegin(); ! emailIter.Done(); emailIter.Next())  {

        TSTRING address = emailIter.EmailAddress();
        std::vector<TSTRING>::iterator i;

        // see if the address is already in the list
        i = std::find(vstrRecipients.begin(),
                      vstrRecipients.end(),
                      address);

        if (i == vstrRecipients.end()) {
          vstrRecipients.push_back(address);
        }

      }
    }
  }
}


static void GetGlobalEmailRecipients(std::vector<TSTRING> &vstrRecipients, 
                                     const cFCOReport& report,
                                     const cTWModeCommon *modeCommon) {


  // let's check for global emailto's...
  //
  if (modeCommon->mGlobalEmail.length() != 0) {

    std::vector<TSTRING> addys;
    const std::string delims = ";,";

    cStringUtil::splitstring(addys, modeCommon->mGlobalEmail, delims);
    
    while (!addys.empty()) {

      std::vector<TSTRING>::iterator p;
      TSTRING addr = addys.back();

      // make sure it's not already in there...
      //
      p = std::find(vstrRecipients.begin(),
                    vstrRecipients.end(),
                    addr);

      // put it away if it's unique...
      //
      if (p == vstrRecipients.end()) {
        vstrRecipients.push_back(addr);
      }

      addys.pop_back(); // remove that one...

    }
  }

}


///////////////////////////////////////////////////////////////////////////////
//
// EmailReportTo - send only the relevant portions of the report the address
//
///////////////////////////////////////////////////////////////////////////////
static bool EmailReportTo(const TSTRING &toAddress, const cFCOReportHeader& header, const cFCOReport& report, 
                          const cTWModeCommon *modeCommon,
                          const bool bForceFullReport)
{
  std::auto_ptr<cMailMessage> reportMail;

  // allocate the right kind of emailer object based on what came out of the config file.
  switch (modeCommon->mMailMethod) 
    {
    default:
      ASSERT(false);
      return false;
    case cMailMessage::MAIL_BY_SMTP:
      reportMail = std::auto_ptr<cMailMessage>(new cSMTPMailMessage(modeCommon->mSmtpHost, modeCommon->mSmtpPort));
      break;
    case cMailMessage::MAIL_BY_PIPE:
      reportMail = std::auto_ptr<cMailMessage>(new cPipedMailMessage(modeCommon->mMailProgram));
      break;
    case cMailMessage::MAIL_BY_MAPI:
#if SUPPORTS_MAPI
      reportMail = std::auto_ptr<cMailMessage>(new cMAPIMailMessage);
      break;
#else
      ASSERT(false);
      return false;
#endif
    }

  TSTRING strTempFilename;

  // send the report
  try
    {
      //    we have to write the text report to a temporary file...
      iFSServices::GetInstance()->GetTempDirName( strTempFilename );
      strTempFilename += _T("tripwire-report-XXXXXX");
      iFSServices::GetInstance()->MakeTempFilename( strTempFilename );
      strTempFilename += _T(".txt");

      //    print the report
      cEmailReportViewer trv( header, report, toAddress, bForceFullReport );
      trv.PrintTextReport( strTempFilename, modeCommon->mEmailReportLevel );

      //      format the subject line with number of violations and the most severe violation found
      //      set up the cMailMessage class, and send it if they need it or want it
      //    (Yes, it seems odd to generate the report, only to potentially delete it without sending it.)
      if ( trv.GetNumberViolations() > 0 || modeCommon->mMailNoViolations )
        {
          // print message that we're emailing to this specific recipient
          iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, _T("%s %s\n"), 
                                              TSS_GetString( cTripwire, tripwire::STR_EMAIL_REPORT_TO).c_str(), 
                                              cDisplayEncoder::EncodeInline( toAddress ).c_str());
          reportMail->AddRecipient(toAddress);

          TSTRING machineName;
          iFSServices::GetInstance()->GetMachineNameFullyQualified(machineName);
          reportMail->SetFrom(TSS_GetString( cTripwire, tripwire::STR_EMAIL_FROM) + machineName);
          reportMail->SetFromName(TSS_GetString(cTW, tw::STR_TSS_PRODUCT_NAME));
             
          reportMail->SetSubject( trv.SingleLineReport() );
         
          // don't attach a report if the user just requests a subject line
          if( cTextReportViewer::SINGLE_LINE == modeCommon->mEmailReportLevel )
            reportMail->SetBody( _T(" ") );
          else
            reportMail->AttachFile( strTempFilename.c_str() );

          reportMail->Send();
        }

      //    delete temp file
      iFSServices::GetInstance()->FileDelete( strTempFilename );
    }
  catch(eError& e)
    {
      e.SetFatality( false ); // we want to continue from email errors, so make them non-fatal
      cTWUtil::PrintErrorMsg(e);
      TCERR << TSS_GetString( cTripwire, tripwire::STR_ERR_EMAIL_REPORT) << std::endl;
      // delete temp file
      iFSServices::GetInstance()->FileDelete( strTempFilename );
      return false;
    }

  return true;
}


static bool DoEmailReports(const std::vector<std::string> &vstrRecipients, 
                           const cFCOReportHeader& header,
                           const cFCOReport& report, 
                           const cTWModeCommon *modeCommon,
                           const bool bForceFullReport) {

  // send each report out.
  bool ret = true;
  for (std::vector<std::string>::size_type i=0; i<vstrRecipients.size(); i++)
    {
      TSTRING addr;
      cStringUtil::Convert(addr, vstrRecipients[i]);
      bool success = EmailReportTo(addr, header, report, modeCommon, bForceFullReport);
      if (!success)
        ret = false;
    }

  return (ret);
}


///////////////////////////////////////////////////////////////////////////////
//
// EmailReport - returns false if any send operation failed, true if
// they all succeeded. returns false if no recipients were specified anywhere
// in the policy file.
//
///////////////////////////////////////////////////////////////////////////////
bool cTWCmdLineUtil::EmailReport(const cFCOReportHeader& header, const cFCOReport& report, 
                                 const cTWModeCommon *modeCommon)
{
  // print message that says that we're emailing
  iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, _T("%s\n"), 
                                      TSS_GetString( cTripwire, 
                                                     tripwire::STR_EMAIL_BEGIN).c_str());

  bool emailSent = false;
  bool ret = true;

  // Get the list of recipients
  std::vector<TSTRING> vstrRecipients;

  GetEmailRecipients(vstrRecipients, report, modeCommon);
  emailSent |= (vstrRecipients.size() != 0);

  if (vstrRecipients.size() != 0)
    ret = DoEmailReports(vstrRecipients, header, report, modeCommon, false);
  
  if (ret) { // if not, probably catostrophic

    vstrRecipients.clear();
    GetGlobalEmailRecipients(vstrRecipients, report, modeCommon);
    emailSent |= (vstrRecipients.size() != 0);

    if (vstrRecipients.size() != 0)
      ret = DoEmailReports(vstrRecipients, header, report, modeCommon, true);
      
  }


  if (!emailSent) {

    // Send a message to the user, alerting them that no email has been sent.
    iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, _T("%s\n"), 
                                        TSS_GetString( cTripwire, tripwire::STR_NO_EMAIL_RECIPIENTS).c_str());
  }
  
  return ret;
}


bool cTWCmdLineUtil::SendEmailTestMessage(const TSTRING &mAddress, const cTWModeCommon *modeCommon)
{
   std::auto_ptr<cMailMessage> reportMail;

   // allocate the right kind of emailer object based on what came out of the config file.
   switch (modeCommon->mMailMethod) 
   {
   default:
      ASSERT(false);
      return false;
   case cMailMessage::MAIL_BY_SMTP:
      reportMail = std::auto_ptr<cMailMessage>(new cSMTPMailMessage(modeCommon->mSmtpHost, modeCommon->mSmtpPort));
      break;
   case cMailMessage::MAIL_BY_PIPE:
      reportMail = std::auto_ptr<cMailMessage>(new cPipedMailMessage(modeCommon->mMailProgram));
      break;
   case cMailMessage::MAIL_BY_MAPI:
#if SUPPORTS_MAPI
      reportMail = std::auto_ptr<cMailMessage>(new cMAPIMailMessage);
      break;
#else
      ASSERT(false);
      return false;
#endif
   }


   // print message that we're emailing to this specific recipient
   iUserNotify::GetInstance()->Notify( iUserNotify::V_NORMAL, _T("%s %s\n"), 
                              TSS_GetString( cTripwire, tripwire::STR_TEST_EMAIL_TO).c_str(), 
                                        cDisplayEncoder::EncodeInline( mAddress ).c_str());

   // send the report
   try
   {
        //      set up the cMailMessage class, and send it
      reportMail->AddRecipient(mAddress);

        TSTRING machineName;
        iFSServices::GetInstance()->GetMachineNameFullyQualified(machineName);
        reportMail->SetFrom(TSS_GetString( cTripwire, tripwire::STR_EMAIL_FROM) + machineName);
        reportMail->SetFromName(TSS_GetString(cTW, tw::STR_TSS_PRODUCT_NAME));

        reportMail->SetSubject   (TSS_GetString( cTripwire, tripwire::STR_TEST_EMAIL_SUBJECT));
        reportMail->SetBody      (TSS_GetString( cTripwire, tripwire::STR_TEST_EMAIL_BODY));
      reportMail->Send();
   }
   catch(eError& e)
   {
      cTWUtil::PrintErrorMsg(e);
      TCERR << TSS_GetString( cTripwire, tripwire::STR_ERR_EMAIL_TEST) << std::endl;
      return false;
   }

   return true;
}


#ifdef GMMS

//
// Use gmms to send violation notification
//
// verbosity==1 One line summary of whole report
// verbosity==2 One line summary of each violation
//
bool cTWCmdLineUtil::GmmsReport(const cFCOReportHeader& header, const cFCOReport& report, 
                        const TSTRING &gmmsProg, const TSTRING &gmmsOptions, int verbosity) // throw (eGmmsError)
{
   int violationCount = 0, maxSeverity = 0;
   TSTRING alerts;

   // loop through all generes
    cFCOReportGenreIter genreIter(report);
    for (genreIter.SeekBegin(); !genreIter.Done(); genreIter.Next())
   {
      // loop through all specs
      cFCOReportSpecIter specIter(report, genreIter.GetGenre());
      for (specIter.SeekBegin(); ! specIter.Done(); specIter.Next())
      {

         ASSERT(true);

         // Get severity
         int32 severity = specIter.GetAttr()->GetSeverity();

         // Get rulename
         TSTRING rulename = specIter.GetAttr()->GetName();

         // Go through all of the added file rule violations
         cIterProxy<iFCOIter> addedIter(specIter.GetAddedSet()->GetIter());
         for (addedIter->SeekBegin(); !addedIter->Done(); addedIter->Next())
         {
            if (severity > maxSeverity)
               maxSeverity = severity;
            
            if (verbosity == 2)
            {
               // get filename, send one line summary of this violation
               TSTRING filename = addedIter->FCO()->GetName().AsString();
               alerts += FormatGmmsViolationSummary(header.GetCreationTime(), rulename, filename, severity);
               alerts += _T("\n");
            }
            
            violationCount++;
         }

         // Go through all of the removed file rule violations
         cIterProxy<iFCOIter> removedIter(specIter.GetRemovedSet()->GetIter());
         for (removedIter->SeekBegin(); !removedIter->Done(); removedIter->Next())
         {
            if (severity > maxSeverity)
               maxSeverity = severity;
            
            if (verbosity == 2)
            {
               // get filename, send one line summary of this violation
               TSTRING filename = removedIter->FCO()->GetName().AsString();
               alerts += FormatGmmsViolationSummary(header.GetCreationTime(), rulename, filename, severity);
               alerts += _T("\n");
            }

            violationCount++;
         }

         // Go through all of the changed file rule violations
         cFCOReportChangeIter changedIter(specIter);
         for (changedIter.SeekBegin(); !changedIter.Done(); changedIter.Next())
         {
            if (severity > maxSeverity)
               maxSeverity = severity;

            if (verbosity == 2)
            {
               // get filename, send one line summary of this violation
               TSTRING filename = changedIter.GetOld()->GetName().AsString();
               alerts += FormatGmmsViolationSummary(header.GetCreationTime(), rulename, filename, severity);
               alerts += _T("\n");
            }

            violationCount++;
         }
      }
   }

   // Send one line summary of the whole report if that's what they want,
   // or if there were no violations and the verbosity level is 2. That way
   // verbosity level 2 and no violations will still send an all-is-well notification.
   if (verbosity == 1 || (verbosity == 2 && violationCount==0))
   {
      alerts += FormatGmmsReportSummary(header.GetCreationTime(), violationCount, maxSeverity);
      alerts += _T("\n");
   }

   ASSERT(!alerts.empty());
   SendGmmsAlert(gmmsProg, gmmsOptions, alerts);

   return true;
}


//
// Use this with gmms verbosity level 1
//
TSTRING FormatGmmsReportSummary(int32 date, int violationCount, int maxSeverity)
{
   TSTRING strDate;
   cTWLocale::FormatTime(date, strDate);

   TOSTRINGSTREAM summary;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_START);
   summary << strDate;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_VERBOSITY1A);
   summary << violationCount;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_VERBOSITY1B);
   summary << maxSeverity;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_END);

   return summary.str();
}


//
// Use this with gmms verbosity level 2
//
TSTRING FormatGmmsViolationSummary(int32 date, const TSTRING& rulename, const TSTRING& filename, int severity)
{
   TSTRING strDate;
   cTWLocale::FormatTime(date, strDate);

   TOSTRINGSTREAM summary;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_START);
   summary << strDate;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_VERBOSITY2A);
   summary << rulename;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_VERBOSITY2B);
   summary << filename;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_VERBOSITY2C);
   summary << severity;
   summary << TSS_GetString( cTripwire, tripwire::STR_GMMS_END);

   return summary.str();
}

static void SendGmmsAlert(const TSTRING& gmmsProg, const TSTRING& gmmsOptions, const TSTRING& alerts) // throw (eGmmsError)
{
#if IS_UNIX

   // construct the command line for starting gmms
   TSTRING command = gmmsProg + _T(" ") + gmmsOptions;

   // preserve environment variables necessary for gmms to run
    le_set("GEOPLEX_DEPLOY");
   le_set("LD_LIBRARY_PATH");
   le_set("PATH");
    
   // open the process
   FILE *mpFile = mpopen((char*)command.c_str(), _T("w"));
   if(!mpFile)
        throw eGmmsError(eGmmsError::ERR_COULDNT_OPEN_PIPE, gmmsProg);

   // write the alert(s) to the process's standard-in.
   if (_ftprintf(mpFile, "%s", alerts.c_str()) < 0)
        throw eGmmsError(eGmmsError::ERR_COULDNT_WRITE_TO_PIPE, gmmsProg);

   // end the process by closing standard-in
   int result = mpclose( mpFile );
   if (result != 0)
   {
      // complain about gmms returning an error code.
      throw eGmmsError(eGmmsError::ERR_CMD_FAILED, gmmsProg);
   }  

   // unset environment variables necessary for gmms to run
    le_unset("GEOPLEX_DEPLOY");
   le_unset("LD_LIBRARY_PATH");
   le_unset("PATH");

#else

   // GMMS is not for NT, so just do something cheap and easy for debugging purposes.
   std::cerr << cStringUtil::TstrToStr(alerts);

#endif
}

#endif

