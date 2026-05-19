#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdint>  // Para uint16_t, uint32_t
#include <opencv2/opencv.hpp>
#include <mpi.h>

using namespace std;
using namespace cv;

/**
 * Estructura para almacenar los parámetros de una línea detectada
 */
struct Linea {
    double rho;
    double theta;
    int votos;
};

/**
 * Clase para calcular la transformada de Hough en paralelo con MPI
 * Optimizada usando uint16_t para TODOS los acumuladores y coordenadas
 */
class HoughTransformMPI {
private:
    int rank;           // ID del proceso
    int num_procs;      // Número total de procesos
    
    // Parámetros de la imagen
    int height, width;
    int diag_len;
    int n_rho, n_theta;
    int rho_offset;
    
    // Parámetros de la transformada
    double theta_res;
    int rho_res;
    
    // Umbrales y ventanas
    int umbral_votos;
    int ventana_x, ventana_y;
    int dist_min_rho, dist_min_theta;
    
    // Acumulador local (usando uint16_t)
    vector<uint16_t> acumulador_local;
    
public:
    /**
     * Constructor
     */
    HoughTransformMPI(int argc, char** argv) {
        MPI_Init(&argc, &argv);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
        
        // Parámetros por defecto
        rho_res = 1;
        theta_res = M_PI / 180.0/10.0;
        umbral_votos = 1450;
        ventana_x = 51;
        ventana_y = 7;
        dist_min_rho = 60;
        dist_min_theta = 4;
    }
    
    ~HoughTransformMPI() {
        MPI_Finalize();
    }
    
    /**
     * Imprime mensaje solo en el proceso maestro (rank 0)
     */
    void print_master(const string& mensaje) {
        if (rank == 0) {
            cout << mensaje << endl;
        }
    }
    
    /**
     * Procesa la imagen en paralelo con MPI optimizado (uint16_t global)
     */
    Mat procesar_imagen_paralelo(const string& archivo) {
        Mat bordes;
        vector<Point> puntos_borde_global;
        
        // El proceso 0 lee la imagen y distribuye los puntos de borde
        if (rank == 0) {
            bordes = imread(archivo, IMREAD_GRAYSCALE);
            if (bordes.empty()) {
                print_master("Error: No se pudo cargar la imagen");
                MPI_Abort(MPI_COMM_WORLD, 1);
                return Mat();
            }
            
            height = bordes.rows;
            width = bordes.cols;
            diag_len = (int)sqrt(height*height + width*width);
            n_theta = (int)(M_PI / theta_res);
            n_rho = 2 * diag_len + 1;
            rho_offset = diag_len;
            
            print_master("=== INFORMACIÓN DE LA IMAGEN ===");
            print_master("Dimensiones: " + to_string(height) + " x " + to_string(width));
            print_master("Diagonal: " + to_string(diag_len));
            print_master("Acumulador: " + to_string(n_rho) + " x " + to_string(n_theta));
            print_master("Procesos disponibles: " + to_string(num_procs));
            
            size_t memoria_acumulador = (size_t)n_rho * n_theta * sizeof(uint16_t);
            print_master("Memoria por acumulador: " + to_string(memoria_acumulador / (1024*1024)) + " MB");
            print_master("Memoria total (todos procesos): " + to_string(memoria_acumulador * num_procs / (1024*1024)) + " MB");
            
            // Encontrar todos los puntos de borde
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    if (bordes.at<uchar>(y, x) > 0) {
                        puntos_borde_global.push_back(Point(x, y));
                    }
                }
            }
            
            print_master("Puntos de borde encontrados: " + to_string(puntos_borde_global.size()));
            
