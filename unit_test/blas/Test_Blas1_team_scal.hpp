#include<gtest/gtest.h>
#include<Kokkos_Core.hpp>
#include<Kokkos_Random.hpp>
#include<KokkosBlas1_team_scal.hpp>
#include<KokkosBlas1_dot.hpp>
#include<KokkosKernels_TestUtils.hpp>

namespace Test {
  template<class ViewTypeA, class ViewTypeB, class Device>
  void impl_test_team_scal(int N) {

    typedef Kokkos::TeamPolicy<Device>        team_policy ;
    typedef typename team_policy::member_type team_member ;

    //Launch M teams of the maximum number of threads per team
    int M = 4;
    const team_policy policy( M, Kokkos::AUTO );
    const int team_data_siz = (N%M == 0)?(N/M):(N/M+1);

    typedef typename ViewTypeA::value_type ScalarA;
    typedef typename ViewTypeB::value_type ScalarB;
    typedef Kokkos::Details::ArithTraits<ScalarA> AT;

    typedef Kokkos::View<ScalarA*[2],
       typename std::conditional<
                std::is_same<typename ViewTypeA::array_layout,Kokkos::LayoutStride>::value,
                Kokkos::LayoutRight, Kokkos::LayoutLeft>::type,Device> BaseTypeA;
    typedef Kokkos::View<ScalarB*[2],
       typename std::conditional<
                std::is_same<typename ViewTypeB::array_layout,Kokkos::LayoutStride>::value,
                Kokkos::LayoutRight, Kokkos::LayoutLeft>::type,Device> BaseTypeB;


    ScalarA a(3);
    typename AT::mag_type eps = AT::epsilon()*1000;
    typename AT::mag_type zero = AT::abs( AT::zero() );
    typename AT::mag_type one = AT::abs( AT::one() );

    BaseTypeA b_x("X",N);
    BaseTypeB b_y("Y",N);
    BaseTypeB b_org_y("Org_Y",N);
    

    ViewTypeA x = Kokkos::subview(b_x,Kokkos::ALL(),0);
    ViewTypeB y = Kokkos::subview(b_y,Kokkos::ALL(),0);
    typename ViewTypeA::const_type c_x = x;
    typename ViewTypeB::const_type c_y = y;

    typename BaseTypeA::HostMirror h_b_x = Kokkos::create_mirror_view(b_x);
    typename BaseTypeB::HostMirror h_b_y = Kokkos::create_mirror_view(b_y);

    typename ViewTypeA::HostMirror h_x = Kokkos::subview(h_b_x,Kokkos::ALL(),0);
    typename ViewTypeB::HostMirror h_y = Kokkos::subview(h_b_y,Kokkos::ALL(),0);

    Kokkos::Random_XorShift64_Pool<typename Device::execution_space> rand_pool(13718);

    Kokkos::fill_random(b_x,rand_pool,ScalarA(1));
    Kokkos::fill_random(b_y,rand_pool,ScalarB(1));

    Kokkos::fence();

    Kokkos::deep_copy(b_org_y,b_y);

    Kokkos::deep_copy(h_b_x,b_x);
    Kokkos::deep_copy(h_b_y,b_y);

    ScalarA expected_result(0);
    for(int i=0;i<N;i++)
    { expected_result += ScalarB(a*h_x(i)) * ScalarB(a*h_x(i)); }

    Kokkos::parallel_for( "KokkosBlas::Test::TeamScal", policy, KOKKOS_LAMBDA ( const team_member &teamMember ) {
       const int teamId = teamMember.league_rank();
       KokkosBlas::Experimental::scal(teamMember, Kokkos::subview(y,Kokkos::make_pair(teamId*team_data_siz,(teamId < M-1)?(teamId+1)*team_data_siz:N)), a, Kokkos::subview(x,Kokkos::make_pair(teamId*team_data_siz,(teamId < M-1)?(teamId+1)*team_data_siz:N)));
    } );

    {
      ScalarB nonconst_nonconst_result = KokkosBlas::dot(y,y);
      typename AT::mag_type divisor = AT::abs(expected_result) == zero ? one : AT::abs(expected_result);
      typename AT::mag_type diff = AT::abs( nonconst_nonconst_result - expected_result )/divisor;
      EXPECT_NEAR_KK( diff, zero, eps );
    }
 
    Kokkos::deep_copy(b_y,b_org_y);
    
    Kokkos::parallel_for( "KokkosBlas::Test::TeamScal", policy, KOKKOS_LAMBDA ( const team_member &teamMember ) {
       const int teamId = teamMember.league_rank();
       KokkosBlas::Experimental::scal(teamMember, Kokkos::subview(y,Kokkos::make_pair(teamId*team_data_siz,(teamId < M-1)?(teamId+1)*team_data_siz:N)), a, Kokkos::subview(c_x,Kokkos::make_pair(teamId*team_data_siz,(teamId < M-1)?(teamId+1)*team_data_siz:N)));
    } );

    {
      ScalarB const_nonconst_result = KokkosBlas::dot(y,y);
      typename AT::mag_type divisor = AT::abs(expected_result) == zero ? one : AT::abs(expected_result);
      typename AT::mag_type diff = AT::abs( const_nonconst_result - expected_result )/divisor;
      EXPECT_NEAR_KK( diff, zero, eps );
    }
  }

