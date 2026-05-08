# Hough_Transform

-----------

En el campo del procesamiento de imágenes y la visión artificial, uno de los desafíos más críticos es la extracción de características geométricas a partir de datos visuales crudos. Las imágenes del mundo real suelen presentar ruido, variaciones de iluminación y formas incompletas.

El problema principal radica en que los algoritmos de detección de bordes tradicionales (como Canny o Sobel) entregan nubes de puntos o fragmentos de líneas, pero no "comprenden" la estructura global de la imagen.Permite identificar que un conjunto de píxeles dispersos pertenece, a una misma entidad (como una carretera, un borde de un documento o una tubería).

-----------

Este proyecto implementa la Transformada de Hough para transformar el problema de detección del espacio de la imagen al espacio de parámetros.Al utilizar la representación polar de las rectas:

 $x \cos(\theta) + y \sin(\theta) = \rho$

 -----------

Bordes de Cany 

 <img width="515" height="375" alt="image" src="https://github.com/user-attachments/assets/8225c246-e86a-4e99-bc1d-c2e615f03681" />


Transformada de Hough 

<img width="515" height="353" alt="image" src="https://github.com/user-attachments/assets/a04d8747-7280-46d4-92fa-b1d6f68669e4" />
