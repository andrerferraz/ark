/*
  Copyright (C)

  2001: Macadamian Technologies Inc (author: Jian Huang, jian@macadamian.com)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

// Qt includes
#include <qdir.h>
#include <qdragobject.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qregexp.h> 

// KDE includes
#include <kdebug.h>
#include <kkeydialog.h>
#include <kmessagebox.h>
#include <krun.h>
#include <ktempfile.h>
#include <kmimemagic.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <klocale.h>

// ark includes
#include "arkapp.h"
#include "dirDlg.h"
#include "selectDlg.h"
#include "extractdlg.h"
#include "arch.h"
#include "arkwidgetpart.h"
#include "arkwidgetbase.h"
#include "arksettings.h"
#include "filelistview.h"
#include "arkutils.h"

ArkWidgetPart::ArkWidgetPart( QWidget *parent, const char *name ) :
	QWidget(parent, name), ArkWidgetBase( this ),
	m_bViewInProgress(false), mpTempFile(0)
{
	setIconText("Ark Kparts");
	createFileListView();
}

ArkWidgetPart::~ArkWidgetPart()
{
	// Call common function from arkwidgetbase using true because is part
	cleanArkTmpDir( true );
}

/*******************************************************************
*                        updateStatusTotals
*
*       update file information when open or close an archive
*/
void 
ArkWidgetPart::updateStatusTotals()
{
	m_nNumFiles = 0;
	m_nSizeOfFiles = 0;
	if (archiveContent)
	{
		FileLVI *pItem = (FileLVI *)archiveContent->firstChild();
		while (pItem)
		{
			++m_nNumFiles;
			
			m_nSizeOfFiles += pItem->fileSize();
			pItem = (FileLVI *)pItem->nextSibling();
		}
	}
}

/*********************************************************************
*                          file_open
*
*               display the archive file contents
*/
void 
ArkWidgetPart::file_open(const QString & strFile, const KURL &fileURL)
{
	QFileInfo fileInfo(strFile);
	if (!fileInfo.exists())
	{
		KMessageBox::error(this, i18n("The archive %1 does not exist.").arg(strFile));
		return;
	}
	else if (!fileInfo.isReadable())
	{
		KMessageBox::error(this, i18n("You don't have permission to access that archive.") );
		return;
	}
	
	// see if the user is just opening the same file that's already
	// open (erm...)
	// TODO: Is this relevant with a part that'll likely use temp files?
	
	if (strFile == m_strArchName && m_bIsArchiveOpen)
	{
		return;
	}
	
	if ( isArchiveOpen() )
	{
		file_close();
	}

	// Set the current archive filename to the filename
	m_strArchName = strFile;
	m_url = fileURL;
	
	// display the archive contents
	showZip(strFile);
}

void 
ArkWidgetPart::edit_view_last_shell_output()
{
	viewShellOutput();
}


/*********************************************************************
*                            showZip
*
*     open a archive file and show its contents in a list view
*/
void 
ArkWidgetPart::showZip( const QString & _filename )
{
	createFileListView();
	openArchive( _filename );
}


/*********************************************************************
*                           slotOpen
*
*         This slot is invoked when an archive is opened
*/
void 
ArkWidgetPart::slotOpen(Arch *_newarch, bool _success, const QString & _filename, int )
{
	archiveContent->setUpdatesEnabled(true);
	archiveContent->triggerUpdate();
	
	if ( _success )
	{
		QFileInfo fi( _filename );
		QString path = fi.dirPath( true );
		m_settings->setLastOpenDir( path );
		
		if ( _filename.left(9) == QString("/tmp/ark.") || !fi.isWritable() )
		{
			_newarch->setReadOnly(true);
			QApplication::restoreOverrideCursor(); // no wait cursor during a msg box
			KMessageBox::information(this, i18n("This archive is read-only. If you want to save it under a new name, go to the File menu and select Save As."), i18n("Information"), "ReadOnlyArchive");
			QApplication::setOverrideCursor( waitCursor );
		}
		setCaption( _filename );
		arch = _newarch;
		updateStatusTotals();
		
		m_bIsArchiveOpen = true;
		QString extension;
		m_bIsSimpleCompressedFile = (COMPRESSED_FORMAT == m_archType);
	}
	QApplication::restoreOverrideCursor();
}


