# TP2 - Algoritmos en sistemas distribuidos

Implementción de blockchain.

## Requisitos

- [OpenMPI](https://www.open-mpi.org/)

## Build
```
make
```

## Run

```
make run
```

O directo con MPI<sup>1</sup>:
```
mpiexec --oversubscribe -np CANT_NODOS ./blockchain DIFFICULTY
```

- `CANT_NODOS`: cantidad de nodos en la red.
- `DIFFICULTY`: dificultad de la blockchain. Default: `18`.

Save logs in file:

```
mpiexec --oversubscribe -np CANT_NODOS ./blockchain | tee OUT_FILE
```

## Referencias

1. https://github.com/open-mpi/ompi/issues/3133#issuecomment-338961555
