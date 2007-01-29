/*
 ark -- archiver for the KDE project

 Copyright (C) 2003: Georg Robbers <georg.robbers@urz.uni-hd.de>

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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <kaboutdata.h>
#include <kcomponentdata.h>

#include "ark_part.h"
#include "arkfactory.h"
//Added by qt3to4:
#include <QByteArray>

KComponentData *ArkFactory::s_instance = 0L;
KAboutData* ArkFactory::s_about = 0L;
int ArkFactory::instanceNumber = 0;

K_EXPORT_COMPONENT_FACTORY( libarkpart, ArkFactory )

ArkFactory::~ArkFactory()
{
    delete s_instance;
    delete s_about;
    s_instance = 0L;
}

KParts::Part * ArkFactory::createPartObject( QWidget *parentWidget,
                  QObject *parent,
                  const char *classname,
                  const QStringList &args )
{
    bool readWrite = false; // for e.g. Browser/View or KParts::ReadOnlyPart
    if ( QByteArray( classname ) == "KParts::ReadWritePart" 
         || QByteArray( classname ) == "ArkPart" )
    {
            readWrite = true;
    }
    ArkPart* obj = new ArkPart( parentWidget, parent,
                                args, readWrite );
        //kDebug( 1601 ) << "classname is: " << QCString( classname ) << endl;
        return obj;
}

const KComponentData &ArkFactory::componentData()
{
    instanceNumber++;
    if( !s_instance )
    {
        s_about = ArkPart::createAboutData();
        s_instance = new KComponentData( s_about );
    }
    return *s_instance;
}

