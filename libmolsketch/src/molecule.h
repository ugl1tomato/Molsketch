/***************************************************************************
 *   Copyright (C) 2007 by Harm van Eersel                                 *
 *   Copyright (C) 2009 by Tim Vandermeersch                               *
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

/** @file
* This file is part of molsKetch and defines the class Molecule.
*
* @author Harm van Eersel <devsciurus@xs4all.nl>
*/


#ifndef MOLECULE_H
#define MOLECULE_H

#include <molsketch/graphicsitemtypes.h>

#include <QList>
#include <QGraphicsItemGroup>

class QString;
class QPoint;
class QPainter;
class QXmlStreamReader;
class QXmlStreamWriter;

namespace OpenBabel {
  class OBMol;
}

namespace Molsketch {

  class Atom;
  class Bond;
  class Ring;
  class MolScene;
  class ElectronSystem; // under construction

/**
 * Represents a molecule on the scene. It can be created either as an empty molecule, 
 * with a set of atoms and bonds or as a copy of another molecule.
 *
 * @author Harm van Eersel <devsciurus@xs4all.nl>
 * @since Hydrogen
 */
class Molecule : public QGraphicsItemGroup
{
  public:
    // Enabling typecasting
    enum { Type = GraphicsItemTypes::MoleculeType };
    /**
     * @return The QGraphicsItem type of the class. Needed for Qt typecasting.
     */
    int type() const { return Type; }



    // Constructors and destructor
    /** Creates a molecule with @p parent on MolScene @p scene. */
    Molecule(QGraphicsItem* parent = 0, MolScene* scene = 0);
    /** Creates a molecule from the atoms and bonds of the sets with @p parent on MolScene @p scene. */
    Molecule(QSet<Atom*>, QSet<Bond*>, QGraphicsItem* parent = 0, MolScene* scene = 0);
    /** Creates a copy of molecule @p mol with @p parent on MolScene @p scene. */
    Molecule(Molecule* mol, QGraphicsItem* parent = 0, MolScene* scene = 0);



    QRectF boundingRect() const;

    /** Paint method to draw the atom onto a QPainter. Needed for Qt painting.*/
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);


    // Manipulation methods

    /**
    * This method add an atom to the molecule with @p element as element at position @p position.
    *
    * @param element element of the atom to be added to the molecule
    * @param position the position where the atom should be added to the molecules in scene coordinates
    * 
    * @return return a pointer to the new atom which has been added to the molecule
    */
    Atom* addAtom(const QString &element, const QPointF &position, bool implicitHydrogen, QColor c = QColor (0, 0, 0));

    /**
    * This method adds an existing atom to the molecule.
    * 
    * @param atom a pointer to the atom that should be added to the molecule.
    *
    * @return return a pointer to the atom just added to the molecule.
    */
    Atom* addAtom(Atom* atom);

    /**
    * This method removes an atom from the molecule. The bonds connected to this atom will be removed as well. 
    * The atom and bonds will be removed from the scene, but the ownership will be passed on to the caller. This 
    * can be neccecary for undo operations. 
    * 
    * @param atom a pointer to the atom that should be removed from the molecule.
    *
    * @return a list with the bonds which were connected to the removed atom..
    */
    QList<Bond*> delAtom(Atom* atom);

    /** Adds a bond between @p atomA and @p atomB with @p order and @p type. */
    Bond* addBond(Atom* atomA, Atom* atomB, int order = 1, int type = 0, QColor c = QColor (0, 0, 0));
    /** Adds existing bond @p bond to the molecule. */
    Bond* addBond(Bond* bond);
    /** Deletes @p bond from the molecule. */
    void delBond(Bond* bond);

//    /** Automaticly adds an atom with a bond to @p startAtom at a convenient position. */
//   void addAutoAtom(Atom* startAtom);

    /**
      * Splits the molecule up in different seperate molecules. Used to clean up the molecule after removing the connection 
    * between two or more parts of the molecule. 
    *
    * @return a list of the submolecules of which this molecule exists.
    */
    QList<Molecule*> split();


    /**
    * This method rebuilds the atom by removing all atoms and bonds from the molecule and consequently readding them. 
    * This is a hack to reset the boundingbox of the molecule after moving one of the atoms and is this method is in need 
    * of a proper solution.
    */
    void rebuild();
	  void numberAtoms ();


//   void normalize();
//   void setAtomSize(qreal pt);

    // Query methods
    /** Returns a pointer to the atom at position @p pos, or NULL id none. */
    Atom* atomAt(const QPointF &pos) const;
//    /** Returns a pointer to the atom with @p id. */
//     Atom* atom(int id) const;
	  
	  Atom* atomN (const int n) const;

    /** Returns a pointer to the bond at position @p pos or NULL id none. */
    Bond* bondAt(const QPointF &pos) const;
//    /** Returns a pointer to bond with @p id. */
//     Bond* bond(int id) const;
    /** Returns a list of the bonds connected to @p atom. */
    QList<Bond*> bonds(const Atom* atom);
    /** Returns a pointer to the bond between @p atomA and @p atomB, or a NULL if none. */
    Bond* bondBetween(Atom* atomA, Atom* atomB) const;

    /**
     * @return @c true if the molecule exists of two seperate submolecules, and @c false otherwise.
     */
    bool canSplit() const;

    /** 
     * Get a list of the atoms in the molecule. 
     */
    const QList<Atom*>& atoms() const;
    /** 
     * Get a list of the bonds in the molecule 
     */
    const QList<Bond*>& bonds() const;
	  
    /**
     * Get a list of the rings in the molecule.
     */
    const QList<Ring*>& rings() const
    {
      return m_rings;
    }


    /** Returns the MolScene of the molecule. */
    virtual MolScene* scene() const;

    /** Returns the molecule formula. */
    QString formula() const;
    /** Returns the weigth of the molecule. */
    double weight() const;
    /** Returns the charge of the molecule. */
    int charge() const;
    /** Returns the charge as a string with an appropiate + or - sign. */
    QString chargeID() const;
	  
    /**
     * Get the SMILES for this molecule.
     */
    QString smiles() const;

    OpenBabel::OBMol* OBMol() const;
    void perceiveRings();


    /**
     * Read Molecule data from the specified XML stream.
     */
    void readXML(QXmlStreamReader &xml);
    /**
     * Write this Molecule to the specified XML stream.
     */
    void writeXML(QXmlStreamWriter &xml);

    /**
     * Invalidate the electron systems. To be called when Atom/Bond properties
     * change.
     */
    void invalidateElectronSystems();


  protected:
	  
	  
//    /** Event handler for mouse press events on the molecule. */
//     virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
//    /** Event handler for mouse release events on the molecule.*/
//    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
   /** Event handler for changes of the molecule. Needed for rotation handling.*/
   QVariant itemChange(GraphicsItemChange change, const QVariant &value);

   /**
    * Update the internal ElectronSystem representation based on the current
    * bond orders.
    *
    * 1. create a SigmaElectrons instance for each bond
    * 2. create a PiElectrons instance for each 
    */
   void updateElectronSystems();
   
   bool m_electronSystemsUpdate;
   QList<ElectronSystem*> m_electronSystems;



  private:
    // Internal representation
    /** A list of pointers to the atoms of the molecule. Used as internal reprentation. */
    QList<Atom*> m_atomList;
    /** A list of pointers to the bonds of the molecule. Used as internal representation. */
    QList<Bond*> m_bondList;
    
    QList<Ring*> m_rings;
  };

} // namespace

#endif
