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

#include "KokkosKernels_BitUtils.hpp"

namespace KokkosKernels{

namespace Experimental{

namespace Graph{
namespace Impl{


template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename pool_memory_space, typename struct_visit_t>
struct KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
  TriangleCount{
  const nnz_lno_t numrows; //num rows in A

  const size_type *row_mapA; //A row pointers
  const nnz_lno_t *entriesA; // A column indices

  const size_type *row_pointer_begins_B;
  const size_type *row_pointer_ends_B;
  const nnz_lno_t *entriesSetIndicesB;
  const nnz_lno_t *entriesSetsB;

  size_type *rowmapC;
  nnz_lno_t* entriesC;

  const nnz_lno_t pow2_hash_size;
  const nnz_lno_t pow2_hash_func;
  const nnz_lno_t MaxRoughNonZero;

  const size_t shared_memory_size;
  const int vector_size;
  pool_memory_space m_space;
  const KokkosKernels::Experimental::Util::ExecSpaceType my_exec_space;


  const int unit_memory; //begins, nexts, and keys. No need for vals yet.
  const int suggested_team_size;
  const int thread_memory;
  nnz_lno_t shmem_key_size;
  nnz_lno_t shared_memory_hash_func;
  nnz_lno_t shmem_hash_size;
  const nnz_lno_t team_row_chunk_size;
  const int set_size;
  const int set_shift;
  const int count_or_fill_mode = 0;
  const nnz_lno_t *min_size_row_for_each_row;
  const struct_visit_t visit_applier;
  /**
   * \brief constructor
   * \param m_: input row size of A
   * \param row_mapA_: row pointers of A
   * \param entriesA_: col indices of A
   * \param row_ptr_begins_B_: beginning of the rows of B
   * \param row_ptr_ends_B_:end of the rows of B
   * \param entriesSetIndicesB_: column set indices of B
   * \param entriesSetsB_: columns sets of B
   * \param rowmapC_: output rowmap C
   * \param hash_size_: global hashmap hash size.
   * \param MaxRoughNonZero_: max flops for row.
   * \param sharedMemorySize_: shared memory size.
   * \param suggested_team_size_: suggested team size
   * \param team_row_chunk_size_: suggested team chunk size
   * \param my_exec_space_ : execution space.
   */
  TriangleCount(
      const nnz_lno_t m_,
      const size_type *row_mapA_,
      const nnz_lno_t *entriesA_,

      const size_type *row_ptr_begins_B_,
      const size_type *row_ptr_ends_B_,
      const nnz_lno_t *entriesSetIndicesB_,
      const nnz_lno_t *entriesSetsB_,

      size_type *rowmapC_,
      nnz_lno_t *entriesC_,

      const nnz_lno_t hash_size_,
      const nnz_lno_t MaxRoughNonZero_,
      const size_t sharedMemorySize_,
      const int suggested_team_size_,
      const nnz_lno_t team_row_chunk_size_,
      const int vector_size_,
      pool_memory_space mpool_,
      const KokkosKernels::Experimental::Util::ExecSpaceType my_exec_space_,
      int mode_,
      const nnz_lno_t *min_size_row_for_each_row_,
      const struct_visit_t visit_applier_,
      bool KOKKOSKERNELS_VERBOSE):
        numrows(m_),
        row_mapA (row_mapA_),
        entriesA(entriesA_),
        row_pointer_begins_B(row_ptr_begins_B_),
        row_pointer_ends_B(row_ptr_ends_B_),
        entriesSetIndicesB(entriesSetIndicesB_),
        entriesSetsB(entriesSetsB_),
        rowmapC(rowmapC_),
        entriesC(entriesC_),
        pow2_hash_size(hash_size_),
        pow2_hash_func(hash_size_ - 1),
        MaxRoughNonZero(MaxRoughNonZero_),
        shared_memory_size(sharedMemorySize_),
        vector_size (vector_size_),
        m_space(mpool_),
        my_exec_space(my_exec_space_),
        unit_memory(sizeof(nnz_lno_t) * 2 + sizeof(nnz_lno_t) * 3),
        suggested_team_size(suggested_team_size_),
        thread_memory((shared_memory_size /8 / suggested_team_size_) * 8),
        shmem_key_size(),
        shared_memory_hash_func(),
        shmem_hash_size(1),
        team_row_chunk_size(team_row_chunk_size_),
        set_size (sizeof(nnz_lno_t) * 8),
        set_shift(log(double(sizeof(nnz_lno_t) * 8)) / log(2.0) + 0.5),
        count_or_fill_mode(mode_),
        min_size_row_for_each_row(min_size_row_for_each_row_),
        visit_applier(visit_applier_)
  {

    //how many keys I can hold?
    //thread memory - 3 needed entry for size.
    shmem_key_size = ((thread_memory - sizeof(nnz_lno_t) * 4) / unit_memory);

    //put the hash size closest power of 2.
    //we round down here, because we want to store more keys,
    //conflicts are cheaper.
    while (shmem_hash_size * 2 <=  shmem_key_size){
      shmem_hash_size = shmem_hash_size * 2;
    }
    //for and opeation we get -1.
    shared_memory_hash_func = shmem_hash_size - 1;

    //increase the key size wit the left over from hash size.
    shmem_key_size = shmem_key_size + ((shmem_key_size - shmem_hash_size) ) / 4;
    //round it down to 2, because of some alignment issues.
    shmem_key_size = (shmem_key_size >> 1) << 1;

    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\tTriangleCount "
          << " thread_memory:" << thread_memory
          << " unit_memory:" << unit_memory
          << " adjusted hashsize:" << shmem_hash_size
          << " adjusted shmem_key_size:" << shmem_key_size
          << " using "<< (shmem_key_size * 4  + shmem_hash_size) * sizeof (nnz_lno_t) +    sizeof(nnz_lno_t) * 3
          << " of thread_memory: " << thread_memory
          << " set_shift:" << set_shift << " set_size:" << set_size
          << std::endl;
    }
  }

  KOKKOS_INLINE_FUNCTION
  nnz_lno_t get_thread_id(const nnz_lno_t row_index) const{
    switch (my_exec_space){
    default:
      return row_index;
#if defined( KOKKOS_HAVE_SERIAL )
    case KokkosKernels::Experimental::Util::Exec_SERIAL:
      return 0;
#endif
#if defined( KOKKOS_HAVE_OPENMP )
    case KokkosKernels::Experimental::Util::Exec_OMP:
      return Kokkos::OpenMP::hardware_thread_id();
#endif
#if defined( KOKKOS_HAVE_PTHREAD )
    case KokkosKernels::Experimental::Util::Exec_PTHREADS:
      return Kokkos::Threads::hardware_thread_id();
#endif
#if defined( KOKKOS_HAVE_QTHREAD)
    case KokkosKernels::Experimental::Util::Exec_QTHREADS:
      return Kokkos::Qthread::hardware_thread_id();
#endif
#if defined( KOKKOS_HAVE_CUDA )
    case KokkosKernels::Experimental::Util::Exec_CUDA:
      return row_index;
#endif
    }
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreDenseAccumulatorTag2&, const team_member_t & teamMember) const {


    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);

    //dense accumulators
    nnz_lno_t *indices = NULL;
    nnz_lno_t *sets = NULL;
    nnz_lno_t *sets2 = NULL;

    volatile nnz_lno_t * tmp = NULL;

    nnz_lno_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    //we need as much as column size for sets.
    sets = (nnz_lno_t *) tmp;
    tmp += MaxRoughNonZero; //this is set as column size before calling dense accumulators.

