#!/bin/bash
export LC_NUMERIC=C   # Fuerza punto decimal en todo el script

OUTPUT="resultados.csv"
IMAGEN="./images/bordes_binarios.jpg"

echo "procesos,ejecucion,tiempo" > "$OUTPUT"

for N in 1 2 3 4 5 6 7 8
do
    echo "======================================"
    echo "Benchmark con $N procesos"
    echo "======================================"

    # Arrays para almacenar tiempos (opcional, usaremos acumuladores)
    suma=0
    suma2=0
    count=0

    for RUN in $(seq 1 10)   # Cambia a 10 para medición real, o déjalo variable
    do
        echo "Run $RUN"

        # Captura de tiempo (funciona con punto decimal)
        T=$(/usr/bin/time -f "%e" \
            mpiexec -n ../machinefile ./hough_mpi "$IMAGEN" \
            2>&1 > /dev/null)

        # Validar que T es un número (puede incluir punto)
        if [[ ! $T =~ ^[0-9]+\.?[0-9]*$ ]]; then
            echo "ADVERTENCIA: Tiempo inválido: '$T'"
            continue
        fi

        echo "Tiempo: $T s"
        echo "$N,$RUN,$T" >> "$OUTPUT"

        # Acumular (usar bc o awk; aquí usamos bc con punto decimal seguro)
        suma=$(echo "$suma + $T" | bc -l)
        suma2=$(echo "$suma2 + ($T * $T)" | bc -l)
        count=$((count + 1))
    done

    if [ $count -eq 0 ]; then
        echo "No hubo ejecuciones válidas para N=$N"
        continue
    fi

    # Calcular media y desviación con bc (usando count real)
    media=$(echo "scale=6; $suma / $count" | bc -l)
    var=$(echo "scale=6; ($suma2 / $count) - ($media * $media)" | bc -l)
    # sqrt no negativo (por si error numérico)
    desviacion=$(echo "scale=6; sqrt($var)" | bc -l)

    echo ""
    echo "Procesos: $N"
    printf "Media: %.4f s\n" "$media"
    printf "Desviacion estandar: %.4f s\n" "$desviacion"
    echo ""
done

echo "Resultados guardados en $OUTPUT"
