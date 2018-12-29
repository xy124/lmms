/*
 * EffectRackView.cpp - view for effectChain model
 *
 * Copyright (c) 2006-2007 Danny McRae <khjklujn@netscape.net>
 * Copyright (c) 2008-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include <QApplication>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QDragEnterEvent>

#include "EffectRackView.h"
#include "EffectSelectDialog.h"
#include "EffectView.h"
#include "Effect.h"
#include "DummyEffect.h"
#include "GroupBox.h"
#include "StringPairDrag.h"
#include "DataFile.h"


EffectRackView::EffectRackView( EffectChain* model, QWidget* parent ) :
	QWidget( parent ),
	ModelView( NULL, this )
{
	QVBoxLayout* mainLayout = new QVBoxLayout( this );
	mainLayout->setMargin( 5 );
	setAcceptDrops(true);

	m_effectsGroupBox = new GroupBox( tr( "EFFECTS CHAIN" ) );
	mainLayout->addWidget( m_effectsGroupBox );

	QVBoxLayout* effectsLayout = new QVBoxLayout( m_effectsGroupBox );
	effectsLayout->setSpacing( 0 );
	effectsLayout->setContentsMargins( 2, m_effectsGroupBox->titleBarHeight() + 2, 2, 2 );

	m_scrollArea = new QScrollArea;
	m_scrollArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
	m_scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	m_scrollArea->setPalette( QApplication::palette( m_scrollArea ) );
	m_scrollArea->setFrameStyle( QFrame::NoFrame );
	m_scrollArea->setWidget( new QWidget );

	effectsLayout->addWidget( m_scrollArea );

	QPushButton* addButton = new QPushButton;
	addButton->setText( tr( "Add effect" ) );

	effectsLayout->addWidget( addButton );

	connect( addButton, SIGNAL( clicked() ), this, SLOT( addEffect() ) );


	m_lastY = 0;

	setModel( model );
}



EffectRackView::~EffectRackView()
{
	clearViews();
}





void EffectRackView::clearViews()
{
	while( m_effectViews.size() )
	{
		EffectView * e = m_effectViews[m_effectViews.size() - 1];
		m_effectViews.pop_back();
		delete e;
	}
}




void EffectRackView::moveUp( EffectView* view )
{
	fxChain()->moveUp( view->effect() );
	if( view != m_effectViews.first() )
	{
		int i = 0;
		for( QList<EffectView *>::Iterator it = m_effectViews.begin();
					it != m_effectViews.end(); it++, i++ )
		{
			if( *it == view )
			{
				break;
			}
		}

		EffectView * temp = m_effectViews[ i - 1 ];

		m_effectViews[i - 1] = view;
		m_effectViews[i] = temp;

		update();
	}
}




void EffectRackView::startEffectDrag(QMouseEvent* event, EffectView* view)
{
	// TODO: copy action if ctrl is down in mouse event ? ;)

	QPixmap thumbnail = view->grab().scaled(
			128, 128,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation );

	DataFile dataFile( DataFile::DragNDropData );
	QDomElement parent = dataFile.createElement( "effect" );
	Effect * effect = view->effect();
	if( DummyEffect* dummy = dynamic_cast<DummyEffect*>(effect) )
	{
		parent.appendChild( dummy->originalPluginData() );
	}
	else
	{
		QDomElement ef = effect->saveState( dataFile, parent );
		ef.setAttribute( "name", QString::fromUtf8( effect->descriptor()->name ) );
		ef.appendChild( effect->key().saveXML( dataFile ) );
		parent.appendChild(ef);
	}

	dataFile.content().appendChild(parent);

	// TODO: save automation track links too!

	QString value = dataFile.toString();
	StringPairDrag * drag = new StringPairDrag(this);
	deletePlugin(view);
	update();
	Qt::DropAction res = drag->exec(QString("effect"), value, thumbnail, Qt::MoveAction,
			Qt::MoveAction);
	update();
}


void EffectRackView::genericDragEnter(QDragEnterEvent *event)
{
	QString type = StringPairDrag::decodeKey(event);
	StringPairDrag::processDragEnterEvent(event, "effect");
}


void EffectRackView::moveDown( EffectView* view )
{
	if( view != m_effectViews.last() )
	{
		// moving next effect up is the same
		moveUp( *( std::find( m_effectViews.begin(), m_effectViews.end(), view ) + 1 ) );
	}
}

void EffectRackView::genericDrop(QDropEvent* event, EffectView* view)
{
	QString type = StringPairDrag::decodeKey( event );
	if (type != "effect")
	{
		// Should never be reached! (as enterdrag filters already!)
		//event->setDropAction(Qt::CancelAction);
		//event->accept();
		return;
	}
	if (!(event->possibleActions() & Qt::MoveAction))
	{
		// we accept only moves! later we accept
		return;
	}

	int index_to;
	if (view == NULL)
	{
		// insert at the end:
		index_to = m_effectViews.size();
	}
	else
	{
		index_to = m_effectViews.indexOf(view);
	}


	QDomDocument doc;
	QString value = StringPairDrag::decodeValue(event);
	doc.setContent(value);
	// only importing first effect tag so far:
	// (maybe later we allow to select multiple effects and d'n'd them all together...
	QDomNode node = doc.elementsByTagName("effect").item(0).firstChild();
	Effect * fx = fxChain()->effectFromDomNode(node);


	fxChain()->insertEffect(index_to, fx);

	EffectView * new_view = createEffectView(fx);
	new_view->show();
	m_effectViews.insert(index_to, new_view);

	update();

	new_view->editControls();
}



void EffectRackView::deletePlugin( EffectView* view )
{
	Effect * e = view->effect();
	m_effectViews.removeOne(view);
	delete view;
	fxChain()->removeEffect( e );
	e->deleteLater();
	update();
}




void EffectRackView::dragEnterEvent(QDragEnterEvent* event)
{
	genericDragEnter(event);
}




void EffectRackView::dropEvent(QDropEvent *event)
{
	genericDrop(event, NULL);
}




EffectView * EffectRackView::createEffectView(Effect * it)
{
	QWidget * w = m_scrollArea->widget();
	EffectView * view = new EffectView( it, w );
	connect( view, SIGNAL( moveUp( EffectView * ) ),
			this, SLOT( moveUp( EffectView * ) ) );
	connect( view, SIGNAL( moveDown( EffectView * ) ),
			this, SLOT( moveDown( EffectView * ) ) );
	connect(view, SIGNAL(genericDrop(QDropEvent *, EffectView *)),
			this, SLOT(genericDrop(QDropEvent *, EffectView *)));
	connect(view, SIGNAL(genericDragEnter(QDragEnterEvent *)),
			this, SLOT(genericDragEnter(QDragEnterEvent *)));
	connect( view, SIGNAL( deletePlugin( EffectView * ) ),
			this, SLOT( deletePlugin( EffectView * ) ),
			Qt::QueuedConnection );
	connect(view, SIGNAL(startEffectDrag(QMouseEvent*, EffectView*)),
			this, SLOT(startEffectDrag(QMouseEvent*, EffectView*)), Qt::QueuedConnection);
	return view;
}




void EffectRackView::update()
{
	QVector<bool> view_map( qMax<int>( fxChain()->m_effects.size(),
						m_effectViews.size() ), false );

	for( QList<Effect *>::Iterator it = fxChain()->m_effects.begin();
					it != fxChain()->m_effects.end(); ++it )
	{
		int i = 0;
		for( QList<EffectView *>::Iterator vit = m_effectViews.begin();
				vit != m_effectViews.end(); ++vit, ++i )
		{
			if( ( *vit )->model() == *it )
			{
				view_map[i] = true;
				break;
			}
		}
		if( i >= m_effectViews.size() )
		{
			// TODO: use real list instead of stupid scroll area!
			EffectView * view = createEffectView(*it);
			view->show();
			m_effectViews.append(view);
			if( i < view_map.size() )
			{
				view_map[i] = true;
			}
			else
			{
				view_map.append( true );
			}

		}
	}

	int i = 0, nView = 0;

	const int EffectViewMargin = 3;
	m_lastY = EffectViewMargin;

	for( QList<EffectView *>::Iterator it = m_effectViews.begin();
					it != m_effectViews.end(); i++ )
	{
		if( i < view_map.size() && view_map[i] == false )
		{
			delete m_effectViews[nView];
			it = m_effectViews.erase( it );
		}
		else
		{
			( *it )->move( EffectViewMargin, m_lastY );
			m_lastY += ( *it )->height();
			++nView;
			++it;
		}
	}

	QWidget * w = m_scrollArea->widget();
	w->setFixedSize( 210 + 2*EffectViewMargin, m_lastY );

	QWidget::update();
}




void EffectRackView::addEffect()
{
	EffectSelectDialog esd( this );
	esd.exec();

	if( esd.result() == QDialog::Rejected )
	{
		return;
	}

	Effect * fx = esd.instantiateSelectedPlugin( fxChain() );

	fxChain()->appendEffect( fx );
	update();

	// Find the effectView, and show the controls
	for( QList<EffectView *>::Iterator vit = m_effectViews.begin();
					vit != m_effectViews.end(); ++vit )
	{
		if( ( *vit )->effect() == fx )
		{
			( *vit )->editControls();

			break;
		}
	}


}




void EffectRackView::modelChanged()
{
	//clearViews();
	m_effectsGroupBox->setModel( &fxChain()->m_enabledModel );
	connect( fxChain(), SIGNAL( aboutToClear() ), this, SLOT( clearViews() ) );
	update();
}