  template<class ViewTypeA, class ViewTypeB, class Device>
  void impl_test_team_scal_mv(int N, int K) {

    typedef Kokkos::TeamPolicy<Device>        team_policy ;
    typedef typename team_policy::member_type team_member ;

    //Launch K teams of the maximum number of threads per team
    const team_policy policy( K, Kokkos::AUTO );

    typedef typename ViewTypeA::value_type ScalarA;
    typedef typename ViewTypeB::value_type ScalarB;
    typedef Kokkos::Details::ArithTraits<ScalarA> AT;

    typedef multivector_layout_adapter<ViewTypeA> vfA_type;
    typedef multivector_layout_adapter<ViewTypeB> vfB_type;

    typename vfA_type::BaseType b_x("A",N,K);
    typename vfB_type::BaseType b_y("B",N,K);
    typename vfB_type::BaseType b_org_y("B",N,K);

    ViewTypeA x = vfA_type::view(b_x);
    ViewTypeB y = vfB_type::view(b_y);

    typedef multivector_layout_adapter<typename ViewTypeA::HostMirror> h_vfA_type;
    typedef multivector_layout_adapter<typename ViewTypeB::HostMirror> h_vfB_type;

    typename h_vfA_type::BaseType h_b_x = Kokkos::create_mirror_view(b_x);
    typename h_vfB_type::BaseType h_b_y = Kokkos::create_mirror_view(b_y);

    typename ViewTypeA::HostMirror h_x = h_vfA_type::view(h_b_x);
    typename ViewTypeB::HostMirror h_y = h_vfB_type::view(h_b_y);

    Kokkos::Random_XorShift64_Pool<typename Device::execution_space> rand_pool(13718);

    Kokkos::fill_random(b_x,rand_pool,ScalarA(1));
    Kokkos::fill_random(b_y,rand_pool,ScalarB(1));

    Kokkos::fence();

    Kokkos::deep_copy(b_org_y,b_y);

    Kokkos::deep_copy(h_b_x,b_x);
    Kokkos::deep_copy(h_b_y,b_y);

    ScalarA a(3);
    typename ViewTypeA::const_type c_x = x;

    ScalarA* expected_result = new ScalarA[K];
    for(int j=0;j<K;j++) {
      expected_result[j] = ScalarA();
      for(int i=0;i<N;i++)
      { expected_result[j] += ScalarB(a*h_x(i,j)) * ScalarB(a*h_x(i,j)); }
    }

    typename AT::mag_type eps = AT::epsilon()*1000;
    typename AT::mag_type zero = AT::abs( AT::zero() );
    typename AT::mag_type one = AT::abs( AT::one() );

    Kokkos::View<ScalarB*,Kokkos::HostSpace> r("Dot::Result",K);

    Kokkos::parallel_for( "KokkosBlas::Test::TeamScal", policy, KOKKOS_LAMBDA ( const team_member &teamMember ) {
       const int teamId = teamMember.league_rank();
       KokkosBlas::Experimental::scal(teamMember, Kokkos::subview(y,Kokkos::ALL(),teamId), a, Kokkos::subview(x,Kokkos::ALL(),teamId));
    } );

    KokkosBlas::dot(r,y,y);
    Kokkos::fence();
    for(int k=0;k<K;k++) {
      ScalarA nonconst_scalar_result = r(k);
      typename AT::mag_type divisor = AT::abs(expected_result[k]) == zero ? one : AT::abs(expected_result[k]);
      typename AT::mag_type diff = AT::abs( nonconst_scalar_result - expected_result[k] )/divisor;
      EXPECT_NEAR_KK( diff, zero, eps );
    }

    Kokkos::deep_copy(b_y,b_org_y);

    Kokkos::parallel_for( "KokkosBlas::Test::TeamScal", policy, KOKKOS_LAMBDA ( const team_member &teamMember ) {
       const int teamId = teamMember.league_rank();
       KokkosBlas::Experimental::scal(teamMember, Kokkos::subview(y,Kokkos::ALL(),teamId), a, Kokkos::subview(c_x,Kokkos::ALL(),teamId));
    } );

    KokkosBlas::dot(r,y,y);
    Kokkos::fence();
    for(int k=0;k<K;k++) {
      ScalarA const_scalar_result = r(k);
      typename AT::mag_type divisor = AT::abs(expected_result[k]) == zero ? one : AT::abs(expected_result[k]);
      typename AT::mag_type diff = AT::abs( const_scalar_result - expected_result[k] )/divisor;
      EXPECT_NEAR_KK( diff, zero, eps );
    }

    // Generate 'params' view with dimension == number of multivectors; each entry will be different scalar to scale y
    Kokkos::View<ScalarA*,Device> params("Params",K);
    for(int j=0; j<K; j++) {
      Kokkos::View<ScalarA,Device> param_j(params,j);
      Kokkos::deep_copy(param_j,ScalarA(3+j));
    }

    // Update expected_result for next 3 vector tests
    for(int j=0;j<K;j++) {
      expected_result[j] = ScalarA();
      for(int i=0;i<N;i++)
      { expected_result[j] += ScalarB((3.0+j)*h_x(i,j)) * ScalarB((3.0+j)*h_x(i,j)); }
    }

    Kokkos::parallel_for( "KokkosBlas::Test::TeamScal", policy, KOKKOS_LAMBDA ( const team_member &teamMember ) {
       const int teamId = teamMember.league_rank();
       KokkosBlas::Experimental::scal(teamMember, Kokkos::subview(y,Kokkos::ALL(),teamId), params(teamId), Kokkos::subview(x,Kokkos::ALL(),teamId));
    } );

    KokkosBlas::dot(r,y,y);
    Kokkos::fence();
    for(int k=0;k<K;k++) {
      ScalarA nonconst_vector_result = r(k);
      typename AT::mag_type divisor = AT::abs(expected_result[k]) == zero ? one : AT::abs(expected_result[k]);
      typename AT::mag_type diff = AT::abs( nonconst_vector_result - expected_result[k] )/divisor;
      EXPECT_NEAR_KK( diff, zero, eps );
    }

    Kokkos::deep_copy(b_y,b_org_y);
    
    Kokkos::parallel_for( "KokkosBlas::Test::TeamScal", policy, KOKKOS_LAMBDA ( const team_member &teamMember ) {
       const int teamId = teamMember.league_rank();
       KokkosBlas::Experimental::scal(teamMember, Kokkos::subview(y,Kokkos::ALL(),teamId), params(teamId), Kokkos::subview(c_x,Kokkos::ALL(),teamId));
    } );

    KokkosBlas::dot(r,y,y);
    Kokkos::fence();
    for(int k=0;k<K;k++) {
      ScalarA const_vector_result = r(k);
      typename AT::mag_type divisor = AT::abs(expected_result[k]) == zero ? one : AT::abs(expected_result[k]);
      typename AT::mag_type diff = AT::abs( const_vector_result - expected_result[k] )/divisor;
      EXPECT_NEAR_KK( diff, zero, eps );
    }

    delete [] expected_result;
  }
}



