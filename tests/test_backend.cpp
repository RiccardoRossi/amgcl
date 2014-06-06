#define BOOST_TEST_MODULE MatrixCRS
#include <set>

#include <boost/test/unit_test.hpp>
#include <boost/random.hpp>
#include <boost/foreach.hpp>
#include <boost/mpl/list.hpp>

#include <amgcl/backend/builtin.hpp>
#include <amgcl/backend/block_crs.hpp>

#ifdef AMGCL_HAVE_EIGEN
#include <amgcl/backend/eigen.hpp>
#endif

#ifdef AMGCL_HAVE_VEXCL
#include <amgcl/backend/vexcl.hpp>
#endif

#ifdef AMGCL_HAVE_VIENNACL
#include <amgcl/backend/viennacl.hpp>
#endif

typedef boost::mpl::list<
    amgcl::backend::block_crs<float>
    , amgcl::backend::block_crs<double>
#ifdef AMGCL_HAVE_EIGEN
    , amgcl::backend::eigen<float>
    , amgcl::backend::eigen<double>
#endif
#ifdef AMGCL_HAVE_VIENNACL
    , amgcl::backend::viennacl< viennacl::compressed_matrix<float> >
    , amgcl::backend::viennacl< viennacl::compressed_matrix<double> >
    , amgcl::backend::viennacl< viennacl::ell_matrix<float> >
    , amgcl::backend::viennacl< viennacl::ell_matrix<double> >
    , amgcl::backend::viennacl< viennacl::hyb_matrix<float> >
    , amgcl::backend::viennacl< viennacl::hyb_matrix<double> >
#endif
    > cpu_backends;

#ifdef AMGCL_HAVE_VEXCL
typedef boost::mpl::list<
    amgcl::backend::vexcl<float>,
    amgcl::backend::vexcl<double>
    > vexcl_backends;
#endif

template <typename P, typename C, typename V>
void random_problem(size_t n, size_t m, size_t nnz_per_row,
        std::vector<P> &ptr,
        std::vector<C> &col,
        std::vector<V> &val,
        std::vector<V> &vec
        )
{
    ptr.reserve(n + 1);
    col.reserve(nnz_per_row * n);

    boost::random::mt19937 rng;
    boost::random::uniform_int_distribution<C> random_column(0, m - 1);

    ptr.push_back(0);
    for(size_t i = 0; i < n; i++) {
        std::set<C> cs;
        for(size_t j = 0; j < nnz_per_row; ++j)
            cs.insert(random_column(rng));

        BOOST_FOREACH(C c, cs) col.push_back(c);

        ptr.push_back(static_cast<P>( col.size() ));
    }

    boost::random::uniform_real_distribution<V> random_value(0, 1);

    val.resize( col.size() );
    BOOST_FOREACH(V &v, val) v = random_value(rng);

    vec.resize(n);
    BOOST_FOREACH(V &v, vec) v = random_value(rng);
}

template <class Backend>
void test_backend(typename Backend::params const prm = typename Backend::params())
{
    typedef typename Backend::value_type V;
    typedef typename Backend::index_type I;

    typedef typename Backend::matrix  matrix;
    typedef typename Backend::vector  vector;

    typedef amgcl::backend::crs<V, I> ref_matrix;

    const size_t n = 256;

    std::vector<I> ptr;
    std::vector<I> col;
    std::vector<V> val;
    std::vector<V> vec;

    random_problem(n, n, 16, ptr, col, val, vec);

    boost::shared_ptr<ref_matrix> Aref = boost::make_shared<ref_matrix>(n, n, ptr, col, val);
    boost::shared_ptr<matrix>     Atst = Backend::copy_matrix(Aref, prm);

    boost::shared_ptr<vector> x = Backend::copy_vector(vec, prm);
    boost::shared_ptr<vector> y = Backend::create_vector(n, prm);

    amgcl::backend::clear(*y);
    for(size_t i = 0; i < n; ++i)
        BOOST_CHECK_EQUAL(static_cast<V>((*y)[i]), V());

    std::vector<V> y_ref(n, 0);

    amgcl::backend::spmv(1, *Aref, vec, 1, y_ref);
    amgcl::backend::spmv(1, *Atst, *x,  1, *y);

    for(size_t i = 0; i < n; ++i)
        BOOST_CHECK_CLOSE(static_cast<V>((*y)[i]), y_ref[i], 1e-4);

    amgcl::backend::residual(*y, *Atst, *x, *y);
    BOOST_CHECK_SMALL(amgcl::backend::norm(*y), static_cast<V>(1e-4));
}

BOOST_AUTO_TEST_SUITE( backend_ops )

BOOST_AUTO_TEST_CASE_TEMPLATE(CpuBackends, Backend, cpu_backends)
{
    test_backend<Backend>();
}

#ifdef AMGCL_HAVE_VEXCL
BOOST_AUTO_TEST_CASE_TEMPLATE(VexCLBackends, Backend, vexcl_backends)
{
    vex::Context ctx( vex::Filter::Env && vex::Filter::DoublePrecision );
    std::cout << ctx << std::endl;

    typename Backend::params prm;
    prm.q = ctx;

    test_backend<Backend>(prm);
}
#endif

BOOST_AUTO_TEST_SUITE_END()