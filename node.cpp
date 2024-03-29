#include "node.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

// Cantidad total de nodos en la red
int total_nodes;

// Rank del nodo
int mpi_rank;

// Blockchain del nodo
Block *last_block_in_chain;

// DB de bloques del nodo
map<string, Block> node_blocks;

// Exclusión mutua entre recepción de bloques
// y agregado de bloques recien minados
mutex mu_node;

// Evita recibir bloques si acaba de minar uno nuevo
atomic_bool isBroadcasting;

// Dificultad de la blockchain
size_t default_defficulty;

ofstream *m_output_file;

chrono::steady_clock::time_point start_time;


// Cuando me llega una cadena adelantada, y tengo que pedir los nodos que me faltan
// Si nos separan más de VALIDATION_BLOCKS bloques de distancia entre las cadenas, se descarta por seguridad
bool verificar_y_migrar_cadena(const Block *rBlock, const MPI_Status *status) {
  // Enviar mensaje TAG_CHAIN_HASH
  MPI_Send(rBlock->block_hash, HASH_SIZE, MPI_CHAR, status->MPI_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD);

  // Recibir mensaje TAG_CHAIN_RESPONSE
  Block *blockchain = new Block[VALIDATION_BLOCKS];
  MPI_Status recv_status;
  MPI_Recv(blockchain, VALIDATION_BLOCKS, *MPI_BLOCK, status->MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD,
           &recv_status);

  int count;
  string actual_hash;

  MPI_Get_count(&recv_status, *MPI_BLOCK, &count);

  printf("[%d] Recibí %d bloques enviados por %d \n", mpi_rank, count, status->MPI_SOURCE);

  if (count <= 0)
    goto end;

  // Verificar que los bloques recibidos
  // sean válidos y se puedan acoplar a la cadena


  // El primer bloque de la lista contiene el hash pedido
  // y el mismo index que el bloque original.
  if (strcmp(rBlock->block_hash, blockchain[0].block_hash) != 0) goto end;

  block_to_hash(&(blockchain[0]), actual_hash);

  // Chequeo que el hash del primer bloque este bien
  // El hash del bloque recibido es igual al calculado
  // por la función block_to_hash.
  if(actual_hash.compare(blockchain[0].block_hash) != 0)
    goto end;

  // Cada bloque siguiente de la lista, contiene el hash
  // definido en previous_block_hash del actual elemento.
  // Cada bloque siguiente de la lista, contiene el índice
  // anterior al actual elemento.
  for (size_t i = 1; i < count; i++) {
    block_to_hash(&blockchain[i], actual_hash);
    if (actual_hash.compare(blockchain[i].block_hash) != 0 ||
        strcmp(blockchain[i-1].previous_block_hash, blockchain[i].block_hash) != 0 ||
        blockchain[i-1].index != blockchain[i].index + 1)
      goto end;

    // Si dentro de los bloques recibidos por alice alguno ya estaba
    // dentro de node_blocks (o el último tiene índice 1)
    if (node_blocks.find(blockchain[i].block_hash) != node_blocks.end() ||
        blockchain[i].index == 1) {
      int cant_blocks_added = blockchain[0].index - last_block_in_chain->index;
      for (int k = 0; k < cant_blocks_added; k++) {
        auto act_time = chrono::steady_clock::now();
        if (mpi_rank == 0)
          *m_output_file << chrono::duration <double, milli>(act_time - start_time).count() << endl;
      }

      // Agrego todos los bloques anteriores a node_blocks y marco el primero
      // como el nuevo último bloque de la cadena (last_block_in_chain).
      for (size_t j = 0; j <= i; j++)
        node_blocks[blockchain[j].block_hash] = blockchain[j];

      printf("[%d] Agregué %zu bloques de los que me mandó %d \n", mpi_rank, cant_blocks_added, status->MPI_SOURCE);

      Block *new_last_block = new Block;
      *new_last_block = blockchain[0];
      last_block_in_chain = new_last_block;

      delete[] blockchain;
      return true;
    }
  }

  // De lo contrario, descarto la cadena y los nuevos
  // bloques por seguridad.

  end:
  delete[] blockchain;
  return false;
}

bool agregar_como_ultimo_bloque(const Block *rBlock, const MPI_Status *status) {
  memcpy(last_block_in_chain, rBlock, sizeof(Block));
  auto act_time = chrono::steady_clock::now();
  if (mpi_rank == 0)
    *m_output_file << chrono::duration <double, milli>(act_time - start_time).count() << endl;
  printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
  return true;
}

