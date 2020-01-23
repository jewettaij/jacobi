///   @file  jacobi.hpp
///   @brief Calculate the eigenvalues and eigevectors of a symmetric matrix
///          using the Jacobi eigenvalue algorithm.
#ifndef _JACOBI_HPP
#define _JACOBI_HPP

#include <cmath>
#include <cassert>

namespace jacobi_public_domain {

/// @class Jacobi
/// @brief Calculate the eigenvalues and eigevectors of a symmetric matrix
///        using the Jacobi eigenvalue algorithm.
///        The algorithm implemented here follows the strategy explained here:
///        https://en.wikipedia.org/wiki/Jacobi_eigenvalue_algorithm
///        https://web.stanford.edu/class/cme335/lecture7.pdf
/// @note  The "Vector" and "Matrix" type arguments can be any 
///        C or C++ object that support indexing, including pointers or vectors.

template<typename Scalar, typename Vector, typename Matrix>
class Jacobi
{
  int n;                   //!< the size of the matrix
  // The next 3 data members store the rotation, translation and scale
  // after optimal superposition
  int *max_indx_rw;      //!< for each row, store the index of the maximum off-diagonal element
  // Precomputed cosine, sin, and tangent of the most recent rotation angle:
  Scalar c;                //!< = cos(θ)
  Scalar s;                //!< = sin(θ)
  Scalar t;                //!< = tan(θ),  (note |t|<=1)

public:

  /// @brief  Specify the size of the matrices you want to diagonalize later.
  /// @param n  the number of rows and columns in the matrix
  Jacobi(int n) {
    Init();
    SetSize(n);
  }

  // @typedef choose the criteria for sorting eigenvalues and eigenvectors
  typedef enum eSortCriteria {
    DO_NOT_SORT,
    SORT_DECREASING_EVALS,
    SORT_INCREASING_EVALS,
    SORT_DECREASING_ABS_EVALS,
    SORT_INCREASING_ABS_EVALS
  } SortCriteria;

  /// @brief Calculate the eigenvalues and eigevectors of a symmetric matrix
  ///        using the Jacobi eigenvalue algorithm:
  ///        https://en.wikipedia.org/wiki/Jacobi_eigenvalue_algorithm
  ///        Return true if the algorithm failed to converge.
  /// @returns The number_of_sweeps (= number_of_iterations / (n*(n-1)/2)).
  ///          If this equals max_num_sweeps, the algorithm failed to converge.
  /// @note  The contents of the matrix M is modified during the calculation.
  /// @note  To reduce the computation time further, set calc_evects=false.
  int
  Diagonalize(Matrix M,      //!< the matrix you wish to diagonalize (size n)
              Vector eval,   //!< store the eigenvalues here
              Matrix evec,   //!< store the eigenvectors here (in rows)
              SortCriteria sort_criteria=SORT_DECREASING_EVALS,//!<sort results?
              bool calc_evects=true,     //!< calculate the eigenvectors?
              int max_num_sweeps = 50);  //!< limit the number of iterations

private:

  /// @brief Calculate the components of a rotation matrix which performs a
  ///        i,j plane by an angle (θ) that (when multiplied on both sides)
  ///        will zero the ij'th element of M, so that afterwards M[i][j] = 0
  ///        (This will also zero M[j][i] since M is assumed to be symmetric.)
  ///        The results will be stored in c, s, and t
  ///        (which store cos(θ), sin(θ), and tan(θ), respectively).
  void CalcRot(Matrix M,   //!< matrix
               int i,      //!< row index
               int j);     //!< column index

  /// @brief Apply the (previously calculated) rotation matrix to matrix M
  ///        by multiplying it on both sides (a "similarity transform").
  ///        (To save time, only update the elements in the upper-right
  ///         triangular region of the matrix.)
  void ApplyRot(Matrix M,  //!< matrix
                int i,     //!< row index
                int j);     //!< column index

  /// @brief Multiply matrix E on the right by the (previously calculated)
  ///         rotation matrix.
  void ApplyRotRight(Matrix E,  //!< matrix
                     int i,     //!< row index
                     int j);    //!< column index

  ///@brief Find the off-diagonal index in row i whose absolute value is largest
  int MaxEntryRow(Matrix M, int i) const;

  /// @brief Find the indices (i_max, j_max) marking the location of the
  ///        entry in the matrix with the largest absolute value.  This
  ///        exploits the max_indx_rw[] array to find the answer in O(n) time.
  /// @returns void.  After it is invoked, the location of the largest matrix
  ///        element will be stored in the i_max and j_max arguments.
  void MaxEntry(Matrix M, int& i_max, int& j_max) const;