    sets2 = (nnz_lno_t *) tmp;
    tmp += MaxRoughNonZero; //this is set as column size before calling dense accumulators.


    //indices only needs max row size.
    indices = (nnz_lno_t *) tmp;


    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end),
        [&] (const nnz_lno_t& row_index) {
      nnz_lno_t insertion_count = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      nnz_lno_t num_el = 0;
      if (col_size){

        //first insert the minimum row.
        nnz_lno_t min_row_b = this->min_size_row_for_each_row[row_index];
        size_type min_row_begin = row_pointer_begins_B[min_row_b];
        nnz_lno_t min_row_left_work = row_pointer_ends_B[min_row_b] - min_row_begin;

        //traverse columns of B
        for (nnz_lno_t i = 0; i < min_row_left_work; ++i){
          const size_type adjind = i + min_row_begin;
          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          //here we assume that each element in row is unique.
          //we need to change compression so that it will always
          //return unique rows.
          indices[insertion_count++] = b_set_ind;
          sets[b_set_ind] = b_set;
          sets2[b_set_ind] = 1;
        }


        //traverse columns of A
        for (nnz_lno_t colind = 0; colind < col_size; ++colind){
          size_type a_col = colind + col_begin;


          nnz_lno_t rowB = entriesA[a_col];
          if (rowB == min_row_b) continue;
          size_type rowBegin = row_pointer_begins_B[rowB];
          nnz_lno_t left_work = row_pointer_ends_B[rowB] - rowBegin;


          //traverse columns of B
          for (nnz_lno_t i = 0; i < left_work; ++i){
            const size_type adjind = i + rowBegin;
            nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
            nnz_lno_t b_set = entriesSetsB[adjind];
            //make a intersection.
            sets[b_set_ind] = sets[b_set_ind] & b_set;
            ++sets2[b_set_ind];
          }
        }
      }

      switch (count_or_fill_mode){
      case 0: //count mode
      default:
      {
        for (nnz_lno_t ii = 0; ii < insertion_count; ++ii){
          nnz_lno_t set_ind = indices[ii];
          nnz_lno_t c_rows = sets[set_ind];
          if (sets2[set_ind] != col_size) continue;
          //count number of set bits
          /*
          nnz_lno_t num_el2 = 0;
          for (; c_rows; num_el2++) {
            c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
          }
          num_el += num_el2;
          */
          num_el += KokkosKernels::Experimental::Util::set_bit_count(c_rows);
        }
        rowmapC[row_index] = num_el;
        //std::cout << "row_index:" << row_index << " num_el:" << num_el << std::endl;
      }
      break;
      case 1: //fill mode
      {
        size_type num_el = rowmapC[row_index];

        for (nnz_lno_t ii = 0; ii < insertion_count; ++ii){
          const nnz_lno_t set_ind = indices[ii];

          if (sets2[set_ind] != col_size) continue;
          nnz_lno_t c_rows = sets[set_ind];
          const nnz_lno_t shift = set_ind << set_shift;
          //int current_row = 0;
          nnz_lno_t unit = 1;
          while (c_rows){
            int least_set = KokkosKernels::Experimental::Util::least_set_bit(c_rows) - 1;
            entriesC[num_el++] = shift + least_set;
            c_rows = c_rows & ~(unit << least_set);

            /*
            if (c_rows & unit){
              //insert indices.
              entriesC[num_el++] = shift + current_row;
            }
            current_row++;
            c_rows = c_rows & ~unit;
            unit = unit << 1;
            */
          }
        }
      }
      break;
      case 2:
      {
        for (nnz_lno_t ii = 0; ii < insertion_count; ++ii){
          const nnz_lno_t set_ind = indices[ii];

          if (sets2[set_ind] != col_size) continue;
          nnz_lno_t c_rows = sets[set_ind];
          //const nnz_lno_t shift = set_ind << set_shift;
          if (c_rows){
            visit_applier(row_index, set_ind, c_rows, tid);
          }

        }
      }
      break;

      }
    }
    );

    m_space.release_chunk(indices);
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreDenseAccumulatorTag&, const team_member_t & teamMember) const {


    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);

    //dense accumulators
    nnz_lno_t *indices = NULL;
    nnz_lno_t *sets = NULL;
    nnz_lno_t *sets2 = NULL;

    volatile nnz_lno_t * tmp = NULL;

    nnz_lno_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    //we need as much as column size for sets.
    sets = (nnz_lno_t *) tmp;
    tmp += MaxRoughNonZero; //this is set as column size before calling dense accumulators.
    sets2 = (nnz_lno_t *) tmp;
    tmp += MaxRoughNonZero; //this is set as column size before calling dense accumulators.

