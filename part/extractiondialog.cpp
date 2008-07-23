/*
 * ark -- archiver for the KDE project
 *
 * Copyright (C) 2007 Henrique Pinto <henrique.pinto@kdemail.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include "extractiondialog.h"
#include "settings.h"

#include <KLocale>
#include <KIconLoader>

#include <QDir>

ExtractionDialogUI::ExtractionDialogUI( QWidget *parent )
	: QFrame( parent )
{
	setupUi( this );
}

ExtractionDialog::ExtractionDialog(QWidget *parent )
	: KDirSelectDialog()
{
	m_ui = new ExtractionDialogUI( this );
	//setMainWidget( m_ui );
	
	//m_ui->information->setText(QString("The root has %1 files.").arg(model.rowCount()));

	mainWidget()->layout()->addWidget(m_ui);
	setButtons( Ok | Cancel );
	setCaption( i18n( "Extract" ) );
	m_ui->iconLabel->setPixmap( DesktopIcon( "archive-extract" ) );
	m_ui->iconLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum );

	m_ui->filesToExtractGroupBox->hide();
	m_ui->allFilesButton->setChecked( true );
	m_ui->extractAllLabel->show();

	//m_ui->recentFolders->addItems( ArkSettings::recentExtractionFolders() );

	if ( ArkSettings::recentExtractionFolders().isEmpty() )
	{
		setCurrentUrl( QDir::currentPath() );
	}
	else
	{
		setCurrentUrl( ArkSettings::recentExtractionFolders().last() );
	}

	m_ui->openFolderCheckBox->setChecked( ArkSettings::openDestinationFolderAfterExtraction() );
}

void ExtractionDialog::setCurrentUrl(const QString& url)
{
	KDirSelectDialog::setCurrentUrl(url);
}

void ExtractionDialog::setSubfolder(QString subfolder)
{
	m_ui->subfolder->setText(subfolder);
}

QString ExtractionDialog::subFolder() const
{
	return m_ui->subfolder->text();
}

ExtractionDialog::~ExtractionDialog()
{
	delete m_ui;
	m_ui = 0;
}

void ExtractionDialog::showSelectedFilesOption()
{
	m_ui->filesToExtractGroupBox->show();
	m_ui->selectedFilesButton->setChecked( true );
	m_ui->extractAllLabel->hide();
}

bool ExtractionDialog::extractAllFiles()
{
	return m_ui->allFilesButton->isChecked();
}

bool ExtractionDialog::openDestinationAfterExtraction()
{
	return m_ui->openFolderCheckBox->isChecked();
}

KUrl ExtractionDialog::destinationDirectory()
{
	return url().path();
}

#include "extractiondialog.moc"