template<class ScalarA, class ScalarB, class Device>
int test_team_scal() {

#if defined(KOKKOSKERNELS_INST_LAYOUTLEFT) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
  typedef Kokkos::View<ScalarA*, Kokkos::LayoutLeft, Device> view_type_a_ll;
  typedef Kokkos::View<ScalarB*, Kokkos::LayoutLeft, Device> view_type_b_ll;
  Test::impl_test_team_scal<view_type_a_ll, view_type_b_ll, Device>(0);
  Test::impl_test_team_scal<view_type_a_ll, view_type_b_ll, Device>(13);
  Test::impl_test_team_scal<view_type_a_ll, view_type_b_ll, Device>(124);
  //Test::impl_test_team_scal<view_type_a_ll, view_type_b_ll, Device>(132231);
#endif

#if defined(KOKKOSKERNELS_INST_LAYOUTRIGHT) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
  typedef Kokkos::View<ScalarA*, Kokkos::LayoutRight, Device> view_type_a_lr;
  typedef Kokkos::View<ScalarB*, Kokkos::LayoutRight, Device> view_type_b_lr;
  Test::impl_test_team_scal<view_type_a_lr, view_type_b_lr, Device>(0);
  Test::impl_test_team_scal<view_type_a_lr, view_type_b_lr, Device>(13);
  Test::impl_test_team_scal<view_type_a_lr, view_type_b_lr, Device>(124);
  //Test::impl_test_team_scal<view_type_a_lr, view_type_b_lr, Device>(132231);
#endif

#if defined(KOKKOSKERNELS_INST_LAYOUTSTRIDE) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
  typedef Kokkos::View<ScalarA*, Kokkos::LayoutStride, Device> view_type_a_ls;
  typedef Kokkos::View<ScalarB*, Kokkos::LayoutStride, Device> view_type_b_ls;
  Test::impl_test_team_scal<view_type_a_ls, view_type_b_ls, Device>(0);
  Test::impl_test_team_scal<view_type_a_ls, view_type_b_ls, Device>(13);
  Test::impl_test_team_scal<view_type_a_ls, view_type_b_ls, Device>(124);
  //Test::impl_test_team_scal<view_type_a_ls, view_type_b_ls, Device>(132231);
#endif

#if !defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS)
  Test::impl_test_team_scal<view_type_a_ls, view_type_b_ll, Device>(124);
  Test::impl_test_team_scal<view_type_a_ll, view_type_b_ls, Device>(124);
#endif

  return 1;
}