    //indices only needs max row size.
    indices = (nnz_lno_t *) tmp;


    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end),
        [&] (const nnz_lno_t& row_index) {
      nnz_lno_t insertion_count = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      nnz_lno_t num_el = 0;

      //traverse columns of A
      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;

        nnz_lno_t rowB = entriesA[a_col];


        size_type rowBegin = row_pointer_begins_B[rowB];
        nnz_lno_t left_work = row_pointer_ends_B[rowB] - rowBegin;

        //traverse columns of B
        for (nnz_lno_t i = 0; i < left_work; ++i){
          const size_type adjind = i + rowBegin;
          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          //if sets are not set before, add this to indices.
          if (sets[b_set_ind] == 0){
            indices[insertion_count++] = b_set_ind;
          }
          sets2[b_set_ind] = sets2[b_set_ind] | (sets[b_set_ind] & b_set);
          //make a union.
          sets[b_set_ind] = sets[b_set_ind] | b_set;
        }
      }

      switch (count_or_fill_mode){
      case 0: //count mode
      default:
      {
        for (nnz_lno_t ii = 0; ii < insertion_count; ++ii){
          nnz_lno_t set_ind = indices[ii];
          //nnz_lno_t c_rows = sets[set_ind];
          nnz_lno_t c_rows = sets2[set_ind];
          sets[set_ind] = 0;
          sets2[set_ind] = 0;

          //count number of set bits
          /*
          nnz_lno_t num_el2 = 0;
          for (; c_rows; num_el2++) {
            c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
          }
          num_el += num_el2;
          */
          num_el += KokkosKernels::Experimental::Util::set_bit_count(c_rows);
        }
        rowmapC[row_index] = num_el;
      }
      break;
      case 1: //fill mode
      {
        size_type num_el = rowmapC[row_index];

        for (nnz_lno_t ii = 0; ii < insertion_count; ++ii){
          const nnz_lno_t set_ind = indices[ii];
          const nnz_lno_t shift = set_ind << set_shift;

          //nnz_lno_t c_rows = sets[set_ind];
          nnz_lno_t c_rows = sets2[set_ind];
          sets[set_ind] = 0;
          sets2[set_ind] = 0;

          //int current_row = 0;
          nnz_lno_t unit = 1;
          while (c_rows){

            int least_set = KokkosKernels::Experimental::Util::least_set_bit(c_rows) - 1;
            entriesC[num_el++] = shift + least_set;
            c_rows = c_rows & ~(unit << least_set);
            /*
            if (c_rows & unit){
              //insert indices.
              entriesC[num_el++] = shift + current_row;
            }
            current_row++;
            c_rows = c_rows & ~unit;
            unit = unit << 1;
            */
          }
        }
      }
      break;
      case 2: //fill mode
      {
        for (nnz_lno_t ii = 0; ii < insertion_count; ++ii){
          const nnz_lno_t set_ind = indices[ii];
          //const nnz_lno_t shift = set_ind << set_shift;

          //nnz_lno_t c_rows = sets[set_ind];
          nnz_lno_t c_rows = sets2[set_ind];
          sets[set_ind] = 0;
          sets2[set_ind] = 0;

          if (c_rows){
            visit_applier(row_index, set_ind, c_rows, tid);
          }
        }
      }
      break;
      }
    }
    );

    m_space.release_chunk(indices);
  }


  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag&, const team_member_t & teamMember) const {

    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);


    //get memory from memory pool.
    volatile nnz_lno_t * tmp = NULL;
    nnz_lno_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    //set first to globally used hash indices.
    nnz_lno_t *globally_used_hash_indices = (nnz_lno_t *) tmp;
    tmp += pow2_hash_size;

    //create hashmap accumulator.
    KokkosKernels::Experimental::UnorderedHashmap::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t> hm2;

    //set memory for hash begins.
    hm2.hash_begins = (nnz_lno_t *) (tmp);
    tmp += pow2_hash_size ;

    hm2.hash_nexts = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;

    //holds the keys
    hm2.keys = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;
    hm2.values = (nnz_lno_t *) (tmp);

    //this is my values2 array. it is parallel to values.
    //currently hashmap accumulator wont use it.
    tmp += MaxRoughNonZero;
    nnz_lno_t *values2 = (nnz_lno_t *) (tmp);

    hm2.hash_key_size = pow2_hash_size;
    hm2.max_value_size = MaxRoughNonZero;

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index){
      nnz_lno_t globally_used_hash_count = 0;
      nnz_lno_t used_hash_size = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      //traverse columns of A.
      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;
        nnz_lno_t rowB = entriesA[a_col];

        size_type rowBegin = row_pointer_begins_B[rowB];
        nnz_lno_t left_work = row_pointer_ends_B[rowB] - rowBegin;

        //traverse columns of B
        for (nnz_lno_t i = 0; i < left_work; ++i){

          const size_type adjind = i + rowBegin;

          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          nnz_lno_t hash = b_set_ind & pow2_hash_func;

          //insert it to first hash.
          hm2.sequential_insert_into_hash_mergeOr_TriangleCount_TrackHashes(
              hash,
              b_set_ind, b_set, values2,
              &used_hash_size,
              hm2.max_value_size,&globally_used_hash_count,
              globally_used_hash_indices
          );
        }
      }

      switch (count_or_fill_mode){
      case 0: //count mode
      default:
      {      //when done with all insertions, traverse insertions and get the size.
        nnz_lno_t num_el = 0;
        for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
          nnz_lno_t c_rows = values2[ii];
          //nnz_lno_t num_el2 = 0;

          //the number of set bits.
          /*
          for (; c_rows; num_el2++) {
            c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
          }
          num_el += num_el2;
          */
          num_el += KokkosKernels::Experimental::Util::set_bit_count(c_rows);
        }

        //set the row size.
        rowmapC[row_index] = num_el;
      }
      break;
      case 1: //fill mode
      {
        size_type num_el = rowmapC[row_index];

        for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
          const nnz_lno_t c_rows_setind = hm2.keys[ii];
          nnz_lno_t c_rows = values2[ii];
          const nnz_lno_t shift = c_rows_setind << set_shift;

          //int current_row = 0;
          nnz_lno_t unit = 1;

          while (c_rows){

            int least_set = KokkosKernels::Experimental::Util::least_set_bit(c_rows) - 1;
            entriesC[num_el++] = shift + least_set;
            c_rows = c_rows & ~(unit << least_set);
            /*
            if (c_rows & unit){
              //insert indices.
              entriesC[num_el++] = shift + current_row;
            }
            current_row++;
            c_rows = c_rows & ~unit;
            unit = unit << 1;
            */
          }
        }
      }
      break;

      case 2: //fill mode
      {
        for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
          const nnz_lno_t c_rows_setind = hm2.keys[ii];
          nnz_lno_t c_rows = values2[ii];
          if (c_rows){
            visit_applier(row_index, c_rows_setind, c_rows, tid);
          }

        }
      }
      break;
      }

      //clear the begins.
      for (int i = 0; i < globally_used_hash_count; ++i){
        nnz_lno_t dirty_hash = globally_used_hash_indices[i];
        hm2.hash_begins[dirty_hash] = -1;
      }
    });

    m_space.release_chunk(globally_used_hash_indices);
  }



  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag2&, const team_member_t & teamMember) const {
    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);


    //get memory from memory pool.
    volatile nnz_lno_t * tmp = NULL;
    nnz_lno_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    //set first to globally used hash indices.
    nnz_lno_t *globally_used_hash_indices = (nnz_lno_t *) tmp;
    tmp += pow2_hash_size;

    //create hashmap accumulator.
    KokkosKernels::Experimental::UnorderedHashmap::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t> hm2;

    //set memory for hash begins.
    hm2.hash_begins = (nnz_lno_t *) (tmp);
    tmp += pow2_hash_size ;

    hm2.hash_nexts = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;

    //holds the keys
    hm2.keys = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;
    hm2.values = (nnz_lno_t *) (tmp);

    //this is my values2 array. it is parallel to values.
    //currently hashmap accumulator wont use it.
    tmp += MaxRoughNonZero;
    nnz_lno_t *values2 = (nnz_lno_t *) (tmp);

    hm2.hash_key_size = pow2_hash_size;
    hm2.max_value_size = MaxRoughNonZero;

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index){
      nnz_lno_t globally_used_hash_count = 0;
      nnz_lno_t used_hash_size = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      //std::cout << "row:" << row_index << std::endl;
      if (col_size){
        //first insert the minimum row.
        nnz_lno_t min_row_b = this->min_size_row_for_each_row[row_index];
        size_type min_row_begin = row_pointer_begins_B[min_row_b];
        nnz_lno_t min_row_left_work = row_pointer_ends_B[min_row_b] - min_row_begin;

        //traverse columns of B
        for (nnz_lno_t i = 0; i < min_row_left_work; ++i){
          const size_type adjind = i + min_row_begin;
          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          nnz_lno_t hash = b_set_ind & pow2_hash_func;


          //std::cout << "\t union hash:" << hash << " bset:" << b_set << " b_set_ind:" << b_set_ind << std::endl;

          //insert it to first hash.
          hm2.sequential_insert_into_hash_TriangleCount_TrackHashes(
              hash,
              b_set_ind, b_set, values2,
              &used_hash_size,
              hm2.max_value_size,
              &globally_used_hash_count,
              globally_used_hash_indices
          );
        }


        //traverse columns of A.
        for (nnz_lno_t colind = 0; colind < col_size; ++colind){
          size_type a_col = colind + col_begin;
          nnz_lno_t rowB = entriesA[a_col];
          if (rowB == min_row_b) continue;
          size_type rowBegin = row_pointer_begins_B[rowB];
          nnz_lno_t left_work = row_pointer_ends_B[rowB] - rowBegin;

          //traverse columns of B
          for (nnz_lno_t i = 0; i < left_work; ++i){

            const size_type adjind = i + rowBegin;


            nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
            nnz_lno_t b_set = entriesSetsB[adjind];
            nnz_lno_t hash = b_set_ind & pow2_hash_func;

            //std::cout << "\t and hash:" << hash << " bset:" << b_set << " b_set_ind:" << b_set_ind << std::endl;
            //insert it to first hash.
            hm2.sequential_insert_into_hash_mergeAnd_TriangleCount_TrackHashes(
                hash,
                b_set_ind, b_set, values2,
                &used_hash_size,
                hm2.max_value_size,&globally_used_hash_count,
                globally_used_hash_indices
            );
          }
        }
      }

      switch (count_or_fill_mode){
      case 0: //count mode
      default:
      {      //when done with all insertions, traverse insertions and get the size.
        nnz_lno_t num_el = 0;
        for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
          if (values2[ii] != col_size) continue;
          nnz_lno_t c_rows = hm2.values[ii];


          //the number of set bits.
          /*
          nnz_lno_t num_el2 = 0;
          for (; c_rows; num_el2++) {
            c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
          }
          num_el += num_el2;
          */
          num_el += KokkosKernels::Experimental::Util::set_bit_count(c_rows);
        }

        //set the row size.
        rowmapC[row_index] = num_el;
      }
      break;
      case 1: //fill mode
      {
        size_type num_el = rowmapC[row_index];

        for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
          if (values2[ii] != col_size) continue;
          nnz_lno_t c_rows_setind = hm2.keys[ii];
          nnz_lno_t c_rows = hm2.values[ii];
          const nnz_lno_t shift = c_rows_setind << set_shift;

          //int current_row = 0;
          nnz_lno_t unit = 1;

          while (c_rows){

            int least_set = KokkosKernels::Experimental::Util::least_set_bit(c_rows) - 1;
            entriesC[num_el++] = shift + least_set;
            c_rows = c_rows & ~(unit << least_set);
            /*
            if (c_rows & unit){
              //insert indices.
              entriesC[num_el++] = shift + current_row;
            }
            current_row++;
            c_rows = c_rows & ~unit;
            unit = unit << 1;
            */
          }
        }
      }
      break;
      case 2: //fill mode
      {
        for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
          if (values2[ii] != col_size) continue;
          nnz_lno_t c_rows_setind = hm2.keys[ii];
          nnz_lno_t c_rows = hm2.values[ii];
          if (c_rows){
            visit_applier(row_index, c_rows_setind, c_rows, tid);
          }

        }
      }
      break;


      }



      //clear the begins.
      for (int i = 0; i < globally_used_hash_count; ++i){
        nnz_lno_t dirty_hash = globally_used_hash_indices[i];
        hm2.hash_begins[dirty_hash] = -1;
      }
    });

    m_space.release_chunk(globally_used_hash_indices);
  }


  KOKKOS_INLINE_FUNCTION
  void operator()(const GPUTag&, const team_member_t & teamMember) const {

    nnz_lno_t team_row_begin = teamMember.league_rank()  * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);


    //int thread_memory = (shared_memory_size / 8 / teamMember.team_size()) * 8;
    char *all_shared_memory = (char *) (teamMember.team_shmem().get_shmem(shared_memory_size));

    //shift it to the thread private part
    all_shared_memory += thread_memory * teamMember.team_rank();

    //used_hash_sizes hold the size of 1st and 2nd level hashes
    volatile nnz_lno_t *used_hash_sizes = (volatile nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;

    nnz_lno_t *globally_used_hash_count = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;


    nnz_lno_t * begins = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shmem_hash_size;

    //poins to the next elements
    nnz_lno_t * nexts = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shmem_key_size;

    //holds the keys
    nnz_lno_t * keys = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shmem_key_size;
    nnz_lno_t * vals = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shmem_key_size;
    nnz_lno_t * vals_counts = (nnz_lno_t *) (all_shared_memory);
    nnz_lno_t * vals_counts_gmem = NULL;
    nnz_lno_t *globally_used_hash_indices = NULL;
    KokkosKernels::Experimental::UnorderedHashmap::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t>
    hm(shmem_hash_size, shmem_key_size, begins, nexts, keys, vals);

    KokkosKernels::Experimental::UnorderedHashmap::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t>
    hm2(pow2_hash_size, MaxRoughNonZero,
        NULL, NULL, NULL, NULL);
    hm2.max_value_size = MaxRoughNonZero;

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end),
        [&] (const nnz_lno_t& row_index) {

      //initialize begins.
      Kokkos::parallel_for(
          Kokkos::ThreadVectorRange(teamMember, shmem_hash_size),
          [&] (nnz_lno_t i) {begins[i] = -1; });

      //initialize hash usage sizes
      Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
        used_hash_sizes[0] = 0;
        used_hash_sizes[1] = 0;
        globally_used_hash_count[0] = 0;
      });

      bool is_global_alloced = false;
      //first we inserted the smallest size row to hashmap
      nnz_lno_t min_row_col = this->min_size_row_for_each_row[row_index];
      {
        size_type rowBegin = row_pointer_begins_B[min_row_col];
        nnz_lno_t left_work_ = row_pointer_ends_B[min_row_col] - rowBegin;

        while (left_work_){
          nnz_lno_t work_to_handle = KOKKOSKERNELS_MACRO_MIN(vector_size, left_work_);
          nnz_lno_t b_col_ind = -1;
          nnz_lno_t b_val = -1;
          nnz_lno_t hash = -1;
          int shmem_success = 0;
          Kokkos::parallel_for(
              Kokkos::ThreadVectorRange(teamMember, work_to_handle),
              [&] (nnz_lno_t i) {
            const size_type adjind = i + rowBegin;
            b_col_ind = entriesSetIndicesB[adjind];
            b_val = entriesSetsB[adjind] ;
            hash = b_col_ind & shared_memory_hash_func;
            shmem_success = 1;
          });

          int overall_num_unsuccess = 1;
          //first insert to shared memory.
          //if we run out of shared memory and allocated global memory.
          //then skip this part.
          if (!is_global_alloced){
            shmem_success = hm.vector_atomic_insert_into_hash(
                teamMember, vector_size,
                hash, b_col_ind, b_val,
                used_hash_sizes,
                shmem_key_size,
                vals_counts);

            overall_num_unsuccess = 0;
            Kokkos::parallel_reduce( Kokkos::ThreadVectorRange(teamMember, vector_size),
                [&] (const int threadid, int &overall_num_unsuccess_) {
              overall_num_unsuccess_ += shmem_success;
            }, overall_num_unsuccess);
          }

          if (overall_num_unsuccess){
            if (!is_global_alloced){

              volatile nnz_lno_t * tmp = NULL;
              nnz_lno_t tid = get_thread_id(row_index);
              //the code gets internal compiler error on gcc 4.7.2
              //assuming that this part only runs on GPUs for now, below fix
              //has the exact same behaviour and runs okay.
              //size_t tid = row_index;

              while (tmp == NULL){
                Kokkos::single(Kokkos::PerThread(teamMember),[&] (volatile nnz_lno_t * &memptr) {
                  memptr = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
                }, tmp);
              }

              is_global_alloced = true;
              globally_used_hash_indices = (nnz_lno_t *) tmp;
              tmp += pow2_hash_size ;

              hm2.hash_begins = (nnz_lno_t *) (tmp);
              tmp += pow2_hash_size ;
              hm2.hash_nexts = (nnz_lno_t *) (tmp);
              tmp += MaxRoughNonZero;
              hm2.keys= (nnz_lno_t *) (tmp);
              tmp += MaxRoughNonZero;
              hm2.values = (nnz_lno_t *) (tmp);
              tmp += MaxRoughNonZero;
              vals_counts_gmem = (nnz_lno_t *)tmp;
            }
            if (shmem_success) {
              hash = b_col_ind & pow2_hash_func;
            }

            hm2.vector_atomic_insert_into_hash_TrackHashes(
                teamMember, vector_size,
                hash,b_col_ind,b_val,
                used_hash_sizes + 1, hm2.max_value_size
                ,globally_used_hash_count, globally_used_hash_indices,
                vals_counts_gmem
            );
          }
          left_work_ -= work_to_handle;
          rowBegin += work_to_handle;
        }
      }
      //now we will insert the others, calculate intersection.
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t a_row_size = row_mapA[row_index + 1] - col_begin;
      nnz_lno_t ii = a_row_size;
      while(ii--){

        size_type a_col = col_begin + ii;
        nnz_lno_t rowB = entriesA[a_col];
        //if this is the min row, then skip.
        if (rowB == min_row_col) continue;

        size_type rowBegin = row_pointer_begins_B[rowB];
        nnz_lno_t left_work_ = row_pointer_ends_B[rowB] - rowBegin;

        while (left_work_){
          nnz_lno_t work_to_handle = KOKKOSKERNELS_MACRO_MIN(vector_size, left_work_);
          nnz_lno_t b_col_ind = -1;
          nnz_lno_t b_val = -1;
          nnz_lno_t hash = -1;

          Kokkos::parallel_for( Kokkos::ThreadVectorRange(teamMember, work_to_handle),
              [&] (nnz_lno_t i) {
            const size_type adjind = i + rowBegin;
            b_col_ind = entriesSetIndicesB[adjind];
            b_val = entriesSetsB[adjind] ;
            hash = b_col_ind & shared_memory_hash_func;
          });

          int num_unsuccess = hm.vector_atomic_insert_into_hash_mergeAND(
              teamMember, vector_size,
              hash, b_col_ind, b_val,
              vals_counts);


          if (is_global_alloced){

            int overall_num_unsuccess = 0;
            Kokkos::parallel_reduce( Kokkos::ThreadVectorRange(teamMember, vector_size),
                [&] (const int threadid, int &overall_num_unsuccess_) {
              overall_num_unsuccess_ += num_unsuccess;
            }, overall_num_unsuccess);

            if (overall_num_unsuccess){
              hash = -1;
              if (num_unsuccess) {
                hash = b_col_ind & pow2_hash_func;
              }


              hm2.vector_atomic_insert_into_hash_mergeAND(
                  teamMember, vector_size,
                  hash,b_col_ind,b_val,
                  vals_counts_gmem
              );

            }
          }
          left_work_ -= work_to_handle;
          rowBegin += work_to_handle;
        }
      }


      Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
        if (used_hash_sizes[0] > shmem_key_size) used_hash_sizes[0] = shmem_key_size;
      });

      switch (count_or_fill_mode){
      case 0: //count mode
      default:
      {
        nnz_lno_t num_el = 0;
        Kokkos::parallel_reduce(
            Kokkos::ThreadVectorRange(teamMember, used_hash_sizes[0]),
            [&] (nnz_lno_t i, nnz_lno_t &acc) {
          if (vals_counts[i] == a_row_size){
            nnz_lno_t c_rows = vals[i];
            nnz_lno_t num_el2 = 0;
            for (; c_rows; num_el2++) {
              c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
            }
            acc += num_el2;
          }
        },
        num_el);

        nnz_lno_t num_global_el = 0;
        Kokkos::parallel_reduce(
            Kokkos::ThreadVectorRange(teamMember, used_hash_sizes[1]),
            [&] (nnz_lno_t i, nnz_lno_t &num_el) {
          if (vals_counts_gmem[i] == a_row_size){
            nnz_lno_t c_rows = hm2.values[i];
            nnz_lno_t num_el2 = 0;
            for (; c_rows; num_el2++) {
              c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
            }
            num_el += num_el2;
          }
        }, num_global_el);
        rowmapC[row_index] = num_el + num_global_el;
      }
      break;
      case 1: //fill mode
      {
        size_type row_begin = rowmapC[row_index];
        nnz_lno_t num_compressed_elements = used_hash_sizes[0];
        used_hash_sizes[0] = 0;
        Kokkos::parallel_for(
            Kokkos::ThreadVectorRange(teamMember, num_compressed_elements),
            [&] (nnz_lno_t i) {
          if (vals_counts[i] == a_row_size){
            nnz_lno_t c_rows_setind = hm.keys[i];
            nnz_lno_t c_rows = hm.values[i];

            int current_row = 0;
            nnz_lno_t unit = 1;

            while (c_rows){
              if (c_rows & unit){
                size_type wind = Kokkos::atomic_fetch_add(used_hash_sizes, 1);
                entriesC[wind + row_begin] = set_size * c_rows_setind + current_row;
              }
              current_row++;
              c_rows = c_rows & ~unit;
              unit = unit << 1;
            }
          }
        });


        if (is_global_alloced){
          Kokkos::parallel_for(
              Kokkos::ThreadVectorRange(teamMember, used_hash_sizes[1]),
              [&] (nnz_lno_t i) {
            if (vals_counts_gmem[i] == a_row_size){
              nnz_lno_t c_rows_setind = hm2.keys[i];
              nnz_lno_t c_rows = hm2.values[i];
              int current_row = 0;
              nnz_lno_t unit = 1;
              while (c_rows){
                if (c_rows & unit){

                  size_type wind = Kokkos::atomic_fetch_add(used_hash_sizes, 1);
                  entriesC[wind + row_begin] = set_size * c_rows_setind + current_row;
                }
                current_row++;
                c_rows = c_rows & ~unit;
                unit = unit << 1;
              }
            }
          });
        }
      }
      break;
      }
      if (is_global_alloced){
        nnz_lno_t dirty_hashes = globally_used_hash_count[0];
        Kokkos::parallel_for(
            Kokkos::ThreadVectorRange(teamMember, dirty_hashes),
            [&] (nnz_lno_t i) {
          nnz_lno_t dirty_hash = globally_used_hash_indices[i];
          hm2.hash_begins[dirty_hash] = -1;
        });

        Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
          m_space.release_chunk(globally_used_hash_indices);
        });
      }


    });
  }

  size_t team_shmem_size (int team_size) const {
    return shared_memory_size;
  }

};