  // @brief Sort the rows in M according to the numbers in v (also sorted)
  void SortRows(Vector v, //!< vector containing the keys used for sorting
                Matrix M, //!< matrix whose rows will be sorted according to v
                int n,    //!< size of the vector and matrix
                SortCriteria s=SORT_DECREASING_EVALS //!< sort decreasing order?
                ) const;

  // memory management:
  void SetSize(int n);
  void Alloc(int n);
  void Init();
  void Dealloc();

  // memory management: copy constructor, swap, and assignment operator
  Jacobi(const Jacobi<Scalar, Vector, Matrix>& source);
  void swap(Jacobi<Scalar, Vector, Matrix> &other);
  Jacobi<Scalar, Vector, Matrix>& operator = (Jacobi<Scalar, Vector, Matrix> source);

}; // class Jacobi





// -------------- IMPLEMENTATION --------------


/// brief  Calculate the components of a rotation matrix which performs a
///        i,j plane by an angle (θ) that (when multiplied on both sides)
///        will zero the ij'th element of M, so that afterwards M[i][j] = 0
///        (This will also zero M[j][i] since M is assumed to be symmetric.)

template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
CalcRot(Matrix M,    //!< matrix
        int i,       //!< row index
        int j)       //!< column index
{
  t = 1.0; // = tan(θ)
  Scalar M_jj_ii = (M[j][j] - M[i][i]);
  if (M_jj_ii != 0.0) {
    // kappa = (M[j][j] - M[i][i]) / (2*M[i][j])
    Scalar kappa = M_jj_ii;
    t = 0.0;
    Scalar M_ij = M[i][j];
    if (M_ij != 0.0) {
      kappa /= (2.0*M_ij);
      // t satisfies: t^2 + 2*t*kappa - 1 = 0
      // (choose the root which has the smaller absolute value)
      t = 1.0 / (std::sqrt(1 + kappa*kappa) + std::abs(kappa));
      if (kappa < 0.0)
        t = -t;
    }
  }
  assert(std::abs(t) <= 1.0);
  c = 1.0 / std::sqrt(1 + t*t);
  s = c*t;
}


/// brief   Perform a similarity transform by multiplying matrix M on both
///         sides by a rotation matrix (applying its inverse to the other side)
///         This matrix performs a rotation in the i,j plane by angle θ.
///         Also update the max_indx_rw[] array.
/// details This function assumes that i<j and that cos(θ), sin(θ), and tan(θ)
///         have already been computed (by invoking CalcRot()).
///         To save time, since the matrix is symmetric, the elements
///         below the diagonal (ie. M[u][v] where u>v) are not computed.
/// @code
///   M' = R^T * M * R
/// where R the rotation in the i,j plane and ^T denotes the transpose.
///                 i         j
///       _                             _
///      |  1                            | 
///      |    .                          |
///      |      .                        |
///      |        1                      |
///      |          c   ...   s          |
///      |          .  .      .          |
/// R  = |          .    1    .          |
///      |          .      .  .          |
///      |          -s  ...   c          |
///      |                      1        |
///      |                        .      |
///      |                          .    |
///      |_                           1 _|
/// @endcode
///
/// Let M' denote the matrix M after multiplication by R^T and R.
/// The components of M' are:
///   M'_uv =  Σ_w  Σ_z   R_wu * M_wz * R_zv
///
/// Note that a Givens rotation at location i,j will modify all of the matrix
/// elements containing at least one index which is either i or j
/// such as: M[w][i], M[i][w], M[w][j], M[j][w].
/// Check and see whether these modified matrix elements exceed the 
/// corresponding values in max_indx_rw[] array for that row.
/// If so, then update max_indx_rw for that row.
/// This is somewhat complicated by the fact that we must only consider
/// matrix elements in the upper-right triangle strictly above the diagonal.
/// (ie. matrix elements whose second index is > the first index).
/// The modified elements we must consider are marked with an "X" below:
///
/// @code
///                 i         j
///       _                             _
///      |  .       X         X          | 
///      |    .     X         X          |
///      |      .   X         X          |
///      |        . X         X          |
///      |          X X X X X 0 X X X X  |  i
///      |            .       X          |
///      |              .     X          |
/// M  = |                .   X          |
///      |                  . X          |
///      |                    X X X X X  |  j
///      |                      .        |
///      |                        .      |
///      |                          .    |
///      |_                           . _|
/// @endcode

template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
ApplyRot(Matrix M,  //!< matrix
         int i,     //!< row index
         int j)     //!< column index
{
  // Recall that:
  // c = cos(θ)
  // s = sin(θ)
  // t = tan(θ) (which should be <= 1.0)

  // Compute the diagonal elements of M which have changed:
  assert(std::abs(t) <= 1.0);
  M[i][i] -= t * M[i][j];
  M[j][j] += t * M[i][j];
  // Note: This is algebraically equivalent to:
  // M[i][i] = c*c*M[i][i] + s*s*M[j][j] - 2*s*c*M[i][j]
  // M[j][j] = s*s*M[i][i] + c*c*M[j][j] + 2*s*c*M[i][j]

  // Compute the off-diagonal elements of M which have changed:

  assert(i < j);

  M[i][j] = 0.0;

  // compute M[w][i] and M[i][w] for all w!=i
  for (int w=0; w < i; w++) {           // 0 <= w <  i  <  j < n
    M[i][w] = M[w][i]; //backup the previous value (needed later) below diagonal
    M[w][i] = c*M[w][i] - s*M[w][j];
    if (std::abs(M[w][i]) > std::abs(M[w][max_indx_rw[w]])) max_indx_rw[w] = i;
  }
  for (int w=i+1; w < j; w++) {         // 0 <= i <  w  <  j < n
    M[w][i] = M[i][w]; //backup the previous value (needed later) below diagonal
    M[i][w] = c*M[i][w] - s*M[w][j];
    if (std::abs(M[i][w]) > std::abs(M[i][max_indx_rw[i]])) max_indx_rw[i] = w;
  }
  for (int w=j+1; w < n; w++) {         // 0 <= i < j+1 <= w < n
    M[w][i] = M[i][w]; //backup the previous value (needed later) below diagonal
    M[i][w] = c*M[i][w] - s*M[j][w];
    if (std::abs(M[i][w]) > std::abs(M[i][max_indx_rw[i]])) max_indx_rw[i] = w;
  }

  // compute M[w][j] and M[w][j] for all w!=j
  for (int w=0; w < i; w++) {    // 0 <=  w  <  i <  j < n
    //M[w][j] = s*M[w][i] + c*M[w][j];
    M[w][j] = s*M[i][w] + c*M[w][j]; //Note:M[w][i] was updated, M[i][w] is not
    if (std::abs(M[w][j]) > std::abs(M[w][max_indx_rw[w]])) max_indx_rw[w] = j;
  }
  for (int w=i+1; w < j; w++) {    // 0 <= i+1 <= w <  j < n
    //M[w][j] = s*M[i][w] + c*M[w][j];
    M[w][j] = s*M[w][i] + c*M[w][j]; //Note:M[i][w] was updated, M[w][i] is not
    if (std::abs(M[w][j]) > std::abs(M[w][max_indx_rw[w]])) max_indx_rw[w] = j;
  }
  for (int w=j+1; w < n; w++) {    // 0 <=  i  <  j <  w < n
    //M[j][w] = s*M[i][w] + c*M[j][w];
    M[j][w] = s*M[w][i] + c*M[j][w]; //Note:M[i][w] was updated, M[w][i] is not
    if (std::abs(M[j][w]) > std::abs(M[j][max_indx_rw[j]])) max_indx_rw[j] = w;
  }
}




/// brief   Multiply matrix M on the RIGHT side only by a rotation matrix.
///         This matrix performs a rotation in the i,j plane by angle θ  (where
///         the arguments "s" and "c" refer to cos(θ) and sin(θ), respectively).
/// @code
///   E'_uv = Σ_w  E_uw * R_wv
/// @endcode

template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
ApplyRotRight(Matrix E,  //!< matrix
              int i,     //!< row index
              int j)     //!< column index
{
  // Recall that c = cos(θ) and s = sin(θ)
  for (int u = 0; u < n; u++) {
    E[u][i] = c*E[u][i] - s*E[u][j];
    E[u][j] = s*E[u][i] + c*E[u][j];
  }
}



template<typename Scalar, typename Vector, typename Matrix>
int Jacobi<Scalar, Vector, Matrix>::
MaxEntryRow(Matrix M, int i) const {
  assert(i < n-1);
  int j_max = i+1;
  for(int j = i+2; j < n; j++)
    if (std::abs(M[i][j]) > std::abs(M[i][j_max]))
      j_max = j;
  return j_max;
}



template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
MaxEntry(Matrix M, int& i_max, int& j_max) const {
  // find the maximum entry in the matrix M in O(n) time
  i_max = 0; // (start with an arbitrary
  j_max = 1; //  off-diagonal element: M[0][1])
  Scalar max_entry = std::abs(M[i_max][j_max]);
  int nm1 = n-1;
  for (int i=0; i < nm1; i++) {
    int j = max_indx_rw[i];
    if (std::abs(M[i][j]) > max_entry) {
      max_entry = std::abs(M[i][j]);
      i_max = i;
      j_max = j;
    }
  }
  #ifndef NDEBUG
  // -- remove the next 4 lines before publishing --
  // make sure that the maximum element really is stored at i_max, j_max
  for (int i = 0; i < nm1; i++)
    for (int j = i+1; j < n; j++)
      assert(std::abs(M[i][j]) <= max_entry);
  // --
  #endif
}



template<typename Scalar, typename Vector, typename Matrix>
int Jacobi<Scalar, Vector, Matrix>::
Diagonalize(Matrix M,          //!< the matrix you wish to diagonalize (size n)
            Vector eval,          //!< store the eigenvalues here
            Matrix evec,          //!< store the eigenvectors here (in rows)
            SortCriteria sort_criteria, //!<sort results?
            bool calc_evects,     //!< calculate the eigenvectors?
            int max_num_sweeps)   //!< limit the number of iterations ("sweeps")
{
  for (int i = 0; i < n-1; i++)
    //Initialize the "max_indx_rw[]" array (useful for finding the max entry)
    max_indx_rw[i] = MaxEntryRow(M, i);

  int n_iters;                                  // number of pivots chosen
  int max_num_iters = max_num_sweeps*n*(n-1)/2; //"sweep" = n*(n-1)/2 iters
  for (n_iters=0; n_iters < max_num_iters; n_iters++) {
    int i,j;
    MaxEntry(M, i, j); // Find the maximum entry in the matrix. Store in i,j

    if (M[i][j] == 0.0)
      break;

    if ((M[i][i] + M[i][j] == M[i][i]) && (M[j][j] + M[i][j] == M[j][j])) {
      // If M[i][j] is small compared to M[i][i] and M[j][j], set it to 0.
      M[i][j] = 0.0;
      max_indx_rw[i] = MaxEntryRow(M, i);//must also update max_indx_rw[i]
      continue;
    }

    CalcRot(M, i, j);  // Calculate the parameters of the rotation matrix.
    ApplyRot(M, i, j); // Apply this rotation to the M matrix.

    if (calc_evects)   // Optional: If the caller wants the eigenvectors, then
      ApplyRotRight(evec,i,j); // Apply the rotation to the eigenvector matrix

  } //for (int n_iters=0; n_iters < max_num_iters; n_iters++)

  for (int i = 0; i < n; i++)
    eval[i] = M[i][i];

  // Sort eigenvalues and eigenvectors?
  if (sort_criteria != DO_NOT_SORT)
    SortRows(eval, evec, n, sort_criteria);

  return n_iters / (n*(n-1)/2); //returns the number of "sweeps" (converged?)
}



//Sort the rows of a matrix "evec" by the numbers contained in "eval"
template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
SortRows(Vector eval, Matrix evec, int n, SortCriteria sort_criteria) const
{
  for (int i = 0; i < n; i++) {
    int i_max = i;
    for (int j = i+1; j < n; j++) {
      // find the "maximum" element in the array starting at position i+1
      switch (sort_criteria) {
      case SORT_DECREASING_EVALS:
        if (eval[j] > eval[i_max])
          i_max = j;
      case SORT_INCREASING_EVALS:
        if (eval[j] < eval[i_max])
          i_max = j;
      case SORT_DECREASING_ABS_EVALS:
        if (std::abs(eval[j]) > std::abs(eval[i_max]))
          i_max = j;
        break;
      case SORT_INCREASING_ABS_EVALS:
        if (std::abs(eval[j]) < std::abs(eval[i_max]))
          i_max = j;
        break;
      default:
        assert(false);
        break;
      }
    }
    std::swap(eval[i], eval[i_max]); // sort "eval"
    for (int k = 0; k < n; k++)
      std::swap(evec[i][k], evec[i_max][k]); // sort "evec"
  }
}



template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
Init() {
  n = 0;
  max_indx_rw = nullptr;
}

template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
SetSize(int n) {
  Dealloc();
  Alloc(n);
}

template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
Alloc(int n) {
  this->n = n;
  max_indx_rw = new int[n];
  assert(max_indx_rw);
}

template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
Dealloc() {
  if (max_indx_rw)
    delete [] max_indx_rw;
  Init();
}

template<typename Scalar, typename Vector, typename Matrix>
Jacobi<Scalar, Vector, Matrix>::
Jacobi(const Jacobi<Scalar, Vector, Matrix>& source)
{
  Init();
  Alloc(source.n);
  assert(n == source.n);
  std::copy(source.max_indx_rw,
            source.max_indx_rw + n,
            max_indx_rw);
}

template<typename Scalar, typename Vector, typename Matrix>
void Jacobi<Scalar, Vector, Matrix>::
swap(Jacobi<Scalar, Vector, Matrix> &other) {
  std::swap(max_indx_rw, other.max_indx_rw);
  std::swap(n, other.n);
}

template<typename Scalar, typename Vector, typename Matrix>
Jacobi<Scalar, Vector, Matrix>&
Jacobi<Scalar, Vector, Matrix>::
operator = (Jacobi<Scalar, Vector, Matrix> source) {
  this->swap(source);
  return *this;
}



} // namespace jacobi


#endif //#ifndef _JACOBI_HPP

