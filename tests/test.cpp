#include <iostream>
#include <cmath>
#include <iomanip>
#include <cstdlib>
#include <chrono>
#include <random>
#include <algorithm>
#include <vector>
#include <array>
#include "matrix_alloc.hpp"
#include "jacobi.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::setprecision;
using std::vector;
using std::array;
using namespace matrix_alloc;
using namespace jacobi_public_domain;


// This code works with various types of C++ matrices (for example,
// double **, vector<vector<double>> array<array<double,5>,5>).
// I use "#if defined" statements to test different matrix types.
// For some of these (eg. array<array<double,5>,5>), the size of the matrix
// must be known at compile time.  I specify that size now.
#if defined USE_ARRAY_OF_ARRAYS
const int NF=5;  //(the array size must be known at compile time)
#elif defined USE_C_FIXED_SIZE_ARRAYS
const int NF=5;  //(the array size must be known at compile time)
#endif


// @brief  Are two numbers "similar"?
template<typename Scalar>
inline static bool Similar(Scalar a, Scalar b,
                           Scalar eps=1.0e-06, Scalar ratio=1.0e-06)
{
  return ((std::abs(a-b)<=std::abs(eps))
          ||
          (std::abs(a-b)<=std::abs(ratio)*0.5*(std::abs(a)+std::abs(b))));
}

/// @brief  Are two vectors (containing n numbers) similar?
template<typename Scalar, typename Vector>
inline static bool SimilarVec(Vector a, Vector b, int n,
                              Scalar eps=1.0e-06, Scalar ratio=1.0e-06)
{
  for (int i = 0; i < n; i++)
    if (not Similar(a[i], b[i], eps, ratio))
      return false;
  return true;
}

/// @brief  Are two vectors (or their reflections) similar?
template<typename Scalar, typename Vector>
inline static bool SimilarVecUnsigned(Vector a, Vector b, int n,
                                      Scalar eps=1.0e-06, Scalar ratio=1.0e-06)
{
  if (SimilarVec(a, b, n, eps))
    return true;
  else {
    for (int i = 0; i < n; i++)
      if (not Similar(a[i], -b[i], eps, ratio))
        return false;
    return true;
  }
}


/// @brief  Multiply two matrices A and B, store the result in C. (C = AB).

template<typename Matrix, typename ConstMatrix>
void mmult(ConstMatrix A, //<! input array
           ConstMatrix B, //<! input array
           Matrix C,      //<! store result here
           int m,      //<! number of rows of A
           int n=0,    //<! optional: number of columns of B (=m by default)
           int K=0     //<! optional: number of columns of A = num rows of B (=m by default)
           )
{
  if (n == 0) n = m; // if not specified, then assume the matrices are square
  if (K == 0) K = m; // if not specified, then assume the matrices are square

  for (int i = 0; i < m; i++)
    for (int j = 0; j < n; j++)
      C[i][j] = 0.0;

  // perform matrix multiplication
  for (int i = 0; i < m; i++)
    for (int j = 0; j < n; j++)
      for (int k = 0; k < K; k++)
        C[i][j] += A[i][k] * B[k][j];
}



/// @brief
///Sort the rows of a matrix "evec" by the numbers contained in "eval"
///(This is a simple O(n^2) sorting method, but O(n^2) is a lower bound anyway.)
///This is the same as the Jacobi::SortRows(), but that function is private.
template<typename Scalar, typename Vector, typename Matrix>
void
SortRows(Vector eval,
         Matrix evec,
         int n,
         bool sort_decreasing=true,
         bool sort_abs=false)
{
  for (int i = 0; i < n-1; i++) {
    int i_max = i;
    for (int j = i+1; j < n; j++) {
      if (sort_decreasing) {
        if (sort_abs) { //sort by absolute value?
          if (std::abs(eval[j]) > std::abs(eval[i_max]))
            i_max = j;
        }
        else if (eval[j] > eval[i_max])
          i_max = j;
      }
      else {
        if (sort_abs) { //sort by absolute value?
          if (std::abs(eval[j]) < std::abs(eval[i_max]))
            i_max = j;
        }
        else if (eval[j] < eval[i_max])
          i_max = j;
      }
    }
    std::swap(eval[i], eval[i_max]); // sort "eval"
    for (int k = 0; k < n; k++)
      std::swap(evec[i][k], evec[i_max][k]); // sort "evec"
  }
}



/// @brief  Generate a random orthonormal n x n matrix

