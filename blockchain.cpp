#include "node.h"
#include <mpi.h>
#include <fstream>
#include <string.h>

using namespace std;

//using MPI::COMM_WORLD; using MPI::ANY_SOURCE; using MPI::ANY_TAG;
//using MPI::INT; using MPI::CHAR; using MPI::BOOL;
//using MPI::Status;

// Variables de MPI
MPI_Datatype *MPI_BLOCK;

string FILE_OUTPUT_PREFIX = "blockchain_exp_";

int main(int argc, char **argv) {
  // Inicializo MPI
  int provided;
  int status = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  if (status != MPI_SUCCESS) {
    fprintf(stderr, "Error de MPI al inicializar.\n");
    MPI_Abort(MPI_COMM_WORLD, status);
  }

  //Defino un nuevo tipo de datos de MPI para Block: MPI_BLOCK
  MPI_BLOCK = new MPI_Datatype;
  define_block_data_type_for_MPI(MPI_BLOCK);

  // Control del buffering: sin buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // Dificultad de la blockchain
  int difficulty = argc > 1 ? atoi(argv[1]) : 18;
  int total_nodes;
  MPI_Comm_size(MPI_COMM_WORLD, &total_nodes);


  string file_name = FILE_OUTPUT_PREFIX + to_string(total_nodes) + "_" + to_string(difficulty) + ".csv";

  //Output file
  ofstream outputFile;
  outputFile.open(file_name, fstream::in | fstream::out | fstream::trunc);
  printf("------------------------------------------\n%s %s\n", to_string(total_nodes).c_str(), to_string(difficulty).c_str());

  auto startTrain = chrono::steady_clock::now();
  auto endTrain = chrono::steady_clock::now();

  //Llama a la función que maneja cada nodo
  node(difficulty, &outputFile);

  // Limpio MPI
  MPI_Finalize();
  delete MPI_BLOCK;

  outputFile.close();
  return 0;
}
