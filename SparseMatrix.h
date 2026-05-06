#pragma once
#include <string>
#include <variant>
#include <optional>
#include <functional>

// Valor de celda: puede ser numérico o texto
using CellValue = std::variant<double, std::string>;

// ─── Nodo de la lista enlazada cruzada ───────────────────────────────────────
struct CellNode {
    int row;
    int col;
    CellValue value;

    CellNode* nextInRow;   // siguiente nodo en la misma fila
    CellNode* nextInCol;   // siguiente nodo en la misma columna

    CellNode(int r, int c, CellValue v)
        : row(r), col(c), value(v), nextInRow(nullptr), nextInCol(nullptr) {}
};

// ─── Nodo cabecera de fila ────────────────────────────────────────────────────
struct RowHeader {
    int row;
    CellNode* first;   // primer nodo de la fila
    RowHeader* next;   // siguiente cabecera de fila

    explicit RowHeader(int r) : row(r), first(nullptr), next(nullptr) {}
};

// ─── Nodo cabecera de columna ─────────────────────────────────────────────────
struct ColHeader {
    int col;
    CellNode* first;   // primer nodo de la columna
    ColHeader* next;   // siguiente cabecera de columna

    explicit ColHeader(int c) : col(c), first(nullptr), next(nullptr) {}
};

// ─── Clase principal: Sparse Matrix ──────────────────────────────────────────
class SparseMatrix {
public:
    SparseMatrix();
    ~SparseMatrix();

    // Operaciones básicas de celda
    void        insertCell(int row, int col, CellValue value);
    std::optional<CellValue> queryCell(int row, int col) const;
    bool        modifyCell(int row, int col, CellValue value);
    bool        deleteCell(int row, int col);

    // Operaciones sobre filas y columnas
    void        deleteRow(int row);
    void        deleteCol(int col);
    void        deleteRange(int r1, int c1, int r2, int c2);

    // Operaciones de agregación
    double      sumRow(int row) const;
    double      sumCol(int col) const;
    double      sumRange(int r1, int c1, int r2, int c2) const;
    double      avgRow(int row) const;
    double      avgCol(int col) const;
    double      avgRange(int r1, int c1, int r2, int c2) const;
    std::optional<double> maxRange(int r1, int c1, int r2, int c2) const;
    std::optional<double> minRange(int r1, int c1, int r2, int c2) const;

    // Soporte de fórmulas
    std::optional<CellValue> evaluateFormula(const std::string& formula) const;

    // Iteración (para la GUI)
    void forEachCell(std::function<void(int, int, const CellValue&)> fn) const;

    // Helpers
    static bool  isNumeric(const CellValue& v);
    static double toDouble(const CellValue& v);
    static std::string toString(const CellValue& v);

private:
    RowHeader* rowHeaders_;   // lista de cabeceras de fila
    ColHeader* colHeaders_;   // lista de cabeceras de columna

    // Acceso/creación de cabeceras
    RowHeader* getOrCreateRowHeader(int row);
    ColHeader* getOrCreateColHeader(int col);
    RowHeader* findRowHeader(int row) const;
    ColHeader* findColHeader(int col) const;

    // Elimina un nodo del índice de fila y columna sin liberar memoria
    void unlinkFromRow(CellNode* node);
    void unlinkFromCol(CellNode* node);

    // Limpieza total
    void clear();
};
