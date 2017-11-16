/** @file gsHTensorBasis.hpp

    @brief Provides implementation of HTensorBasis common operations.

    This file is part of the G+Smo library.
    
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): G. Kiss, A. Mantzaflaris, J. Speh
*/

#pragma once 

#include <gsUtils/gsMesh/gsMesh.h>

#include <gsIO/gsXml.h>
#include <gsIO/gsXmlGenericUtils.hpp>

namespace gismo
{

template<unsigned d, class T>
gsMatrix<T> gsHTensorBasis<d,T>::support() const
{
    return m_bases[0]->support();
}

//i in continuous numbering
template<unsigned d, class T>
gsMatrix<T> gsHTensorBasis<d,T>::support(const unsigned & i) const
{
    // Get the level
    int lvl = levelOf(i);
    // Return the the support
    return m_bases[lvl]->support( m_xmatrix[lvl][ i - m_xmatrix_offset[lvl] ] );
}

// S.K.
template<unsigned d, class T> inline
int gsHTensorBasis<d,T>::getLevelAtPoint(const gsMatrix<T> & Pt) const
{
    GISMO_ASSERT(Pt.cols() == 1, "Waiting for single point");
    point loIdx;

    const int maxLevel = m_tree.getMaxInsLevel();

    for( int i =0; i < Dim; i++)
        loIdx[i] = m_bases[maxLevel]->knots(i).uFind( Pt(i,0) ).uIndex();

    return m_tree.levelOf( loIdx, maxLevel);
}

template<unsigned d, class T> inline
void gsHTensorBasis<d,T>::getLevelUniqueSpanAtPoints(const  gsMatrix<T> & Pt,
                                                     gsVector<unsigned> & lvl,
                                                     gsMatrix<unsigned> & loIdx ) const
{
    lvl.resize( Pt.cols() );
    loIdx.resize( Pt.rows(), Pt.cols() );
    lvl.setZero();
    loIdx.setZero();
    for( index_t i = 0; i < Pt.cols(); i++)
    {
        lvl[i] = static_cast<unsigned>( getLevelAtPoint( Pt.col(i) ) );
        for( index_t j = 0; j < Pt.rows(); j++)
            loIdx(j,i) = m_bases[ lvl[i] ]->knots(j).uFind( Pt(j,i) ).uIndex() ;
    }
}

template<unsigned d, class T> inline
void gsHTensorBasis<d,T>::numActive(const gsMatrix<T> & u, gsVector<unsigned>& result) const 
{
    result.resize( u.cols() );
    result.setZero();

    point low, upp, cur;
    const int maxLevel = m_tree.getMaxInsLevel();

    for(index_t p = 0; p < u.cols(); p++ ) //for all input points
    {
        for(int i = 0; i != d; ++i)
            low[i] = m_bases[maxLevel]->knots(i).uFind(u(i,p)).uIndex();

        // Identify the level of the point
        const int lvl = m_tree.levelOf(low, maxLevel);

        for(int i = 0; i <= lvl; i++)
        {
            m_bases[i]->active_cwise(u.col(p), low, upp);
            cur = low;
            do //iterate over all points in [low,upp]
            {
                CMatrix::const_iterator it =
                    m_xmatrix[i].find_it_or_fail( m_bases[i]->index(cur) );
                
                if( it != m_xmatrix[i].end() )// if index is found
                    result[p]++;
            }
            while( nextCubePoint(cur,low,upp) );
        }
    }
}

template<unsigned d, class T>
void gsHTensorBasis<d, T>::addConnectivity(int lvl, gsMesh<T> & mesh) const
{
    const gsVector<unsigned, d> & low = gsVector<unsigned, d>::Zero();

    const gsTensorBSplineBasis<d, T> & bb = *m_bases[lvl];
    const CMatrix & cmat = m_xmatrix[lvl];

    // Last tensor-index in level lvl
    gsVector<unsigned, d> end(d);
    for (unsigned i = 0; i < d; ++i)
        end(i) = bb.component(i).size() - 1;

    unsigned k, s;
    gsVector<unsigned, d> v, upp;
    for (unsigned i = 0; i < d; ++i) // For all axes
    {
        s = bb.stride(i);
        v = low;
        upp = end;
        upp[i] = 0; // suppress to face v[i]==0

        do // Insert all edges normal to axis i
        {
            k = bb.index(v);
            for (unsigned j = 0; j != end[i]; ++j)
            {
                if (cmat.bContains(k) && cmat.bContains(k + s))
                {
                    // inefficient for now
                    const index_t kInd = m_xmatrix_offset[lvl] +
                        (std::lower_bound(cmat.begin(), cmat.end(), k)
                         - cmat.begin());

                    // inefficient for now
                    const index_t kNextInd = m_xmatrix_offset[lvl] +
                        (std::lower_bound(cmat.begin(), cmat.end(), k + s)
                         - cmat.begin());

                    mesh.addEdge(kInd, kNextInd);
                }
                k += s;
            }
        } while (nextCubePoint(v, low, upp));
    }
}

template<unsigned d, class T>
void gsHTensorBasis<d, T>::connectivity(const gsMatrix<T> & nodes, int level, gsMesh<T> & mesh) const
{
    const index_t sz = size();
    GISMO_ASSERT(nodes.rows() == sz, "Invalid input.");

    // Add all vertices
    for (index_t i = 0; i< sz; ++i)
        mesh.addVertex(nodes.row(i).transpose());

    addConnectivity(level, mesh);
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::connectivity(const gsMatrix<T> & nodes, gsMesh<T> & mesh) const 
{
    const index_t sz  = size();
    GISMO_ASSERT( nodes.rows() == sz, "Invalid input.");

    // Add vertices
    for(index_t i = 0; i< sz; ++i )
        mesh.addVertex( nodes.row(i).transpose() );

    // For all levels
    for(unsigned lvl = 0; lvl <= maxLevel(); lvl++)
    {
        addConnectivity(lvl, mesh);
    }
}

template<unsigned d, class T>
index_t gsHTensorBasis<d,T>::size() const 
{
    return m_xmatrix_offset.back();
}
template<unsigned d, class T>
void gsHTensorBasis<d,T>::refine_withCoefs(gsMatrix<T> & coefs, gsMatrix<T> const & boxes)
{
    std::vector<gsSortedVector<unsigned> > OX = m_xmatrix;
    refine(boxes);
    gsSparseMatrix<> transf;
    this->transfer(OX, transf);
    std::cout<<"tranf orig:\n"<<transf<<std::endl;
    coefs = transf*coefs;
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::refineElements_withCoefs(gsMatrix<T> & coefs,std::vector<unsigned> const & boxes)
{
    std::vector<gsSortedVector<unsigned> > OX = m_xmatrix;
    refineElements(boxes);
    gsSparseMatrix<> transf;
    this->transfer(OX, transf);
    //std::cout<<"tranf orig:\n"<<transf<<std::endl;
    coefs = transf*coefs;
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::refineElements_withTransfer(std::vector<unsigned> const & boxes, gsSparseMatrix<T> & tran)
{
    std::vector<gsSortedVector<unsigned> > OX = m_xmatrix;
    this->refineElements(boxes);
    this->transfer(OX, tran);
}


template<unsigned d, class T>
void gsHTensorBasis<d,T>::refineElements_withCoefs2(gsMatrix<T> & coefs,std::vector<unsigned> const & boxes)
{
    std::vector<gsSortedVector<unsigned> > OX = m_xmatrix;
    refineElements(boxes);
    gsSparseMatrix<> transf;
    this->transfer2(OX, transf);
    //std::cout<<"tranf 2:\n"<<transf<<std::endl;
    coefs = transf*coefs;
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::uniformRefine_withCoefs(gsMatrix<T>& coefs, int numKnots, int mul)
{
    std::vector<gsSortedVector<unsigned> > OX = m_xmatrix;
    //uniformRefine(numKnots);
    //gsMatrix<> transf;
    //CMatrix temp;
    //this->m_xmatrix.insert(this->m_xmatrix.begin(), temp);
    //gsVector<unsigned> level;
    //gsMatrix<unsigned> p1, p2;
    //this->m_tree.getBoxes(p1,p2,level);
    std::vector<unsigned> boxes;
    unsigned lvl;
    
    for ( typename hdomain_type::literator it = m_tree.beginLeafIterator(); it.good(); it.next() )
    {
        //        gsInfo <<" level : "<< it.level() <<"\n";
        //        gsInfo <<" lower : "<< it.lowerCorner() <<"\n";
        //        gsInfo <<" upper : "<< it.upperCorner() <<"\n";

        lvl = it.level() + 1;
        const point & l = it.lowerCorner();
        const point & u = it.upperCorner();

        boxes.push_back(lvl);
        for( unsigned i = 0; i < d; i++)
            boxes.push_back( l(i) * 2);
        for( unsigned i = 0; i < d; i++)
            boxes.push_back( u(i) * 2);
    }

    this->clone()->refineElements_withCoefs(coefs, boxes);
    this->uniformRefine(numKnots, mul);
    //this->m_xmatrix.erase(this->m_xmatrix.begin(),this->m_xmatrix.begin()+1);
    //coefs = transf*coefs;
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::refine(gsMatrix<T> const & boxes, int refExt)
{
    GISMO_ASSERT(boxes.rows() == d, "refine() needs d rows of boxes.");
    GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provied refine() with them.");

#ifndef NDEBUG
    gsMatrix<T> para = support();
    for(int i = 0; i < boxes.cols()/2; i++)
    {
        for( unsigned j = 0; j < d; j++ )
        {
            GISMO_ASSERT( para(j,0) <= boxes(j, 2*i) ,
                          "In refine() the first corner is outside the computational domain.");
            GISMO_ASSERT( para(j,1) >= boxes(j, 2*i+1),
                          "In refine() the second corner is outside the computational domain." );
        }
    }
#endif
    
    if( refExt == 0 )
    {
        // If there is no refinement-extension, just use the
        // "regular" refinement function refine( gsMatrix )
        this->refine( boxes );

        // Make sure there are enough levels
        needLevel( m_tree.getMaxInsLevel() );
    }
    else
    {
        // If there is a refinement-extension, we will have to use
        // refineElements( std::vector )
        //
        // Each box will be represented by 2*d+1 entries specifying
        // <level to be refined to>,<lower corner>,<upper corner>
        const int offset = 2*d+1;

        // Initialize vector of size
        // "entries per box" times "number of boxes":
        std::vector<unsigned> refVector( offset * boxes.cols()/2 );
        gsMatrix<T> ctr(d,1);

        // Loop over all boxes:
        for(index_t i = 0; i < boxes.cols()/2; i++)
        {
            ctr = ( boxes.col( 2*i ) + boxes.col( 2*i+1) )/2;

            // Compute the level we want to refine to.
            // Note that, if the box extends over several elements,
            // the level at the centerpoint will be taken for reference
            const int refLevel = getLevelAtPoint( ctr ) + 1;

            // Make sure there are enough levels
            needLevel( refLevel );

            for(index_t j = 0; j < boxes.rows();j++)
            {
                // Convert the parameter coordinates to (unique) knot indices
                const gsKnotVector<T> & kv = m_bases[refLevel]->knots(j);
                int k1 = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd(),
                                           boxes(j,2*i  ) ) - 1).uIndex();
                int k2 = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd()+1,
                                           boxes(j,2*i+1) ) - 1).uIndex();

                // If applicable, add the refinement extension.
                // Note that extending by one cell on level L means
                // extending by two cells in level L+1
                ( k1 < 2*refExt ? k1=0 : k1-=2*refExt );
                const index_t maxKtIndex = kv.size();
                ( k2 + 2*refExt >= maxKtIndex ? k2=maxKtIndex-1 : k2+=2*refExt);

                // Store the data...
                refVector[i*offset]       = refLevel;
                refVector[i*offset+1+j]   = k1;
                refVector[i*offset+1+j+d] = k2;
            }
        }

        // ...and refine
        this->refineElements( refVector );
    }

    // Update the basis
    update_structure();
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::refine(gsMatrix<T> const & boxes) 
{
    GISMO_ASSERT(boxes.rows() == d, "refine() needs d rows of boxes.");
    GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provide refine() with them.");

#ifndef NDEBUG
    gsMatrix<T> para = support();
    for(int i = 0; i < boxes.cols()/2; i++)
    {
        for( unsigned j = 0; j < d; j++ )
        {
            GISMO_ASSERT( para(j,0) <= boxes(j, 2*i) ,
                          "In refine() the first corner is outside the computational domain.");
            GISMO_ASSERT( para(j,1) >= boxes(j, 2*i+1),
                          "In refine() the second corner is outside the computational domain." );
        }
    }
#endif
    
    gsVector<unsigned,d> k1, k2;
    for(index_t i = 0; i < boxes.cols()/2; i++)
    {
        // 1. Get a small cell containing the box
        const int fLevel = m_bases.size()-1;

        for(index_t j = 0; j < k1.size();j++)
        {
            const gsKnotVector<T> & kv = m_bases.back()->knots(j);
            k1[j] = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd(),
                                      boxes(j,2*i  ) ) - 1).uIndex();
            k2[j] = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd()+1,
                                      boxes(j,2*i+1) ) - 1).uIndex();

            // Trivial boxes trigger some refinement
            if ( k1[j] == k2[j])
            {
                if (0!=k1[j]) {--k1[j];}
                ++k2[j];
            }
        }

