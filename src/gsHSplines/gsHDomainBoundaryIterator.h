/** @file gsHDomainBoundaryIterator.h

    @brief Provides declaration of iterator on boundary of hierarchical basis.

    This file is part of the G+Smo library. 

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    
    Author(s): A. Mantzaflaris
*/

#pragma once

#include <gsHSplines/gsHDomain.h>

#include <gsNurbs/gsTensorBSplineBasis.h>

#include <gsCore/gsDomainIterator.h>

namespace gismo
{

// Documentation in gsDomainIterator
/**
  * @brief 
  * Re-implements gsDomainIterator for iteration over all boundary
  * elements of a <b>hierarchical</b> parameter domain.
  * 
  * <em>See
  * gsDomainIterator for more detailed documentation and an example of
  * the typical use!!!</em>\n Used, e.g., for basis of classes
  * gsHTensorBasis or gsTHBSplineBasis.
  *
  * \ingroup HSplines
  */

template<typename T, unsigned d>
class gsHDomainBoundaryIterator: public gsDomainIterator<T>
{
public:

    typedef kdnode<d, unsigned> node;

    typedef typename node::point point; 

    typedef typename std::vector<T>::const_iterator  uiter;

    typedef gsHDomain<d,unsigned> hDomain;

    typedef typename hDomain::const_literator leafIterator;

public:

    gsHDomainBoundaryIterator(const gsHTensorBasis<d, T> & hbs, 
                              const boxSide & s )
        : gsDomainIterator<T>(hbs)
    {
        // Initialize mesh data
        m_meshStart.resize(d);
        m_meshEnd  .resize(d);

        // Initialize cell data
        m_curElement.resize(d);
        m_lower     .resize(d);
        m_upper     .resize(d);

        // Allocate breaks
        m_breaks = std::vector<std::vector<T> >(d, std::vector<T>());

        // Set to one quadrature point by default
        m_quadrature.setNodes( gsVector<int>::Ones(d) );

        // Get the side information
        par = s.parameter();
        dir = s.direction();

        initLeaf(hbs.tree());
    }

    // ---> Documentation in gsDomainIterator.h
    bool next()
    {
        this->m_isGood = nextLexicographic(m_curElement, m_meshStart, m_meshEnd);

        if (this->m_isGood) // new element in m_leaf
            updateElement();
        else // went through all elements in m_leaf
            this->m_isGood = nextLeaf();

        return this->m_isGood;
    }

    // ---> Documentation in gsDomainIterator.h
    bool next(index_t increment)
    {
        for (index_t i = 0; i < increment; i++)
            this->m_isGood = nextLexicographic(m_curElement, m_meshStart, m_meshEnd);

        if (this->m_isGood) // new element in m_leaf
            updateElement();
        else // went through all elements in m_leaf
            this->m_isGood = nextLeaf();

        return this->m_isGood;
    }
    
    /// Resets the iterator so that it can be used for another
    /// iteration through all boundary elements.
    void reset()
    {
        const gsHTensorBasis<d, T>* hbs =  dynamic_cast<const gsHTensorBasis<d, T> *>(m_basis);
        initLeaf(hbs->tree());
    }
    
    // ---> Documentation in gsDomainIterator.h Compute a suitable
    // quadrature rule of the given order for the current element
    void computeQuadratureRule(const gsVector<int>& numIntNodes)
    {
        GISMO_ASSERT( numIntNodes[dir]==1, 
                      "Can only use one point in the fixed direction." );

        m_quadrature.setNodes(numIntNodes);
        m_quadrature.mapTo(m_lower, m_upper, this->quNodes, this->quWeights);
    }

    void computeQuadratureRuleDefault()
    {
        // uses same formula as gsGaussAssembler::getNumIntNodesFor( gsBasis )
        gsVector<int> numIntNodes( m_basis->dim() );
        for (int i = 0; i < m_basis->dim(); ++i)
            numIntNodes[i] = m_basis->degree(i) + 1;

        numIntNodes[dir] = 1;
        computeQuadratureRule( numIntNodes );
    }
    
    // get the basis function indices which are active in the current
    // element
    void getActiveFunctions(gsMatrix<unsigned>& act)
    {
        this->m_basis->active_into(center, act);
    }
    
    const gsMatrix<unsigned>& computeActiveFunctions()
    {
        this->m_basis->active_into(center, this->activeFuncs);
        return this->activeFuncs;
    }

    const gsVector<T>& lowerCorner() const { return m_lower; }

    const gsVector<T>& upperCorner() const { return m_upper; }

    int getLevel() const
    {
        return m_leaf.level();
    }

private:

    gsHDomainBoundaryIterator();

