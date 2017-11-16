/** @file gsMatrixOp.h

    @brief Simple adapter classes to use matrices or linear solvers as gsLinearOperators

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): J. Sogn, C. Hofreither
*/
#pragma once

#include <gsCore/gsLinearAlgebra.h>
#include <gsSolver/gsLinearOperator.h>

namespace gismo
{

// left here for debugging purposes
// template<typename T> struct is_ref { static const bool value = false; };
// template<typename T> struct is_ref<T&> { static const bool value = true; };

/**
  * @brief Simple adapter class to use a matrix (or matrix-like
  * object) as a linear operator. Needed for the iterative method
  * classes.
  *
  * \ingroup Solver
  */
template <class MatrixType>
class gsMatrixOp : public gsLinearOperator<typename MatrixType::Scalar>
{
    typedef memory::shared_ptr<MatrixType> MatrixPtr;
    typedef typename MatrixType::Nested NestedMatrix;

public:
    typedef typename MatrixType::Scalar T;
    
    /// Shared pointer for gsMatrixOp
    typedef memory::shared_ptr<gsMatrixOp> Ptr;

    /// Unique pointer for gsMatrixOp   
    typedef memory::unique_ptr<gsMatrixOp> uPtr;
    
    /// @brief Constructor taking a reference
    ///
    /// @note This does not copy the matrix. Make sure that the matrix
    /// is not deleted too early (alternatively use constructor by
    /// shared pointer)
    gsMatrixOp(const MatrixType& mat, bool sym=false)
    : m_mat(), m_expr(mat.derived()), m_symmetric(sym)
    {
        //gsDebug<<typeid(m_expr).name()<<" Ref: "<<is_ref<NestedMatrix>::value<<"\n";
    }
    
    /// @brief Constructor taking a shared pointer
    gsMatrixOp(const MatrixPtr& mat, bool sym=false)
    : m_mat(mat), m_expr(m_mat->derived()), m_symmetric(sym)
    { }

    /// @brief Make function returning a smart pointer
    ///
    /// @note This does not copy the matrix. Make sure that the matrix
    /// is not deleted too early or provide a shared pointer.
    static uPtr make(const MatrixType& mat, bool sym=false)
    { return memory::make_unique( new gsMatrixOp(mat,sym) ); }

    /// Make function returning a smart pointer
    static uPtr make(const MatrixPtr& mat, bool sym=false)
    { return memory::make_unique( new gsMatrixOp(mat,sym) ); }
    
    void apply(const gsMatrix<T> & input, gsMatrix<T> & x) const
    {
        if (m_symmetric)
            x.noalias() = m_expr.template selfadjointView<Lower>() * input;
        else
            x.noalias() = m_expr * input;
    }

    index_t rows() const {return m_expr.rows();}

    index_t cols() const {return m_expr.cols();}

    ///Returns the matrix
    NestedMatrix matrix() const { return m_expr; }

private:
    const MatrixPtr m_mat; ///< Shared pointer to matrix (if needed)
    NestedMatrix   m_expr; ///< Nested Eigen expression
    bool      m_symmetric;
};

/** @brief This essentially just calls the gsMatrixOp constructor, but
  * the use of a template functions allows us to let the compiler do
  * type inference, so we don't need to type out the matrix type
  * explicitly.
  *
  * Examples:
  * \code
  * gsMatrix<> M;
  * M.setRandom(10,10);
  * gsLinearOperator<>::Ptr op  = makeMatrixOp(M);
  * gsLinearOperator<>::Ptr opT = makeMatrixOp(M.transpose());
  * gsLinearOperator<>::Ptr opB = makeMatrixOp(M.block(0,0,5,5) );
  * \endcode
  *
  * Note that
  * \code
  * gsLinearOperator<>::Ptr opInv = makeMatrixOp(M.inverse());
  * \endcode
  * will re-compute the inverse of the matrix every time the operator
  * opInv is applied, so this is not advised.
  * 
  * @note If a matrix is provided, only a reference is stored. Make
  * sure that the matrix is not deleted too early or provide a shared
  * pointer.
  * 
  * \ingroup Solver
  */
template <class Derived>
typename gsMatrixOp<Derived>::uPtr makeMatrixOp(const Eigen::EigenBase<Derived>& mat, bool sym=false)
{
    return gsMatrixOp<Derived>::make(mat.derived(), sym);
}

/** @brief This essentially just calls the gsMatrixOp constructor, but
  * the use of a template functions allows us to let the compiler do
  * type inference, so we don't need to type out the matrix type
  * explicitly.
  *
  * Example:
  * \code
  * gsMatrix<>::Ptr M(new gsMatrix<>);
  * M->setRandom(10,10);
  * gsLinearOperator<>::Ptr op  = makeMatrixOp(M);
  * \endcode
  *
  * Alternatively:
  * \code
  * gsMatrix<> M;
  * M.setRandom(10,10);
  * gsLinearOperator<>::Ptr op = makeMatrixOp(M.moveToPtr());
  * \endcode
  * 
  * \ingroup Solver
  */
template <class Derived>
typename gsMatrixOp<Derived>::uPtr makeMatrixOp(const memory::shared_ptr<Derived> & mat, bool sym=false)
{
    return memory::make_unique(new gsMatrixOp<Derived>(mat, sym));
}

// We need an additional guide for the compiler to be able to work well with unique ptrs
template <class Derived>
typename gsMatrixOp<Derived>::uPtr makeMatrixOp(memory::unique_ptr<Derived> mat, bool sym=false)
{
    return memory::make_unique(new gsMatrixOp<Derived>(memory::shared_ptr<Derived>(mat.release()), sym));
}

/** @brief Simple adapter class to use an Eigen solver (having a
 * compute() and a solve() method) as a linear operator.
 *
 * \ingroup Solver
 */
template <class SolverType>
class gsSolverOp : public gsLinearOperator<typename SolverType::Scalar>
{
public:
    typedef typename SolverType::Scalar T;

