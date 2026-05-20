#!/bin/bash

OUTPUT="resultados.csv"

echo "procesos,ejecucion,tiempo" > $OUTPUT

for N in 1 2 3 4
do
    echo "======================================"
    echo "Benchmark con $N procesos"
    echo "======================================"

    suma=0
    suma2=0

    for RUN in $(seq 1 10)
    do
        echo "Run $RUN"

        # Ejecutar y capturar tiempo real
        T=$(
            (/usr/bin/time -f "%e" \
            mpiexec -n $N -f ../machinefile \
            ./hough_mpi bordes_binarios.jpg \
            > /dev/null) 2>&1
        )

        echo "$N,$RUN,$T" >> $OUTPUT

        # Acumular para media y desviación
        suma=$(awk "BEGIN {print $suma + $T}")
        suma2=$(awk "BEGIN {print $suma2 + ($T * $T)}")

    done

    media=$(awk "BEGIN {print $suma / 10}")

    desviacion=$(awk "BEGIN {
        mean = $media;
        variance = ($suma2 / 10) - (mean * mean);
        print sqrt(variance);
    }")

    echo ""
    echo "Procesos: $N"
    echo "Media: $media s"
    echo "Desviacion estandar: $desviacion s"
    echo ""

done

echo "Resultados guardados en $OUTPUT"
