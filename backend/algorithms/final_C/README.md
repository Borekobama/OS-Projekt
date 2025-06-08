C Version für Linux.

Kompilieren: gcc -o parallel_computation parallel_computation.c -lpthread -lm

Als Koordinator ausführen: ./parallel_computation c 127.0.0.1 5000
Als Worker ausführen: ./parallel_computation w 127.0.0.1 5001 127.0.0.1 5000