    typedef typename SolverType::MatrixType MatrixType;
    
    /// Shared pointer for gsSolverOp
    typedef memory::shared_ptr<gsSolverOp> Ptr;

    /// Unique pointer for gsSolverOp   
    typedef memory::unique_ptr<gsSolverOp> uPtr;
    
    
    /// Constructor taking a matrix
    gsSolverOp(const MatrixType& mat)
    {
        GISMO_ASSERT(mat.rows() == mat.cols(), "Need square matrix");
        m_size = mat.rows();

        m_solver.compute(mat);
    }

    /// Constructor taking a shared pointer
    gsSolverOp(const typename memory::shared_ptr<MatrixType>& mat)
    {
        GISMO_ASSERT(mat->rows() == mat->cols(), "Need square matrix");
        m_size = mat->rows();

        m_solver.compute(*mat);
    }
    
    /// Make function taking a matrix OR a shared pointer
    static uPtr make(const MatrixType& mat) { return memory::make_unique( new gsSolverOp(mat) ); }    
    
    void apply(const gsMatrix<T> & input, gsMatrix<T> & x) const
    {
        x = m_solver.solve(input);
    }

    index_t rows() const { return m_size; }

    index_t cols() const { return m_size; }

    /// Access the solver class
    SolverType& solver()                { return m_solver; }

