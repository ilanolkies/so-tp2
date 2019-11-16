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
mpiexec --oversubscribe -np 4 ./blockchain
```

## Referencias

1. https://github.com/open-mpi/ompi/issues/3133#issuecomment-338961555
