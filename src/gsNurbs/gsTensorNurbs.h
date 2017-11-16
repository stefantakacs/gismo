/** @file gsTensorNurbs.h

    @brief Represents a tensor-product NURBS patch

    This file is part of the G+Smo library. 

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    
    Author(s): A. Mantzaflaris
*/
 
#pragma once

#include <gsCore/gsGeometry.h>
#include <gsNurbs/gsTensorNurbsBasis.h>

#include <gsTensor/gsTensorTools.h> // todo: move to hpp

namespace gismo
{

/** \brief 
    A tensor product Non-Uniform Rational B-spline function
    (NURBS) of parametric dimension \em d, with arbitrary target
    dimension.

    This is the geometry type associated with gsTensorNurbsBasis.
    
    \tparam d the parametric dimension of the tensor product
    \tparam T coefficient type
    \tparam KnotVectorType the type of knot vector the NURBS bases use

    \ingroup geometry
    \ingroup Nurbs
*/

template<unsigned d, class T>
class gsTensorNurbs : public gsGeoTraits<d,T>::GeometryBase
{

public: 
    typedef gsKnotVector<T> KnotVectorType;

    typedef typename gsGeoTraits<d,T>::GeometryBase Base;

    typedef T Scalar_t;
    
    typedef gsTensorBSplineBasis<d,T> TBasis;      // underlying tensor basis
    
    /// Family type
    typedef gsBSplineBasis<T>  Family_t;
    
    // rational version of tensor basis (basis for this geometry)
    typedef gsTensorNurbsBasis<d,T>   Basis;
    
    /// Shared pointer for gsTensorNurbs
    typedef memory::shared_ptr< gsTensorNurbs > Ptr;

    /// Unique pointer for gsTensorNurbs
    typedef memory::unique_ptr< gsTensorNurbs > uPtr;

public:

    /// Default empty constructor
    gsTensorNurbs() : Base() { }

    gsTensorNurbs( const Basis & basis,  gsMatrix<T> coefs ) :
    Base( basis, give(coefs) ) { }

    /// Construct 2D tensor NURBS by knot vectors, degrees and coefficient matrix
    /// All weights are set to be 1.
    gsTensorNurbs( gsKnotVector<T> const& KV1, gsKnotVector<T> const & KV2,
                   gsMatrix<T> tcoefs)
    {
        GISMO_ASSERT(d==2, "Wrong dimension: tried to make a "<< d
                     <<"D NURBS using 2 knot-vectors.");

        gsBSplineBasis<T>    * Bu    = new gsBSplineBasis<T>(KV1);
        gsBSplineBasis<T>    * Bv    = new gsBSplineBasis<T>(KV2);

        TBasis   *tbasis = new TBasis(Bu,Bv) ;//d==2
      
        this->m_basis = new Basis(tbasis) ;
        this->m_coefs.swap(tcoefs);
        GISMO_ASSERT(tbasis->size()== m_coefs.rows(), 
                     "Coefficient matrix for the NURBS does not have "
                     "the expected number of control points (rows)." );
    }

    /// Construct 2D tensor NURBS by knot vectors, degrees, weights and coefficient matrix
    gsTensorNurbs( gsKnotVector<T> const& KV1, gsKnotVector<T> const & KV2,
                   gsMatrix<T> tcoefs, gsMatrix<T> wgts)
    {
        GISMO_ASSERT(d==2, "Wrong dimension: tried to make a "<< d
                     <<"D NURBS using 2 knot-vectors.");

        gsBSplineBasis<T>    * Bu    = new gsBSplineBasis<T>(KV1);
        gsBSplineBasis<T>    * Bv    = new gsBSplineBasis<T>(KV2);

        TBasis   *tbasis = new TBasis(Bu,Bv) ;//d==2
      
        GISMO_ASSERT(tbasis->size()== tcoefs.rows(), 
                     "Coefficient matrix for the NURBS does not have "
                     "the expected number of control points (rows)." );

        this->m_basis = new Basis(tbasis , give(wgts)) ;
        this->m_coefs.swap(tcoefs);
    }

    /// Construct 3D tensor NURBS by knot vectors, degrees and coefficient matrix
    /// \a tcoefs, \a wgts become empty after the constructor is called
    gsTensorNurbs( gsKnotVector<T> const & KV1, 
                   gsKnotVector<T> const & KV2, 
                   gsKnotVector<T> const & KV3,
                   gsMatrix<T>  tcoefs, 
                   gsMatrix<T> wgts )
    {
        GISMO_ASSERT(d==3, "Wrong dimension: tried to make a "<< d
                     <<"D NURBS using 3 knot-vectors.");
      
        gsBSplineBasis<T> * Bu= new gsBSplineBasis<T>(KV1);
        gsBSplineBasis<T> * Bv= new gsBSplineBasis<T>(KV2);
        gsBSplineBasis<T> * Bw= new gsBSplineBasis<T>(KV3);
        TBasis *tbasis = new TBasis(Bu,Bv,Bw) ;//d==3
      
        GISMO_ASSERT(tbasis->size()== tcoefs.rows(), 
                     "Coefficient matrix for the NURBS does not have "
                     "the expected number of control points (rows)." );

        this->m_basis = new Basis(tbasis, give(wgts));
        this->m_coefs.swap(tcoefs);
    }