            // Verificar que el máximo de votos esperado no exceda uint16_t
            int max_votos_esperado = puntos_borde_global.size();
            if (max_votos_esperado > 65535) {
                cout << "ADVERTENCIA: Máximo de votos esperado (" << max_votos_esperado 
                     << ") excede 65535. Podría haber desbordamiento de uint16_t." << endl;
                cout << "Considere usar uint32_t si esto ocurre." << endl;
            }
        }
        
        // Broadcast de parámetros de la imagen
        MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&diag_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&n_rho, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&n_theta, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&rho_offset, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        // Distribuir puntos de borde entre procesos usando uint16_t
        int num_puntos_global = puntos_borde_global.size();
        MPI_Bcast(&num_puntos_global, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        // Calcular cuántos puntos recibe cada proceso
        int puntos_por_proc = num_puntos_global / num_procs;
        int puntos_extra = num_puntos_global % num_procs;
        
        vector<int> counts(num_procs), displacements(num_procs);
        int offset = 0;
        for (int i = 0; i < num_procs; i++) {
            counts[i] = (puntos_por_proc + (i < puntos_extra ? 1 : 0)) * 2; // *2 porque cada punto tiene x,y
            displacements[i] = offset;
            offset += counts[i];
        }
        
        // Preparar buffer para enviar puntos usando uint16_t
        vector<uint16_t> puntos_buffer_global;
        if (rank == 0) {
            puntos_buffer_global.resize(num_puntos_global * 2);
            for (int i = 0; i < num_puntos_global; i++) {
                // Verificar que las coordenadas quepan en uint16_t
                if (puntos_borde_global[i].x > 65535 || puntos_borde_global[i].y > 65535) {
                    cerr << "Error: Coordenada excede el límite de uint16_t" << endl;
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                puntos_buffer_global[i*2] = (uint16_t)puntos_borde_global[i].x;
                puntos_buffer_global[i*2 + 1] = (uint16_t)puntos_borde_global[i].y;
            }           
        }
        
        // Cada proceso recibe su porción de puntos como uint16_t
        vector<uint16_t> puntos_buffer_local(counts[rank]);
        MPI_Scatterv(puntos_buffer_global.data(), counts.data(), displacements.data(),
                    MPI_UINT16_T, puntos_buffer_local.data(), counts[rank], MPI_UINT16_T, 0, MPI_COMM_WORLD);
        
        // Convertir buffer local a puntos
        vector<Point> puntos_locales;
        puntos_locales.reserve(puntos_buffer_local.size() / 2);
        for (size_t i = 0; i < puntos_buffer_local.size(); i += 2) {
            puntos_locales.push_back(Point(puntos_buffer_local[i], puntos_buffer_local[i+1]));
        }
        
        if (rank == 0) {
            cout << "Proceso " << rank << " recibe " << puntos_locales.size() << " puntos" << endl;
        }
        
        // Calcular acumulador local (usando uint16_t)
        for (int i=0; i < 1; i++){
          acumulador_local.assign(n_rho * n_theta, 0);
          calcular_acumulador_optimizado(puntos_locales, acumulador_local);
        }
        
        // Reducir acumuladores (sumar todos) usando uint16_t
        // IMPORTANTE: Usamos MPI_UINT16_T y MPI_SUM, pero debemos verificar desbordamiento
        vector<uint16_t> acumulador_global(n_rho * n_theta, 0);
        
        // Usar MPI_Reduce con uint16_t
        MPI_Reduce(acumulador_local.data(), acumulador_global.data(), 
                  n_rho * n_theta, MPI_UINT16_T, MPI_SUM, 0, MPI_COMM_WORLD);
        
        if (rank == 0) {
            print_master("\n=== ACUMULADOR GLOBAL (uint16_t) ===");
            uint16_t max_votos = *max_element(acumulador_global.begin(), acumulador_global.end());
            print_master("Máximo de votos: " + to_string(max_votos));
            print_master("Umbral utilizado: " + to_string(umbral_votos));
            
            if (max_votos == 65535) {
                print_master("ADVERTENCIA: Se alcanzó el máximo de uint16_t (65535)");
                print_master("Considere usar uint32_t si los votos exceden este límite");
            }
            
            // Convertir a vector<int> para procesamiento (solo en proceso maestro)
            vector<int> acumulador_int(acumulador_global.begin(), acumulador_global.end());
            vector<Linea> lineas = encontrar_lineas(acumulador_int);
            
            // Dibujar líneas
            Mat imagen_lineas = dibujar_lineas(bordes, lineas);
            
            // Guardar resultados
            imwrite("lineas_finales_hough_paralelo.jpg", imagen_lineas);
            guardar_resultados(acumulador_int, lineas);
            
            return imagen_lineas;
        }
        
        return Mat();
    }
    
    /**
     * Calcula el acumulador de Hough para un conjunto de puntos (optimizado)
     */
    void calcular_acumulador_optimizado(const vector<Point>& puntos, vector<uint16_t>& acumulador) {
        for (int t = 0; t < n_theta; t++) {
	    for (int r = 0; r < n_rho; r++) {
                int idx = r * n_theta + t;
 		acumulador[idx] = 0;
 	    }
 	}
               
        // Para cada punto de borde
        for (const auto& p : puntos) {
            int x = p.x;
            int y = p.y;
            
            // Para cada theta
            for (int t = 0; t < n_theta; t++) {
                // Calcular rho = x*cos(theta) + y*sin(theta)
//                int rho = (int)(x * cos_theta[t] + y * sin_theta[t]);
                double theta = t * theta_res;
                int rho = (int)(x * cos(theta) + y * sin(theta));
                int rho_idx = rho + rho_offset;
                
                if (rho_idx >= 0 && rho_idx < n_rho) {
                    int idx = rho_idx * n_theta + t;
                    // Verificar desbordamiento antes de incrementar
                    if (acumulador[idx] < 65535) {
                        acumulador[idx]++;
                    } else {
                        // Esto no debería ocurrir con imágenes normales
                        static bool warning_printed = false;
                        if (!warning_printed && rank == 0) {
                            cerr << "ADVERTENCIA: Desbordamiento de uint16_t en celda del acumulador" << endl;
                            warning_printed = true;
                        }
                    }
                }
            }
        }
    }
    
    /**
     * Encuentra las líneas mediante supresión de no máximos
     */
    vector<Linea> encontrar_lineas(const vector<int>& acumulador) {
        // Crear máscara de máximos locales
        vector<bool> es_maximo_local(n_rho * n_theta, false);
        
        int mitad_x = ventana_x / 2;
        int mitad_y = ventana_y / 2;
        
        // Encontrar máximos locales
        #pragma omp parallel for collapse(2) if(n_rho * n_theta > 1000000)
        for (int r = 0; r < n_rho; r++) {
            for (int t = 0; t < n_theta; t++) {
                int idx = r * n_theta + t;
                int valor_actual = acumulador[idx];
                
                if (valor_actual >= umbral_votos) {
                    bool es_maximo = true;
                    
                    // Verificar ventana
                    for (int dr = -mitad_x; dr <= mitad_x && es_maximo; dr++) {
                        for (int dt = -mitad_y; dt <= mitad_y && es_maximo; dt++) {
                            int nr = r + dr;
                            int nt = t + dt;
                            
                            if (nr >= 0 && nr < n_rho && nt >= 0 && nt < n_theta) {
                                int nidx = nr * n_theta + nt;
                                if (acumulador[nidx] > valor_actual) {
                                    es_maximo = false;
                                }
                            }
                        }
                    }
                    
                    if (es_maximo) {
                        es_maximo_local[idx] = true;
                    }
                }
            }
        }
        
        // Recolectar picos
        vector<Linea> lineas;
        for (int r = 0; r < n_rho; r++) {
            for (int t = 0; t < n_theta; t++) {
                int idx = r * n_theta + t;
                if (es_maximo_local[idx]) {
                    Linea linea;
                    linea.rho = r - rho_offset;
                    linea.theta = t * theta_res;
                    linea.votos = acumulador[idx];
                    lineas.push_back(linea);
                }
            }
        }
        
        // Ordenar por votos (descendente)
        sort(lineas.begin(), lineas.end(), 
             [](const Linea& a, const Linea& b) { return a.votos > b.votos; });
        
        // Supresión adicional por distancia
        vector<Linea> lineas_filtradas;
        for (const auto& linea : lineas) {
            bool cercana = false;
            
            for (const auto& existente : lineas_filtradas) {
                double diff_rho = abs(linea.rho - existente.rho);
                double diff_theta = abs(linea.theta - existente.theta) * 180.0 / M_PI;
                
                if (diff_rho < dist_min_rho && diff_theta < dist_min_theta) {
                    cercana = true;
                    break;
                }
            }
            
            if (!cercana) {
                lineas_filtradas.push_back(linea);
            }
        }
        
        if (rank == 0) {
            cout << "\n=== LÍNEAS DETECTADAS ===" << endl;
            cout << "Picos iniciales: " << lineas.size() << endl;
            cout << "Líneas finales: " << lineas_filtradas.size() << endl;
            
            for (size_t i = 0; i < min(lineas_filtradas.size(), size_t(10)); i++) {
                cout << "  " << (i+1) << ". ρ=" << lineas_filtradas[i].rho 
                     << ", θ=" << lineas_filtradas[i].theta * 180.0 / M_PI 
                     << "°, votos=" << lineas_filtradas[i].votos << endl;
            }
        }
        
        return lineas_filtradas;
    }
    
    /**
     * Dibuja las líneas encontradas en la imagen
     */
    Mat dibujar_lineas(const Mat& bordes, const vector<Linea>& lineas) {
        Mat imagen_lineas;
        cvtColor(bordes, imagen_lineas, COLOR_GRAY2BGR);
        
        vector<Scalar> colores = {
            Scalar(0, 255, 0),   // Verde
            Scalar(255, 0, 0),   // Azul
            Scalar(0, 0, 255),   // Rojo
            Scalar(255, 255, 0), // Amarillo
            Scalar(255, 0, 255), // Magenta
            Scalar(0, 255, 255)  // Cyan
        };
        
        int escala_dibujo = diag_len * 2;
        
        for (size_t i = 0; i < lineas.size(); i++) {
            const auto& linea = lineas[i];
            double a = cos(linea.theta);
            double b = sin(linea.theta);
            double x0 = a * linea.rho;
            double y0 = b * linea.rho;
            
            Point p1((int)(x0 + escala_dibujo * (-b)), 
                     (int)(y0 + escala_dibujo * (a)));
            Point p2((int)(x0 - escala_dibujo * (-b)), 
                     (int)(y0 - escala_dibujo * (a)));
            
            Scalar color = colores[i % colores.size()];
            line(imagen_lineas, p1, p2, color, 3);            
        }
        
        return imagen_lineas;
    }
    
    /**
     * Guarda resultados en archivo de texto
     */
    void guardar_resultados(const vector<int>& acumulador, const vector<Linea>& lineas) {
        if (rank != 0) return;
        
        int max_votos = *max_element(acumulador.begin(), acumulador.end());
        int min_votos = *min_element(acumulador.begin(), acumulador.end());
        double mean_votos = std::accumulate(acumulador.begin(), acumulador.end(), 0.0) / acumulador.size();
        
        FILE* f = fopen("resultados_hough_paralelo.txt", "w");
        if (f) {
            fprintf(f, "=== TRANSFORMADA DE HOUGH PARALELA CON MPI (FULL uint16_t) ===\n\n");
            fprintf(f, "Dimensiones de la imagen: %d x %d\n", height, width);
            fprintf(f, "Diagonal: %d píxeles\n", diag_len);
            fprintf(f, "Procesos utilizados: %d\n\n", num_procs);
            
            fprintf(f, "Parámetros:\n");
            fprintf(f, "  - Umbral de votos: %d\n", umbral_votos);
            fprintf(f, "  - Ventana supresión: %dx%d\n", ventana_x, ventana_y);
            fprintf(f, "  - Distancia mínima ρ: %d\n", dist_min_rho);
            fprintf(f, "  - Distancia mínima θ: %d grados\n\n", dist_min_theta);
            
            fprintf(f, "Estadísticas del acumulador:\n");
            fprintf(f, "  - Máximo de votos: %d\n", max_votos);
            fprintf(f, "  - Mínimo de votos: %d\n", min_votos);
            fprintf(f, "  - Media de votos: %.2f\n\n", mean_votos);
            
            fprintf(f, "Optimización:\n");
            fprintf(f, "  - Tipo de dato: uint16_t (2 bytes)\n");
            fprintf(f, "  - Ahorro de memoria: 50%% vs int (4 bytes)\n");
            fprintf(f, "  - Ahorro en comunicaciones: 50%%\n\n");
            
            fprintf(f, "Líneas detectadas: %zu\n\n", lineas.size());
            fprintf(f, "  #   Rho (píxeles)   Theta (grados)   Votos\n");
            fprintf(f, "  -   -------------   --------------   -----\n");
            
            for (size_t i = 0; i < lineas.size(); i++) {
                fprintf(f, "  %2zu   %8.1f      %8.2f        %d\n",
                       i+1, lineas[i].rho, lineas[i].theta * 180.0 / M_PI, lineas[i].votos);
            }
            
            fclose(f);
        }
        
        cout << "\nArchivos guardados:" << endl;
        cout << "  - lineas_finales_hough_paralelo.jpg" << endl;
        cout << "  - resultados_hough_paralelo.txt" << endl;
    }
};

/**
 * Función principal
 */
int main(int argc, char** argv) {
    string archivo_entrada = "bordes_binarios.jpg";
    
    if (argc > 1) {
        archivo_entrada = argv[1];
    }
    
    // Pequeña pausa para que MPI inicialice correctamente
    HoughTransformMPI hough(argc, argv);
    
    // Versión paralela optimizada con uint16_t
    hough.procesar_imagen_paralelo(archivo_entrada);
    
    return 0;
}