/*********************************************************************
*                       slotExtractDone
*
*         This slot is invoked when an archive is extracted
*/
void 
ArkWidgetPart::slotExtractDone()
{
	QApplication::restoreOverrideCursor();  
	
	if (m_bViewInProgress)
	{ 
		m_bViewInProgress = false;
		m_pKRunPtr = new KRun (m_strFileToView);
	}
	else if (m_bDragInProgress)
	{
		m_bDragInProgress = false;
		QStrList list;
		for (QStringList::Iterator it = mDragFiles.begin(); it != mDragFiles.end(); ++it)
		{
			QString URL;
			URL = m_settings->getTmpDir();
			URL += *it;
			list.append( QUriDrag::localFileToUri(URL) );
		}
		
		QUriDrag *d = new QUriDrag(list, archiveContent->viewport());
		m_bDropSourceIsSelf = true;
		d->dragCopy();
		m_bDropSourceIsSelf = false;
	}
	
	if(m_extractList != 0) delete m_extractList;
	m_extractList = NULL;
	archiveContent->setUpdatesEnabled(true);

        if ( m_extractRemote )
	{
		KURL srcDirURL( m_settings->getTmpDir() + "extrtmp/" );
		KURL src;
		QString srcDir( m_settings->getTmpDir() + "extrtmp/" );
	   	QDir dir( srcDir );
	       	QStringList lst( dir.entryList() );
		lst.remove( "." );
		lst.remove( ".." );

		KURL::List srcList;
	       	for( QStringList::ConstIterator it = lst.begin(); it != lst.end() ; ++it)
                {
			src = srcDirURL;
			src.addPath( *it );
			srcList.append( src );
		}

		m_extractURL.adjustPath( 1 );

		KIO::CopyJob *job = KIO::copy( srcList, m_extractURL );
    		connect( job, SIGNAL(result(KIO::Job*)),
             		this, SLOT(slotExtractRemoteDone(KIO::Job*)) );

		m_extractRemote = false;
	}

}


/*********************************************************************
*                          file_close
*
*                     close an archive file
*/
void 
ArkWidgetPart::file_close()
{
	closeArch();
	if (isArchiveOpen())
	{
		closeArch();
		setCaption(QString::null);
		if (mpTempFile)
		{
			mpTempFile->unlink();
			delete mpTempFile;
			mpTempFile = NULL;
		}
		updateStatusTotals();
		updateStatusSelection();
	}
	else closeArch();
}


/*********************************************************************
*                    reportExtractFailures
*
*    reports extract failures when Overwrite = False and the file
*    exists already in the destination directory.
*    If list is null, it means we are extracting all files.
*    Otherwise the list contains the files we are to extract.
*/
bool 
ArkWidgetPart::reportExtractFailures(const QString & _dest, QStringList *_list)
{
	QString strFilename, tmp;
	bool bRedoExtract = false;
	
	QApplication::restoreOverrideCursor();
	
	Q_ASSERT(_list != NULL);
	QString strDestDir = _dest;
	
	// make sure the destination directory has a / at the end.
	if (strDestDir.at(0) != '/')
	{
		strDestDir += '/';
	}
	
	if ( _list->isEmpty() )
	{
		// make the list
		FileListView *flw = fileList();
		FileLVI *flvi = (FileLVI*)flw->firstChild();
		while (flvi)
		{
			tmp = flvi->fileName();
			_list->append(tmp);
			flvi = (FileLVI*)flvi->itemBelow();
		}
	}
	QStringList existingFiles;
	// now the list contains all the names we must verify.
	for ( QStringList::Iterator it = _list->begin(); it != _list->end(); ++it )
	{
		strFilename = *it;
		QString strFullName = strDestDir + strFilename;
		if (QFile::exists(strFullName))
		{
			existingFiles.append(strFilename);
		}
	}
	
	int numFilesToReport = existingFiles.count();
	
	// now report on the contents
	if (numFilesToReport == 1)
	{ 
		strFilename = *(existingFiles.at(0));
		QString message = i18n("%1 will not be extracted because it will overwrite an existing file.\nGo back to Extract Dialog?").arg(strFilename);
		bRedoExtract = KMessageBox::questionYesNo(this, message) == KMessageBox::Yes;
	}
	else if ( numFilesToReport != 0 )
	{
		ExtractFailureDlg *fDlg = new ExtractFailureDlg( &existingFiles, this );
		bRedoExtract = !fDlg->exec();
	}
	return bRedoExtract;
}

