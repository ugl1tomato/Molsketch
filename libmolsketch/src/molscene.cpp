/***************************************************************************
 *   Copyright (C) 2007-2008 by Harm van Eersel                            *
 *   Copyright (C) 2009 Tim Vandermeersch                                  *
 *   Copyright (C) 2009 by Nicola Zonta                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <cmath>
#include <math.h>
#include <iostream>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QClipboard>
#include <QApplication>
#include <QListWidgetItem>
#include <QTableWidgetItem>
#include <QKeyEvent>
#include <QUndoStack>
#include <QProcess>
#include <QDir>
#include <QDesktopServices>
#include <QDebug>

#include "molscene.h"

#include "element.h"
#include "atom.h"
#include "bond.h"
#include "residue.h"

#include "molecule.h"
#include "mollibitem.h"
#include "commands.h"
#include "smilesitem.h"
#include "mimemolecule.h"
#include "TextInputItem.h"
#include "tool.h"
#include "toolgroup.h"
#include "math2d.h"
#include "osra.h"

#include <openbabel/mol.h>
#include <openbabel/atom.h>
#include <openbabel/bond.h>
#include <openbabel/obiter.h>

using namespace OpenBabel;




namespace Molsketch {

  using namespace Commands;


  //////////////////////////////////////////////////////////////////////////////
  //
  // Constructor & destructor
  //
  //////////////////////////////////////////////////////////////////////////////

  MolScene::MolScene(QObject* parent) : QGraphicsScene(parent)
  {
    m_toolGroup = new ToolGroup(this);

    // Set the default color to black
    m_color = QColor(0, 0, 0);


    // Create the TextInputItem that will be shown to edit text in the scene
    m_inputTextItem = new TextInputItem();
    addItem(m_inputTextItem);
    // hide it for now...
    m_inputTextItem->hide();


    //Initializing properties
    m_editMode = MolScene::DrawMode;
    m_atomSize = 5;
    m_carbonVisible = false;
    m_hydrogenVisible = true;
    m_chargeVisible = true;
    m_electronSystemsVisible = false;
    m_autoAddHydrogen = true;
    m_renderMode = RenderLabels;

    // Prepare undo m_stack
    m_stack = new QUndoStack(this);
    connect(m_stack, SIGNAL(indexChanged(int)), this, SIGNAL(documentChange()));
    connect(m_stack, SIGNAL(indexChanged(int)), this, SIGNAL(selectionChange()));
    connect(m_stack, SIGNAL(indexChanged(int)), this, SLOT(update()));

    // Set initial size
    QRectF sizerect(-5000,-5000,10000,10000);
    setSceneRect(sizerect);
  }

  MolScene::~MolScene()
  {
    // Clear the scene
    clear();   
  }

  void MolScene::addResidue (QPointF pos, QString name)
  {
		m_stack ->push (new AddResidue (new Residue (pos, name, 0, this)));
	}
  // Commands
	
  QColor MolScene::color() const
  {
    return m_color;
  }

  void MolScene::setColor (QColor c)
  {
		m_color = c;
		foreach (QGraphicsItem* item, selectedItems()) {
			if (item->type() == Atom::Type) {
				dynamic_cast<Atom*>(item) ->setColor(c);
			}
			else if (item->type() == Residue::Type) {
				dynamic_cast<Residue*>(item) ->setColor(c);
			}
		}
		foreach (QGraphicsItem* item, items()) {
			if (item->type() == Bond::Type) {
				Bond *b = dynamic_cast<Bond*>(item);
				if (b-> beginAtom() ->isSelected () && b->endAtom() ->isSelected()) b->setColor(c);
			}
		}
			
	}

  void MolScene::setCarbonVisible(bool value)
  {
    m_carbonVisible = value;
  }

  void MolScene::setHydrogenVisible(bool value)
  {
    m_hydrogenVisible = value;
  }

  void MolScene::setAtomSize( qreal size )
  {
    m_atomSize = size;
  }
	


  void MolScene::alignToGrid()
  {
    m_stack->beginMacro(tr("aligning to grid"));
    foreach(QGraphicsItem* item,items()) 
      if (item->type() == Molecule::Type) 
        m_stack->push(new MoveItem(item,toGrid(item->scenePos()) - item->scenePos()));
    m_stack->endMacro();
    update();
  }

  void MolScene::setEditMode(int mode)
  {
    // Reset moveflag (movebug)
    foreach(QGraphicsItem* item, items()) 
      item->setFlag(QGraphicsItem::ItemIsMovable, false);

    // enable moving for all Molecule and atom items
    foreach(QGraphicsItem* item, items())
      if (item->type() == Molecule::Type || item->type() == Atom::Type) 
        item->setFlag(QGraphicsItem::ItemIsSelectable,mode == MolScene::MoveMode);



    // Set the new edit mode and signal other components
    m_editMode = mode;
    emit editModeChange( mode );
  }

  void MolScene::cut()
  {
    /* TODO Using the desktop clipboard*/
    // Check if something is selected
    if (selectedItems().isEmpty()) return;

    // Then do a copy
    copy();

    // Finally delete the selected items
    m_stack->beginMacro(tr("cutting items"));
    foreach (QGraphicsItem* item, selectedItems())
      if (item->type() == Molecule::Type) m_stack->push(new DelItem(item));
    m_stack->endMacro();
  }

  void MolScene::copy()
  {
    // Check if something is selected
    if (selectedItems().isEmpty()) return;

    /* TODO Using the desktop clipboard */
    // Access the clipboard
    QClipboard* clipboard = qApp->clipboard();

    // Calculate total boundingrect
    QRectF totalRect;
    foreach(QGraphicsItem* item, selectedItems())
    {
      QRectF itemRect = item->boundingRect();
      itemRect.translate(item->scenePos());
      totalRect |= itemRect;
    }
    // Add to internal clipboard
    foreach(QGraphicsItem* item, m_clipItems) delete item;
    m_clipItems.clear();
    foreach(QGraphicsItem* item, selectedItems())
      if (item->type() == Molecule::Type)
        m_clipItems.append(new Molecule(dynamic_cast<Molecule*>(item)));

    // Clear selection
    QList<QGraphicsItem*> selList(selectedItems());
    clearSelection();

    // Choose the datatype
    //   clipboard->setText("Test");
    clipboard->setImage(renderImage(totalRect));
    //   clipboard->mimeData( );

    // Restore selection
    foreach(QGraphicsItem* item, selList) item->setSelected(true);

    // Emit paste available signal
    emit pasteAvailable(!m_clipItems.isEmpty());
  }

  void MolScene::paste()
  {
    // Access the clipboard
    //   QClipboard* clipboard = qApp->clipboard();
    /* TODO Using the system clipboard*/

    // Paste all items on the internal clipboard
    m_stack->beginMacro(tr("pasting items"));
    foreach(Molecule* item, m_clipItems) m_stack->push(new AddItem(new Molecule(item),this));
    m_stack->endMacro();
  }

  void MolScene::convertImage()
  {
    QClipboard* clipboard = qApp->clipboard();
    QImage img = clipboard->image();

    if (!img.isNull()) {
      m_stack->beginMacro(tr("converting image using OSRA"));
      QString tmpimg = QDesktopServices::storageLocation(QDesktopServices::TempLocation) + QDir::separator() + "osra.png";
      img.save(tmpimg, "PNG", 100);
      Molecule* mol = call_osra(tmpimg);
      if (mol) 
        m_stack->push(new AddItem(new Molecule(mol), this));
      QFile::remove(tmpimg);
      m_stack->endMacro();
    }
  }
 
  void MolScene::clear()
  {
    // Purge the undom_stack
    m_stack->clear();

    QGraphicsScene::clear();

    // Reinitialize the scene
    //m_hintPoints.clear();
    //initHintItems();
    setEditMode(MolScene::DrawMode);

    m_inputTextItem = new TextInputItem();
    addItem(m_inputTextItem);
    // hide it for now...
    m_inputTextItem->hide();

  }

  QImage MolScene::renderMolToImage (Molecule *mol)
  {
		QRectF rect = mol ->boundingRect();
		QImage image(int(rect.width()),int(rect.height()),QImage::Format_RGB32);
		image.fill(QColor("white").rgb());
		
		// Creating and setting the painter
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing);
		
		// Rendering in the image and saving to file
		render(&painter,QRectF(0,0,rect.width(),rect.height()),QRectF (mol ->mapToScene (rect.topLeft ()), mol ->mapToScene (rect.bottomRight ())));
		return image;
	}
	
	
  QImage MolScene::renderImage(const QRectF &rect)
  {
    // Creating an image
    QImage image(int(rect.width()),int(rect.height()),QImage::Format_RGB32);
    image.fill(QColor("white").rgb());

    // Creating and setting the painter
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    // Rendering in the image and saving to file
    render(&painter,QRectF(0,0,rect.width(),rect.height()),rect);

    return image;
  }

  void MolScene::addMolecule(Molecule* mol)
  {
    Q_CHECK_PTR(mol);
    if (!mol) return;
    m_stack->beginMacro(tr("add molecule"));
    m_stack->push(new AddItem(mol,this)); 
    if (mol->canSplit()) m_stack->push(new SplitMol(mol));
    m_stack->endMacro();
  }

  void MolScene::selectAll()
  {
    // Switch to move mode to make selection posible
    setEditMode(MolScene::MoveMode);

    // Clear any previous selection
    clearSelection();

    // Mark all atoms as selected
    foreach (QGraphicsItem* item, items())
    {
      if (item->type() == Atom::Type)
        item->setSelected(true);
    }
  }


  void MolScene::setHoverRect( QGraphicsItem* item )
  {
    if (item) {
      m_hoverRect->setPath(item->shape());
      m_hoverRect->setPos(item->scenePos());
      //       m_hoverRect->setVisible(true);
      addItem(m_hoverRect);
    } else {
      //     m_hoverRect->setVisible(false);
      removeItem(m_hoverRect);
    }
  }


  // Queries

  bool MolScene::carbonVisible( ) const
  {
    return m_carbonVisible;
  }

  bool MolScene::hydrogenVisible( ) const
  {
    return m_hydrogenVisible;
  }



  qreal MolScene::atomSize( ) const
  {
    return m_atomSize;
  }

  int MolScene::editMode() const
  {
    return m_editMode;
  }

  MolScene::RenderMode MolScene::renderMode() const
  {
    return m_renderMode;
  }

  void MolScene::setRenderMode(MolScene::RenderMode mode)
  {
    m_renderMode = mode;
  }

  QPointF MolScene::toGrid(const QPointF &position)
  {
    QPointF p = position;
    int factor = 40;
    p.rx() = floor(p.x() / factor) * factor;
    p.ry() = floor(p.y() / factor) * factor;

    return p;
  }


  Molecule* MolScene::moleculeAt(const QPointF &pos)
  {
    // Check if there is a molecule at this position
    foreach(QGraphicsItem* item,items(pos))
      if (item->type() == Molecule::Type) return dynamic_cast<Molecule*>(item);

    // Else return NULL
    return 0;

  }

  bool MolScene::textEditItemAt (const QPointF &pos)
  {
	    foreach(QGraphicsItem* item,items(pos))
		if (item->type() == TextInputItem::Type) return true;
		return false;
	}

  Atom* MolScene::atomAt(const QPointF &pos)
  {
    // Check if there is a atom at this position
    foreach(QGraphicsItem* item,items(pos))
      if (item->type() == Atom::Type) return dynamic_cast<Atom*>(item);

    // Can't find an atom at that location
    return 0;
  }

  Bond* MolScene::bondAt(const QPointF &pos)
  {
    // Check if there is a bond at this position
    foreach( QGraphicsItem* item,items(pos))
      if (item->type() == Bond::Type) return dynamic_cast<Bond*>(item);

    // Else return NULL
    return 0;
  }

  // Event handlers

  bool MolScene::event(QEvent* event)
  {
    // Execute default behaivior
    bool accepted = QGraphicsScene::event(event);

    // Check whether copying is available
    if ((event->type() == QEvent::GraphicsSceneMouseRelease) || (event->type() == QEvent::KeyRelease))
    {
      emit copyAvailable(!selectedItems().isEmpty());
      //     emit pasteAvailable(!m_clipItems.isEmpty());
      emit selectionChange( );
    }

    // Execute default behavior
    return accepted;
  }

  void MolScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
  {
    m_toolGroup->activeTool()->mousePressEvent(event);

    // Execute default behavior
    QGraphicsScene::mousePressEvent(event);	
  }

  void MolScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
  {
    m_toolGroup->activeTool()->mouseMoveEvent(event);

    // Execute default behavior
    QGraphicsScene::mouseMoveEvent(event);
  }


  void MolScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
  {
    m_toolGroup->activeTool()->mouseReleaseEvent(event);

    // Execute the normal behavior
    QGraphicsScene::mouseReleaseEvent(event);
  }


  void MolScene::mouseDoubleClickEvent( QGraphicsSceneMouseEvent * event )
  {
    m_toolGroup->activeTool()->mouseDoubleClickEvent(event);

    // Execute default behavior
    QGraphicsScene::mouseDoubleClickEvent( event );
  }



  //////////////////////////////////////////////////////////////////////////////
  //
  // Text Mode
  //
  //////////////////////////////////////////////////////////////////////////////

	void MolScene::textModePress(QGraphicsSceneMouseEvent* event) {
		if (textEditItemAt (event ->scenePos())) {
			m_inputTextItem ->setFocus();
		}
		else {
		Atom * atom = atomAt(event->scenePos());
		if (atom) {
			m_inputTextItem ->clickedOn (atom);

				}
/*		
		else {
			QGraphicsTextItem *text = addText("");
			text ->show ();
			text ->setFlag(QGraphicsItem::ItemIsSelectable);
			text->setTextInteractionFlags(Qt::TextEditorInteraction);
			text->setPos (event->buttonDownScenePos(event->button()));
			text ->setFocus ();

		}
*/
		
			}
	}
	
	void MolScene::textModeRelease(QGraphicsSceneMouseEvent* event) 
  {
    Q_UNUSED(event)
	}
	



	




























  void MolScene::keyPressEvent(QKeyEvent* keyEvent)
  {
	  if ( !m_inputTextItem ->hasFocus ()) {
    // Declare item
    QGraphicsItem* item;
    Atom* atom;
    //   Bond* bond;
    //   Molecule* mol;
    QSet<Molecule*> molSet;

    switch (keyEvent->key())
    {
      case Qt::Key_Delete:
        m_stack->beginMacro(tr("removing item(s)"));
        // First delete all selected molecules
        foreach (item, selectedItems())
          if (item->type() == Molecule::Type)
          {
            m_stack->push(new DelItem(item));
          }
        //       // Then delete 
        //       foreach (item, selectedItems())
        //         if (item->type() == Bond::Type)
        //         {
        //           bond = dynamic_cast<Bond*>(item);
        //           mol = bond->molecule();
        //           m_stack->push(new DelBond(bond));
        //           if (mol->canSplit()) m_stack->push(new SplitMol(mol));
        //         };

        // Then delete all selected atoms
        foreach (item, selectedItems())
          if (item->type() == Atom::Type)
          {
            atom = dynamic_cast<Atom*>(item);
            molSet << atom->molecule();
            m_stack->push(new DelAtom(atom));
          }

        // Cleanup the affected molecules
        foreach (Molecule* mol, molSet)
        {
          if (mol->canSplit()) m_stack->push(new SplitMol(mol));
          if (mol->atoms().isEmpty()) m_stack->push(new DelItem(mol));
        }

        // Finally delete all the residues
        foreach (item, selectedItems()) m_stack->push(new DelItem(item));

        m_stack->endMacro();
        keyEvent->accept();
        break;
		case Qt::Key_Backspace:
			m_stack->beginMacro(tr("removing item(s)"));
			// First delete all selected molecules
			foreach (item, selectedItems())
			if (item->type() == Molecule::Type)
			{
				m_stack->push(new DelItem(item));
			}
			//       // Then delete 
			//       foreach (item, selectedItems())
			//         if (item->type() == Bond::Type)
			//         {
			//           bond = dynamic_cast<Bond*>(item);
			//           mol = bond->molecule();
			//           m_stack->push(new DelBond(bond));
			//           if (mol->canSplit()) m_stack->push(new SplitMol(mol));
			//         };
			
			// Then delete all selected atoms
			foreach (item, selectedItems())
			if (item->type() == Atom::Type)
			{
				atom = dynamic_cast<Atom*>(item);
				molSet << atom->molecule();
				m_stack->push(new DelAtom(atom));
			}
			
			// Cleanup the affected molecules
			foreach (Molecule* mol, molSet)
        {
			if (mol->canSplit()) m_stack->push(new SplitMol(mol));
			if (mol->atoms().isEmpty()) m_stack->push(new DelItem(mol));
        }
			
			// Finally delete all the residues
			foreach (item, selectedItems()) m_stack->push(new DelItem(item));
			
			m_stack->endMacro();
			keyEvent->accept();
			break;
			
			
      case Qt::Key_Up:
        m_stack->beginMacro("moving item(s)");
        foreach (item, selectedItems())
          m_stack->push(new MoveItem(item,QPointF(0,-10)));
        m_stack->endMacro();
        keyEvent->accept();
        break;
      case Qt::Key_Down:
        m_stack->beginMacro("moving item(s)");
        foreach (item, selectedItems())
          m_stack->push(new MoveItem(item,QPointF(0,10)));
        m_stack->endMacro();
        keyEvent->accept();
        break;
      case Qt::Key_Left:
        m_stack->beginMacro("moving item(s)");
        foreach (item, selectedItems())
          m_stack->push(new MoveItem(item,QPointF(-10,0)));
        m_stack->endMacro();
        keyEvent->accept();
        break;
      case Qt::Key_Right:
        m_stack->beginMacro("moving item(s)");
        foreach (item, selectedItems())
          m_stack->push(new MoveItem(item,QPointF(10,0)));
        m_stack->endMacro();
        keyEvent->accept();
        break;
      case Qt::Key_Escape:
        clearSelection();
        break;
      default:
        keyEvent->ignore();
    }
	  }
  
    // execute default behaviour (needed for text tool)
    QGraphicsScene::keyPressEvent(keyEvent);
  }

  QImage MolScene::toImage (OpenBabel::OBMol *obmol)
  {
    Molecule *mol = toMol(obmol);
		QImage im = renderMolToImage (mol);
		removeItem (mol);
		delete mol;
		return im;
	}

	
  Molecule *MolScene::toMol (OpenBabel::OBMol *obmol)
  {
    qreal k = 40/*bondLength()*/ / 1.5; // FIXME
		Molecule *mol = new Molecule ();
		mol->setPos(QPointF(0,0));
		qreal x = 0;
		qreal y = 0;
		OpenBabel::OBAtom *first_atom = obmol ->GetAtom (1);
		if (first_atom) {
			x = first_atom ->x ();
			y = -first_atom ->y ();
		}
		std::vector <Atom *>ats;
		std::vector <Bond *>bonds;

		//	for (unsigned int i = 0; i <= obmol ->NumAtoms();i++)
		FOR_ATOMS_OF_MOL(obatom,obmol)
		{
			//	OpenBabel::OBAtom *obatom = obmol ->GetAtom(i);
			//  			scene->addRect(QRectF(atom->GetX(),atom->GetY(),5,5));
			//           Atom* atom =
			ats.push_back (new Atom (QPointF((obatom->x() - x)*k,(-obatom->y()-y)*k),number2symbol(obatom->GetAtomicNum()), autoAddHydrogen (), mol));
			//mol->addAtom();
		}
		
		// Add bonds one-by-one
		/// Mind the numbering!
		//	for (unsigned int i = 0; i < obmol ->NumBonds();i++)
		FOR_BONDS_OF_MOL(obbond,obmol)
		{
			// Loading the OpenBabel objects
			//	OpenBabel::OBBond *obbond = obmol ->GetBond(i);
			OpenBabel::OBAtom *a1 = obbond->GetBeginAtom();
			OpenBabel::OBAtom *a2 = obbond->GetEndAtom();
			if (a1 ->IsHydrogen()) continue;
			if (a2 ->IsHydrogen()) continue;
			
			Atom* atomA = 0;
			Atom* atomB = 0;
			
			if (a1 ->GetIdx() > 0 && (a1 ->GetIdx() -1) <ats.size ()) 
			atomA = ats [a1 ->GetIdx() -1];
			if (a2 ->GetIdx() > 0 && (a2 ->GetIdx() -1) <ats.size ()) 
			atomB = ats [a2 ->GetIdx() -1];
			std::cerr<< a2 ->GetIdx() -1<<"  "<<a1 ->GetIdx() -1<<std::endl;

			if (atomA && atomB)	{
				Bond* bond  = new Bond (atomA,atomB,obbond->GetBondOrder());
				// Set special bond types
				if (obbond->IsWedge())
					bond->setType( Bond::Wedge );
				if (obbond->IsHash()) 
					bond->setType( Bond::Hash );
				
				bonds.push_back(bond);

			}
			// Normalizing
			//             factor = scene->getBondLength()/obbond->GetLength();
		}
		
		// // Normalizing molecule
		// mol->scale(factor,factor);
		// mol->setAtomSize(LABEL_SIZE/factor);
		for (unsigned int i=0; i < ats.size (); i++) {
			if (ats[i] ->element () == "H") continue;
			mol ->addAtom (ats[i]);
		}
		for (unsigned int i=0; i < bonds.size (); i++) {

			Atom *a1 = bonds[i] ->beginAtom ();
			Atom *a2 = bonds[i] ->endAtom ();
			if (a1 ->element () == "H") continue;
			if (a2 ->element () == "H") continue;
			mol ->addBond (bonds[i]);
		}		
		//int nu = 0;
		//QList <Atom *> atts = mol ->atoms ();

                /*
		for (int i = 0; i < atts.size () ; i++) {
			atts[i] ->setNumber(i); 
		}
                */
                mol->numberAtoms();
		return mol;
		
	}
	

	
	

  QUndoStack * MolScene::stack()
  {
    return m_stack;
  }

  QFont MolScene::atomSymbolFont() const
  {
    return m_atomSymbolFont;
  }

  void MolScene::setAtomSymbolFont(const QFont & font)
  {
    m_atomSymbolFont = font;
  }



} // namespace