template<typename Scalar, typename Matrix>
void GenRandOrth(Matrix R,
                 int n,
                 std::default_random_engine &generator)
{
  std::normal_distribution<double> gaussian_distribution(0,1);
  std::vector<Scalar> v(n);

  for (int i = 0; i < n; i++) {
    // Generate a vector, "v", in a random direction subject to the constraint
    // that it is orthogonal to the first i-1 rows-vectors of the R matrix.
    Scalar rsq = 0.0;
    while (rsq == 0.0) {
      // Generate a vector in a random direction
      // (This works because we are using a normal (Gaussian) distribution)
      for (int j = 0; j < n; j++)
        v[j] = gaussian_distribution(generator);

      //Now subtract from v, the projection of v onto the first i-1 rows of R.
      //This will produce a vector which is orthogonal to these i-1 row-vectors.
      //(They are already normalized and orthogonal to each other.)
      for (int k = 0; k < i; k++) {
        Scalar v_dot_Rk = 0.0;
          for (int j = 0; j < n; j++)
            v_dot_Rk += v[j] * R[k][j];
        for (int j = 0; j < n; j++)
          v[j] -= v_dot_Rk * R[k][j];
      }
      // check if it is linearly independent of the other vectors and non-zero
      rsq = 0.0;
      for (int j = 0; j < n; j++)
        rsq += v[j]*v[j];
    }
    // Now normalize the vector
    Scalar r_inv = 1.0 / std::sqrt(rsq);
    for (int j = 0; j < n; j++)
      v[j] *= r_inv;
    // Now copy this vector to the i'th row of R
    for (int j = 0; j < n; j++)
      R[i][j] = v[j];
  } //for (int i = 0; i < n; i++)
} //void GenRandOrth()



/// @brief  Generate a random symmetric n x n matrix, M.
/// This function generates random numbers for the eigenvalues ("evals_known")
/// as well as the eigenvectors ("evects_known"), and uses them to generate M.
/// The "eval_magnitude_range" argument specifies the the base-10 logarithm
/// of the range of eigenvalues desired.  The "n_degeneracy" argument specifies
/// the number of repeated eigenvalues desired (if any).
/// @returns  This function does not return a value.  However after it is
///           invoked, the M matrix will be filled with random numbers.
///           Additionally, the "evals" and "evects" arguments will contain
///           the eigenvalues and eigenvectors (one eigenvector per row)
///           of the matrix.  Later, they can be compared with the eigenvalues
///           and eigenvectors calculated by Jacobi::Diagonalize()