template<class ScalarA, class ScalarB, class Device>
int test_team_scal_mv() {

#if defined(KOKKOSKERNELS_INST_LAYOUTLEFT) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
  typedef Kokkos::View<ScalarA**, Kokkos::LayoutLeft, Device> view_type_a_ll;
  typedef Kokkos::View<ScalarB**, Kokkos::LayoutLeft, Device> view_type_b_ll;
  Test::impl_test_team_scal_mv<view_type_a_ll, view_type_b_ll, Device>(0,5);
  Test::impl_test_team_scal_mv<view_type_a_ll, view_type_b_ll, Device>(13,5);
  Test::impl_test_team_scal_mv<view_type_a_ll, view_type_b_ll, Device>(124,5);
  //Test::impl_test_team_scal_mv<view_type_a_ll, view_type_b_ll, Device>(132231,5);
#endif

#if defined(KOKKOSKERNELS_INST_LAYOUTRIGHT) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
  typedef Kokkos::View<ScalarA**, Kokkos::LayoutRight, Device> view_type_a_lr;
  typedef Kokkos::View<ScalarB**, Kokkos::LayoutRight, Device> view_type_b_lr;
  Test::impl_test_team_scal_mv<view_type_a_lr, view_type_b_lr, Device>(0,5);
  Test::impl_test_team_scal_mv<view_type_a_lr, view_type_b_lr, Device>(13,5);
  Test::impl_test_team_scal_mv<view_type_a_lr, view_type_b_lr, Device>(124,5);
  //Test::impl_test_team_scal_mv<view_type_a_lr, view_type_b_lr, Device>(132231,5);
#endif

#if defined(KOKKOSKERNELS_INST_LAYOUTSTRIDE) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
  typedef Kokkos::View<ScalarA**, Kokkos::LayoutStride, Device> view_type_a_ls;
  typedef Kokkos::View<ScalarB**, Kokkos::LayoutStride, Device> view_type_b_ls;
  Test::impl_test_team_scal_mv<view_type_a_ls, view_type_b_ls, Device>(0,5);
  Test::impl_test_team_scal_mv<view_type_a_ls, view_type_b_ls, Device>(13,5);
  Test::impl_test_team_scal_mv<view_type_a_ls, view_type_b_ls, Device>(124,5);
  //Test::impl_test_team_scal_mv<view_type_a_ls, view_type_b_ls, Device>(132231,5);
#endif

#if !defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS)
  Test::impl_test_team_scal_mv<view_type_a_ls, view_type_b_ll, Device>(124,5);
  Test::impl_test_team_scal_mv<view_type_a_ll, view_type_b_ls, Device>(124,5);
#endif

  return 1;
}

