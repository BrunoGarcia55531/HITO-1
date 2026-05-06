#include "SparseMatrix.h"
#include <stdexcept>
#include <sstream>
#include <functional>
#include <cmath>
#include <limits>
#include <cctype>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════════
//  Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════════
SparseMatrix::SparseMatrix() : rowHeaders_(nullptr), colHeaders_(nullptr) {}

SparseMatrix::~SparseMatrix() { clear(); }

// ═══════════════════════════════════════════════════════════════════════════════
//  Helpers estáticos
// ═══════════════════════════════════════════════════════════════════════════════
bool SparseMatrix::isNumeric(const CellValue& v) {
    return std::holds_alternative<double>(v);
}

double SparseMatrix::toDouble(const CellValue& v) {
    return std::get<double>(v);
}

std::string SparseMatrix::toString(const CellValue& v) {
    if (std::holds_alternative<double>(v)) {
        std::ostringstream oss;
        double d = std::get<double>(v);
        if (d == std::floor(d))
            oss << static_cast<long long>(d);
        else
            oss << d;
        return oss.str();
    }
    return std::get<std::string>(v);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Gestión de cabeceras
// ═══════════════════════════════════════════════════════════════════════════════
RowHeader* SparseMatrix::findRowHeader(int row) const {
    RowHeader* cur = rowHeaders_;
    while (cur && cur->row != row) cur = cur->next;
    return cur;
}

ColHeader* SparseMatrix::findColHeader(int col) const {
    ColHeader* cur = colHeaders_;
    while (cur && cur->col != col) cur = cur->next;
    return cur;
}

RowHeader* SparseMatrix::getOrCreateRowHeader(int row) {
    // Buscar posición de inserción ordenada
    RowHeader* prev = nullptr;
    RowHeader* cur  = rowHeaders_;
    while (cur && cur->row < row) { prev = cur; cur = cur->next; }
    if (cur && cur->row == row) return cur;

    RowHeader* nh = new RowHeader(row);
    nh->next = cur;
    if (prev) prev->next = nh; else rowHeaders_ = nh;
    return nh;
}

ColHeader* SparseMatrix::getOrCreateColHeader(int col) {
    ColHeader* prev = nullptr;
    ColHeader* cur  = colHeaders_;
    while (cur && cur->col < col) { prev = cur; cur = cur->next; }
    if (cur && cur->col == col) return cur;

    ColHeader* ch = new ColHeader(col);
    ch->next = cur;
    if (prev) prev->next = ch; else colHeaders_ = ch;
    return ch;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Desvinculación de nodo (sin liberar)
// ═══════════════════════════════════════════════════════════════════════════════
void SparseMatrix::unlinkFromRow(CellNode* node) {
    RowHeader* rh = findRowHeader(node->row);
    if (!rh) return;
    if (rh->first == node) {
        rh->first = node->nextInRow;
    } else {
        CellNode* prev = rh->first;
        while (prev && prev->nextInRow != node) prev = prev->nextInRow;
        if (prev) prev->nextInRow = node->nextInRow;
    }
    node->nextInRow = nullptr;
}

void SparseMatrix::unlinkFromCol(CellNode* node) {
    ColHeader* ch = findColHeader(node->col);
    if (!ch) return;
    if (ch->first == node) {
        ch->first = node->nextInCol;
    } else {
        CellNode* prev = ch->first;
        while (prev && prev->nextInCol != node) prev = prev->nextInCol;
        if (prev) prev->nextInCol = node->nextInCol;
    }
    node->nextInCol = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  1. Insertar celda  O(k)
// ═══════════════════════════════════════════════════════════════════════════════
void SparseMatrix::insertCell(int row, int col, CellValue value) {
    // Si ya existe, sólo actualizar valor
    RowHeader* rh = findRowHeader(row);
    if (rh) {
        CellNode* cur = rh->first;
        while (cur) {
            if (cur->col == col) { cur->value = value; return; }
            cur = cur->nextInRow;
        }
    }

    CellNode* node = new CellNode(row, col, value);

    // Enlazar en fila (orden ascendente de columna)
    rh = getOrCreateRowHeader(row);
    CellNode* prev = nullptr;
    CellNode* cur  = rh->first;
    while (cur && cur->col < col) { prev = cur; cur = cur->nextInRow; }
    node->nextInRow = cur;
    if (prev) prev->nextInRow = node; else rh->first = node;

    // Enlazar en columna (orden ascendente de fila)
    ColHeader* ch = getOrCreateColHeader(col);
    CellNode* prevC = nullptr;
    CellNode* curC  = ch->first;
    while (curC && curC->row < row) { prevC = curC; curC = curC->nextInCol; }
    node->nextInCol = curC;
    if (prevC) prevC->nextInCol = node; else ch->first = node;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  2. Consultar celda  O(k)
// ═══════════════════════════════════════════════════════════════════════════════
std::optional<CellValue> SparseMatrix::queryCell(int row, int col) const {
    RowHeader* rh = findRowHeader(row);
    if (!rh) return std::nullopt;
    CellNode* cur = rh->first;
    while (cur) {
        if (cur->col == col) return cur->value;
        if (cur->col > col) break;
        cur = cur->nextInRow;
    }
    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  3. Modificar celda  O(k)
// ═══════════════════════════════════════════════════════════════════════════════
bool SparseMatrix::modifyCell(int row, int col, CellValue value) {
    RowHeader* rh = findRowHeader(row);
    if (!rh) return false;
    CellNode* cur = rh->first;
    while (cur) {
        if (cur->col == col) { cur->value = value; return true; }
        cur = cur->nextInRow;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  4. Eliminar celda  O(k)
// ═══════════════════════════════════════════════════════════════════════════════
bool SparseMatrix::deleteCell(int row, int col) {
    RowHeader* rh = findRowHeader(row);
    if (!rh) return false;
    CellNode* cur  = rh->first;
    CellNode* prev = nullptr;
    while (cur) {
        if (cur->col == col) {
            // Desvincular de fila
            if (prev) prev->nextInRow = cur->nextInRow;
            else      rh->first       = cur->nextInRow;
            // Desvincular de columna
            unlinkFromCol(cur);
            delete cur;
            return true;
        }
        prev = cur;
        cur  = cur->nextInRow;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5. Eliminar fila  O(k)
// ═══════════════════════════════════════════════════════════════════════════════
void SparseMatrix::deleteRow(int row) {
    RowHeader* rh = findRowHeader(row);
    if (!rh) return;

    CellNode* cur = rh->first;
    while (cur) {
        CellNode* next = cur->nextInRow;
        unlinkFromCol(cur);
        delete cur;
        cur = next;
    }
    rh->first = nullptr;

    // Remover cabecera de fila
    RowHeader* prev = nullptr;
    RowHeader* rCur = rowHeaders_;
    while (rCur && rCur != rh) { prev = rCur; rCur = rCur->next; }
    if (prev) prev->next  = rh->next;
    else      rowHeaders_ = rh->next;
    delete rh;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  6. Eliminar columna  O(k)
// ═══════════════════════════════════════════════════════════════════════════════
void SparseMatrix::deleteCol(int col) {
    ColHeader* ch = findColHeader(col);
    if (!ch) return;

    CellNode* cur = ch->first;
    while (cur) {
        CellNode* next = cur->nextInCol;
        unlinkFromRow(cur);
        delete cur;
        cur = next;
    }
    ch->first = nullptr;

    // Remover cabecera de columna
    ColHeader* prev = nullptr;
    ColHeader* cCur = colHeaders_;
    while (cCur && cCur != ch) { prev = cCur; cCur = cCur->next; }
    if (prev) prev->next  = ch->next;
    else      colHeaders_ = ch->next;
    delete ch;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  7. Eliminar rango  O(k * rango)
// ═══════════════════════════════════════════════════════════════════════════════
void SparseMatrix::deleteRange(int r1, int c1, int r2, int c2) {
    if (r1 > r2) std::swap(r1, r2);
    if (c1 > c2) std::swap(c1, c2);

    RowHeader* rh = rowHeaders_;
    while (rh) {
        if (rh->row >= r1 && rh->row <= r2) {
            CellNode* cur  = rh->first;
            CellNode* prev = nullptr;
            while (cur) {
                if (cur->col >= c1 && cur->col <= c2) {
                    CellNode* next = cur->nextInRow;
                    if (prev) prev->nextInRow = next;
                    else      rh->first       = next;
                    unlinkFromCol(cur);
                    delete cur;
                    cur = next;
                } else {
                    prev = cur;
                    cur  = cur->nextInRow;
                }
            }
        }
        rh = rh->next;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  8-9. SUMA
// ═══════════════════════════════════════════════════════════════════════════════
double SparseMatrix::sumRow(int row) const {
    double s = 0;
    RowHeader* rh = findRowHeader(row);
    if (!rh) return s;
    for (CellNode* n = rh->first; n; n = n->nextInRow)
        if (isNumeric(n->value)) s += toDouble(n->value);
    return s;
}

double SparseMatrix::sumCol(int col) const {
    double s = 0;
    ColHeader* ch = findColHeader(col);
    if (!ch) return s;
    for (CellNode* n = ch->first; n; n = n->nextInCol)
        if (isNumeric(n->value)) s += toDouble(n->value);
    return s;
}

double SparseMatrix::sumRange(int r1, int c1, int r2, int c2) const {
    if (r1 > r2) std::swap(r1, r2);
    if (c1 > c2) std::swap(c1, c2);
    double s = 0;
    RowHeader* rh = rowHeaders_;
    while (rh) {
        if (rh->row >= r1 && rh->row <= r2) {
            for (CellNode* n = rh->first; n; n = n->nextInRow)
                if (n->col >= c1 && n->col <= c2 && isNumeric(n->value))
                    s += toDouble(n->value);
        }
        rh = rh->next;
    }
    return s;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  10. PROMEDIO
// ═══════════════════════════════════════════════════════════════════════════════
double SparseMatrix::avgRow(int row) const {
    double s = 0; int cnt = 0;
    RowHeader* rh = findRowHeader(row);
    if (!rh) return 0;
    for (CellNode* n = rh->first; n; n = n->nextInRow)
        if (isNumeric(n->value)) { s += toDouble(n->value); ++cnt; }
    return cnt ? s / cnt : 0;
}

double SparseMatrix::avgCol(int col) const {
    double s = 0; int cnt = 0;
    ColHeader* ch = findColHeader(col);
    if (!ch) return 0;
    for (CellNode* n = ch->first; n; n = n->nextInCol)
        if (isNumeric(n->value)) { s += toDouble(n->value); ++cnt; }
    return cnt ? s / cnt : 0;
}

double SparseMatrix::avgRange(int r1, int c1, int r2, int c2) const {
    if (r1 > r2) std::swap(r1, r2);
    if (c1 > c2) std::swap(c1, c2);
    double s = 0; int cnt = 0;
    RowHeader* rh = rowHeaders_;
    while (rh) {
        if (rh->row >= r1 && rh->row <= r2)
            for (CellNode* n = rh->first; n; n = n->nextInRow)
                if (n->col >= c1 && n->col <= c2 && isNumeric(n->value))
                    { s += toDouble(n->value); ++cnt; }
        rh = rh->next;
    }
    return cnt ? s / cnt : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  11. MÁXIMO / MÍNIMO
// ═══════════════════════════════════════════════════════════════════════════════
std::optional<double> SparseMatrix::maxRange(int r1, int c1, int r2, int c2) const {
    if (r1 > r2) std::swap(r1, r2);
    if (c1 > c2) std::swap(c1, c2);
    std::optional<double> mx;
    RowHeader* rh = rowHeaders_;
    while (rh) {
        if (rh->row >= r1 && rh->row <= r2)
            for (CellNode* n = rh->first; n; n = n->nextInRow)
                if (n->col >= c1 && n->col <= c2 && isNumeric(n->value)) {
                    double v = toDouble(n->value);
                    if (!mx || v > *mx) mx = v;
                }
        rh = rh->next;
    }
    return mx;
}

std::optional<double> SparseMatrix::minRange(int r1, int c1, int r2, int c2) const {
    if (r1 > r2) std::swap(r1, r2);
    if (c1 > c2) std::swap(c1, c2);
    std::optional<double> mn;
    RowHeader* rh = rowHeaders_;
    while (rh) {
        if (rh->row >= r1 && rh->row <= r2)
            for (CellNode* n = rh->first; n; n = n->nextInRow)
                if (n->col >= c1 && n->col <= c2 && isNumeric(n->value)) {
                    double v = toDouble(n->value);
                    if (!mn || v < *mn) mn = v;
                }
        rh = rh->next;
    }
    return mn;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  12. Evaluador de fórmulas simples   =A1+B2*3   etc.
//      Convierte referencia de celda (ej "B3") a (fila, col) usando
//      notación A=col 1, B=col 2, ... y fila 1-based.
// ═══════════════════════════════════════════════════════════════════════════════
namespace {

// "A3" → col=1, row=3.  Devuelve false si no es referencia válida.
bool parseCellRef(const std::string& token, int& row, int& col) {
    size_t i = 0;
    int c = 0;
    while (i < token.size() && std::isalpha((unsigned char)token[i])) {
        c = c * 26 + (std::toupper((unsigned char)token[i]) - 'A' + 1);
        ++i;
    }
    if (i == 0 || i == token.size()) return false;
    std::string rowStr = token.substr(i);
    if (rowStr.empty()) return false;
    for (char ch : rowStr) if (!std::isdigit((unsigned char)ch)) return false;
    row = std::stoi(rowStr);
    col = c;
    return true;
}

// Tokenizer simple: devuelve números, refs de celda y operadores +,-,*,/
struct Token { char op; double num; bool isOp; };

std::vector<Token> tokenize(const std::string& expr,
                             const SparseMatrix* sm) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < expr.size()) {
        char c = expr[i];
        if (c == '+' || c == '-' || c == '*' || c == '/') {
            tokens.push_back({c, 0, true});
            ++i;
        } else if (std::isdigit((unsigned char)c) || c == '.') {
            size_t j = i;
            while (j < expr.size() && (std::isdigit((unsigned char)expr[j]) || expr[j] == '.')) ++j;
            tokens.push_back({0, std::stod(expr.substr(i, j - i)), false});
            i = j;
        } else if (std::isalpha((unsigned char)c)) {
            size_t j = i;
            while (j < expr.size() && (std::isalpha((unsigned char)expr[j]) || std::isdigit((unsigned char)expr[j]))) ++j;
            std::string ref = expr.substr(i, j - i);
            int row, col;
            if (parseCellRef(ref, row, col)) {
                auto val = sm->queryCell(row, col);
                double v = 0;
                if (val && SparseMatrix::isNumeric(*val)) v = SparseMatrix::toDouble(*val);
                tokens.push_back({0, v, false});
            }
            i = j;
        } else {
            ++i; // ignorar espacios
        }
    }
    return tokens;
}

// Evaluación de dos pasadas: primero * /, luego + -
double evalTokens(const std::vector<Token>& tokens) {
    // Paso 1: * /
    std::vector<Token> pass2;
    size_t i = 0;
    while (i < tokens.size()) {
        if (!tokens[i].isOp) {
            double val = tokens[i].num;
            while (i + 1 < tokens.size() && tokens[i+1].isOp &&
                   (tokens[i+1].op == '*' || tokens[i+1].op == '/')) {
                char op = tokens[i+1].op;
                double rhs = tokens[i+2].num;
                val = (op == '*') ? val * rhs : val / rhs;
                i += 2;
            }
            pass2.push_back({0, val, false});
        } else {
            pass2.push_back(tokens[i]);
        }
        ++i;
    }
    // Paso 2: + -
    double result = 0;
    if (!pass2.empty() && !pass2[0].isOp) result = pass2[0].num;
    for (size_t j = 1; j + 1 < pass2.size(); j += 2) {
        if (pass2[j].isOp) {
            double rhs = pass2[j+1].num;
            if (pass2[j].op == '+') result += rhs;
            else if (pass2[j].op == '-') result -= rhs;
        }
    }
    return result;
}
} // namespace

std::optional<CellValue> SparseMatrix::evaluateFormula(const std::string& formula) const {
    if (formula.empty() || formula[0] != '=') return std::nullopt;
    std::string expr = formula.substr(1);
    // Eliminar espacios
    expr.erase(std::remove(expr.begin(), expr.end(), ' '), expr.end());
    try {
        auto tokens = tokenize(expr, this);
        if (tokens.empty()) return std::nullopt;
        double result = evalTokens(tokens);
        return CellValue{result};
    } catch (...) {
        return std::nullopt;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Iteración sobre celdas activas
// ═══════════════════════════════════════════════════════════════════════════════
void SparseMatrix::forEachCell(std::function<void(int, int, const CellValue&)> fn) const {
    RowHeader* rh = rowHeaders_;
    while (rh) {
        CellNode* n = rh->first;
        while (n) { fn(n->row, n->col, n->value); n = n->nextInRow; }
        rh = rh->next;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Limpieza total de memoria
// ═══════════════════════════════════════════════════════════════════════════════
void SparseMatrix::clear() {
    RowHeader* rh = rowHeaders_;
    while (rh) {
        CellNode* n = rh->first;
        while (n) { CellNode* nx = n->nextInRow; delete n; n = nx; }
        RowHeader* next = rh->next;
        delete rh;
        rh = next;
    }
    rowHeaders_ = nullptr;

    ColHeader* ch = colHeaders_;
    while (ch) { ColHeader* next = ch->next; delete ch; ch = next; }
    colHeaders_ = nullptr;
}