template <typename Scalar, typename Vector, typename Matrix>
void GenRandSymm(Matrix M,       //<! store the matrix here
                 int n,          //<! matrix size
                 Vector evals,   //<! store the eigenvalues of here
                 Matrix evects,  //<! store the eigenvectors here
                 std::default_random_engine &generator,//<! makes random numbers
                 Scalar min_eval_size=0.1, //<! minimum possible eigenvalue size
                 Scalar max_eval_size=10.0,//<! maximum possible eigenvalue size
                 int n_degeneracy=1//<!number of repeated eigevalues(1disables)
                 )
{
  assert(n_degeneracy <= n);
  std::uniform_real_distribution<Scalar> random_real01;
  #if defined USE_VECTOR_OF_VECTORS
  vector<vector<Scalar> > D(n, vector<Scalar>(n));
  vector<vector<Scalar> > tmp(n, vector<Scalar>(n));
  #elif defined USE_ARRAY_OF_ARRAYS
  array<array<Scalar, NF>, NF> D;
  array<array<Scalar, NF>, NF> tmp;
  #elif defined USE_C_FIXED_SIZE_ARRAYS
  Scalar D[NF][NF], tmp[NF][NF];
  #else
  #define USE_C_POINTER_TO_POINTERS
  Scalar  **D, **tmp;
  Alloc2D(n, n, &D);
  Alloc2D(n, n, &tmp);
  #endif

  // Randomly generate the eigenvalues
  for (int i = 0; i < n; i++) {
    // Use a "log-uniform distribution" (a.k.a. "reciprocal distribution")
    // (This is a way to specify numbers with a precise range of magnitudes.)
    assert((min_eval_size > 0.0) && (max_eval_size > 0.0));
    Scalar log_min = std::log(std::abs(min_eval_size));
    Scalar log_max = std::log(std::abs(max_eval_size));
    Scalar log_eval = (log_min + random_real01(generator)*(log_max-log_min));
    evals[i] = std::exp(log_eval);
    // also consider both positive and negative eigenvalues:
    if (random_real01(generator) < 0.5)
      evals[i] = -evals[i];
  }

  // Does the user want us to force some of the eigenvalues to be the same?
  if (n_degeneracy > 1) {
    int *permutation = new int[n]; //a random permutation from 0...n-1
    for (int i = 0; i < n; i++)
      permutation[i] = i;
    std::shuffle(permutation, permutation+n, generator);
    for (int i = 1; i < n_degeneracy; i++) //set the first n_degeneracy to same
      evals[permutation[i]] = evals[permutation[0]];
    delete [] permutation;
  }

  // D is a diagonal matrix whose diagonal elements are the eigenvalues
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
      D[i][j] = ((i == j) ? evals[i] : 0.0);

  // Now randomly generate the (transpose of) the "evects" matrix
  GenRandOrth<Scalar, Matrix>(evects, n, generator); //(will transpose it later)

  // Construct the test matrix, M, where M = Rt * D * R

  // Original code:
  //mmult(evects, D, tmp, n);  // <--> tmp = Rt * D
  // Unfortunately, C++ guesses the types incorrectly.  Must manually specify:
  // #ifdefs making the code ugly again:
  #if defined USE_VECTOR_OF_VECTORS
  mmult<vector<vector<Scalar> >&, vector<vector<Scalar> >&>
  #elif defined USE_ARRAY_OF_ARRAYS
  mmult<array<array<Scalar,NF>,NF>&, array<array<Scalar,NF>,NF>&>
  #elif defined USE_C_FIXED_SIZE_ARRAYS
  mmult<Scalar *[NF], Scalar *[NF]>
  #else
  mmult<Scalar**, Scalar const *const *>
  #endif
       (evects, D, tmp, n);

  for (int i = 0; i < n-1; i++)
    for (int j = i+1; j < n; j++)
      std::swap(evects[i][j], evects[j][i]); //transpose "evects"

  // Original code:
  //mmult(tmp, evects, M, n);
  // Unfortunately, C++ guesses the types incorrectly.  Must manually specify:
  // #ifdefs making the code ugly again:
  #if defined USE_VECTOR_OF_VECTORS
  mmult<vector<vector<Scalar> >&, vector<vector<Scalar> >&>
  #elif defined USE_ARRAY_OF_ARRAYS
  mmult<array<array<Scalar,NF>,NF>&, array<array<Scalar,NF>,NF>&>
  #elif defined USE_C_FIXED_SIZE_ARRAYS
  mmult<Scalar *[NF], Scalar *[NF]>
  #else
  mmult<Scalar**, Scalar const *const *>
  #endif
       (tmp, evects, M, n);
  //at this point M = Rt*D*R (where "R"="evects")

  #if defined USE_C_POINTER_TO_POINTERS
  Dealloc2D(&D);
  Dealloc2D(&tmp);
  #endif
} // GenRandSymm()



