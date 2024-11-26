#include "../cxxopts.hpp"
#include <algorithm> // std::max, std::min
#include <iostream>
#include <mpi.h>

#include "lcs_distributed.h"

/**
 * Distributed LCS. This version divides the score matrix into blocks of
 * columns and assigns each block to a process.
 *
 * Example (Numbers represent process ranks):
  ```
  [ 0 0 1 1 2 2 ]
  [ 0 0 1 1 2 2 ]
  [ 0 0 1 1 2 2 ]
  [ 0 0 1 1 2 2 ]
  [ 0 0 1 1 2 2 ]
  [ 0 0 1 1 2 2 ]
  ```
 * Each process then traverses its sub-matrix in diagonal-major order.
 *
 * With this task mapping, each process only depends on data from the process
 * to its left, which makes communication quite simple. When computing an entry
 * in a process' leftmost column, it must receive the data from the entry in
 * the same row but rightmost column of the left neighboring process. After
 * computing an entry in the rightmost column, the data from that entry must
 * be sent to the neighboring process to the right.
 *
 * If the specific longest common subsequence is required, then the sub-matrices
 * can be gathered together once all of the entries have been computed. (This
 * could probably be optimized by performing the trace-back locally and then
 * sending the indices + subsequence string to the left neighboring process
 * instead of sending the entire sub-matrices over the network.).
 *
 * If only the length of the longest common subsequence is required, then this
 * gathering step can be skipped.
 *
 * */
class LCSDistributedColumn : public LongestCommonSubsequenceDistributed
{
protected:
  int **local_matrix;
  int **global_matrix;

  /* Need to keep track of this info globally for MPI_Gatherv(). */
  int *start_cols;
  int *sub_str_widths;

  std::string global_sequence_b;

  void computeDiagonal(int diagonal_index)
  {
    Pair diagonal_start = getDiagonalStart(diagonal_index);
    int i = diagonal_start.first;
    int j = diagonal_start.second;
    int max_i = matrix_height - 1;
    int min_j = 1;
    int comm_value;
    while (i <= max_i && j >= min_j)
    {
      /* If we are computing a cell in the leftmost column of our local matrix,
      then we need to get data from the cells in the rightmost column of our
      neighboring process to the left. Unless we are the leftmost process. */
      if (j == min_j && world_rank != 0)
      {
        MPI_Recv(
            &comm_value,
            1, // Only need a single value.
            MPI_UNSIGNED,
            world_rank - 1, // Source: Get from neighbor to the left.
            i,              // Tag: Row index.
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);
        // Store the value in the local matrix.
        matrix[i][j - 1] = comm_value;
      }

      computeCell(i, j);
      // matrix[i][j] = ++count;

      /* If we are computing a cell in the rightmost column of our local
      matrix, we must send the results to our neighbor to the right once we
      are done. Unless we are the rightmost process. */
      if (j == matrix_width - 1 && world_rank != world_size - 1)
      {
        comm_value = matrix[i][j];
        MPI_Send(
            &comm_value,
            1,
            MPI_UNSIGNED,
            world_rank + 1, // Destination: Send to neighbor to the right.
            i,
            MPI_COMM_WORLD);
      }

      i++; // Go down by one.
      j--; // Go left by one.
    }
  }

  /** Gathers the sub-matrices from each process together in the root process. */
  void gather()
  {
    printPerProcessMatrix();

    int global_matrix_width = global_sequence_b.length() + 1;

    // Gather all of the data into the root process:
    if (world_rank == 0)
    {

      printf("Copy main to local Complete!\n");
      // Allocate space for the combined matrix.
      global_matrix = new int *[matrix_height];
      for (int row = 0; row < matrix_height; row++)
      {
        global_matrix[row] = new int[global_matrix_width];
        // Fill first column with zeros.
        global_matrix[row][0] = 0;
      }
      for (int col = 0; col < global_matrix_width; col++)
      {
        // Fill first row with zeros.
        global_matrix[0][col] = 0;
      }

      printf("Initialize global Complete!\n");
      /* Sub-matrices may be of different widths, and because the matrix is
      divided column-wise, passing the entire sub-matrix with MPI_Gather would
      not properly order the combined matrix.

      However, if we call MPI_Gatherv for each row, the resultant ordering
      should be correct. */
      // int *recv_buffer = new int[global_matrix_width - 1];

      // We can skip the first row since it is all zeros.
      for (int row = 1; row < matrix_height; row++)
      {
        MPI_Gatherv(
            matrix[row] + 1,
            sub_str_widths[0],
            MPI_INT,
            global_matrix[row] + 1,
            sub_str_widths,
            start_cols,
            MPI_INT,
            0, // Root process.
            MPI_COMM_WORLD);
      }

      sequence_b = global_sequence_b;
      length_b = sequence_b.length();
      /* Use local matrix to keep track of the processes' computed sub matrix. */
      local_matrix = matrix;
      matrix = global_matrix;
      global_matrix = nullptr;
      matrix_width = global_matrix_width;
      max_length = std::min(length_a, length_b);
    }
    else
    {
      // We can skip the first row since it is all zeros.
      for (int row = 1; row < matrix_height; row++)
      {
        MPI_Gatherv(
            matrix[row] + 1,
            sub_str_widths[world_rank],
            MPI_INT,
            nullptr,
            sub_str_widths,
            start_cols,
            MPI_INT,
            0, // Root process.
            MPI_COMM_WORLD);
      }
    }
  }

