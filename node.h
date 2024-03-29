#ifndef NODE_H
#define NODE_H

#include <mpi.h>
#include "block.h"
#include <fstream>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <cstdlib>
#include <queue>
#include <atomic>
#include <mutex>
#include <mpi.h>
#include <map>


#define TAG_NEW_BLOCK 10
#define TAG_CHAIN_HASH 21
#define TAG_CHAIN_RESPONSE 22
#define MAX_BLOCKS 200

extern MPI_Datatype *MPI_BLOCK;

void broadcast_block(const Block *block);
void *proof_of_work(void *ptr);
int node(int difficulty, ofstream* file);
bool validate_block_for_chain(const Block *rBlock, const MPI_Status *status);
bool verificar_y_migrar_cadena(const Block *rBlock, const MPI_Status *status);
bool agregar_como_ultimo_bloque(const Block *rBlock, const MPI_Status *status);
bool need_to_finish();

#endif // NODE_H