    /// Access the solver class
    const SolverType& solver() const    { return m_solver; }

private:
    SolverType m_solver;
    index_t m_size;
};


/// @brief Convenience function to create an LU solver with partial
/// pivoting (for dense matrices) as a gsLinearOperator.
///
/// \ingroup Solver
template <class T, int _Rows, int _Cols, int _Opt>
typename gsSolverOp< Eigen::PartialPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >::uPtr  makePartialPivLUSolver(const gsMatrix<T, _Rows, _Cols, _Opt> & mat)
{
    return memory::make_unique( new gsSolverOp< Eigen::PartialPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >(mat) );
}

/// @brief Convenience function to create an LU solver with partial
/// pivoting (for dense matrices) as a gsLinearOperator taking a
/// shared pointer.
///
/// \ingroup Solver
template <class T, int _Rows, int _Cols, int _Opt>
typename gsSolverOp< Eigen::PartialPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >::uPtr  makePartialPivLUSolver(const memory::shared_ptr< gsMatrix<T, _Rows, _Cols, _Opt> > & mat)
{
    return memory::make_unique( new gsSolverOp< Eigen::PartialPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >(mat) );
}


/// @brief Convenience function to create an LU solver with full
/// pivoting (for dense matrices) as a gsLinearOperator.
///
/// \ingroup Solver
template <class T, int _Rows, int _Cols, int _Opt>
typename gsSolverOp< Eigen::FullPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >::uPtr  makeFullPivLUSolver(const gsMatrix<T, _Rows, _Cols, _Opt> & mat)
{
    return memory::make_unique( new gsSolverOp< Eigen::FullPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >(mat) );
}

/// @brief Convenience function to create an LU solver with full
/// pivoting (for dense matrices) as a gsLinearOperator taking a
/// shared pointer.
///
/// \ingroup Solver
template <class T, int _Rows, int _Cols, int _Opt>
typename gsSolverOp< Eigen::FullPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >::uPtr  makeFullPivLUSolver(const memory::shared_ptr< gsMatrix<T, _Rows, _Cols, _Opt> > & mat)
{
    return memory::make_unique( new gsSolverOp< Eigen::FullPivLU< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >(mat) );
}


/// @brief Convenience function to create a Cholesky (LDL^T) solver
/// (for dense matrices) as a gsLinearOperator.
///
/// @note Works only on symmetric (stored in lower half) and positive
/// (semi-)definite matrices.
///
/// \ingroup Solver
template <class T, int _Rows, int _Cols, int _Opt>
typename gsSolverOp< Eigen::LDLT< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >::uPtr  makeCholeskySolver(const gsMatrix<T, _Rows, _Cols, _Opt> & mat)
{
    return memory::make_unique( new gsSolverOp< Eigen::LDLT< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >(mat) );
}

/// @brief Convenience function to create a Cholesky (LDL^T) solver
/// (for dense matrices) as a gsLinearOperator taking a shared
/// pointer.
///
/// @note Works only on symmetric (stored in lower half) and positive
/// (semi-)definite matrices.
///
/// \ingroup Solver
template <class T, int _Rows, int _Cols, int _Opt>
typename gsSolverOp< Eigen::LDLT< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >::uPtr  makeCholeskySolver(const memory::shared_ptr< gsMatrix<T, _Rows, _Cols, _Opt> > & mat)
{
    return memory::make_unique( new gsSolverOp< Eigen::LDLT< Eigen::Matrix<T, _Rows, _Cols, _Opt> > >(mat) );
}


/// @brief Convenience function to create a sparse LU solver as a
/// gsLinearOperator.
///
/// @note This uses the default COLAMD column ordering.
///
/// \ingroup Solver
template <typename T, int _Opt, typename _Index>
typename gsSolverOp< typename gsSparseSolver<T>::LU >::uPtr  makeSparseLUSolver(const gsSparseMatrix<T,_Opt,_Index> & mat)
{
    return memory::make_unique( new gsSolverOp< typename gsSparseSolver<T>::LU >(mat) );
}

/// @brief Convenience function to create a sparse LU solver as a
/// gsLinearOperator taking a shared pointer.
///
/// @note This uses the default COLAMD column ordering.
///
/// \ingroup Solver
template <typename T, int _Opt, typename _Index>
typename gsSolverOp< typename gsSparseSolver<T>::LU >::uPtr  makeSparseLUSolver(const memory::shared_ptr< gsSparseMatrix<T,_Opt,_Index> > & mat)
{
    return memory::make_unique( new gsSolverOp< typename gsSparseSolver<T>::LU >(mat) );
}


/// @brief Convenience function to create a sparse Cholesky
/// (simplicial LDL^T) solver as a gsLinearOperator.
///
/// @note Works only on sparse, symmetric (stored in lower half) and
/// positive definite matrices.
///
/// \ingroup Solver
template <typename T, int _Opt, typename _Index>
typename gsSolverOp< typename gsSparseSolver<T>::SimplicialLDLT >::uPtr  makeSparseCholeskySolver(const gsSparseMatrix<T,_Opt,_Index> & mat)
{
    return memory::make_unique( new gsSolverOp<typename  gsSparseSolver<T>::SimplicialLDLT >(mat) );
}

/// @brief Convenience function to create a sparse Cholesky
/// (simplicial LDL^T) solver as a gsLinearOperator.
///
/// @note Works only on sparse, symmetric (stored in lower half) and
/// positive definite matrices taking a shared pointer.
///
/// \ingroup Solver
template <typename T, int _Opt, typename _Index>
typename gsSolverOp< typename gsSparseSolver<T>::SimplicialLDLT >::uPtr  makeSparseCholeskySolver(const memory::shared_ptr< gsSparseMatrix<T,_Opt,_Index> > & mat)
{
    return memory::make_unique( new gsSolverOp<typename  gsSparseSolver<T>::SimplicialLDLT >(mat) );
}


} // namespace gismo