template <typename HandleType,
          typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
          typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename struct_visit_t>
void KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    triangle_count_ai(
        const int is_symbolic_or_numeric,
        const nnz_lno_t m,
        const size_type* row_mapA_,
        const nnz_lno_t * entriesA_,

        const size_type bnnz,
        const size_type * rowmapB_begins,
        const size_type * rowmapB_ends,
        const nnz_lno_t * entriesBSetIndex,
        const nnz_lno_t * entriesBSets,
        size_type * rowmapC,
        nnz_lno_t *entriesC, //null if it is symbolic, otherwise not null!
        struct_visit_t visit_applier
    ){


  const nnz_lno_t * min_result_row_for_each_row = this->handle->get_spgemm_handle()->get_min_col_of_row().data();
  nnz_lno_t max_row_size = this->handle->get_spgemm_handle()->get_max_result_nnz();

  typedef KokkosKernels::Experimental::Util::UniformMemoryPool< MyTempMemorySpace, nnz_lno_t> pool_memory_space;


  int suggested_vector_size = this->handle->get_suggested_vector_size(this->b_row_cnt, bnnz);

  //this kernel does not really work well if the vector size is less than 4.
  if (suggested_vector_size < 4 && MyEnumExecSpace == KokkosKernels::Experimental::Util::Exec_CUDA){
    if (KOKKOSKERNELS_VERBOSE) std::cout << "\tVecSize:" << suggested_vector_size << " Setting it to 4" << std::endl;
    suggested_vector_size = 4;
  }
  int suggested_team_size = this->handle->get_suggested_team_size(suggested_vector_size);
  nnz_lno_t team_row_chunk_size = this->handle->get_team_work_size(suggested_team_size,concurrency, a_row_cnt);

  nnz_lno_t dense_col_size = this->b_col_cnt / (sizeof (nnz_lno_t) * 8)+ 1;

  //round up maxNumRoughNonzeros to closest power of 2.
  nnz_lno_t min_hash_size = 1;
  while (max_row_size > min_hash_size){
    min_hash_size *= 2;
  }

  //set the chunksize.
  size_t sparse_accumulator_chunksize = min_hash_size ; //this is for used hash indices
  sparse_accumulator_chunksize += min_hash_size ; //this is for the hash begins
  sparse_accumulator_chunksize += max_row_size ; //this is for hash nexts
  sparse_accumulator_chunksize += max_row_size ; //this is for hash keys
  sparse_accumulator_chunksize += max_row_size ; //this is for hash values - 1
  sparse_accumulator_chunksize += max_row_size ; //this is for hash values - 2

  size_t dense_accumulator_chunksize = max_row_size; //this is for used keys
  dense_accumulator_chunksize += dense_col_size ; //this is for values-1
  dense_accumulator_chunksize += dense_col_size ; //this is for values-2

  //initizalize value for the mem pool
  int pool_init_val = -1;

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tDense_col_size:" << dense_col_size << " max_row_size:" << max_row_size << std::endl;
    std::cout << "\tSparse chunksize:" << sparse_accumulator_chunksize << " dense_chunksize:"
                << dense_accumulator_chunksize << " concurrency:" << concurrency << std::endl;
  }
  size_t accumulator_chunksize = sparse_accumulator_chunksize;
  bool use_dense_accumulator = false;
  if (
      (  (spgemm_algorithm == KokkosKernels::Experimental::Graph::SPGEMM_KK_TRIANGLE_DEFAULT_IA_UNION ||
          spgemm_algorithm == KokkosKernels::Experimental::Graph::SPGEMM_KK_TRIANGLE_DEFAULT ||
          spgemm_algorithm == KokkosKernels::Experimental::Graph::SPGEMM_KK_TRIANGLE_IA_DEFAULT) &&
          ((concurrency <=  sizeof (nnz_lno_t) * 8) || (dense_accumulator_chunksize < sparse_accumulator_chunksize)))
          ||
          (spgemm_algorithm == KokkosKernels::Experimental::Graph::SPGEMM_KK_TRIANGLE_DENSE_IA_UNION ||
              spgemm_algorithm == KokkosKernels::Experimental::Graph::SPGEMM_KK_TRIANGLE_DENSE ||
              spgemm_algorithm == KokkosKernels::Experimental::Graph::SPGEMM_KK_TRIANGLE_IA_DENSE)){

    use_dense_accumulator = true;
    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\tUsing Dense Accumulator instead. Sparse chunksize:" <<
          sparse_accumulator_chunksize << " dense_chunksize:" << dense_accumulator_chunksize << " concurrency:" << concurrency << std::endl;
    }
    accumulator_chunksize = dense_accumulator_chunksize;
    //if speed is set, and exec space is cpu, then  we use dense accumulators.
    //or if memspeed is set, and concurrency is not high, we use dense accumulators.
    max_row_size = dense_col_size;
    pool_init_val = 0;
  }

  nnz_lno_t num_chunks = concurrency / suggested_vector_size;
  KokkosKernels::Experimental::Util::PoolType my_pool_type = KokkosKernels::Experimental::Util::OneThread2OneChunk;
  if (MyEnumExecSpace == KokkosKernels::Experimental::Util::Exec_CUDA) {
    my_pool_type = KokkosKernels::Experimental::Util::ManyThread2OneChunk;
  }


