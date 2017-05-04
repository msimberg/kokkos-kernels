/*
//@HEADER
// ************************************************************************
//
//               KokkosKernels 0.9: Linear Algebra and Graph Kernels
//                 Copyright 2017 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Siva Rajamanickam (srajama@sandia.gov)
//
// ************************************************************************
//@HEADER
*/
#ifndef _KOKKOS_TRIANGLE_HPP
#define _KOKKOS_TRIANGLE_HPP
#include "KokkosKernels_IOUtils.hpp"
#include "KokkosKernels_Handle.hpp"
#include "KokkosKernels_SPGEMM_impl.hpp"
namespace KokkosKernels{

namespace Experimental{

namespace Graph{
template <typename KernelHandle,
typename alno_row_view_t_,
typename alno_nnz_view_t_,
typename blno_row_view_t_,
typename blno_nnz_view_t_,
typename clno_row_view_t_>
void triangle_count(
    KernelHandle *handle,
    typename KernelHandle::nnz_lno_t m,
    typename KernelHandle::nnz_lno_t n,
    typename KernelHandle::nnz_lno_t k,
    alno_row_view_t_ row_mapA,
    alno_nnz_view_t_ entriesA,
    bool transposeA,
    blno_row_view_t_ row_mapB,
    blno_nnz_view_t_ entriesB,
    bool transposeB,
    clno_row_view_t_ row_mapC){

  typedef typename KernelHandle::SPGEMMHandleType spgemmHandleType;
  spgemmHandleType *sh = handle->get_spgemm_handle();
  switch (sh->get_algorithm_type()){
  case SPGEMM_KK_TRIANGLE_LL:
  {
    KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
    <KernelHandle,
    alno_row_view_t_, alno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t,
    blno_row_view_t_, blno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t>
    kspgemm (handle,m,n,k,row_mapA, entriesA, transposeA, row_mapB, entriesB, transposeB);
    kspgemm.KokkosSPGEMM_symbolic_triangle(row_mapC);
  }
  break;

  case SPGEMM_KK_TRIANGLE_DEFAULT:
  case SPGEMM_KK_TRIANGLE_DENSE:
  case SPGEMM_KK_TRIANGLE_MEM:
  case SPGEMM_KK_TRIANGLE_IA_DEFAULT:
  case SPGEMM_KK_TRIANGLE_IA_DENSE:
  case SPGEMM_KK_TRIANGLE_IA_MEM:
  default:
  {
    KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
    <KernelHandle,
    alno_row_view_t_, alno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t,
    blno_row_view_t_, blno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t>
    kspgemm (handle,m,n,k,row_mapA, entriesA, transposeA, row_mapB, entriesB, transposeB);
    kspgemm.KokkosSPGEMM_symbolic_triangle(row_mapC);
  }
  break;


  }
  sh->set_call_symbolic();

}


template <typename KernelHandle,
typename alno_row_view_t_,
typename alno_nnz_view_t_,
typename blno_row_view_t_,
typename blno_nnz_view_t_,
typename clno_row_view_t_,
typename clno_nnz_view_t_>
void triangle_enumerate(
    KernelHandle *handle,
    typename KernelHandle::nnz_lno_t m,
    typename KernelHandle::nnz_lno_t n,
    typename KernelHandle::nnz_lno_t k,
    alno_row_view_t_ row_mapA,
    alno_nnz_view_t_ entriesA,

    bool transposeA,
    blno_row_view_t_ row_mapB,
    blno_nnz_view_t_ entriesB,
    bool transposeB,
    clno_row_view_t_ row_mapC,
    clno_nnz_view_t_ &entriesC1,
    clno_nnz_view_t_ &entriesC2 = NULL
){


  typedef typename KernelHandle::SPGEMMHandleType spgemmHandleType;
  spgemmHandleType *sh = handle->get_spgemm_handle();
  if (!sh->is_symbolic_called()){
    triangle_count<KernelHandle,
    alno_row_view_t_, alno_nnz_view_t_,
    blno_row_view_t_, blno_nnz_view_t_,
    clno_row_view_t_>(
        handle, m, n, k,
        row_mapA, entriesA, transposeA,
        row_mapB, entriesB, transposeB,
        row_mapC
    );

    typename clno_row_view_t_::value_type c_nnz_size = handle->get_spgemm_handle()->get_c_nnz();
    if (c_nnz_size){
      entriesC1 = clno_nnz_view_t_ (Kokkos::ViewAllocateWithoutInitializing("entriesC"), c_nnz_size);
      //entriesC2 = clno_nnz_view_t_ (Kokkos::ViewAllocateWithoutInitializing("entriesC"), c_nnz_size);
    }
  }


  switch (sh->get_algorithm_type()){
  default:

  case SPGEMM_KK_TRIANGLE_DEFAULT:
  case SPGEMM_KK_TRIANGLE_DENSE:
  case SPGEMM_KK_TRIANGLE_MEM:
  case SPGEMM_KK_TRIANGLE_IA_DEFAULT:
  case SPGEMM_KK_TRIANGLE_IA_DENSE:
  case SPGEMM_KK_TRIANGLE_IA_MEM:
  {
    KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
    <KernelHandle,
    alno_row_view_t_, alno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t,
    blno_row_view_t_, blno_nnz_view_t_,  typename KernelHandle::in_scalar_nnz_view_t>
    kspgemm (handle,m,n,k,row_mapA, entriesA, transposeA, row_mapB, entriesB, transposeB);
    kspgemm.KokkosSPGEMM_numeric_triangle(row_mapC, entriesC1, entriesC2);
  }
  break;
  }
}

template <typename KernelHandle,
typename alno_row_view_t_,
typename alno_nnz_view_t_,
typename blno_row_view_t_,
typename blno_nnz_view_t_,
typename visit_struct_t>
void triangle_generic(
    KernelHandle *handle,
    typename KernelHandle::nnz_lno_t m,
    typename KernelHandle::nnz_lno_t n,
    typename KernelHandle::nnz_lno_t k,
    alno_row_view_t_ row_mapA,
    alno_nnz_view_t_ entriesA,
    bool transposeA,
    blno_row_view_t_ row_mapB,
    blno_nnz_view_t_ entriesB,
    bool transposeB,
    visit_struct_t visit_struct){

  typedef typename KernelHandle::SPGEMMHandleType spgemmHandleType;
  spgemmHandleType *sh = handle->get_spgemm_handle();
  switch (sh->get_algorithm_type()){
  case SPGEMM_KK_TRIANGLE_LL:
  case SPGEMM_KK_TRIANGLE_DEFAULT:
  case SPGEMM_KK_TRIANGLE_DENSE:
  case SPGEMM_KK_TRIANGLE_MEM:
  case SPGEMM_KK_TRIANGLE_IA_DEFAULT:
  case SPGEMM_KK_TRIANGLE_IA_DENSE:
  case SPGEMM_KK_TRIANGLE_IA_MEM:
  default:
  {
    KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
    <KernelHandle,
    alno_row_view_t_, alno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t,
    blno_row_view_t_, blno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t>
    kspgemm (handle,m,n,k,row_mapA, entriesA, transposeA, row_mapB, entriesB, transposeB);
    kspgemm.KokkosSPGEMM_generic_triangle(visit_struct);
  }
  break;


  }
}

template <typename KernelHandle,
typename alno_row_view_t_,
typename alno_nnz_view_t_,
typename visit_struct_t>
void triangle_generic(
    KernelHandle *handle,
    typename KernelHandle::nnz_lno_t m,
    alno_row_view_t_ row_mapA,
    alno_nnz_view_t_ entriesA,
    visit_struct_t visit_struct){

  typedef typename KernelHandle::nnz_lno_t nnz_lno_t;
  typedef typename KernelHandle::size_type size_type;

  typedef typename KernelHandle::SPGEMMHandleType spgemmHandleType;
  typedef typename KernelHandle::nnz_lno_persistent_work_view_t nnz_lno_persistent_work_view_t;
  typedef typename KernelHandle::row_lno_persistent_work_view_t row_lno_persistent_work_view_t;

  typedef typename KernelHandle::HandleExecSpace ExecutionSpace;


  spgemmHandleType *sh = handle->get_spgemm_handle();
  Kokkos::Impl::Timer timer1;

  //////SORT BASE ON THE SIZE OF ROWS/////
  bool sort_lower_triangle = sh->get_sort_lower_triangular();
  if (sort_lower_triangle){

    if(sh->get_lower_triangular_permutation().data() == NULL){
      nnz_lno_persistent_work_view_t new_indices(Kokkos::ViewAllocateWithoutInitializing("new_indices"), m);
      bool sort_decreasing_order = true;
      ////If true we place the largest row to top, so that largest row size will be minimized in lower triangle.
      if (sh->get_algorithm_type() == SPGEMM_KK_TRIANGLE_DEFAULT ||
          sh->get_algorithm_type() == SPGEMM_KK_TRIANGLE_DENSE ||
          sh->get_algorithm_type() == SPGEMM_KK_TRIANGLE_MEM){
        sort_decreasing_order = false;
        //if false we place the largest row to bottom, so that largest column is minimizedin lower triangle.
      }
      KokkosKernels::Experimental::Util::kk_sort_by_row_size<size_type, nnz_lno_t, ExecutionSpace>(
          m, row_mapA.data(), new_indices.data(), sort_decreasing_order);
      sh->set_lower_triangular_permutation(new_indices);
    }
  }
  if (handle->get_verbose()){
    std::cout << "Preprocess Sorting Time:" << timer1.seconds() << std::endl;
  }
  //////SORT BASE ON THE SIZE OF ROWS/////

  /////////CREATE LOWER TRIANGLE///////
  bool create_lower_triangular = sh->get_create_lower_triangular();
  row_lno_persistent_work_view_t lower_triangular_matrix_rowmap;
  nnz_lno_persistent_work_view_t lower_triangular_matrix_entries;
  timer1.reset();
  if (create_lower_triangular){
    sh->get_lower_triangular_matrix(lower_triangular_matrix_rowmap, lower_triangular_matrix_entries);
    if( lower_triangular_matrix_rowmap.data() == NULL ||
        lower_triangular_matrix_entries.data() == NULL){

      alno_nnz_view_t_ null_values;
      nnz_lno_persistent_work_view_t new_indices = sh->get_lower_triangular_permutation();

      KokkosKernels::Experimental::Util::kk_get_lower_triangle
      <alno_row_view_t_, alno_nnz_view_t_, alno_nnz_view_t_,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, alno_nnz_view_t_,
      nnz_lno_persistent_work_view_t, ExecutionSpace> (
          m,
          row_mapA, entriesA, null_values,
          lower_triangular_matrix_rowmap, lower_triangular_matrix_entries, null_values,
          new_indices, handle->is_dynamic_scheduling()
      );
      sh->set_lower_triangular_matrix(lower_triangular_matrix_rowmap, lower_triangular_matrix_entries);
    }
  }
  if (handle->get_verbose()){
    std::cout << "Preprocess Create Lower Triangular Time:" << timer1.seconds() << std::endl;
  }

  /////////CREATE LOWER TRIANGLE///////

  ////
  ///CREATE INCIDENCE MATRIX
  ///
  timer1.reset();
  row_lno_persistent_work_view_t incidence_transpose_rowmap;
  nnz_lno_persistent_work_view_t incidence_transpose_entries;

  row_lno_persistent_work_view_t incidence_rowmap;
  nnz_lno_persistent_work_view_t incidence_entries;
  switch (sh->get_algorithm_type()){

  //IF it is one of below, we perform I^T x (A) or (L).
  //so create the transpose of I.
  case SPGEMM_KK_TRIANGLE_IA_DEFAULT:
  case SPGEMM_KK_TRIANGLE_IA_DENSE:
  case SPGEMM_KK_TRIANGLE_IA_MEM:
  case SPGEMM_KK_TRIANGLE_DEFAULT_IA_UNION:
  case SPGEMM_KK_TRIANGLE_DENSE_IA_UNION:
  case SPGEMM_KK_TRIANGLE_MEM_IA_UNION:

  {
    //these are the algorithms that requires transpose of the incidence matrix.
    sh->get_lower_triangular_matrix(lower_triangular_matrix_rowmap, lower_triangular_matrix_entries);

    if( lower_triangular_matrix_rowmap.data() == NULL ||
        lower_triangular_matrix_entries.data() == NULL){
      std::cout << "Creating lower triangular A" << std::endl;

      alno_nnz_view_t_ null_values;
      nnz_lno_persistent_work_view_t new_indices = sh->get_lower_triangular_permutation();

      KokkosKernels::Experimental::Util::kk_get_lower_triangle
      <alno_row_view_t_, alno_nnz_view_t_, alno_nnz_view_t_,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, alno_nnz_view_t_,
      nnz_lno_persistent_work_view_t, ExecutionSpace> (
          m,
          row_mapA, entriesA, null_values,
          lower_triangular_matrix_rowmap, lower_triangular_matrix_entries, null_values,
          new_indices, handle->is_dynamic_scheduling()
      );
    }
    KokkosKernels::Experimental::Util::kk_create_incidence_tranpose_matrix_from_lower_triangle
    <alno_row_view_t_, alno_nnz_view_t_,
    row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t,
    ExecutionSpace>
        (m,
        lower_triangular_matrix_rowmap, lower_triangular_matrix_entries,
        incidence_transpose_rowmap, incidence_transpose_entries,
        handle->is_dynamic_scheduling());
  }
  break;

  //IF it is one of below, we perform (A) or (L) x I
  //so create I.
  case SPGEMM_KK_TRIANGLE_DEFAULT:
  case SPGEMM_KK_TRIANGLE_DENSE:
  case SPGEMM_KK_TRIANGLE_MEM:
  {
    //these are the algorithms that requires the incidence matrix.

    KokkosKernels::Experimental::Util::kk_create_incidence_matrix_from_original_matrix
            < alno_row_view_t_, alno_nnz_view_t_,
              row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t,
              ExecutionSpace>
            (m,
                row_mapA, entriesA,
            incidence_rowmap, incidence_entries, sh->get_lower_triangular_permutation(),
            handle->is_dynamic_scheduling());
  }
  break;
  default:
  case SPGEMM_KK_TRIANGLE_LL:
  {
    break;
  }
  }

  if (handle->get_verbose()){
    std::cout << "Preprocess Incidence Matrix Create Time:" << timer1.seconds() << std::endl;
  }
  ////
  ///CREATE INCIDENCE MATRIX END
  ///


  switch (sh->get_algorithm_type()){
  default:
  case SPGEMM_KK_TRIANGLE_LL:
  {
    break;
  }
  case SPGEMM_KK_TRIANGLE_DEFAULT:
  case SPGEMM_KK_TRIANGLE_DENSE:
  case SPGEMM_KK_TRIANGLE_MEM:

  {
    if (create_lower_triangular){
      KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
      <KernelHandle,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, typename KernelHandle::in_scalar_nnz_view_t,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, typename KernelHandle::in_scalar_nnz_view_t>
      kspgemm (handle,m,m,incidence_entries.dimension_0() / 2,
          lower_triangular_matrix_rowmap, lower_triangular_matrix_entries,
          false, //transpose ignore.
          incidence_rowmap, incidence_entries,
          false);
      kspgemm.KokkosSPGEMM_generic_triangle(visit_struct);
    }
    else {
      KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
      <KernelHandle,
      alno_row_view_t_, alno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, typename KernelHandle::in_scalar_nnz_view_t>
      kspgemm (handle,m,m,incidence_entries.dimension_0() / 2,
          row_mapA, entriesA,
          false, //transpose ignore.
          incidence_rowmap, incidence_entries,
          false);
      kspgemm.KokkosSPGEMM_generic_triangle(visit_struct);
    }
  }

  break;
  case SPGEMM_KK_TRIANGLE_IA_DEFAULT:
  case SPGEMM_KK_TRIANGLE_IA_DENSE:
  case SPGEMM_KK_TRIANGLE_IA_MEM:
  case SPGEMM_KK_TRIANGLE_DEFAULT_IA_UNION:
  case SPGEMM_KK_TRIANGLE_DENSE_IA_UNION:
  case SPGEMM_KK_TRIANGLE_MEM_IA_UNION:

  {

    if (create_lower_triangular){

      KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
      <KernelHandle,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, typename KernelHandle::in_scalar_nnz_view_t,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, typename KernelHandle::in_scalar_nnz_view_t>
      kspgemm (handle,
          incidence_transpose_rowmap.dimension_0() - 1,m,m,
          incidence_transpose_rowmap, incidence_transpose_entries,
          false, //transpose ignore.
          lower_triangular_matrix_rowmap, lower_triangular_matrix_entries,
          false);
      kspgemm.KokkosSPGEMM_generic_triangle(visit_struct);
    }
    else {


      KokkosKernels::Experimental::Graph::Impl::KokkosSPGEMM
      <KernelHandle,
      row_lno_persistent_work_view_t, nnz_lno_persistent_work_view_t, typename KernelHandle::in_scalar_nnz_view_t,
      alno_row_view_t_, alno_nnz_view_t_, typename KernelHandle::in_scalar_nnz_view_t>
      kspgemm (handle,
          incidence_transpose_rowmap.dimension_0() - 1,m,m,
          incidence_transpose_rowmap, incidence_transpose_entries,
          false, //transpose ignore.
          row_mapA, entriesA,
          false);
      kspgemm.KokkosSPGEMM_generic_triangle(visit_struct);
    }
  }
  break;


  }

}

}
}
}
#endif