        // 2. Find the smallest level in which the box is completely contained
        //const int level = m_tree.query3(k1,k2,fLevel) + 1;
        // make sure that the grid is computed ( needLevel(level) )
        //const tensorBasis & tb = tensorLevel(level);
        //GISMO_UNUSED(tb);

        // Sink box
        m_tree.sinkBox(k1, k2, fLevel);
        // Make sure we have enough levels
        needLevel( m_tree.getMaxInsLevel() );
    }

    // Update the basis
    update_structure();
}


/*
  template<unsigned d, class T>
  void gsHTensorBasis<d,T>::refine(gsDomainIterator<T> const & boxes) 
  {

  }
*/

template<unsigned d, class T>
void gsHTensorBasis<d,T>::refineElements(std::vector<unsigned> const & boxes)
{
    gsVector<unsigned int, d> i1;
    gsVector<unsigned int, d> i2;

    GISMO_ASSERT( (boxes.size()%(2*d + 1))==0,
                  "The points did not define boxes properly. The boxes were not added to the basis.");
    for( unsigned int i = 0; i < (boxes.size())/(2*d+1); i++)
    {
        for( unsigned j = 0; j < d; j++ )
        {
            i1[j] = boxes[(i*(2*d+1))+j+1];
            i2[j] = boxes[(i*(2*d+1))+d+j+1];
        }
        insert_box(i1,i2,boxes[i*(2*d+1)]);
    }

    update_structure();
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::refineSide(const boxSide side, index_t lvl)
{
    const index_t dir = side.direction();
    const index_t par = side.parameter();
    gsMatrix<T> rf = this->support();
    rf(dir,!par) = rf(dir,par);
    for (index_t i = 0; i!=lvl; ++i) // lazy impl., this can be more efficient
        this->refine(rf);
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::matchWith(const boundaryInterface & bi,
                                    const gsBasis<T> & other,
                                    gsMatrix<unsigned> & bndThis,
                                    gsMatrix<unsigned> & bndOther) const
{
    if( const Self_t * _other = dynamic_cast<const Self_t*>( &other) )
    {
        gsVector<unsigned> N(d);

        // tens1 will store the tensor-index on side second(),...
        gsVector<unsigned> tens0(d), tens1(d);

        // see if the orientation is preserved on side second()
        const gsVector<bool> dirOrient = bi.dirOrientation();

        const gsVector<int> dirMap = bi.dirMap();

        // get the global indices of the basis functions which are
        // active on the interface
        bndThis = this->boundary( bi.first().side() );

        // this is only for checking whether, at least, both involved
        // bases have the same number of DOF on the interface.
        bndOther= _other->boundary( bi.second().side() );
        GISMO_ASSERT( bndThis.rows() == bndOther.rows(),
                      "Input error, sizes do not match: "
                      <<bndThis.rows()<<"!="<<bndOther.rows() );
        // bndOther gets overwritten completely, so here is the setZero():
        bndOther.setZero();

        for( index_t i=0; i < bndThis.rows(); i++)
        {
            // get the level of the basis function on side first()
            unsigned L = this->levelOf( bndThis(i,0) );
            // get the flat tensor index
            // (i.e., the single-number-index on level L)...
            unsigned flat0 = this->flatTensorIndexOf( bndThis(i,0) );
            // ... and change it to the tensor-index.
            tens0 = this->tensorLevel(L).tensorIndex( flat0 );

            // ...flat1 the corresponding flat index
            // (single-number on level)...
            unsigned flat1 = 0;
            // ...and cont1 the corresponding continued (global) index.
            unsigned cont1 = 0;

            // get the sizes of the components of the tensor-basis on this level,
            // i.e., the sizes of the univariate bases corresponding
            // to the respective coordinate directions
            for( unsigned j=0; j < d; j++)
                N[j] = _other->tensorLevel(L).component(j).size();

            // get the tensor-index of the basis function on level L on
            // second() that should be matched with flatp/tens0
            for( unsigned j=0; j<d; j++)
            {
                // coordinate direction j on first() gets
                // mapped to direction jj on second()
                int jj = dirMap[j];
                // store the respective component of the tensor-index
                tens1[jj] = tens0[j];

                if( jj == bi.second().direction() )
                {
                    // if jj is the direction() of the interface,
                    // however, we need either the first
                    // or last basis function
                    if( bi.second().parameter() ) // true = 1 = end
                        tens1[jj] = N[jj]-1;
                    else
                        tens1[jj] = 0;
                }
                else
                {
                    // otherwise, check if the orientation is
                    // preserved. If necessary, flip it.
                    if( !dirOrient[j] )
                        tens1[jj] = N[jj]-1 - tens1[jj];
                }

            }

            flat1 = _other->tensorLevel(L).index( tens1 );

            // compute the "continuous" index on second(), i.e., the index
            // in the numbering which is global over all levels.
            cont1 = _other->flatTensorIndexToHierachicalIndex( flat1, L );
            // this is the index that has to be matched with bndThis(i,0)
            bndOther( i, 0 ) = cont1;
        }
        return;
    }
    gsWarn<<"Cannot match with "<< other <<"\n";
}

//protected functions

// Construct the characteristic matrix of level \a level ; i.e., set
// all the matrix entries corresponding to active functions to one and
// the rest to zero.
template<unsigned d, class T>
void gsHTensorBasis<d,T>::set_activ1(int level)
{
    typedef typename gsKnotVector<T>::smart_iterator knotIter;

    //gsDebug<<" Setting level "<< level <<"\n";
    gsVector<unsigned,d> low, upp;

    CMatrix & cmat = m_xmatrix[level];
    
    // Clear previous entries
    cmat.clear();

    // If a level is to be checked which is larger than
    // the maximum inserted level, nothing need so to be done
    if ( level > static_cast<int>(m_tree.getMaxInsLevel() ) )
        return;


    gsVector<knotIter,d> starts, ends, curr;
    gsVector<unsigned,d> ind;
    ind[0] = 0; // for d==1: warning: may be used uninitialized in this function (snap-ci)

    for(unsigned i = 0; i != d; ++i)
    {
        // beginning of the iteration in i-th direction
        starts[i] = m_bases[level]->knots(i).sbegin() ;
        // end of the iteration in i-th direction
        ends  [i] = m_bases[level]->knots(i).send()-m_deg[i]-1;
    }

    curr = starts;// start iteration
    do
    {
        for(unsigned i = 0; i != d; ++i)
        {
            low[i]  = curr[i].uIndex();  // lower left corner of the support of the function
            upp[i]  = (curr[i]+m_deg[i]+1).uIndex(); // upper right corner of the support
            ind[i]  = curr[i].index(); // index of the function in the matrix
        }

        if ( m_tree.query3(low, upp,level) == level) //if active
            cmat.push_unsorted( m_bases[level]->index( ind ) );

    }
    while (  nextLexicographic(curr,starts,ends) ); // while there are some functions (i.e., some combinations of iterators) left

    cmat.sort();

}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::functionOverlap(const point & boxLow, const point & boxUpp, 
                                          const int level, point & actLow, point & actUpp)
{
    const tensorBasis & tb = *m_bases[level];
    for(unsigned i = 0; i != d; ++i)
    {
        actLow[i] = tb.knots(i).lastKnotIndex (boxLow[i]) - m_deg[i];
        actUpp[i] = tb.knots(i).firstKnotIndex(boxUpp[i]) - 1       ;

        // Note aao:
        //actLow[i] = firstKnotIndex(boxLow[i]);
        //actUpp[i] = tb.knots(i).lastKnotIndex(boxUpp[i])-m_deg[i]-1;
    }
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::setActive()
{
    // for(std::size_t lvl = 0; lvl != m_xmatrix.size(); lvl++)
    //     set_activ1(lvl);
    // return;

    // iterate over leaf-boxes
    //   for all overlapping supports with the box
    //     set obvious to active
    //     for the rest candidates (supp. not fully contained in box ~ !query2)
    //     (equiv: actives on the boundary cells of the box)
    //       query3(supp,box.level) == level (min. is level: no coarser)
    // take care: duplicates from different leaves or adj. cells
    point curr, actUpp;
    gsMatrix<unsigned,d,2> elSupp;

    // try: iteration per level
    for ( typename hdomain_type::literator it = m_tree.beginLeafIterator(); 
          it.good(); it.next() )
    {
        const int lvl = it.level();
        CMatrix & cmat = m_xmatrix[lvl];

        // Get candidate functions
        functionOverlap(it.lowerCorner(), it.upperCorner(), lvl, curr, actUpp);

        do 
        {
            const unsigned gi = m_bases[lvl]->index( curr );
            
            // Get element support
            m_bases[lvl]->elementSupport_into(gi, elSupp);

            if ( (elSupp.col(0).array() >= it.lowerCorner().array()).all() &&
                 (elSupp.col(1).array() <= it.upperCorner().array()).all() )
            {
                // to do: all-at-once
                cmat.push_unsorted( gi );
            }
            else
            {
                // Check if active (iff no overlap with level less than lvl)
                if ( m_tree.query3(elSupp.col(0), elSupp.col(1), lvl) == lvl)
                    cmat.push_unsorted( gi );
            }
        }
        while( nextCubePoint(curr, actUpp) );
    }

    for(std::size_t lvl = 0; lvl != m_xmatrix.size(); ++lvl)
    {
        m_xmatrix[lvl].sort();
        m_xmatrix[lvl].erase( std::unique( m_xmatrix[lvl].begin(), m_xmatrix[lvl].end() ),
                              m_xmatrix[lvl].end() );
    }
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::setActiveToLvl(int level, 
                                         std::vector<CMatrix> & x_matrix_lvl) const
{
    x_matrix_lvl.resize(level+1);

    // vectors of iterators. one iterator for each dimension
    gsVector<typename gsKnotVector<T>::smart_iterator,d> starts, ends, curr;
    gsVector<unsigned,d> ind;
    ind[0] = 0; // for d==1: warning: may be used uninitialized in this function (snap-ci)
    
    gsVector<unsigned,d> low, upp;
    for(int j =0; j < level+1; j++)
    {
        // Clear previous entries
        x_matrix_lvl[j].clear();

        for(unsigned i = 0; i != d; ++i)
        {
            // beginning of the iteration in i-th direction
            starts[i] = m_bases[j]->knots(i).sbegin() ;
            // end of the iteration in i-th direction
            ends  [i] = m_bases[j]->knots(i).send()-m_deg[i]-1;
        }

        curr = starts; // set start of iteration
        do
        {
            for(unsigned i = 0; i != d; ++i)
            {
                low[i]  = curr[i].uIndex(); // lower left corner of the support of the function 
                upp[i]  = (curr[i]+m_deg[i]+1).uIndex(); // upper right corner of the support
                ind[i]  = curr[i].index(); // index of the function in the matrix 
            }
            if(j < level)
            {
                if ( m_tree.query3(low, upp,j) == j)
                { //if active
                    x_matrix_lvl[j].push_unsorted( m_bases[j]->index( ind ) );
                }
            }else{
                if ( m_tree.query3(low, upp,j) >= j)
                { //if active
                    x_matrix_lvl[j].push_unsorted( m_bases[j]->index( ind ) );
                }
            }
        }
        while ( nextLexicographic(curr,starts,ends) ); // while there are some functions (i.e., some combinations of iterators) left

        x_matrix_lvl[j].sort();
    }

}

///private functions
template<unsigned d, class T> inline
void gsHTensorBasis<d,T>::insert_box(gsVector<unsigned,d> const & k1,
                                     gsVector<unsigned,d> const & k2,
                                     int lvl )
{
    // Remember box in History (for debugging)
    // m_boxHistory.push_back( box(k1,k2,lvl) );

    m_tree.insertBox(k1,k2, lvl);
    needLevel( m_tree.getMaxInsLevel() );
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::makeCompressed()
{
    // Compress the tree
    // m_tree.makeCompressed();

    while ( ! m_xmatrix_offset[1] )
    {
        delete m_bases.front();
        m_bases.erase( m_bases.begin() );
        m_tree.decrementLevel();
        m_xmatrix.erase( m_xmatrix.begin() );
        m_xmatrix_offset.erase( m_xmatrix_offset.begin() );
    }
    // Note/to do: cleaning up empty levels at the end as well.
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::flatTensorIndexesToHierachicalIndexes(gsSortedVector< int > & indexes,const int level) const
{
    GISMO_ASSERT( static_cast<size_t>(level) < m_xmatrix.size(), "Requested level does not exist.\n");

    CMatrix::const_iterator xmat_pointer = m_xmatrix[level].begin();
    CMatrix::const_iterator xmat_end = m_xmatrix[level].end();
    gsSortedVector< int >::iterator ind_pointer = indexes.begin();
    gsSortedVector< int >::iterator ind_end = indexes.end();
    unsigned index = 0;
    while(ind_pointer!=ind_end&&xmat_pointer!=xmat_end)
    {
        if(*ind_pointer<static_cast<int>(*xmat_pointer))
        {
            (*ind_pointer)=-1;
            ++ind_pointer;
        }
        else if(*ind_pointer==static_cast<int>(*xmat_pointer))
        {
            (*ind_pointer)=m_xmatrix_offset[level]+index;
            ++xmat_pointer;
            ++index;
            ++ind_pointer;
        }
        else
        {
            ++xmat_pointer;
            ++index;
        }
    }
    while(ind_pointer!=ind_end)
    {
        (*ind_pointer)=-1;
        ++ind_pointer;
    }
}

template<unsigned d, class T>
int gsHTensorBasis<d,T>::flatTensorIndexToHierachicalIndex(unsigned index,const int level) const
{
    if( m_xmatrix.size()<=static_cast<unsigned>(level) )
        return -1;
    CMatrix::const_iterator it = std::lower_bound(m_xmatrix[level].begin(), m_xmatrix[level].end(), index );
    if(it == m_xmatrix[level].end() || *it != index)
        return -1;
    else
        return m_xmatrix_offset[level] + ( it - m_xmatrix[level].begin() );
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::activeBoundaryFunctionsOfLevel(const unsigned level,const boxSide & s,std::vector<bool>& actives) const
{
    needLevel( level );

    const gsMatrix<unsigned> bound = m_bases[level]->boundary(s);
    const index_t sz = bound.rows();
    //gsSortedVector< int > indexes(bound->data(),bound->data()+sz);
    gsSortedVector< int > indexes;
    indexes.resize(sz,-1);
    if(level<=maxLevel())
    {
        for(index_t i = 0;i<sz;++i)
            indexes[i]=(bound)(i,0);
        flatTensorIndexesToHierachicalIndexes(indexes,level);
    }
    actives.resize(indexes.size(),false);
    std::fill (actives.begin(),actives.end(),false);
    for(unsigned i = 0;i<indexes.size();i++)
        if(indexes[i]!=-1)
            actives[i]=true;
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::update_structure() // to do: rename as updateHook
{
    // Make sure we have computed enough levels
    needLevel( m_tree.getMaxInsLevel() );

    // Setup the characteristic matrices
    m_xmatrix.clear();
    m_xmatrix.resize( m_bases.size() );

    // Compress the tree
    m_tree.makeCompressed();

    for(std::size_t i = 0; i != m_xmatrix.size(); i ++)
        set_activ1(i);

    // Store all indices of active basis functions to m_matrix
    //setActive();

    // Compute offsets
    m_xmatrix_offset.clear();
    m_xmatrix_offset.reserve(m_xmatrix.size()+1);
    m_xmatrix_offset.push_back(0);
    for (std::size_t i = 0; i != m_xmatrix.size(); i++)
    {
        m_xmatrix_offset.push_back(
            m_xmatrix_offset.back() + m_xmatrix[i].size() );
    }
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::needLevel(int maxLevel) const
{
    // +1 for the initial basis in m_bases
    const int extraLevels = maxLevel + 1 - m_bases.size();

    for ( int i = 0; i < extraLevels; ++i )
    {
        tensorBasis * next_basis = m_bases.back()->clone().release();
        next_basis->uniformRefine(1);
        m_bases.push_back (next_basis); //note: m_bases is mutable
    }
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::initialize_class(gsBasis<T> const&  tbasis)
{
    // Degrees
    //m_deg = tbasis.cwiseDegree();
    m_deg.resize(d);
    for( unsigned i = 0; i < d; i++)
        m_deg[i] = tbasis.degree(i);

    // Construct the initial basis
    if ( const tensorBasis * tb2 =
              dynamic_cast<const tensorBasis*>(&tbasis) )
    {
        m_bases.push_back(tb2->clone().release());
    }
    else
    {
        GISMO_ERROR("Cannot construct a Hierarchical basis from "<< tbasis );
    }

    // Initialize the binary tree
    point upp;
    for ( unsigned i = 0; i!=d; ++i )
        upp[i] = m_bases[0]->knots(i).uSize()-1;

    m_tree.init(upp);

    // Produce a couple of tensor-product spaces by dyadic refinement
    m_bases.reserve(3);
    for(unsigned int i = 1; i <= 2; i++)
    {
        tensorBasis* next_basis = m_bases[i-1]->clone().release();
        next_basis->uniformRefine(1);
        m_bases.push_back( next_basis );
    }

}


template<unsigned d, class T>
void gsHTensorBasis<d,T>::active_into(const gsMatrix<T> & u, gsMatrix<unsigned>& result) const
{
    point low, upp, cur;
    const int maxLevel = m_tree.getMaxInsLevel();

    std::vector<std::vector<unsigned> > temp_output;//collects the outputs
    temp_output.resize( u.cols() );
    std::size_t sz = 0;

    //gsMatrix<unsigned> activesLvl;

    for(index_t p = 0; p < u.cols(); p++) //for all input points
    {
        const gsMatrix<T> & currPoint = u.col(p);
        for(unsigned i = 0; i != d; ++i)
            low[i] = m_bases[maxLevel]->knots(i).uFind( currPoint(i,0) ).uIndex();

        // Identify the level of the point
        const int lvl = m_tree.levelOf(low, maxLevel);

        for(int i = 0; i <= lvl; i++)
        {
            /*
              m_bases[i]->active_into(currPoint, activesLvl);
              
              std::set_intersection(m_xmatrix[i].begin(), m_xmatrix[i].end(),
              activesLvl.data(), activesLvl.data() + activesLvl.size(),
              std::back_inserter( temp_output[p] ) );
              +++ Renumbering to H-basis indexing
            // */                                 

            // /*
            m_bases[i]->active_cwise(currPoint, low, upp);
            cur = low;
            do
            {
                CMatrix::const_iterator it =
                    m_xmatrix[i].find_it_or_fail( m_bases[i]->index(cur) );

                if( it != m_xmatrix[i].end() )// if index is found
                {
                    temp_output[p].push_back(
                        this->m_xmatrix_offset[i] + (it - m_xmatrix[i].begin() )
                        );
                }
            }
            while( nextCubePoint(cur,low,upp) );
            //*/
        }

        // update result size
        if ( temp_output[p].size() > sz )
            sz = temp_output[p].size();
    }

    result.resize(sz, u.cols() );
    for(index_t i = 0; i < result.cols(); i++)
    {
        result.col(i).topRows(temp_output[i].size())
            = gsAsConstVector<unsigned>(temp_output[i]);
        result.col(i).bottomRows(sz-temp_output[i].size()).setZero();
    }
}

template<unsigned d, class T>
gsMatrix<unsigned>  gsHTensorBasis<d,T>::allBoundary( ) const
{
    std::vector<unsigned> temp;
    gsVector<unsigned,d>  ind;
    for(unsigned i = 0; i <= this->maxLevel(); i++)
        for (CMatrix::const_iterator it = m_xmatrix[i].begin();
             it != m_xmatrix[i].end(); it++)
        {
            ind = this->m_bases[i]->tensorIndex(*it);
            for (unsigned j=0; j!=d; ++j )
                if ( (ind[j]==0) || (ind[j]==unsigned(this->m_bases[i]->size(j)-1)) )
                {
                    temp.push_back(m_xmatrix_offset[i] + (it-m_xmatrix[i].begin()) );
                    break;
                }
        }
    return makeMatrix<unsigned>(temp.begin(),temp.size(),1 );
}

template<unsigned d, class T>
gsMatrix<unsigned>  gsHTensorBasis<d,T>::
boundaryOffset(boxSide const & s,unsigned offset) const
{ 
    //get information on the side
    int k   = s.direction();
    int par = s.parameter();
    
    std::vector<unsigned> temp;
    gsVector<unsigned,d>  ind;
    // i goes through all levels of the hierarchical basis
    for(unsigned i = 0; i <= this->maxLevel(); i++)
    {
        GISMO_ASSERT(static_cast<int>(offset)<this->m_bases[i]->size(k),
                     "Offset cannot be bigger than the amount of basis" 
                     "functions orthogonal to Boxside s!");

        unsigned r = ( par ? this->m_bases[i]->size(k) - 1 -offset : offset);
        for (CMatrix::const_iterator it = m_xmatrix[i].begin();
             it != m_xmatrix[i].end(); it++)
        {
            ind = this->m_bases[i]->tensorIndex(*it);
            if ( ind[k]==r )
                temp.push_back(
                    m_xmatrix_offset[i] +  (it - m_xmatrix[i].begin() )
                    );
        }
    }
    return makeMatrix<unsigned>(temp.begin(),temp.size(),1 );
}

/*
template<unsigned d, class T>
void gsHTensorBasis<d,T>::evalAllDers_into(const gsMatrix<T> & u, int n, 
                                           std::vector<gsMatrix<T> >& result) const;
{
    result.resize(n+1);

}
*/

template<unsigned d, class T>
void gsHTensorBasis<d,T>::uniformRefine(int numKnots, int mul)
{
    GISMO_ASSERT(numKnots == 1, "Only implemented for numKnots = 1");

    GISMO_ASSERT( m_tree.getMaxInsLevel() < static_cast<unsigned>(m_bases.size()),
                  "Problem with max inserted levels: "<< m_tree.getMaxInsLevel()
                  <<"<" << m_bases.size() <<"\n");

    // Delete the first level
    delete m_bases.front();
    m_bases.erase( m_bases.begin() );

    // Keep consistency of finest level
    tensorBasis * last_basis = m_bases.back()->clone().release();
    last_basis->uniformRefine(1,mul);
    m_bases.push_back( last_basis );

    // Lift all indices in the tree by one level
    m_tree.multiplyByTwo();

    update_structure();
}

template<unsigned d, class T>
std::vector< std::vector< std::vector< unsigned int > > > gsHTensorBasis<d,T>::domainBoundariesParams( std::vector< std::vector< std::vector< std::vector< T > > > >& result) const
{
    std::vector< std::vector< std::vector< std::vector< unsigned int > > > > dummy;
    return domainBoundariesGeneric( dummy, result, false );
}

template<unsigned d, class T>
std::vector< std::vector< std::vector< unsigned int > > > gsHTensorBasis<d,T>::domainBoundariesIndices( std::vector< std::vector< std::vector< std::vector< unsigned int > > > >& result ) const
{
    std::vector< std::vector< std::vector< std::vector< T > > > > dummy;
    return domainBoundariesGeneric( result, dummy, true );
}


template<unsigned d, class T>
std::vector< std::vector< std::vector< unsigned int > > > gsHTensorBasis<d,T>::domainBoundariesGeneric(std::vector< std::vector< std::vector< std::vector< unsigned int > > > >& indices,
                                                                                                       std::vector< std::vector< std::vector< std::vector< T > > > >& params,
                                                                                                       bool indicesFlag ) const
{
    indices.clear();
    params.clear();
    std::vector< std::vector< std::vector< int > > > res_aabb;
    std::vector< std::vector< std::vector< unsigned int > > > res_aabb_unsigned;
    std::vector< std::vector< std::vector< std::vector< unsigned int > > > > polylines;

    polylines =  this->m_tree.getPolylines();
    res_aabb.resize( polylines.size() );
    // We cannot simply say
    // result = this->m_tree.getPolylines();
    // because the return value of getPolylines() are vectors of ints and we have no implicit conversion of int to T.

    // We want indices/params to be of the same size as polylines. We achieve this here and in the for cycles.
    if( indicesFlag )
        indices.resize( polylines.size() );
    
    else
        params.resize(polylines.size());

    int maxLevel = static_cast<int>( this->maxLevel() );
    // We precompute the parameter values corresponding to indices of m_maxInsLevel
    // although we don't need them if indicesFlag == true.
    std::vector<T> x_dir(m_bases[maxLevel]->knots(0).unique());
    std::vector<T> y_dir(m_bases[maxLevel]->knots(1).unique());
    
    for(unsigned int i0 = 0; i0 < polylines.size(); i0++)
    {
        if( indicesFlag )
            indices[i0].resize( polylines[i0].size() );
        else
            params[i0].resize( polylines[i0].size() );

        res_aabb[i0].resize( polylines[i0].size() );
        for(unsigned int i1 = 0; i1 < polylines[i0].size(); i1++)
        {
            if( indicesFlag )
                indices[i0][i1].resize( polylines[i0][i1].size() );
            else
                params[i0][i1].resize( polylines[i0][i1].size() );

            res_aabb[i0][i1].resize( 4 );
            res_aabb[i0][i1][0] = 1000000;
            res_aabb[i0][i1][1] = 1000000;
            res_aabb[i0][i1][2] = -10000000;
            res_aabb[i0][i1][3] = -10000000;
            for(unsigned int i2 = 0; i2 < polylines[i0][i1].size(); i2++)
            {
                if( indicesFlag )
                {
                    indices[i0][i1][i2].resize(4);
                    indices[i0][i1][i2][0] = polylines[i0][i1][i2][0];
                    indices[i0][i1][i2][1] = polylines[i0][i1][i2][1];
                    indices[i0][i1][i2][2] = polylines[i0][i1][i2][2];
                    indices[i0][i1][i2][3] = polylines[i0][i1][i2][3];

                }
                else
                {
                    params[i0][i1][i2].resize(4);
                    // We could as well successively push_back() them but this should be slightly more efficient.
                    params[i0][i1][i2][0] = (x_dir[polylines[i0][i1][i2][0]]);
                    params[i0][i1][i2][1] = (y_dir[polylines[i0][i1][i2][1]]);
                    params[i0][i1][i2][2] = (x_dir[polylines[i0][i1][i2][2]]);
                    params[i0][i1][i2][3] = (y_dir[polylines[i0][i1][i2][3]]);
                }
                if(res_aabb[i0][i1][0]>signed(polylines[i0][i1][i2][0]))
                {
                    res_aabb[i0][i1][0] = polylines[i0][i1][i2][0];
                }
                if(res_aabb[i0][i1][1]>signed(polylines[i0][i1][i2][1]))
                {
                    res_aabb[i0][i1][1] = polylines[i0][i1][i2][1];
                }
                if(res_aabb[i0][i1][2]<signed(polylines[i0][i1][i2][2]))
                {
                    res_aabb[i0][i1][2] = polylines[i0][i1][i2][2];
                }
                if(res_aabb[i0][i1][3]<signed(polylines[i0][i1][i2][3]))
                {
                    res_aabb[i0][i1][3] = polylines[i0][i1][i2][3];
                }
            }
        }
    }

    res_aabb_unsigned.resize(res_aabb.size());
    for (unsigned int i = 0; i < res_aabb.size(); i++)
    {
        res_aabb_unsigned[i].resize(res_aabb[i].size());
        for (unsigned int j = 0; j < res_aabb[i].size(); j++)
        {
            res_aabb_unsigned[i][j].resize(res_aabb[i][j].size());
            for (unsigned int k = 0; k < res_aabb[i][j].size(); k++)
            {
                if(res_aabb[i][j][k]<0)
                    gsWarn << "conversion form signed to unsigned\n";
                res_aabb_unsigned[i][j][k] = res_aabb[i][j][k];
            }
        }
    }
    return res_aabb_unsigned;
}


template<unsigned d, class T>
void  gsHTensorBasis<d,T>::transfer(const std::vector<gsSortedVector<unsigned> >& old, gsSparseMatrix<T>& result)
{
    // Note: implementation assumes number of old + 1 m_bases exists in this basis
    needLevel( old.size() );

    tensorBasis T_0_copy = this->tensorLevel(0);

    std::vector< gsSparseMatrix<T,RowMajor> > transfer(m_bases.size()-1);
    std::vector<std::vector<T> > knots(d);

    for(std::size_t i = 1; i < m_bases.size(); ++i)
    {
        //T_0_copy.uniformRefine_withTransfer(transfer[i-1], 1);
        for(unsigned dim = 0; dim != d; ++dim)
        {
            const gsKnotVector<T> & ckv = m_bases[i-1]->knots(dim);
            const gsKnotVector<T> & fkv = m_bases[i  ]->knots(dim);
            ckv.symDifference(fkv, knots[dim]);
            // equivalent (dyadic ref.):
            // ckv.getUniformRefinementKnots(1, knots[dim]);

            //gsDebug << "level: " << i << "\n"
            //        << "direction: " << dim << "\n";
            //gsDebugVar(gsAsMatrix<T>(dirKnots));
        }

        T_0_copy.refine_withTransfer(transfer[i-1], knots);
    }

    // Add missing empty char. matrices
    while ( old.size() >= m_xmatrix.size() )
        m_xmatrix.push_back( gsSortedVector<unsigned>() );

    result = this->coarsening_direct(old, m_xmatrix, transfer);

    // This function automatically adds additional characteristic matrices,
    // even if they are not needed.
    // Check whether the characteristic matrices corresponding to the finest
    // levels are actually used. If they are empty, i.e., if there are no
    // active functions on that level, drop them...
    while( m_xmatrix.back().size() == 0 )
        m_xmatrix.pop_back();

    // ...similarly, erase all those fine bases which are actually not used.
    const int sizeDiff = static_cast<int>( m_bases.size() - m_xmatrix.size() );
    if( sizeDiff > 0 )
    {
        freeAll(m_bases.end() - sizeDiff, m_bases.end());
        m_bases.resize(m_xmatrix.size());
    }
}

template<unsigned d, class T>
void  gsHTensorBasis<d,T>::transfer2(const std::vector<gsSortedVector<unsigned> >& old, gsSparseMatrix<T>& result)
{
    // Note: implementation assumes number of old + 1 m_bases exists in this basis
    needLevel( old.size() );

    tensorBasis T_0_copy = this->tensorLevel(0);
    std::vector< gsSparseMatrix<T,RowMajor> > transfer( m_bases.size()-1 );
    std::vector<std::vector<T> > knots(d);
    
    for(std::size_t i = 1; i < m_bases.size(); ++i)
    {
        //T_0_copy.uniformRefine_withTransfer(transfer[i], 1);
        for(unsigned int dim = 0; dim != d; ++dim)
        {
            const gsKnotVector<T> & ckv = m_bases[i-1]->knots(dim);
            const gsKnotVector<T> & fkv = m_bases[i  ]->knots(dim);
            ckv.symDifference(fkv, knots[dim]);
            // equivalent (dyadic ref.):
            // ckv.getUniformRefinementKnots(1, knots[dim]);

            //gsDebug << "level: " << i << "\n"
            //        << "direction: " << dim << "\n";
            //gsDebugVar(gsAsMatrix<T>(dirKnots));
        }
        T_0_copy.refine_withTransfer(transfer[i-1], knots);
    }

    // Add missing empty char. matrices
    while ( old.size() >= m_xmatrix.size())
        m_xmatrix.push_back( gsSortedVector<unsigned>() );

    result = this->coarsening_direct2(old, m_xmatrix, transfer);
}


template<unsigned d, class T>
void gsHTensorBasis<d,T>::increaseMultiplicity(index_t lvl, int dir, T knotValue, int mult)
{
    GISMO_ASSERT( static_cast<size_t>(lvl) < m_xmatrix.size(),
                  "Requested level does not exist.\n");
        
    if (m_bases[lvl]->knots(dir).has(knotValue))
    {
        for(unsigned int i =lvl;i < m_bases.size();i++)
            m_bases[i]->component(dir).insertKnot(knotValue,mult);
    }
    else
    {
        gsWarn<<"Knot value not in the given knot vector."<<std::endl;
    }

    update_structure();
}


template<unsigned d, class T>
void gsHTensorBasis<d,T>::increaseMultiplicity(index_t lvl, int dir, const std::vector<T> & knotValue, int mult)
{
    for(unsigned int k =0; k < knotValue.size(); ++k)
    {
        if (m_bases[lvl]->knots(dir).has(knotValue[k]))
        {
            for(unsigned int i =lvl;i < m_bases.size(); ++i)
                m_bases[i]->component(dir).insertKnot(knotValue[k],mult);
        }
        else
        {
            gsWarn<<"Knot value not in the given knot vector."<<std::endl;
        }
    }
    update_structure();
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::getBoxesAlongSlice( int dir, T par,std::vector<unsigned>& boxes ) const
{
    gsMatrix<unsigned> b1,b2;
    gsVector<unsigned> level;
    m_tree.getBoxesInLevelIndex(b1,b2,level);
    gsVector<unsigned> min,max;
    for(int i = 0;i<level.rows();i++)
    {
        min = b1.row(i);
        max = b2.row(i);
        const unsigned l = level(i);
        const unsigned par_index = m_bases[l]->knots(dir).uFind(par).uIndex();
        if(l>0 && (par_index>=min(dir)) && (par_index<=max(dir)))
        {
            boxes.push_back(l);
            for(int j=0;j<min.rows();++j)
                if(j!=dir)
                    boxes.push_back(min(j));
            for(int j=0;j<max.rows();++j)
                if(j!=dir)
                    boxes.push_back(max(j));
        }
    }
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::degreeElevate(int const & i, int const dir)
{
    for (size_t level=0;level<m_bases.size();++level)
        m_bases[level]->degreeElevate(i,dir);

    for(unsigned c=0; c<d; ++c)
        m_deg[c]=m_bases[0]->degree(c);

    this->update_structure();
}

template<unsigned d, class T>
void gsHTensorBasis<d,T>::degreeIncrease(int const & i, int const dir)
{
    for (size_t level=0;level<m_bases.size();++level)
        m_bases[level]->degreeIncrease(i,dir);

    for(unsigned c=0; c<d; ++c)
        m_deg[c]=m_bases[0]->degree(c);

    this->update_structure();
}


namespace internal
{

/// Get a HTensorBasis from XML data
template<unsigned d, class T>
class gsXml< gsHTensorBasis<d,T> >
{
private:
    gsXml() { }
public:
    GSXML_COMMON_FUNCTIONS(gsHTensorBasis<TMPLA2(d,T)>);
    static std::string tag () { return "Basis"; }
    static std::string type () { return ""; } // tag ?

    static gsHTensorBasis<d,T> * get (gsXmlNode * node)
    {
        gsXmlAttribute * btype = node->first_attribute("type");
        if ( ! btype )
        {
            gsWarn<< "Basis without a type in the xml file.\n";
            return NULL;
        }
        std::string s = btype->value() ;
        if ( s.compare(0, 9, "HBSplineB" , 9 ) == 0 ) // needs correct d as well
            return gsXml< gsHBSplineBasis<d,T> >::get(node);
        if ( s.compare(0, 10,"THBSplineB", 10) == 0 )
            return gsXml< gsTHBSplineBasis<d,T> >::get(node);

        gsWarn<<"gsXmlUtils: gsHTensorBasis: No known basis \""<<s<<"\". Error.\n";
        return NULL;
    }
    
    static gsXmlNode * put (const gsHTensorBasis<d,T> & obj,
                            gsXmlTree & data )
    {
        const gsBasis<T> * ptr = & obj;

        // Hier. B-splines
        if ( const gsHBSplineBasis<d,T>  * g =
             dynamic_cast<const gsHBSplineBasis<d,T> *>( ptr ) )
            return gsXml< gsHBSplineBasis<d,T> >::put(*g,data);
        
        // Truncated hier. B-splines
        if ( const gsTHBSplineBasis<d,T>  * g =
             dynamic_cast<const gsTHBSplineBasis<d,T> *>( ptr ) )
            return gsXml< gsTHBSplineBasis<d,T> >::put(*g,data);
        
        gsWarn<<"gsXmlUtils put: getBasis: No known basis \""<<obj<<"\". Error.\n";
        return NULL;
    }
};

} // namespace internal


} // namespace gismo
