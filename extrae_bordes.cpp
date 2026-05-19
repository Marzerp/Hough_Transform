#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

/**
 * Procesa una imagen aplicando:
 * 1. Conversión a grises
 * 2. Detector de bordes Canny
 * 3. Umbralización para imagen binaria
 */
Mat procesarImagen(const string& nombreArchivo = "imagen28mp.jpg") {
    
    // 1. Leer la imagen
    Mat imagen = imread(nombreArchivo);
    
    if (imagen.empty()) {
        cerr << "Error: No se pudo cargar la imagen '" << nombreArchivo << "'" << endl;
        return Mat();
    }
    
    cout << "Imagen cargada: " << imagen.cols << " x " << imagen.rows << " píxeles" << endl;
    
    // 2. Convertir a tonos de grises
    Mat imagenGris;
    cvtColor(imagen, imagenGris, COLOR_BGR2GRAY);
    
    // 3. Aplicar detector de bordes de Canny
    Mat bordesCanny;
    Canny(imagenGris, bordesCanny, 100, 150);
    
    // 4. Umbralizar para generar imagen binaria
    Mat bordesBinarios;
    threshold(bordesCanny, bordesBinarios, 160, 255, THRESH_BINARY);
    
    // Mostrar resultados en ventanas
    // Redimensionar para visualización si la imagen es muy grande
    Mat imagenMostrar, grisMostrar, cannyMostrar, binarioMostrar;
    
    double escala = 1.0;
    int maxDim = 800; // Tamaño máximo para mostrar
    
    if (imagen.cols > maxDim || imagen.rows > maxDim) {
        escala = min((double)maxDim / imagen.cols, (double)maxDim / imagen.rows);
        Size nuevoTamano((int)(imagen.cols * escala), (int)(imagen.rows * escala));
        
        resize(imagen, imagenMostrar, nuevoTamano);
        resize(imagenGris, grisMostrar, nuevoTamano);
        resize(bordesCanny, cannyMostrar, nuevoTamano);
        resize(bordesBinarios, binarioMostrar, nuevoTamano);
        
        cout << "Redimensionado para visualización: " << nuevoTamano.width << " x " << nuevoTamano.height << endl;
    } else {
        imagenMostrar = imagen;
        grisMostrar = imagenGris;
        cannyMostrar = bordesCanny;
        binarioMostrar = bordesBinarios;
    }
    
    // Mostrar ventanas
    imshow("1. Imagen Original", imagenMostrar);
    imshow("2. Tonos de Grises", grisMostrar);
    imshow("3. Bordes Canny", cannyMostrar);
    imshow("4. Bordes Binarios", binarioMostrar);
    
    // Mover ventanas a posiciones específicas para mejor organización
    moveWindow("1. Imagen Original", 50, 50);
    moveWindow("2. Tonos de Grises", 50 + imagenMostrar.cols + 10, 50);
    moveWindow("3. Bordes Canny", 50, 50 + imagenMostrar.rows + 10);
    moveWindow("4. Bordes Binarios", 50 + imagenMostrar.cols + 10, 50 + imagenMostrar.rows + 10);
    
    cout << "\nPresiona cualquier tecla en una ventana para continuar..." << endl;
    waitKey(0);
    
    // Cerrar todas las ventanas
    destroyAllWindows();
    
    // Guardar resultados
    imwrite("imagen_gris.jpg", imagenGris);
    imwrite("bordes_canny.jpg", bordesCanny);
    imwrite("bordes_binarios.jpg", bordesBinarios);
    
    cout << "\nProcesamiento completado. Imágenes guardadas:" << endl;
    cout << "- imagen_gris.jpg" << endl;
    cout << "- bordes_canny.jpg" << endl;
    cout << "- bordes_binarios.jpg" << endl;
    
    return bordesBinarios;
}

/**
 * Versión que guarda una imagen combinada con todas las etapas
 */