  virtual void solve() override
  {

    /* Determine number of diagonals in sub-matrix. */
    int n_diagonals = length_b + length_a - 1;
    for (int diagonal = 0; diagonal < n_diagonals; diagonal++)
    {
      computeDiagonal(diagonal);
    }

    gather();

    if (world_rank == 0)
    {
      printMatrix();
      determineLongestCommonSubsequence();
    }
  }

public:
  LCSDistributedColumn(
      const std::string &sequence_a,
      const std::string &sequence_b,
      const int world_size,
      const int world_rank,
      const std::string &global_sequence_b,
      int *start_cols,
      int *sub_str_widths)
      : LongestCommonSubsequenceDistributed(sequence_a, sequence_b, world_size, world_rank),
        global_sequence_b(global_sequence_b),
        start_cols(start_cols),
        sub_str_widths(sub_str_widths)
  {
    global_matrix = nullptr;
    local_matrix = nullptr;
    this->solve();
  }

  virtual ~LCSDistributedColumn()
  {
    if (global_matrix)
    {
      for (int row = 0; row < matrix_height; row++)
      {
        delete[] global_matrix[row];
      }
      delete[] global_matrix;
    }
    if (local_matrix)
    {
      for (int row = 0; row < matrix_height; row++)
      {
        delete[] local_matrix[row];
      }
      delete[] local_matrix;
    }
  }

  void printPerProcessMatrix()
  {
    for (int rank = 0; rank < world_size; rank++)
    {
      if (rank == world_rank)
      {
        std::cout << "\nRank: " << world_rank << "\n";
        printMatrix();
      }
      MPI_Barrier(MPI_COMM_WORLD);
    }
  }
};

int main(int argc, char *argv[])
{
  std::string sequence_a = "dlrkgcqiuyh";
  std::string sequence_b = "drfghjkfdsz";

  MPI_Init(NULL, NULL);

  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  if (world_rank == 0)
  {
    std::cout << "Sequence A: " << sequence_a << std::endl;
    std::cout << "Sequence B: " << sequence_b << std::endl;
  }
  MPI_Barrier(MPI_COMM_WORLD);

  int length_a = sequence_a.length();
  int length_b = sequence_b.length();

  const int min_n_cols_per_process = length_b / world_size;
  const int excess = length_b % world_size;

  /* We need to keep track of which columns are mapped to which processes so
  we can gather them together again at the end with MPI_Gatherv.*/
  int *sub_str_widths = new int[world_size];
  int *start_cols = new int[world_size];
  for (int rank = 0; rank < world_size; rank++)
  {
    int start_col, n_cols;
    n_cols = min_n_cols_per_process;
    if (rank < excess)
    {
      start_col = rank * (min_n_cols_per_process + 1);
      n_cols++;
    }
    else
    {
      start_col = (rank * min_n_cols_per_process) + excess;
    }

    start_cols[rank] = start_col;
    sub_str_widths[rank] = n_cols;
  }

  int start_col = start_cols[world_rank];
  int n_cols = sub_str_widths[world_rank];

  // Divide up sequence B.
  std::string local_sequence_b = sequence_b.substr(start_col, n_cols);

  for (int rank = 0; rank < world_size; rank++)
  {
    if (rank == world_rank)
    {
      std::cout << "\nRank: " << world_rank << " | start_col: " << start_col
                << " | end_col: " << start_col + n_cols - 1 << "\n | local sequence B: "
                << local_sequence_b << std::endl;
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  LCSDistributedColumn lcs(
      sequence_a,
      local_sequence_b,
      world_size,
      world_rank,
      sequence_b,
      start_cols,
      sub_str_widths);

  // Print solution.
  if (world_rank == 0)
  {
    lcs.print();
  }

  delete[] sub_str_widths;
  delete[] start_cols;

  MPI_Finalize();

  return 0;
}