template <typename Scalar>
void TestJacobi(int n, //<! matrix size
                int n_matrices=100, //<! number of matrices to test
                Scalar min_eval_size=0.1,  //<! minimum possible eigenvalue sizw
                Scalar max_eval_size=10.0, //<! maximum possible eigenvalue size
                int n_tests_per_matrix=1, //<! repeat test for benchmarking?

                int n_degeneracy=1, //<! repeated eigenvalues?
                unsigned seed=0 //<! random seed (if 0 then use the clock)
                )
{
  cout << endl << "-- Diagonalization test (real symmetric)  --" << endl;

  // construct a random generator engine using a time-based seed:

  if (seed == 0) // if the caller did not specify a seed, use the system clock
    seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::default_random_engine generator(seed);



  // Create an instance of the Jacobi diagonalizer, and allocate the matrix
  // we will test it on, as well as the arrays that will store the resulting
  // eigenvalues and eigenvectors.
  // The way we do this dependa on what version of the code we are using.
  // This is controlled by "#if defined" statements.
  
  #if defined USE_VECTOR_OF_VECTORS

  Jacobi<Scalar,
         vector<Scalar>&,
         vector<vector<Scalar> >&,
         vector<vector<Scalar> >& >
    ecalc(n);

  // allocate the matrix, eigenvalues, eigenvectors
  vector<vector<Scalar> > M(n, vector<Scalar>(n));
  vector<vector<Scalar> > evects(n, vector<Scalar>(n));
  vector<vector<Scalar> > evects_known(n, vector<Scalar>(n));
  vector<Scalar> evals(n);
  vector<Scalar> evals_known(n);
  vector<Scalar> test_evec(n);

  #elif defined USE_ARRAY_OF_ARRAYS

  n = NF;
  cout << "Testing std::array (fixed size).\n"
    "(Ignoring first argument, and setting matrix size to " << n << ")" << endl;
  Jacobi<Scalar,
         array<Scalar, NF>&,
         array<array<Scalar, NF>, NF>&,
         array<array<Scalar, NF>, NF>& >
    ecalc(n);
  // allocate the matrix, eigenvalues, eigenvectors
  array<array<Scalar, NF>, NF> M;
  array<array<Scalar, NF>, NF> evects;
  array<array<Scalar, NF>, NF> evects_known;
  array<Scalar, NF> evals;
  array<Scalar, NF> evals_known;
  array<Scalar, NF> test_evec;

  #elif defined USE_C_FIXED_SIZE_ARRAYS

  n = NF;
  cout << "Testing C fixed size arrays.\n"
    "(Ignoring first argument, and setting matrix size to " << n << ")" << endl;
  Jacobi<Scalar, Scalar*, Scalar *[NF], Scalar *[NF]> ecalc(n);
  // allocate the matrix, eigenvalues, eigenvectors
  Scalar M[NF][NF];
  Scalar evects[NF][NF];
  Scalar evects_known[NF][NF];
  Scalar evals[NF];
  Scalar evals_known[NF];
  Scalar test_evec[NF];

  #else
 
  #define USE_C_POINTER_TO_POINTERS

  // Note: Normally, you would just use this to instantiate Jacobi:
  // Jacobi<Scalar, Scalar*, Scalar**, Scalar const*const*> ecalc(n);
  // -------------------------
  // ..but since Jacobi manages its own memory using new and delete, I also want
  // to test that the copy constructors, copy operators, and destructors work.
  // The following lines do this:
  Jacobi<Scalar, Scalar*, Scalar**, Scalar const*const*> ecalc_test_mem1(n);
  Jacobi<Scalar, Scalar*, Scalar**, Scalar const*const*> ecalc_test_mem2(2);
  // test the = operator
  ecalc_test_mem2 = ecalc_test_mem1;
  // test the copy constructor
  Jacobi<Scalar, Scalar*, Scalar**, Scalar const*const*> ecalc(ecalc_test_mem2);
  // allocate the matrix, eigenvalues, eigenvectors
  Scalar **M, **evects, **evects_known;
  Alloc2D(n, n, &M);
  Alloc2D(n, n, &evects);
  Alloc2D(n, n, &evects_known);
  Scalar *evals = new Scalar[n];
  Scalar *evals_known = new Scalar[n];
  Scalar *test_evec = new Scalar[n];

  #endif


  // --------------------------------------------------------------------
  // Now, generate random matrices and test Jacobi::Diagonalize() on them.
  // --------------------------------------------------------------------

  for(int imat = 0; imat < n_matrices; imat++) {

    // Create a randomly generated symmetric matrix.
    //This function generates random numbers for the eigenvalues ("evals_known")
    //as well as the eigenvectors ("evects_known"), and uses them to generate M.

    #if defined USE_VECTOR_OF_VECTORS
    GenRandSymm<Scalar, vector<Scalar>&, vector<vector<Scalar> >&>
    #elif defined USE_ARRAY_OF_ARRAYS
    GenRandSymm<Scalar, array<Scalar,NF>&, array<array<Scalar,NF>,NF>&>
    #elif defined USE_C_FIXED_SIZE_ARRAYS
    GenRandSymm<Scalar, Scalar*, Scalar *[NF]>
    #else
    GenRandSymm<Scalar, Scalar*, Scalar**>
    #endif
               (M,
                n,
                evals_known,
                evects_known,
                generator,
                min_eval_size,
                max_eval_size,
                n_degeneracy);

    // Sort the matrix evals and eigenvector rows:
    // Original code:
    //SortRows<Scalar>(evals_known, evects_known, n);
    // Unfortunately, C++ guesses the types incorrectly. Must use #ifdefs again:
    #if defined USE_VECTOR_OF_VECTORS
    SortRows<Scalar, vector<Scalar>&, vector<vector<Scalar> >&>
    #elif defined USE_ARRAY_OF_ARRAYS
    SortRows<Scalar, array<Scalar,NF>&, array<array<Scalar,NF>,NF>&>
    #elif defined USE_C_FIXED_SIZE_ARRAYS
    SortRows<Scalar, Scalar*, Scalar *[NF]>
    #else
    SortRows<Scalar, Scalar*, Scalar**>
    #endif
            (evals_known, evects_known, n);


    if (n_matrices == 1) {
      cout << "Eigenvalues (after sorting):\n";
      for (int i = 0; i < n; i++)
        cout << evals_known[i] << " ";
      cout << "\n";
      cout << "Eigenvectors (rows) which are known in advance:\n";
      for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
          cout << evects_known[i][j] << " ";
        cout << "\n";
      }
      cout << "  (The eigenvectors calculated by Jacobi::Diagonalize() should match these.)\n";
    }

    for (int i_test = 0; i_test < n_tests_per_matrix; i_test++) {

      // Now (finally) calculate the eigenvalues and eigenvectors
      int n_sweeps = ecalc.Diagonalize(M, evals, evects);

      if ((n_matrices == 1) && (i_test == 0)) {
        cout <<"Jacobi::Diagonalize() ran for "<<n_sweeps<<" iters (sweeps).\n";
        cout << "Eigenvalues calculated by Jacobi::Diagonalize()\n";
        for (int i = 0; i < n; i++)
          cout << evals[i] << " ";
        cout << "\n";
        cout << "Eigenvectors (rows) calculated by Jacobi::Diagonalize()\n";
        for (int i = 0; i < n; i++) {
          for (int j = 0; j < n; j++)
            cout << evects[i][j] << " ";
          cout << "\n";
        }
      }
      // 
      Scalar eps=1.0e-06;
      assert(SimilarVec(evals, evals_known, n, eps*max_eval_size, eps));
      //Check that each eigenvector satisfies Mv = λv
      // <-->  Σ_b  M[a][b]*evects[i][b] = evals[i]*evects[i][b]   (for all a)
      for (int i = 0; i < n; i++) {
        for (int a = 0; a < n; a++) {
          test_evec[a] = 0.0;
          for (int b = 0; b < n; b++)
            test_evec[a] += M[a][b] * evects[i][b];
          assert(Similar(test_evec[a], evals[i] * evects[i][a], eps, eps));
        }
      }

    } //for (int i_test = 0; i_test < n_tests_per_matrix; i++)

  } //for(int imat = 0; imat < n_matrices; imat++) {

  #if defined USE_C_POINTER_TO_POINTERS
  Dealloc2D(&M);
  Dealloc2D(&evects);
  Dealloc2D(&evects_known);
  delete [] evals;
  delete [] evals_known;
  delete [] test_evec;
  #endif

} //TestJacobi()


