/** @file gsFitting.hpp

    @brief Provides implementation of data fitting algorithms by least
    squares approximation.

    This file is part of the G+Smo library.
    
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): M. Kapl, G. Kiss, A. Mantzaflaris

*/

#include <gsCore/gsBasis.h>
#include <gsCore/gsGeometry.h>
#include <gsCore/gsLinearAlgebra.h>
#include <gsTensor/gsTensorDomainIterator.h>


namespace gismo
{

template<class T>
gsFitting<T>::~gsFitting() 
{
    if ( m_result )
        delete m_result;
}

template<class T>
gsFitting<T>::  gsFitting(gsMatrix<T> const & param_values, 
                          gsMatrix<T> const & points, 
                          gsBasis<T>  & basis)
{
    m_param_values = param_values;
    m_points = points;
    m_result = NULL;
    m_basis = &basis;
    m_points.transposeInPlace();
}

template<class T>
void gsFitting<T>::compute(T lambda)
{
    // Wipe out previous result
    if ( m_result )
        delete m_result;

    const int num_basis=m_basis->size();
    const int dimension=m_points.cols();

    //left side matrix
    //gsMatrix<T> A_mat(num_basis,num_basis);
    gsSparseMatrix<T> A_mat(num_basis, num_basis);
    //gsMatrix<T>A_mat(num_basis,num_basis);
    //To optimize sparse matrix an estimation of nonzero elements per
    //column can be given here
    int nonZerosPerCol = 1;
    for (int i = 0; i < m_basis->dim(); ++i) // to do: improve
        //nonZerosPerCol *= 2 * m_basis->degree(i) + 1;
        nonZerosPerCol *= ( 2 * m_basis->degree(i) + 1 ) * 4;//AM: Related to bandwidth of basis
    A_mat.reserve( gsVector<index_t>::Constant(num_basis, nonZerosPerCol ) );
    //gsDebugVar( nonZerosPerCol );

    A_mat.setZero(); // ensure that all entries are zero in the beginning
    //right side vector (more dimensional!)
    gsMatrix<T> m_B(num_basis, dimension);
    // enusure that all entries are zero in the beginning
    m_B.setZero(); 
    // building the matrix A and the vector b of the system of linear
    // equations A*x==b
    
    assembleSystem(A_mat, m_B);

    // --- Smoothing matrix computation
    //test degree >=3
    if(lambda > 0)
      applySmoothing(lambda, A_mat);

    //Solving the system of linear equations A*x=b (works directly for a right side which has a dimension with higher than 1)

    //gsDebugVar( A_mat.nonZerosPerCol().maxCoeff() );
    //gsDebugVar( A_mat.nonZerosPerCol().minCoeff() );
    A_mat.makeCompressed();

    typename gsSparseSolver<T>::BiCGSTABILUT solver( A_mat );

    if ( solver.preconditioner().info() != Eigen::Success )
    {
        gsWarn<<  "The preconditioner failed. Aborting.\n";
        m_result = NULL;
        return;
    }
    // Solves for many right hand side  columns
    gsMatrix<T> x;
    x = solver.solve(m_B); //toDense()
    //gsMatrix<T> x (m_B.rows(), m_B.cols());
    //x=A_mat.fullPivHouseholderQr().solve( m_B);
    // Solves for many right hand side  columns
    // finally generate the B-spline curve
    m_result = m_basis->makeGeometry( give(x) ).release();
}


template <class T>
void gsFitting<T>::assembleSystem(gsSparseMatrix<T>& A_mat,
				  gsMatrix<T>& m_B)
{
    const int num_points = m_points.rows();    

    //for computing the value of the basis function
    gsMatrix<T> value;
    gsMatrix<unsigned> actives;


    for(index_t k = 0; k < num_points; k++)
    {
        const gsMatrix<T>& curr_point = m_param_values.col(k);

        //computing the values of the basis functions at the current point
        m_basis->eval_into(curr_point, value);

        // which functions have been computed i.e. which are active
        m_basis->active_into(curr_point, actives);
        
	const index_t numActive = actives.rows();

        for (index_t i = 0; i != numActive; ++i)
        {
            const int ii = actives(i, 0);
            m_B.row(ii) += value(i, 0) * m_points.row(k);
            for (index_t j = 0; j != numActive; ++j)
                A_mat(ii, actives(j, 0)) += value(i, 0) * value(j, 0);
        }
    }
}


template<class T>
void gsFitting<T>::applySmoothing(T lambda, gsSparseMatrix<T> & A_mat)
{
    const int dim = m_basis->dim();
    const int stride = dim*(dim+1)/2;

    gsVector<int> numNodes(dim);
    for ( int i = 0; i!= dim; ++i )
        numNodes[i] = this->m_basis->degree(i);//+1;
    gsGaussRule<T> QuRule( numNodes ); // Reference Quadrature rule
    gsMatrix<T> quNodes, der2, localA;
    gsVector<T> quWeights;
    gsMatrix<unsigned> actives;

    typename gsBasis<T>::domainIter domIt = m_basis->makeDomainIterator();

    for (; domIt->good(); domIt->next() )
    {
        // Map the Quadrature rule to the element and compute basis derivatives
        QuRule.mapTo( domIt->lowerCorner(), domIt->upperCorner(), quNodes, quWeights );
        m_basis->deriv2_into(quNodes, der2);
        m_basis->active_into(domIt->center, actives);
        const index_t numActive = actives.rows();
        localA.setZero(numActive, numActive );

        // perform the quadrature
        for (index_t k = 0; k < quWeights.rows(); ++k)
        {
            const T weight = quWeights[k] * lambda;

            for (index_t i=0; i!=numActive; ++i)
                for (index_t j=0; j!=numActive; ++j)
                {
                    T localAij = 0; // temporary variable

                    for (int s = 0; s < stride; s++)
                    {
                        // The pure second derivatives
                        // d^2u N_i * d^2u N_j + ...
                        if (s < dim)
                        {
                            localAij += der2(i * stride + s, k) *
                                        der2(j * stride + s, k);
                        }
                        // Mixed derivatives 2 * dudv N_i * dudv N_j + ...
                        else
                        {
                            localAij += 2 * der2(i * stride + s, k) *
                                            der2(j * stride + s, k);
                        }
                    }

                    localA(i, j) += weight * localAij;

                    // old code, just for the case if I break something

//                    localA(i,j) += weight * (
//                            // der2.template block<3,1>(i*stride,k)
//                            der2(i*stride  , k) * der2(j*stride  , k) +  // d^2u N_i * d^2u N_j
//                            der2(i*stride+1, k) * der2(j*stride+1, k) +  // d^2v N_i * d^2v N_j
//                            2 * der2(i*stride+2, k) * der2(j*stride+2, k)// dudv N_i * dudv N_j
//                            );
                }
        }
        
        for (index_t i=0; i!=numActive; ++i)
        {
            const int ii = actives(i,0);
            for (index_t j=0; j!=numActive; ++j)
                A_mat( ii, actives(j,0) ) += localA(i,j);
        }
    }
}

template<class T>
void gsFitting<T>::computeErrors()
{
    m_pointErrors.clear();

    gsMatrix<T> val_i;
    //m_result->eval_into(m_param_values.col(0), val_i);
    m_result->eval_into(m_param_values, val_i);
    m_pointErrors.push_back( (m_points.row(0) - val_i.col(0).transpose()).norm() );
    m_max_error = m_min_error = m_pointErrors.back();
    
    for (index_t i = 1; i < m_points.rows(); i++)
    {
        //m_result->eval_into(m_param_values.col(i), val_i);

        const T err = (m_points.row(i) - val_i.col(i).transpose()).norm() ;

        m_pointErrors.push_back(err);

        if ( err > m_max_error ) m_max_error = err;
        if ( err < m_min_error ) m_min_error = err;
    }
}


template<class T>
void gsFitting<T>::computeMaxNormErrors()
{
    m_pointErrors.clear();

    gsMatrix<T> values;
    m_result->eval_into(m_param_values, values);

    for (index_t i = 0; i != m_points.rows(); i++)
    {
        const T err = (m_points.row(i) - values.col(i).transpose()).cwiseAbs().maxCoeff();

        m_pointErrors.push_back(err);

        if ( i == 0 || m_max_error < err ) m_max_error = err;
        if ( i == 0 || err < m_min_error ) m_min_error = err;
    }
}


  
template<class T>
void gsFitting<T>::computeApproxError(T& error, int type) const
{
    gsMatrix<T> results;
    m_result->eval_into(m_param_values, results);
    error = 0;

    //computing the approximation error = sum_i ||x(u_i)-p_i||^2

    for (index_t i = 0; i != m_points.rows(); ++i)
    {
        const T err = (m_points.row(i) - results.col(i).transpose()).squaredNorm();

        switch (type) {
        case 0:
            error += err;
            break;
        case 1:
            error += sqrt(err);
            break;
        default:
            gsWarn << "Unknown type in computeApproxError(error, type)...\n";
            break;
        }
    }
}

template<class T>
void gsFitting<T>::get_Error(std::vector<T>& errors, int type) const
{ 
    errors.clear();
    gsMatrix<T> results;
    m_result->eval_into(m_param_values,results);
    results.transposeInPlace();

    for (index_t row = 0; row != m_points.rows(); row++)
    {
        T err = 0;
        for (index_t col = 0; col != m_points.cols(); col++)
        {
            err += math::pow(m_points(row, col) - results(row, col), 2);
        }

        switch (type)
        {
        case 0:
            errors.push_back(err);
            break;
        case 1:
            errors.push_back(sqrt(err));
            break;
        default:
            gsWarn << "Unknown type in get_Error(errors, type)...\n";
            break;
        }
    }
}

} // namespace gismo