#if defined( KOKKOS_HAVE_CUDA )
  size_t free_byte ;
  size_t total_byte ;
  cudaMemGetInfo( &free_byte, &total_byte ) ;
  size_t required_size = size_t (num_chunks) * accumulator_chunksize * sizeof(nnz_lno_t);
  if (KOKKOSKERNELS_VERBOSE)
    std::cout << "\tmempool required size:" << required_size << " free_byte:" << free_byte << " total_byte:" << total_byte << std::endl;
  if (required_size + num_chunks > free_byte){
    num_chunks = ((((free_byte - num_chunks)* 0.5) /8 ) * 8) / sizeof(nnz_lno_t) / accumulator_chunksize;
  }
  {
    nnz_lno_t min_chunk_size = 1;
    while (min_chunk_size * 2 < num_chunks) {
      min_chunk_size *= 2;
    }
    num_chunks = min_chunk_size;
  }
#endif
  if (KOKKOSKERNELS_VERBOSE){
    std::cout <<  "\tPool Size (MB):" << (num_chunks * accumulator_chunksize * sizeof(nnz_lno_t)) / 1024. / 1024. <<
        " num_chunks:" << num_chunks <<
        " chunksize:" << accumulator_chunksize << std::endl;
  }

  Kokkos::Impl::Timer timer1;
  pool_memory_space m_space(num_chunks, accumulator_chunksize, pool_init_val,  my_pool_type);
  MyExecSpace::fence();
  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tPool Alloc Time:" << timer1.seconds() << std::endl;
  }


  /*
  std::cout << "m:" << m << " bnnz:" << bnnz << std::endl;

  KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
    (row_mapA_, m+1);

  KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
    (entriesA_, this->entriesA.dimension_0());
  KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
    (rowmapB_begins, this->b_row_cnt);
  KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
      (rowmapB_ends, this->b_row_cnt);
  KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
      (entriesBSetIndex, bnnz);
  KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
        (entriesBSets, bnnz);
*/

  TriangleCount<pool_memory_space, struct_visit_t>
  sc(
      m,
      row_mapA_,
      entriesA_,

      rowmapB_begins,
      rowmapB_ends,
      entriesBSetIndex,
      entriesBSets,

      rowmapC,
      entriesC,

      min_hash_size,
      max_row_size,
      shmem_size,
      suggested_team_size,
      team_row_chunk_size,
      suggested_vector_size,
      m_space,
      MyEnumExecSpace,
      is_symbolic_or_numeric,
      min_result_row_for_each_row,
      visit_applier,
      KOKKOSKERNELS_VERBOSE);

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tTriangleCount VecS:" << suggested_vector_size <<
                  " Team::" << suggested_team_size <<
                  " Chunk:" << team_row_chunk_size <<
                  " shmem_size:" << shmem_size << std::endl;
  }

  timer1.reset();

  //nnz_lno_t runcuda = atoi(getenv("runcuda"));
  if (/*runcuda ||*/ MyEnumExecSpace == KokkosKernels::Experimental::Util::Exec_CUDA) {
    Kokkos::parallel_for( gpu_team_policy_t(m / suggested_team_size + 1 , suggested_team_size, suggested_vector_size), sc);
  }
  else {
    if (use_dense_accumulator){
      if (spgemm_algorithm ==  SPGEMM_KK_TRIANGLE_DEFAULT ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_DENSE ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_MEM
          ||
          spgemm_algorithm ==  SPGEMM_KK_TRIANGLE_DEFAULT_IA_UNION ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_MEM_IA_UNION ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_DENSE_IA_UNION){
        if (use_dynamic_schedule){
          Kokkos::parallel_for( dynamic_multicore_dense_team_count_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);

        }
        else {
          Kokkos::parallel_for( multicore_dense_team_count_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);

        }
      }
      else {
        if (use_dynamic_schedule){
          Kokkos::parallel_for( dynamic_multicore_dense_team2_count_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);

        }
        else {
          Kokkos::parallel_for( multicore_dense_team2_count_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
        }
      }
    }    
    else {

      if (spgemm_algorithm ==  SPGEMM_KK_TRIANGLE_DEFAULT ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_DENSE ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_MEM
          ||
          spgemm_algorithm ==  SPGEMM_KK_TRIANGLE_DEFAULT_IA_UNION ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_MEM_IA_UNION ||
          spgemm_algorithm == SPGEMM_KK_TRIANGLE_DENSE_IA_UNION){
        if (use_dynamic_schedule){
            Kokkos::parallel_for( dynamic_multicore_team_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
          }
          else {
            Kokkos::parallel_for( multicore_team_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
          }}
      else {
        if (use_dynamic_schedule){
          Kokkos::parallel_for( dynamic_multicore_team_policy2_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
        }
        else {
          Kokkos::parallel_for( multicore_team_policy2_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
        }
      }

    }
  }
  MyExecSpace::fence();

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tKernel time:" << timer1.seconds() << std::endl<< std::endl;
  }
}

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename c_row_view_t, typename c_lno_nnz_view_t, typename c_scalar_nnz_view_t>
void
  KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    KokkosSPGEMM_numeric_triangle(
      c_row_view_t rowmapC_,
      c_lno_nnz_view_t entriesC_,
      c_scalar_nnz_view_t valuesC_){
  this->KokkosSPGEMM_numeric_triangle_ai(rowmapC_, entriesC_);

}

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename c_row_view_t, typename c_lno_nnz_view_t>
void
  KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    KokkosSPGEMM_numeric_triangle_ai(
      c_row_view_t rowmapC_,
      c_lno_nnz_view_t entriesC_){

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tTRIANGLE GENERATION" << std::endl;
  }

  row_lno_temp_work_view_t new_row_mapB;
  nnz_lno_temp_work_view_t set_index_entries; //will be output of compress matrix.
  nnz_lno_temp_work_view_t set_entries; //will be output of compress matrix
  size_type bnnz =  set_index_entries.dimension_0();
  this->handle->get_spgemm_handle()->get_compressed_b(bnnz, new_row_mapB, set_index_entries, set_entries);


  bool compress_in_single_step = this->handle->get_spgemm_handle()->get_compression_step();

  //get pointers from views.
  size_type const *p_rowmapA = row_mapA.data();
  nnz_lno_t const *p_entriesA = entriesA.data();
  size_type const *p_rowmapB_begins = row_mapB.data();
  size_type const *p_rowmapB_ends = new_row_mapB.data();
  nnz_lno_t const *p_set_index_b = set_index_entries.data();
  nnz_lno_t const *p_set_b = set_entries.data();
  size_type *p_rowmapC = rowmapC_.data();
  nnz_lno_t *p_entriesC = entriesC_.data();

  if (!compress_in_single_step){
    //first get the max flops for a row, which will be used for max row size.
    //If we did compression in single step, row_mapB[i] points the beginning of row i,
    //and new_row_mapB[i] points to the end of row i.
    p_rowmapB_begins = new_row_mapB.data();
    p_rowmapB_ends = p_rowmapB_begins + 1;
  }


  const int is_symbolic_or_numeric = 1;
  struct dummy{

    KOKKOS_INLINE_FUNCTION
    void operator ()(const nnz_lno_t &row, const nnz_lno_t &col_set_ind, const nnz_lno_t & col_set, const nnz_lno_t &threadid) const{
    }
  } dummy;
  this->triangle_count_ai(
      is_symbolic_or_numeric,
      a_row_cnt,
      p_rowmapA, p_entriesA,
      bnnz, p_rowmapB_begins, p_rowmapB_ends,
      p_set_index_b, p_set_b,
      p_rowmapC,
      p_entriesC,
      dummy
  );
}


template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
void KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    KokkosSPGEMM_symbolic_triangle_setup(){

  nnz_lno_t n = this->row_mapB.dimension_0() - 1;
  size_type nnz = this->entriesB.dimension_0();

  //compressed b
  row_lno_temp_work_view_t new_row_mapB(Kokkos::ViewAllocateWithoutInitializing("new row map"), n+1);
  nnz_lno_temp_work_view_t set_index_entries; //will be output of compress matrix.
  nnz_lno_temp_work_view_t set_entries; //will be output of compress matrix


  if (KOKKOSKERNELS_VERBOSE) std::cout << "SYMBOLIC PHASE" << std::endl;
  if (KOKKOSKERNELS_VERBOSE) std::cout << "\tCOMPRESS MATRIX-B PHASE" << std::endl;
  //First Compress B.
  Kokkos::Impl::Timer timer1;
  bool compress_in_single_step = this->handle->get_spgemm_handle()->get_compression_step();


  /*
  std::cout << "n:" << n << " nnz:" << nnz << std::endl;
  KokkosKernels::Experimental::Util::print_1Dview(this->row_mapB);
  KokkosKernels::Experimental::Util::kk_print_1Dview(this->entriesB, false, 800);
  */
  this->compressMatrix(n, nnz,
        this->row_mapB, this->entriesB,
        new_row_mapB, set_index_entries, set_entries,
        compress_in_single_step);

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tNew Size:" << set_index_entries.dimension_0() << " old:" << this->entriesB.dimension_0()
        << " ratio:" << set_index_entries.dimension_0() / double (this->entriesB.dimension_0() )<< std::endl;
    std::cout << "\t\tCOMPRESS MATRIX-B overall time:" << timer1.seconds() << std::endl << std::endl;
  }

  timer1.reset();

  //get pointers from views.
  size_type const *p_rowmapA = row_mapA.data();
  nnz_lno_t const *p_entriesA = entriesA.data();
  size_type const *p_rowmapB_begins = row_mapB.data();
  size_type const *p_rowmapB_ends = new_row_mapB.data();

  if (!compress_in_single_step){
    //first get the max flops for a row, which will be used for max row size.
    //If we did compression in single step, row_mapB[i] points the beginning of row i,
    //and new_row_mapB[i] points to the end of row i.
    p_rowmapB_begins = new_row_mapB.data();
    p_rowmapB_ends = p_rowmapB_begins + 1;
  }

  nnz_lno_t maxNumRoughZeros = 0;
  nnz_lno_persistent_work_view_t min_result_row_for_each_row;





  if (spgemm_algorithm ==  SPGEMM_KK_TRIANGLE_DEFAULT ||
      spgemm_algorithm == SPGEMM_KK_TRIANGLE_DENSE ||
      spgemm_algorithm == SPGEMM_KK_TRIANGLE_MEM
      ||
      spgemm_algorithm ==  SPGEMM_KK_TRIANGLE_DEFAULT_IA_UNION ||
      spgemm_algorithm == SPGEMM_KK_TRIANGLE_MEM_IA_UNION ||
      spgemm_algorithm == SPGEMM_KK_TRIANGLE_DENSE_IA_UNION){
    /*
    std::cout << "calling getMaxRoughRowNNZ_p" << std::endl;
    KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
        (p_rowmapA, a_row_cnt+1);
    KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
        (p_entriesA, entriesA.dimension_0());
    KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
        (p_rowmapB_begins, this->b_row_cnt);
    KokkosKernels::Experimental::Util::print_1Dpointer<size_type, MyTempMemorySpace>
        (p_rowmapB_ends, this->b_row_cnt);
        */

    maxNumRoughZeros = this->getMaxRoughRowNNZ_p(
        a_row_cnt, entriesA.dimension_0(),
        p_rowmapA, p_entriesA,
        p_rowmapB_begins, p_rowmapB_ends);
    //max row size cannot be overeall number of columns.
    //in this case more than number of compressed columns.
    nnz_lno_t dense_col_size = this->b_col_cnt / (sizeof (nnz_lno_t) * 8)+ 1;
    maxNumRoughZeros = KOKKOSKERNELS_MACRO_MIN(dense_col_size, maxNumRoughZeros);
  }
  else {
    //std::cout << "calling getMaxRoughRowNNZIntersection_p" << std::endl;
    min_result_row_for_each_row = nnz_lno_persistent_work_view_t(
          Kokkos::ViewAllocateWithoutInitializing("Min B Row for Each A Row"), this->a_row_cnt);
    maxNumRoughZeros = this->getMaxRoughRowNNZIntersection_p(
        a_row_cnt, entriesA.dimension_0(),
        p_rowmapA, p_entriesA,
        p_rowmapB_begins, p_rowmapB_ends,
        min_result_row_for_each_row.data());
  }


  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tMax Row Flops:" << maxNumRoughZeros  << std::endl;
    std::cout << "\tMax Row Flop Calc Time:" << timer1.seconds()  << std::endl;
  }

  size_type bnnz =  set_index_entries.dimension_0();
  if (this->MyEnumExecSpace == Util::Exec_CUDA){
    KokkosKernels::Experimental::Util::kkp_reduce_diff_view
    <size_type, MyExecSpace> (this->b_row_cnt, p_rowmapB_begins, p_rowmapB_ends, bnnz);
    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\tcompressed_b_size:" << bnnz << bnnz << std::endl;
    }
  }
  this->handle->get_spgemm_handle()->set_min_col_of_row(min_result_row_for_each_row);
  this->handle->get_spgemm_handle()->set_max_result_nnz(maxNumRoughZeros);
  this->handle->get_spgemm_handle()->set_compressed_b(bnnz, new_row_mapB, set_index_entries, set_entries);
}

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename c_row_view_t>
void KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    KokkosSPGEMM_symbolic_triangle(c_row_view_t rowmapC_){
  this->KokkosSPGEMM_symbolic_triangle_setup();
  row_lno_temp_work_view_t new_row_mapB;
  nnz_lno_temp_work_view_t set_index_entries; //will be output of compress matrix.
  nnz_lno_temp_work_view_t set_entries; //will be output of compress matrix
  size_type bnnz =  set_index_entries.dimension_0();
  this->handle->get_spgemm_handle()->get_compressed_b(bnnz, new_row_mapB, set_index_entries, set_entries);


  bool compress_in_single_step = this->handle->get_spgemm_handle()->get_compression_step();

  //get pointers from views.
  size_type const *p_rowmapA = row_mapA.data();
  nnz_lno_t const *p_entriesA = entriesA.data();
  size_type const *p_rowmapB_begins = row_mapB.data();
  size_type const *p_rowmapB_ends = new_row_mapB.data();
  nnz_lno_t const *p_set_index_b = set_index_entries.data();
  nnz_lno_t const *p_set_b = set_entries.data();
  size_type *p_rowmapC = rowmapC_.data();
  //nnz_lno_t *p_entriesC = entriesC_.data();

  if (!compress_in_single_step){
    //first get the max flops for a row, which will be used for max row size.
    //If we did compression in single step, row_mapB[i] points the beginning of row i,
    //and new_row_mapB[i] points to the end of row i.
    p_rowmapB_begins = new_row_mapB.data();
    p_rowmapB_ends = p_rowmapB_begins + 1;
  }

  const int is_symbolic_or_numeric = 0;


  struct dummy{
    KOKKOS_INLINE_FUNCTION
    void operator ()(const nnz_lno_t &row, const nnz_lno_t &col_set_ind, const nnz_lno_t & col_set, const nnz_lno_t &threadid) const{
    }

  } dummy;

  this->triangle_count_ai(
      is_symbolic_or_numeric,
      a_row_cnt,
      p_rowmapA, p_entriesA,
      bnnz, p_rowmapB_begins, p_rowmapB_ends,
      p_set_index_b, p_set_b,
      p_rowmapC,
      NULL,
      dummy
  );

  KokkosKernels::Experimental::Util::kk_exclusive_parallel_prefix_sum
                <c_row_view_t, MyExecSpace>(this->a_row_cnt + 1, rowmapC_);
  MyExecSpace::fence();

  auto d_c_nnz_size = Kokkos::subview(rowmapC_, this->a_row_cnt);
  auto h_c_nnz_size = Kokkos::create_mirror_view (d_c_nnz_size);
  Kokkos::deep_copy (h_c_nnz_size, d_c_nnz_size);
  typename c_row_view_t::non_const_value_type c_nnz_size = h_c_nnz_size();
  this->handle->get_spgemm_handle()->set_c_nnz(c_nnz_size);

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\t"; KokkosKernels::Experimental::Util::kk_print_1Dview(rowmapC_, false, 20);
    std::cout << "\tSize of C found as:" << c_nnz_size << std::endl<< std::endl;
  }
}

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename visit_struct_t>
void KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    KokkosSPGEMM_generic_triangle(visit_struct_t visit_apply){
  this->KokkosSPGEMM_symbolic_triangle_setup();
  row_lno_temp_work_view_t new_row_mapB;
  nnz_lno_temp_work_view_t set_index_entries; //will be output of compress matrix.
  nnz_lno_temp_work_view_t set_entries; //will be output of compress matrix
  size_type bnnz =  set_index_entries.dimension_0();
  this->handle->get_spgemm_handle()->get_compressed_b(bnnz, new_row_mapB, set_index_entries, set_entries);

  bool compress_in_single_step = this->handle->get_spgemm_handle()->get_compression_step();

  //get pointers from views.
  size_type const *p_rowmapA = row_mapA.data();
  nnz_lno_t const *p_entriesA = entriesA.data();
  size_type const *p_rowmapB_begins = row_mapB.data();
  size_type const *p_rowmapB_ends = new_row_mapB.data();
  nnz_lno_t const *p_set_index_b = set_index_entries.data();
  nnz_lno_t const *p_set_b = set_entries.data();
  //size_type *p_rowmapC = rowmapC_.data();
  //nnz_lno_t *p_entriesC = entriesC_.data();

  if (!compress_in_single_step){
    //first get the max flops for a row, which will be used for max row size.
    //If we did compression in single step, row_mapB[i] points the beginning of row i,
    //and new_row_mapB[i] points to the end of row i.
    p_rowmapB_begins = new_row_mapB.data();
    p_rowmapB_ends = p_rowmapB_begins + 1;
  }

  const int is_symbolic_or_numeric = 2;
  this->triangle_count_ai(
      is_symbolic_or_numeric,
      a_row_cnt,
      p_rowmapA, p_entriesA,
      bnnz, p_rowmapB_begins, p_rowmapB_ends,
      p_set_index_b, p_set_b,
      NULL,
      NULL,
      visit_apply
  );
}

}
}
}
}
