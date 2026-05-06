# SpreadCell — Hoja de Cálculo con Matriz Dispersa
### Interfaz gráfica SFML + C++17
---
### Integrantes:
# - Bruno Garcia Lopez
# - Sofia Rosales Bazan
# - Piero Max Ortiz Villafuerte

---

## Estructura del proyecto

```
SpreadCell/
├── main.cpp          ← GUI completa (SFML)
├── SparseMatrix.h    ← Declaración de la matriz dispersa
├── SparseMatrix.cpp  ← Implementación (listas enlazadas cruzadas)
├── CMakeLists.txt    ← Build system
└── README.md         ← Este archivo
```

---

## Requisitos

| Herramienta | Versión mínima |
|-------------|----------------|
| C++ compiler | C++17 (GCC 9+, Clang 10+, MSVC 2019+) |
| CMake        | 3.16+          |
| SFML         | 2.6+           |

---

## Instalación de SFML

### Linux (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install libsfml-dev
```

### Linux (Arch / Manjaro)
```bash
sudo pacman -S sfml
```

### macOS (Homebrew)
```bash
brew install sfml
```

### Windows
1. Descargar SFML 2.6 desde https://www.sfml-dev.org/download.php
2. Extraer en `C:\SFML\`
3. En CMakeLists.txt añadir antes de `find_package`:
   ```cmake
   set(SFML_DIR "C:/SFML/lib/cmake/SFML")
   ```

---

## Compilación

### Linux / macOS
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./SpreadCell
```

### Windows (MinGW)
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
SpreadCell.exe
```

### Windows (Visual Studio)
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
# Abrir SpreadCell.sln y compilar en Release
```

---

## Controles de la interfaz

| Acción | Tecla / Botón |
|--------|---------------|
| Navegar celdas | Flechas del teclado |
| Editar celda | F2 o Enter |
| Confirmar edición | Enter |
| Mover derecha al confirmar | Tab |
| Cancelar edición | Escape |
| Borrar celda | Delete |
| Cargar demo | Ctrl+D |
| Mostrar ayuda | Ctrl+H |
| Scroll vertical | Rueda del ratón |

---

## Operaciones disponibles (ribbon + CLI interna)

### Celda
- **SET** — Insertar o modificar una celda (valor numérico, texto o fórmula)
- **GET** — Consultar el valor de la celda activa
- **DEL** — Eliminar la celda activa

### Fila / Columna / Rango
- **Del Fila** — Elimina todos los nodos de la fila indicada
- **Del Col** — Elimina todos los nodos de la columna indicada
- **Del Rango** — Elimina todos los nodos en un rango rectangular (ej: A1:C4)

### Agregación (muestran resultado con overlay animado)
- **SUMA** — Suma de valores numéricos en un rango
- **PROM** — Promedio de valores numéricos en un rango
- **MAX** — Valor máximo en un rango
- **MIN** — Valor mínimo en un rango
- **S.Fila** — Suma de todos los valores de la fila activa
- **S.Col** — Suma de todos los valores de la columna activa

### Fórmulas
Las celdas soportan expresiones aritméticas que comiencen con `=`:
```
=A1+B2
=A3*2
=B1+B2+B3
=C4-D2*0.5
```

---

## Estructura de datos: Lista Enlazada Cruzada

La clase `SparseMatrix` implementa una **cross-linked list** (lista enlazada cruzada):

```
       col1     col3     col5
fila2:  [n]  →  [n]  →  [n]  → NULL
         |               |
fila4:  [n]             [n]  → NULL
         |
        NULL
```

Cada **nodo** (`CellNode`) contiene:
- `row`, `col` — coordenadas de la celda
- `value` — valor (`double` o `std::string` via `std::variant`)
- `nextInRow` — siguiente nodo en la misma fila
- `nextInCol` — siguiente nodo en la misma columna

**Cabeceras** (`RowHeader`, `ColHeader`) apuntan al primer nodo de cada fila/columna activa.

### Complejidad de operaciones

| Operación | Complejidad | Explicación |
|-----------|-------------|-------------|
| Insertar celda | O(k_r + k_c) | k_r nodos en la fila, k_c en la columna |
| Consultar celda | O(k_r) | Recorre la fila hasta encontrar la columna |
| Modificar celda | O(k_r) | Localiza el nodo y actualiza su valor |
| Eliminar celda | O(k_r + k_c) | Desenlaza de fila y columna |
| Eliminar fila | O(k_r * k_c_avg) | Por cada nodo: desenlaza de su columna |
| Eliminar columna | O(k_c * k_r_avg) | Por cada nodo: desenlaza de su fila |
| SUMA / PROM / MAX / MIN rango | O(nodos en rango) | Un solo recorrido |

**Ventaja frente a matriz densa:** Una hoja 1000×1000 con 500 celdas llenas usa 500 nodos en vez de 1.000.000 posiciones. La inserción/eliminación no requiere desplazar memoria.

---

## Justificación de la estructura (requerida por el proyecto)

Se eligió la **lista enlazada cruzada** porque:

1. **Eficiencia de memoria:** Solo se alocan nodos para celdas con datos. Una hoja típica es >99% vacía.
2. **Inserción/eliminación O(k):** No requiere redimensionar arreglos ni mover bloques de memoria.
3. **Recorrido por fila y columna independiente:** Las dos cadenas de punteros (`nextInRow`, `nextInCol`) permiten iterar eficientemente en cualquier dirección, lo cual es esencial para operaciones de agregación por fila, por columna y por rango.
4. **Frente a `std::map`:** Aunque `std::map<pair<int,int>, value>` sería más simple de implementar, no permite recorrido por fila/columna sin escanear todas las celdas (O(n) siempre). La lista enlazada cruzada mantiene el orden espacial y permite saltar a la fila siguiente en O(1) una vez posicionado.