    /// Navigates to the first leaf on our side
    void initLeaf(const hDomain & tree_domain)
    {
        // Get the first leaf
        m_leaf = tree_domain.beginLeafIterator();

        for (; m_leaf.good(); m_leaf.next() )
        {
            // Check if this leaf is on our side
            if ( leafOnBoundary() )
            {
                updateLeaf();
                return;
            }
        }
        GISMO_ERROR("No leaves.\n");
    }


    /// returns true if there is a another leaf with a boundary element
    bool nextLeaf()
    {
        for (m_leaf.next(); m_leaf.good(); m_leaf.next() )
        {
            // Check if this leaf is on our side
            if ( leafOnBoundary() )
            {
                updateLeaf();
                return true;
            }
        }
        return false;
    }

    /// returns true if the current leaf is on our side
    bool leafOnBoundary() const
    {
        if ( par )
        {
            // AM: a little ugly for now, to be improved
            return 
                static_cast<size_t>(m_leaf.upperCorner().at(dir) )
                == 
                static_cast<const gsHTensorBasis<d,T>*>(m_basis)
                ->tensorLevel(m_leaf.level()).knots(dir).uSize() - 1;// todo: more efficient
        }
        else
        {
            return m_leaf.lowerCorner().at(dir) == 0;
        }
    }

    /// Computes lower, upper and center point of the current element, maps the reference
    /// quadrature nodes and weights to the current element, and computes the
    /// active functions.
    void updateLeaf()
    {
        const point & lower = m_leaf.lowerCorner();
        const point & upper = m_leaf.upperCorner();
        // gsDebug<<"leaf "<<  lower.transpose() <<", " 
        //        << upper.transpose() <<"\n";

        const int level2 = m_leaf.level();

        // Update leaf box
        for (unsigned dim = 0; dim < d; ++dim)
        {
            const unsigned start = lower(dim);
            const unsigned end  = upper(dim) ;

            const gsKnotVector<T> & kv =
                static_cast<const gsHTensorBasis<d,T>*>(m_basis)
                ->tensorLevel(level2).component(dim).knots();

            // knotVals = kv.unique()

            m_breaks[dim].clear();
            if ( dim == dir )
            {
                if ( par )
                {
                    m_breaks[dim].push_back(kv.uValue(end-1));
                    m_breaks[dim].push_back(kv.uValue(end  ));

                    //  = knotValues.begin() + end -1;
                    //  = knotValues.begin() + end   ;
                }
                else
                {
                    m_breaks[dim].push_back(kv.uValue(start));
                    m_breaks[dim].push_back(kv.uValue(start+1));
                }
            }
            else
            {
                for (unsigned index = start; index <= end; ++index)
                    m_breaks[dim].push_back(kv.uValue(index));
            }

            m_curElement(dim) = 
            m_meshStart(dim)  = m_breaks[dim].begin();


            // for n breaks, we have n - 1 elements (spans)
            m_meshEnd(dim) =  m_breaks[dim].end() - 1;
        }

        // We are at a new element, so update cell data
        updateElement();
    }

    /// Computes lower, upper and center point of the current element, maps the reference
    /// quadrature nodes and weights to the current element, and computes the
    /// active functions.
    void updateElement()
    {
        // Update cell data
        for (unsigned i = 0; i < dir ; ++i)
        {
            m_lower[i]  = *m_curElement[i];
            m_upper[i]  = *(m_curElement[i]+1);
            center[i] = T(0.5) * (m_lower[i] + m_upper[i]);
        }
        m_lower[dir] = 
        m_upper[dir] =
        center [dir] = (par ? *(m_curElement[dir]+1) : *m_curElement[dir] );
        for (unsigned i = dir+1; i < d; ++i)
        {
            m_lower[i] = *m_curElement[i];
            m_upper[i] = *(m_curElement[i]+1);
            center [i] = T(0.5) * (m_lower[i] + m_upper[i]);
        }

        // Update quadrature rule
        m_quadrature.mapTo(m_lower, m_upper, this->quNodes, this->quWeights);

        // Update Active basis functions
        computeActiveFunctions();
    }

// =============================================================================
// members
// =============================================================================

public:

    using gsDomainIterator<T>::center;
    using gsDomainIterator<T>::m_basis;

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

private:

    // Boundary parameters
    unsigned dir; // direction normal to the boundary
    bool par;     // parameter value

    // The current leaf node of the tree
    leafIterator m_leaf;

    // Coordinates of the grid cell boundaries
    // \todo remove this member
    std::vector< std::vector<T> > m_breaks;

    // Extent of the tensor grid
    gsVector<uiter, d> m_meshStart, m_meshEnd;

    // Current element as pointers to it's supporting mesh-lines
    gsVector<uiter, d> m_curElement;

    // parameter coordinates of current grid cell
    gsVector<T> m_lower, m_upper;

    // Quadrature rule
    gsGaussRule<T> m_quadrature;

};

} // end namespace gismo