#if defined(KOKKOSKERNELS_INST_FLOAT) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
TEST_F( TestCategory, team_scal_float ) {
    test_team_scal<float,float,TestExecSpace> ();
}
TEST_F( TestCategory, team_scal_mv_float ) {
    test_team_scal_mv<float,float,TestExecSpace> ();
}
#endif

#if defined(KOKKOSKERNELS_INST_DOUBLE) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
TEST_F( TestCategory, team_scal_double ) {
    test_team_scal<double,double,TestExecSpace> ();
}
TEST_F( TestCategory, team_scal_mv_double ) {
    test_team_scal_mv<double,double,TestExecSpace> ();
}
#endif

#if defined(KOKKOSKERNELS_INST_COMPLEX_DOUBLE) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
TEST_F( TestCategory, team_scal_complex_double ) {
    test_team_scal<Kokkos::complex<double>,Kokkos::complex<double>,TestExecSpace> ();
}
TEST_F( TestCategory, team_scal_mv_complex_double ) {
    test_team_scal_mv<Kokkos::complex<double>,Kokkos::complex<double>,TestExecSpace> ();
}
#endif

#if defined(KOKKOSKERNELS_INST_INT) || (!defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS))
TEST_F( TestCategory, team_scal_int ) {
    test_team_scal<int,int,TestExecSpace> ();
}
TEST_F( TestCategory, team_scal_mv_int ) {
    test_team_scal_mv<int,int,TestExecSpace> ();
}
#endif

#if !defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS)
TEST_F( TestCategory, team_scal_double_int ) {
    test_team_scal<double,int,TestExecSpace> ();
}
TEST_F( TestCategory, team_scal_double_mv_int ) {
    test_team_scal_mv<double,int,TestExecSpace> ();
}
#endif