void guardarImagenCombinada(const Mat& original, const Mat& gris, 
                            const Mat& canny, const Mat& binario) {
    // Crear una imagen combinada para comparación
    int height = max(original.rows, max(gris.rows, max(canny.rows, binario.rows)));
    int width = original.cols + gris.cols + canny.cols + binario.cols + 20;
    
    Mat combinada(height, width, CV_8UC3, Scalar(255, 255, 255));
    
    // Copiar cada imagen a su posición
    Mat roi(combinada, Rect(0, 0, original.cols, original.rows));
    if (original.type() == CV_8UC3) {
        original.copyTo(roi);
    } else {
        cvtColor(original, roi, COLOR_GRAY2BGR);
    }
    
    roi = Mat(combinada, Rect(original.cols + 10, 0, gris.cols, gris.rows));
    if (gris.type() == CV_8UC3) {
        gris.copyTo(roi);
    } else {
        cvtColor(gris, roi, COLOR_GRAY2BGR);
    }
    
    roi = Mat(combinada, Rect(original.cols + 10 + gris.cols + 10, 0, 
                               canny.cols, canny.rows));
    if (canny.type() == CV_8UC3) {
        canny.copyTo(roi);
    } else {
        cvtColor(canny, roi, COLOR_GRAY2BGR);
    }
    
    roi = Mat(combinada, Rect(original.cols + 10 + gris.cols + 10 + canny.cols + 10, 0,
                               binario.cols, binario.rows));
    if (binario.type() == CV_8UC3) {
        binario.copyTo(roi);
    } else {
        cvtColor(binario, roi, COLOR_GRAY2BGR);
    }
    
    // Agregar texto a la imagen combinada
    putText(combinada, "Original", Point(original.cols/3, original.rows - 10), 
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
    putText(combinada, "Grises", Point(original.cols + 10 + gris.cols/3, gris.rows - 10), 
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
    putText(combinada, "Canny", Point(original.cols + 20 + gris.cols + canny.cols/3, canny.rows - 10), 
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
    putText(combinada, "Binario", Point(original.cols + 30 + gris.cols + canny.cols + binario.cols/3, binario.rows - 10), 
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
    
    imwrite("comparacion_etapas.jpg", combinada);
    cout << "- comparacion_etapas.jpg (imagen combinada)" << endl;
}

/**
 * Función para probar diferentes parámetros de Canny
 */
void probarParametrosCanny(const string& nombreArchivo = "imagen28mp.jpg") {
    Mat imagen = imread(nombreArchivo);
    if (imagen.empty()) {
        cerr << "Error: No se pudo cargar la imagen" << endl;
        return;
    }
    
    Mat imagenGris;
    cvtColor(imagen, imagenGris, COLOR_BGR2GRAY);
    
    vector<pair<int, int>> parametros = {
        {50, 100},   {50, 150},   {100, 150},  {100, 200},
        {150, 200},  {150, 250},  {200, 300}
    };
    
    for (size_t i = 0; i < parametros.size(); i++) {
        Mat bordes;
        Canny(imagenGris, bordes, parametros[i].first, parametros[i].second);
        
        string titulo = "Canny: " + to_string(parametros[i].first) + 
                       ", " + to_string(parametros[i].second);
        imshow(titulo, bordes);
        moveWindow(titulo, 50 + (i % 4) * 400, 50 + (i / 4) * 400);
    }
    
    cout << "Probando diferentes parámetros de Canny..." << endl;
    cout << "Presiona cualquier tecla para cerrar" << endl;
    waitKey(0);
    destroyAllWindows();
}

/**
 * Función principal
 */
int main(int argc, char** argv) {
    string archivo = "imagen28mp.jpg";
    
    // Permitir especificar un archivo diferente como argumento
    if (argc > 1) {
        archivo = argv[1];
    }
    
    cout << "=== PROCESAMIENTO DE IMAGEN CON OPENCV ===" << endl;
    cout << "Archivo: " << archivo << endl;
    cout << "=========================================" << endl;
    
    // Procesar la imagen
    Mat bordesBinarios = procesarImagen(archivo);
    
    if (bordesBinarios.empty()) {
        cerr << "Error en el procesamiento" << endl;
        return -1;
    }
    
    // Opcional: Guardar imagen combinada
    Mat original = imread(archivo);
    Mat gris = imread("imagen_gris.jpg", IMREAD_GRAYSCALE);
    Mat canny = imread("bordes_canny.jpg", IMREAD_GRAYSCALE);
    
    if (!original.empty() && !gris.empty() && !canny.empty() && !bordesBinarios.empty()) {
        guardarImagenCombinada(original, gris, canny, bordesBinarios);
    }
    
    // Opcional: Probar diferentes parámetros de Canny
    // Descomentar para probar:
    // probarParametrosCanny(archivo);
    
    return 0;
}