/*********************************************************************
*                       action_extract
*
*          extract file(s) from an opened archive file
*/
void
ArkWidgetPart::action_extract()
{
	ExtractDlg *dlg = new ExtractDlg(m_settings);
	
	// if they choose pattern, we have to tell arkwidgetpart to select
	// those files... once we're in the dialog code it's too late.
	connect( dlg, SIGNAL(pattern(const QString &)), this, SLOT(selectByPattern(const QString &)) );
	bool bRedoExtract = false;
	
	if (m_nNumSelectedFiles == 0)
	{
		dlg->disableSelectedFilesOption();
	}
	
	if (archiveContent->currentItem() == NULL)
	{
		dlg->disableCurrentFileOption();
	}

	// list of files to be extracted
	m_extractList = new QStringList;
	if ( dlg->exec() )
	{
		int extractOp = dlg->extractOp();
		
		//m_extractURL will always be the location the user chose to
		//m_extract to, whether local or remote
		m_extractURL = dlg->extractDir();
				
		//extractDir will either be the real, local extract dir,
		//or in case of a extract to remote location, a local tmp dir
		QString extractDir;
		
		if ( !m_extractURL.isLocalFile() )
		{
			extractDir = m_settings->getTmpDir() + "extrtmp/";
			m_extractRemote = true;
			//make sure it's empty since all of it's contents
			//will be copied to the remote extract location
			KIO::NetAccess::del( extractDir );
			if ( !KIO::NetAccess::mkdir( extractDir ) )
			{
				kdWarning(1601) << "Unable to create " << extractDir << endl;
				m_extractRemote = false;
				delete dlg;
				return;
			}
		}
		else
		{
			extractDir = m_extractURL.path();
		}

		// if overwrite is false, then we need to check for failure of
		// extractions.
		bool bOvwrt = m_settings->getExtractOverwrite();

		switch( extractOp )
		{
			case ExtractDlg::All:
				if (!bOvwrt)  // send empty list to indicate we're extracting all
				{
					bRedoExtract = reportExtractFailures( extractDir, m_extractList );
				}
				if ( !bRedoExtract ) // if the user's OK with those failures, go ahead
				{
					// unless we have no space!
					if (Utils::diskHasSpace( extractDir, m_nSizeOfFiles ) )
					{
						arch->unarchFile(0, extractDir);
					}
				}
				break;
			case ExtractDlg::Pattern:
			case ExtractDlg::Selected:
			case ExtractDlg::Current:
				{
					int nTotalSize = 0;
					if ( extractOp != ExtractDlg::Current )
					{
						// make a list to send to unarchFile
						FileListView *flw = fileList();
						FileLVI *flvi = (FileLVI*)flw->firstChild();
						while ( flvi )
						{
							if ( flw->isSelected(flvi) )
							{
								QCString tmp = QFile::encodeName( flvi->fileName() );
								m_extractList->append( tmp );
								nTotalSize += flvi->fileSize();
							}
							flvi = (FileLVI*)flvi->itemBelow();
						}
					}
					else
					{
						FileLVI *pItem = archiveContent->currentItem();
						if ( pItem == 0 )
						{
							return;
						}
						QString tmp = pItem->fileName();  // mj: text(0) won't work
						nTotalSize += pItem->fileSize();
						m_extractList->append( QFile::encodeName(tmp) );
					}
					if ( !bOvwrt )
					{
						bRedoExtract =	reportExtractFailures(extractDir, m_extractList);
					}
					if ( !bRedoExtract )
					{
						if ( Utils::diskHasSpace(extractDir, nTotalSize) )
						{
							arch->unarchFile(m_extractList, extractDir); // extract selected files
						}
					}
					break;
				}
			default:
				Q_ASSERT(0);
				// never happens
				break;
		}
	}

	// user might want to change some options or the selection...
  	if (bRedoExtract)
	{
		action_extract();
	}
	
	delete dlg;
}

/*********************************************************************
*                       action_view
*
*          view a selected file in an opened archive file
*/
void 
ArkWidgetPart::action_view()
{
	FileLVI *pItem = archiveContent->currentItem();
	if (pItem  != NULL )
	{
		showFile( pItem );
	}
}

/*********************************************************************
*                        showFile
*
*          show a selected file using a suitable utility
*/
void 
ArkWidgetPart::showFile( FileLVI *_pItem )
{
	QString name = _pItem->fileName(); // mj: NO text(0)!
	
	QString fullname;
	fullname = "file:";
	fullname += m_settings->getTmpDir();
	fullname += name;

	m_extractList = new QStringList;
	m_extractList->append( name );
	
	m_bViewInProgress = true;
	m_strFileToView = fullname;
	if ( Utils::diskHasSpace( m_settings->getTmpDir(), _pItem->fileSize() ) )
	{
		arch->unarchFile( m_extractList, m_settings->getTmpDir() );
	}
}


/*********************************************************************
*                     slotSelectionChanged
*/
void 
ArkWidgetPart::slotSelectionChanged()
{
	updateStatusSelection();
	emit toKpartsView(m_nNumFiles, m_nNumSelectedFiles);
}


