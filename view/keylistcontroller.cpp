/* -*- mode: c++; c-basic-offset:4 -*-
    controllers/keylistcontroller.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "keylistcontroller.h"
#include "commands/detailscommand.h"

#include <models/keycache.h>
#include <models/keylistmodel.h>

#include <gpgme++/key.h>

#include <QAbstractItemView>
#include <QTreeView>
#include <QTableView>
#include <QPointer>
#include <QItemSelectionModel>

#include <boost/bind.hpp>

#include <algorithm>
#include <cassert>

using namespace Kleo;
using namespace boost;
using namespace GpgME;

class KeyListController::Private {
    friend class ::Kleo::KeyListController;
    KeyListController * const q;
public:
    explicit Private( KeyListController * qq );
    ~Private();

    void connectView( QAbstractItemView * view );
    void connectCommand( Command * cmd );

    void addCommand( Command * cmd ) {
        connectCommand( cmd );
        commands.insert( std::lower_bound( commands.begin(), commands.end(), cmd ), cmd );
    }
    void addView( QAbstractItemView * view ) {
        connectView( view );
        views.insert( std::lower_bound( views.begin(), views.end(), view ), view );
    }
    void removeView( QAbstractItemView * view ) {
        view->disconnect( q );
        view->selectionModel()->disconnect( q );
        views.erase( std::remove( views.begin(), views.end(), view ), views.end() );
    }

public:
    void slotDestroyed( QObject * o ) {
        qDebug( "KeyListController::Private::slotDestroyed( %p )", o );
        views.erase( std::remove( views.begin(), views.end(), o ), views.end() );
	commands.erase( std::remove( commands.begin(), commands.end(), o ), commands.end() );
    }
    void slotDoubleClicked( const QModelIndex & idx );
    void slotActivated( const QModelIndex & idx );
    void slotSelectionChanged( const QItemSelection & old, const QItemSelection & new_ );
    void slotContextMenu( const QPoint & pos );
    void slotCommandFinished();
    void slotAddKey( const Key & key );
    void slotAboutToRemoveKey( const Key & key );
    void slotProgress( const QString & what, int current, int total ) {
        emit q->progress( current, total );
        if ( !what.isEmpty() )
            emit q->message( what );
    }

private:
    std::vector<QAbstractItemView*> views;
    std::vector<Command*> commands;
    QPointer<AbstractKeyListModel> flatModel, hierarchicalModel;
};


KeyListController::Private::Private( KeyListController * qq )
    : q( qq ),
      views(),
      commands(),
      flatModel( 0 ),
      hierarchicalModel( 0 )
{
    // We only connect to PublicKeyCache, since we assume that
    // SecretKeyCache's content is a real subset of PublicKeyCache's:
    connect( PublicKeyCache::mutableInstance().get(), SIGNAL(added(GpgME::Key)),
             q, SLOT(slotAddKey(GpgME::Key)) );
    connect( PublicKeyCache::mutableInstance().get(), SIGNAL(aboutToRemove(GpgME::Key)),
             q, SLOT(slotAboutToRemoveKey(GpgME::Key)) );

}

KeyListController::Private::~Private() {}

KeyListController::KeyListController( QObject * p )
    : QObject( p ), d( new Private( this ) )
{

}

KeyListController::~KeyListController() {}



void KeyListController::Private::slotAddKey( const Key & key ) {
    // ### make model act on keycache directly...
    if ( flatModel )
        flatModel->addKey( key );
    if ( hierarchicalModel )
        hierarchicalModel->addKey( key );
}


void KeyListController::Private::slotAboutToRemoveKey( const Key & key ) {
    // ### make model act on keycache directly...
    if ( flatModel )
        flatModel->removeKey( key );
    if ( hierarchicalModel )
        hierarchicalModel->removeKey( key );
}

void KeyListController::addView( QAbstractItemView * view ) {
    if ( !view || std::binary_search( d->views.begin(), d->views.end(), view ) )
        return;
    d->addView( view );
}

void KeyListController::removeView( QAbstractItemView * view ) {
    if ( !view || !std::binary_search( d->views.begin(), d->views.end(), view ) )
        return;
    d->removeView( view );
}

std::vector<QAbstractItemView*> KeyListController::views() const {
    return d->views;
}

void KeyListController::setFlatModel( AbstractKeyListModel * model ) {
    if ( model == d->flatModel )
        return;

    d->flatModel = model;
    
    if ( model ) {
        model->clear();
        model->addKeys( PublicKeyCache::instance()->keys() );
    }
}

void KeyListController::setHierarchicalModel( AbstractKeyListModel * model ) {
    if ( model == d->hierarchicalModel )
        return;

    d->hierarchicalModel = model;
    
    if ( model ) {
        model->clear();
        model->addKeys( PublicKeyCache::instance()->keys() );
    }
}

AbstractKeyListModel * KeyListController::flatModel() const {
    return d->flatModel;
}

AbstractKeyListModel * KeyListController::hierarchicalModel() const {
    return d->hierarchicalModel;
}

void KeyListController::registerCommand( Command * cmd ) {
    if ( !cmd || std::binary_search( d->commands.begin(), d->commands.end(), cmd ) )
        return;
    d->addCommand( cmd );
    qDebug( "KeyListController::registerCommand( %p )", cmd );
    if ( d->commands.size() == 1 )
        emit commandsExecuting( true );
}

// slot
void KeyListController::cancelCommands() {
    std::for_each( d->commands.begin(), d->commands.end(),
                   bind( &Command::cancel, _1 ) );
}

void KeyListController::Private::connectView( QAbstractItemView * view ) {

    connect( view, SIGNAL(destroyed(QObject*)),
             q, SLOT(slotDestroyed(QObject*)) );
    connect( view, SIGNAL(doubleClicked(QModelIndex)),
             q, SLOT(slotDoubleClicked(QModelIndex)) );
    connect( view, SIGNAL(activated(QModelIndex)),
             q, SLOT(slotActivated(QModelIndex)) );
    connect( view->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
             q, SLOT(slotSelectionChanged(QItemSelection,QItemSelection)) );

    view->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( view, SIGNAL(customContextMenuRequested(QPoint)),
             q, SLOT(slotContextMenu(QPoint)) );
}

void KeyListController::Private::connectCommand( Command * cmd ) {
    if ( !cmd )
        return;
    connect( cmd, SIGNAL(destroyed(QObject*)), q, SLOT(slotDestroyed(QObject*)) );
    connect( cmd, SIGNAL(finished()), q, SLOT(slotCommandFinished()) );
    //connect( cmd, SIGNAL(canceled()), q, SLOT(slotCommandCanceled()) );
    connect( cmd, SIGNAL(info(QString,int)), q, SIGNAL(message(QString,int)) );
    connect( cmd, SIGNAL(progress(QString,int,int)), q, SLOT(slotProgress(QString,int,int)) );
}


void KeyListController::Private::slotDoubleClicked( const QModelIndex & idx ) {
    QAbstractItemView * const view = qobject_cast<QAbstractItemView*>( q->sender() );
    if ( !view || !std::binary_search( views.begin(), views.end(), view ) )
	return;

    DetailsCommand * const c = new DetailsCommand( view, q );

    c->setIndex( idx );
    c->start();
}

void KeyListController::Private::slotActivated( const QModelIndex & idx ) {
    QAbstractItemView * const view = qobject_cast<QAbstractItemView*>( q->sender() );
    if ( !view || !std::binary_search( views.begin(), views.end(), view ) )
	return;
    
}

void KeyListController::Private::slotSelectionChanged( const QItemSelection & old, const QItemSelection & new_ ) {
    const QItemSelectionModel * const sm = qobject_cast<QItemSelectionModel*>( q->sender() );
    if ( !sm )
	return;
    // ### enable/disable actions
}

void KeyListController::Private::slotContextMenu( const QPoint & p ) {
    QAbstractItemView * const view = qobject_cast<QAbstractItemView*>( q->sender() );
    if ( !view || !std::binary_search( views.begin(), views.end(), view ) )
	return;
    
}

void KeyListController::Private::slotCommandFinished() {
    Command * const cmd = qobject_cast<Command*>( q->sender() );
    if ( !cmd || !std::binary_search( commands.begin(), commands.end(), cmd ) )
        return;
    qDebug( "KeyListController::Private::slotCommandFinished( %p )", cmd );
    if ( commands.size() == 1 )
        emit q->commandsExecuting( false );
}

#include "moc_keylistcontroller.cpp"