// Verifica que el bloque tenga que ser incluido en la cadena, y lo agrega si corresponde
bool validate_block_for_chain(const Block *rBlock, const MPI_Status *status) {
  if (valid_new_block(rBlock)) {
    // Agrego el bloque al diccionario, aunque no
    // necesariamente eso lo agrega a la cadena
    node_blocks[string(rBlock->block_hash)] = *rBlock;

    // Si el índice del bloque recibido es 1
    // y mí último bloque actual tiene índice 0,
    //entonces lo agrego como nuevo último.
    if (last_block_in_chain->index == 0 && rBlock->index == 1)
      return agregar_como_ultimo_bloque(rBlock, status);

    // Si el índice del bloque recibido es
    // el siguiente a mí último bloque actual,
    if (rBlock->index == last_block_in_chain->index + 1) {
      // y el bloque anterior apuntado por el recibido es mí último actual,
      // entonces lo agrego como nuevo último.
      if (rBlock->previous_block_hash == last_block_in_chain->block_hash)
        return agregar_como_ultimo_bloque(rBlock, status);

      // pero el bloque anterior apuntado por el recibido no es mí último actual,
      // entonces hay una blockchain más larga que la mía.
      printf("[%d] Perdí la carrera por uno (%d) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
      return verificar_y_migrar_cadena(rBlock, status);
    }

    // Si el índice del bloque recibido es igua al índice de mi último bloque actual,
    // entonces hay dos posibles forks de la blockchain pero mantengo la mía
    if (rBlock->index == last_block_in_chain->index) {
      printf("[%d] Conflicto suave: Conflicto de branch (%d) contra %d \n", mpi_rank, rBlock->index,
             status->MPI_SOURCE);
      return false;
    }

    // Si el índice del bloque recibido es anterior al índice de mi último bloque actual,
    // entonces lo descarto porque asumo que mi cadena es la que está quedando preservada.
    if (rBlock->index == last_block_in_chain->index) {
      printf("[%d] Conflicto suave: Descarto el bloque (%d vs %d) contra %d \n", mpi_rank, rBlock->index,
             last_block_in_chain->index, status->MPI_SOURCE);
      return false;
    }

    // Si el índice del bloque recibido está más de una posición adelantada a mi último bloque actual,
    // entonces me conviene abandonar mi blockchain actual
    if (rBlock->index > last_block_in_chain->index + 1) {
      printf("[%d] Perdí la carrera por varios contra %d \n", mpi_rank, status->MPI_SOURCE);
      return verificar_y_migrar_cadena(rBlock, status);
    }
  }

  printf("[%d] Error duro: Descarto el bloque recibido de %d porque no es válido \n", mpi_rank, status->MPI_SOURCE);
  return false;
}

// Envia el bloque minado a todos los nodos
void broadcast_block(const Block *block) {
  isBroadcasting = true;
  for (uint i = 0; i < total_nodes; i++) {
    uint nodeRank = (i + mpi_rank) % total_nodes;
    if (nodeRank != mpi_rank)
      MPI_Send(block, 1, *MPI_BLOCK, nodeRank, TAG_NEW_BLOCK, MPI_COMM_WORLD);
  }
  isBroadcasting = false;
}

// Proof of work
void *proof_of_work(void *ptr) {
  string hash_hex_str;
  Block block;
  unsigned int mined_blocks = 0;
  while (true) {
    block = *last_block_in_chain;

    // Preparar nuevo bloque
    block.index += 1;
    block.node_owner_number = mpi_rank;
    block.difficulty = default_defficulty;
    block.created_at = static_cast<unsigned long int>(time(NULL));
    memcpy(block.previous_block_hash, block.block_hash, HASH_SIZE);

    // Agregar un nonce al azar al bloque para intentar resolver el problema
    gen_random_nonce(block.nonce);

    // Hashear el contenido (con el nuevo nonce)
    block_to_hash(&block, hash_hex_str);

    // Contar la cantidad de ceros iniciales (con el nuevo nonce)
    if (solves_problem(hash_hex_str, default_defficulty)) {
      mu_node.lock();
      // Verifico que no haya cambiado mientras calculaba
      if (last_block_in_chain->index < block.index) {
        auto act_time = chrono::steady_clock::now();
        if (mpi_rank == 0)
          *m_output_file << chrono::duration <double, milli>(act_time - start_time).count() << endl;
        mined_blocks += 1;
        *last_block_in_chain = block;
        strcpy(last_block_in_chain->block_hash, hash_hex_str.c_str());
        node_blocks[hash_hex_str] = *last_block_in_chain;
        printf("[%d] Agregué un bloque producido con index %d \n", mpi_rank, last_block_in_chain->index);

        broadcast_block(last_block_in_chain);
      }
      mu_node.unlock();
    }

    if (need_to_finish())
      break;
  }


  printf("[%d] Thread Minero terminó \n", mpi_rank);
  return nullptr;
}

int node(int difficulty, ofstream* outputfile) {
  start_time = chrono::steady_clock::now();

  // Tomar valor de mpi_rank y de nodos totales
  MPI_Comm_size(MPI_COMM_WORLD, &total_nodes);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

  // Guardo la difucltad de la blockchain
  default_defficulty = difficulty;

  m_output_file = outputfile;

  // La semilla de las funciones aleatorias depende del mpi_ranking
  srand(time(NULL) + mpi_rank);
  printf("[MPI] Lanzando proceso %u\n", mpi_rank);

  last_block_in_chain = new Block;

  // Inicializo el primer bloque
  last_block_in_chain->index = 0;
  last_block_in_chain->node_owner_number = mpi_rank;
  last_block_in_chain->difficulty = default_defficulty;
  last_block_in_chain->created_at = static_cast<unsigned long int>(time(NULL));
  memset(last_block_in_chain->previous_block_hash, 0, HASH_SIZE);

  // Inicializo mis estructuras
  isBroadcasting = false;

  // Creo thread para minar
  pthread_t thread;
  pthread_create(&thread, nullptr, proof_of_work, nullptr);

  // Recepcion de mensajes de pedido de cadena
  MPI_Status hashStatus;
  MPI_Request hashRequest;
  char hashbuffer[HASH_SIZE];
  int hashFlag = false;
  std::map<string, Block>::iterator found_hash;

  // Recepcion de mensajes de nuevos bloques minados en la red
  MPI_Status blockStatus;
  MPI_Request blockRequest;
  Block blockbuffer;
  Block current_block;
  int blockFlag = false;

  // Recepcion de cadena antes de empezar para no arrancar perdiendo
  MPI_Irecv(hashbuffer, HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD, &hashRequest);
  MPI_Irecv(&blockbuffer, 1, *MPI_BLOCK, MPI_ANY_SOURCE, TAG_NEW_BLOCK, MPI_COMM_WORLD, &blockRequest);

  // variable para esperar almenos tantos ciclos para terminar
  uint cant_wait = 0;

  while (cant_wait < 50000) {
    MPI_Test(&hashRequest, &hashFlag, &hashStatus);

    // Si es un mensaje de pedido de cadena,
    // responderlo enviando los bloques correspondientes
    if (hashFlag) {
      cant_wait = 0;
      found_hash = node_blocks.find(string(hashbuffer));

      Block *blocks_to_send = new Block[VALIDATION_BLOCKS];

      if (found_hash == node_blocks.end())
        goto notFound;

      current_block = found_hash->second;

      for (int i = 0; i < VALIDATION_BLOCKS && current_block.index != 0; i++) {
        blocks_to_send[i] = current_block;
        current_block = node_blocks[current_block.previous_block_hash];
      }

      MPI_Send(blocks_to_send, VALIDATION_BLOCKS, *MPI_BLOCK, hashStatus.MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD);

      notFound:
      delete[] blocks_to_send;
      hashFlag = false;

      // Vuelvo a recibir pedidos de cadena
      MPI_Irecv(hashbuffer, HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD, &hashRequest);
    }

    // Si es un mensaje de nuevo bloque, llamar a la función
    // validate_block_for_chain con el bloque recibido y el estado de MPI
    if (!isBroadcasting) {
      // Si se esta broadcasteando un nodo, evito procesar bloques enviados
      // por otros nodos hasta que termine el broadcast
      MPI_Test(&blockRequest, &blockFlag, &blockStatus);
      if (blockFlag) {
        mu_node.lock();
        validate_block_for_chain(&blockbuffer, &blockStatus);
        mu_node.unlock();

        blockFlag = false;

        // Vuelvo a recibir nuevos bloques
        MPI_Irecv(&blockbuffer, 1, *MPI_BLOCK, MPI_ANY_SOURCE, TAG_NEW_BLOCK, MPI_COMM_WORLD, &blockRequest);
      }
    }

    if (need_to_finish()) {
      cant_wait++;
    }
  }

  printf("[%d] Thread Emisor terminó \n", mpi_rank);

  pthread_join(thread, nullptr);

  delete last_block_in_chain;
  return 0;
}

bool need_to_finish() {
  return last_block_in_chain->index >= BLOCKS_TO_MINE;
}

#pragma clang diagnostic pop
