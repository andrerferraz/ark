/*

 ark -- archiver for the KDE project

 Copyright (C)

 1997-1999: Rob Palmbos palm9744@kettering.edu
 2000: Corel Corporation (author: Emily Ezust, emilye@corel.com)
 2001: Corel Corporation (author: Michael Jarrett, michaelj@corel.com)

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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// C includes
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// QT includes
#include <qdir.h>

// KDE includes
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kprocess.h>

// ark includes
#include "arkwidget.h"
#include "settings.h"
#include "ar.h"

ArArch::ArArch( ArkWidget *_gui, const QString & _fileName )
  : Arch(_gui, _fileName )
{
  m_archiver_program = "ar";
  m_unarchiver_program = QString::null;
  verifyUtilityIsAvailable(m_archiver_program, m_unarchiver_program);

  // Do not set headerString - there is none for Ar
  m_numCols = 5;
  m_dateCol = 4; m_fixYear = 8; m_repairMonth = 5; m_fixDay = 6; m_fixTime = 7;

  m_archCols.append(new ArchColumns(1, QRegExp("[a-zA-Z-]+"), 12)); // Perms
  m_archCols.append(new ArchColumns(2, QRegExp("[^\\s]+"), 128)); //User/grp
  m_archCols.append(new ArchColumns(3, QRegExp("[0-9]+"))); // Size
  m_archCols.append(new ArchColumns(5, QRegExp("[a-zA-Z]+"), 4)); // Month
  m_archCols.append(new ArchColumns(6, QRegExp("[0-9]+"), 2)); // Day
  m_archCols.append(new ArchColumns(7, QRegExp("[0-9:]+"), 6)); // Time
  m_archCols.append(new ArchColumns(8, QRegExp("[0-9]+"), 5)); // Year
  m_archCols.append(new ArchColumns(0, QRegExp("[^\\s][^\\n]+"), 4096));// File

  kdDebug(1601) << "ArArch constructor" << endl;
}

void ArArch::setHeaders()
{
  kdDebug(1601) << "+ArArch::setHeaders" << endl;
  QStringList list;
  list.append(FILENAME_STRING);
  list.append(PERMISSION_STRING);
  list.append(OWNER_GROUP_STRING);
  list.append(SIZE_STRING);
  list.append(TIMESTAMP_STRING);

  // which columns to align right
  int *alignRightCols = new int[1];
  alignRightCols[0] = 3;

  m_gui->setHeaders(&list, alignRightCols, 1);
  delete [] alignRightCols;

  kdDebug(1601) << "-ArArch::setHeaders" << endl;
}

void ArArch::open()
{
  kdDebug(1601) << "+ArArch::open" << endl;
  setHeaders();

  m_buffer = "";

  KProcess *kp = new KProcess;
  *kp << m_archiver_program << "vt" << m_filename;
  connect( kp, SIGNAL(receivedStdout(KProcess*, char*, int)),
	   this, SLOT(slotReceivedTOC(KProcess*, char*, int)));
  connect( kp, SIGNAL(receivedStderr(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));

  connect( kp, SIGNAL(processExited(KProcess*)), this,
	   SLOT(slotOpenExited(KProcess*)));

  if (kp->start(KProcess::NotifyOnExit, KProcess::AllOutput) == false)
    {
      KMessageBox::error( 0, i18n("Could not start a subprocess.") );
      emit sigOpen(this, false, QString::null, 0 );
    }
  kdDebug(1601) << "-ArArch::open" << endl;
}

void ArArch::create()
{
  KProcess *kp = new KProcess;
  kp->clearArguments();
  *kp << m_archiver_program << "c" << m_filename;

  connect( kp, SIGNAL(receivedStdout(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));
  connect( kp, SIGNAL(receivedStderr(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));

  if (kp->start(KProcess::Block) == false)
    {
      KMessageBox::error( 0, i18n("Could not start a subprocess.") );
      emit sigCreate(this, false, m_filename,
		     Arch::Extract | Arch::Delete | Arch::Add
		     | Arch::View);
    }
  else
    emit sigCreate(this, true, m_filename,
		   Arch::Extract | Arch::Delete | Arch::Add
		   | Arch::View);
}

void ArArch::addFile( const QStringList &urls )
{
  kdDebug(1601) << "+ArArch::addFile" << endl;
  KProcess *kp = new KProcess;
  kp->clearArguments();
  *kp << m_archiver_program;

  if (Settings::replaceOnlyWithNewer())
	  *kp << "ru";
  else
	  *kp << "r";

  *kp << m_filename;

  QStringList::ConstIterator iter;
  KURL url( urls.first() );
  QDir::setCurrent( url.directory() );
  for (iter = urls.begin(); iter != urls.end(); ++iter )
  {
    KURL fileURL( *iter );
    *kp << fileURL.fileName();
  }

  connect( kp, SIGNAL(receivedStdout(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));
  connect( kp, SIGNAL(receivedStderr(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));

  connect( kp, SIGNAL(processExited(KProcess*)), this,
	   SLOT(slotAddExited(KProcess*)));

  if (kp->start(KProcess::NotifyOnExit, KProcess::AllOutput) == false)
    {
      KMessageBox::error( 0, i18n("Could not start a subprocess.") );
      emit sigAdd(false);
    }

  kdDebug(1601) << "-ArArch::addFile" << endl;
}

void ArArch::unarchFile(QStringList *_fileList, const QString & _destDir, bool )
{
  // if _fileList is empty, we extract all.
  // if _destDir is empty, abort with error.

  kdDebug(1601) << "+ArArch::unarchFile" << endl;
  QString dest;

  if (_destDir.isEmpty() || _destDir.isNull())
    {
      kdError(1601) << "There was no extract directory given." << endl;
      return;
    }
  else dest = _destDir;

  // ar has no option to specify the destination directory
  // so I have to change to it.

  bool ret = QDir::setCurrent(dest);
 // I already checked the validity of the dir before coming here
  Q_ASSERT(ret);

  KProcess *kp = new KProcess;
  kp->clearArguments();

  *kp << m_archiver_program;
  *kp << "vx";
  *kp << m_filename;

  // if the list is empty, no filenames go on the command line,
  // and we then extract everything in the archive.
  if (_fileList)
    {
      for ( QStringList::Iterator it = _fileList->begin();
	    it != _fileList->end(); ++it )
	{
	  *kp << (*it);
	}
    }

  connect( kp, SIGNAL(receivedStdout(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));
  connect( kp, SIGNAL(receivedStderr(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));

  connect( kp, SIGNAL(processExited(KProcess*)), this,
	   SLOT(slotExtractExited(KProcess*)));

  if (kp->start(KProcess::NotifyOnExit, KProcess::AllOutput) == false)
    {
      KMessageBox::error( 0, i18n("Could not start a subprocess.") );
      emit sigExtract(false);
    }
}

void ArArch::remove(QStringList *list)
{
  kdDebug(1601) << "+ArArch::remove" << endl;

  if (!list)
    return;

  KProcess *kp = new KProcess;
  kp->clearArguments();

  *kp << m_archiver_program << "d" << m_filename;
  for ( QStringList::Iterator it = list->begin();
	it != list->end(); ++it )
    {
      QString str = *it;
      *kp << str;
    }

  connect( kp, SIGNAL(receivedStdout(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));
  connect( kp, SIGNAL(receivedStderr(KProcess*, char*, int)),
	   this, SLOT(slotReceivedOutput(KProcess*, char*, int)));

  connect( kp, SIGNAL(processExited(KProcess*)), this,
	   SLOT(slotDeleteExited(KProcess*)));

  if (kp->start(KProcess::NotifyOnExit, KProcess::AllOutput) == false)
    {
      KMessageBox::error( 0, i18n("Could not start a subprocess.") );
      emit sigDelete(false);
    }

  kdDebug(1601) << "-ArArch::remove" << endl;
}


#include "ar.moc"