int main(int argc, char **argv) {
  int n_size = 2;
  int n_matr = 1;
  double emin = 0.1;
  double emax = 10.0;
  int n_tests = 1;
  int n_degeneracy = 1;
  unsigned seed = 0;

  if (argc <= 1) {
    cerr <<
      "Error: This program requires at least 1 argument.\n"
      "Usage: n_size [n_matr n_tests erange n_degeneracy seed]\n"
      "       n_size  = the size of the matrices\n"
      "           (The remaining arguments are optional.)\n"
      "       n_matr  = the number of randomly generated matrices to test\n"
      "       n_tests = the number of times the eigenvalues and eigenvectors\n"
      "                 are calculated for each matrix.  By default this is 1\n"
      "                 (Increase this to at least 20 if you plan to use this\n"
      "                 program for benchmarking (speed testing), because the time\n"
      "                 needed for generating a random matrix is not negligible.)\n"
      "       min_eval = the smallest possible eigenvalue magnitude (>0, eg. 1e-05)\n"
      "       max_eval = the largest possible eigenvalue magnitude (>0 eg. 1e+05)\n"
      "  n_degeneracy = the number of repeated eigenvalues (disabled by default)\n"
      "          seed = the seed used by the random number generator.\n"
      "                 (By default, the system clock is used.)\n"
         << endl;
    return 1;
  }

  n_size = std::stoi(argv[1]);
  if (argc > 2)
    n_matr = std::stoi(argv[2]);
  if (argc > 3)
    emin = std::stof(argv[3]);
  if (argc > 4)
    emax = std::stof(argv[4]);
  if (argc > 5)
    n_degeneracy = std::stoi(argv[5]);
  if (argc > 6)
    n_tests = std::stoi(argv[6]);
  if (argc > 7)
    seed = std::stoi(argv[7]);

  TestJacobi(n_size, n_matr, emin, emax, n_tests, n_degeneracy, seed);

  cout << "test passed\n" << endl;
  return EXIT_SUCCESS;
}