/*******************************************************************
*                    updateStatusSelection
*/
void 
ArkWidgetPart::updateStatusSelection()
{
	m_nNumSelectedFiles = 0;
	m_nSizeOfSelectedFiles = 0;
	
	if (archiveContent)
	{
		FileLVI * flvi = (FileLVI*)archiveContent->firstChild();
		while (flvi)
		{
			if (flvi->isSelected())
			{
				++m_nNumSelectedFiles;
				m_nSizeOfSelectedFiles += flvi->fileSize();
			}
			flvi = (FileLVI*)flvi->itemBelow();
		}
	}
}


/*******************************************************************
*                       selectByPattern
*
*        select all the files that match a pattern
*/
void 
ArkWidgetPart::selectByPattern(const QString & _pattern) // slot
{
	FileLVI * flvi = (FileLVI*)archiveContent->firstChild();
	QRegExp *glob = new QRegExp(_pattern, true, true); // file globber
	
	archiveContent->clearSelection();
	while ( flvi )
	{
		if ( glob->search(flvi->fileName() ) != -1 )
		{
			archiveContent->setSelected(flvi, true);
		}
		flvi = (FileLVI*)flvi->itemBelow();
	}
	
	delete glob;
}


/*******************************************************************
*                     createFileListView
*/
void 
ArkWidgetPart::createFileListView()
{
	if ( !archiveContent )
	{
		archiveContent = new FileListView(this, this);
		archiveContent->setMultiSelection(true);
		archiveContent->show();
		connect( archiveContent, SIGNAL( selectionChanged() ), this, SLOT( slotSelectionChanged() ) );
	}
	
	archiveContent->clear();
}


/*********************************************************************
*                         badBzipName
*/
bool 
ArkWidgetPart::badBzipName(const QString & _filename)
{
	if (_filename.right(3) == ".BZ" || _filename.right(4) == ".TBZ")
	{
		KMessageBox::error(this, i18n("bzip does not support filename extensions that use capital letters.") );
	}
	else if (_filename.right(4) == ".tbz")
	{
		KMessageBox::error(this, i18n("bzip only supports filenames with the extension \".bz\"."));
	}
	else if (_filename.right(4) == ".BZ2" ||  _filename.right(5) == ".TBZ2")
	{
		KMessageBox::error(this, i18n("bzip2 does not support filename extensions that use capital letters."));
	}
	else if (_filename.right(5) == ".tbz2")
	{
		KMessageBox::error(this, i18n("bzip2 only supports filenames with the extension \".bz2\".") );
	}
	else
	{
		return false;
	}
	return true;
}


/*********************************************************************
*                          openArchive
*
*          open an archive file using a suitable utility
*/
void 
ArkWidgetPart::openArchive(const QString & _filename )
{
	// figure out what kind of archive it is
	Arch *newArch = 0;
	QString extension;
	
	goodFileType=1;
	// create Arch object accorting to the archive file type
	// use m_url to make a more intelligent guess at fileType
	m_archType = Arch::getArchType(_filename, extension, m_url);
	if( 0 == (newArch = Arch::archFactory(m_archType, m_settings, this, _filename) ) )
	{
		if ( !badBzipName(_filename) )
		{
			goodFileType=0;
			// it's still a bad name, so let's try to figure out what it is.
			// Maybe it just needs the proper extension!
			KMimeMagic *mimePtr = KMimeMagic::self();
			KMimeMagicResult * mimeResultPtr = mimePtr->findFileType(_filename);
			QString mimetype = mimeResultPtr->mimeType();
			if ( mimetype == "application/x-gzip" )
			{
				KMessageBox::error(this, i18n("Gzip archives need to have an extension `gz'."));
				return;
			}
			else if (mimetype == "application/x-zoo")
			{
				KMessageBox::error(this, i18n("Zoo archives need to have an extension `zoo'."));
				return;
			}
			else
			{
				KMessageBox::error( this, i18n("Unknown archive format or corrupted archive") );
				return;
			}
		}
		else
		{
			return;
		}
	}
	
	if ( !newArch->utilityIsAvailable() )
	{
		goodFileType=0;
		KMessageBox::error(this, i18n("The utility %1 is not in your PATH.\nPlease install it or contact your system administrator.").arg(newArch->getUtility()));
		return;
	}
	
	connect( newArch, SIGNAL(sigOpen(Arch *, bool, const QString &, int)),
			this, SLOT(slotOpen(Arch *, bool, const QString &,int)) );
	connect( newArch, SIGNAL(sigExtract(bool)), this, SLOT(slotExtractDone()));

	newArch->open();  //open the archive file
}

/*********************************************************************
*                        resizeEvent
*
*            resize the window size of the list view
*/
void 
ArkWidgetPart::resizeEvent ( QResizeEvent * newSize) 
{
	if ( archiveContent )
	{
		archiveContent->resize( newSize->size() );
	}
}

#include "arkwidgetpart.moc"

