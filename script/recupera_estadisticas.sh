#!/bin/bash

OUTPUT="resultados.csv"

echo "procesos,ejecucion,tiempo" > $OUTPUT

for N in 1 2 3 4 5 6 7 8
do
    echo "======================================"
    echo "Benchmark con $N procesos"
    echo "======================================"

    suma=0
    suma2=0
    validos=0

    for RUN in $(seq 1 10)
    do
        echo "Run $RUN"

        # Capturar SOLO el tiempo
        T=$(
            (/usr/bin/time -f "%e" \
            mpiexec -n $N -f ../machinefile \
            ./hough_mpi ./images/bordes_binarios.jpg \
            > /dev/null) 2>&1

        )

        # Validar que sea numérico
        if [[ $T =~ ^[0-9]+(\.[0-9]+)?$ ]]; then

            echo "$N,$RUN,$T" >> $OUTPUT

            suma=$(awk "BEGIN {print $suma + $T}")
            suma2=$(awk "BEGIN {print $suma2 + ($T * $T)}")

            validos=$((validos + 1))

        else
            echo "Run $RUN falló"
        fi

    done

    if [ $validos -gt 0 ]; then

        media=$(awk "BEGIN {print $suma / $validos}")

        desviacion=$(awk "BEGIN {
            mean = $media;
            variance = ($suma2 / $validos) - (mean * mean);

            if (variance < 0)
                variance = 0;

            print sqrt(variance);
        }")

        echo ""
        echo "Procesos: $N"
        echo "Media: $media s"
        echo "Desviacion estandar: $desviacion s"
        echo ""

    else
        echo "Todos los runs fallaron"
    fi

done

echo "Resultados guardados en $OUTPUT"