    /// Construct 3D tensor NURBS by knot vectors, degrees and coefficient matrix
    /// All weights are set to be 1.
    gsTensorNurbs( gsKnotVector<T> const & KV1, 
                   gsKnotVector<T> const & KV2, 
                   gsKnotVector<T> const & KV3,
                   gsMatrix<T> tcoefs)
    {
        GISMO_ASSERT(d==3, "Wrong dimension: tried to make a "<< d
                     <<"D NURBS using 3 knot-vectors.");
      
        gsBSplineBasis<T> * Bu= new gsBSplineBasis<T>(KV1);
        gsBSplineBasis<T> * Bv= new gsBSplineBasis<T>(KV2);
        gsBSplineBasis<T> * Bw= new gsBSplineBasis<T>(KV3);
        TBasis *tbasis = new TBasis(Bu,Bv,Bw) ;//d==3
      
        GISMO_ASSERT(tbasis->size()== tcoefs.rows(), 
                     "Coefficient matrix for the NURBS does not have "
                     "the expected number of control points (rows)." );

        this->m_basis = new Basis(tbasis) ;
        this->m_coefs.swap(tcoefs);
    }

    GISMO_BASIS_ACCESSORS
    
    public:

// ***********************************************
// Virtual member functions required by the base class
// ***********************************************

    GISMO_CLONE_FUNCTION(gsTensorNurbs, virtual)

    /// Prints the object as a string.
    std::ostream &print(std::ostream &os) const
    { os << "Tensor-NURBS geometry "<< "R^"<< this->parDim() << 
            " --> R^"<< this->geoDim()<< ", #control pnts= "<< this->coefsSize() <<": "
         << this->coef(0) <<" ... "<< this->coef(this->coefsSize()-1); 
        os << "\nweights: "
           << this->basis().weights().at(0) <<" ... "
           << this->basis().weights().at(this->coefsSize()-1)
           <<"\n" ;
        return os; }

// ***********************************************
// Additional members for tensor NURBS
// ***********************************************

    /// Returns a reference to the knot vector \a i
    const KnotVectorType & knots(const int i) const 
    { return this->basis().source().knots(i); } 

    KnotVectorType & knots(const int i) 
    { return this->basis().source().knots(i); } 

    /// Inserts knot \a knot at direction \a dir, \a i times
    void insertKnot( T knot, int dir, int i = 1)
    {
        GISMO_ASSERT( i>0, "multiplicity must be at least 1");
        GISMO_ASSERT( dir >= 0 && static_cast<unsigned>(dir) < d,
                      "Invalid basis component "<< dir <<" requested for degree elevation" );
        
        const index_t n = this->m_coefs.cols();
        gsTensorBSplineBasis<d,T> & tbs = this->basis().source();

        gsVector<index_t,d> sz;
        tbs.size_cwise(sz);
        
        swapTensorDirection(0, dir, sz, m_coefs  );
        std::swap(sz[0],sz[dir]);
        swapTensorDirection(0, dir, sz, weights());
        const index_t nc = sz.template tail<d-1>().prod();
        m_coefs  .resize( sz[0], n * nc );
        weights().resize( sz[0], nc     );
        
        gsBoehm(tbs.knots(dir), weights(), knot, i, false);
        gsBoehm(tbs.knots(dir), m_coefs  , knot, i, true );
        sz[0] = m_coefs.rows();

        const index_t ncoef = sz.prod();
        m_coefs  .resize(ncoef, n );
        weights().resize(ncoef, 1 );
        swapTensorDirection(0, dir, sz, m_coefs  );
        std::swap(sz[0],sz[dir]);
        swapTensorDirection(0, dir, sz, weights());
    }

    /// Access to i-th weight
    T & weight(int i) const { return this->basis().weight(i); }

    /// Returns the NURBS weights
    gsMatrix<T> & weights() const { return this->basis().weights(); }

    /// Returns the NURBS weights as non-const reference
    gsMatrix<T> & weights() { return this->basis().weights(); }
    
    /// Returns the degree of the basis wrt direction i 
    unsigned degree(unsigned i) const 
    { return this->basis().source().component(i).degree(); }

/// Toggle orientation wrt coordinate k
    void reverse(unsigned k)
    { 
        GISMO_ASSERT(d==2, "only 2D for now");

        gsVector<int> str(d); 
        gsVector<int> sz (d); 

        gsTensorBSplineBasis<d,T> & tbsbasis = this->basis().source();

        sz[0]  = tbsbasis.component(k ).size();
        sz[1]  = tbsbasis.component(!k).size();
        str[0] = tbsbasis.source().stride( k );
        str[1] = tbsbasis.source().stride(!k );
    
        gsMatrix<T> & w  = tbsbasis.weights();

        for  ( int i=0; i< sz[0]; i++ )
            for  ( int j=0; j< sz[1]/2; j++ )
            {
                this->m_coefs.row(i*str[0] + j*str[1] ).swap(
                    this->m_coefs.row(i*str[0] + (sz[1]-j-1)*str[1] )
                    );

                w.row(i*str[0] + j*str[1] ).swap(
                    w.row(i*str[0] + (sz[1]-j-1)*str[1] )
                    );
            }
        tbsbasis.component(k).reverse();
    }


protected:
    // todo: check function: check the coefficient number, degree, knot vector ...

    using gsGeometry<T>::m_coefs;
    using gsGeometry<T>::m_basis;

// Data members
private:

}; // class gsTensorNurbs


// ***********************************************
// ***********************************************


} // namespace gismo
