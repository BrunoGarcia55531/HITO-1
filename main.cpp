/*
 * ╔══════════════════════════════════════════════════════════════════════╗
 * ║   Hoja de Cálculo con Matriz Dispersa — Interfaz SFML               ║
 * ║   Requiere: SFML 2.6+, C++17                                        ║
 * ╚══════════════════════════════════════════════════════════════════════╝
 *
 * Controles:
 *   Clic / Flechas    → navegar celdas
 *   Enter / F2        → editar celda activa
 *   Esc               → cancelar edición
 *   Delete            → borrar celda activa
 *   Ctrl+D            → cargar datos demo
 *   Ctrl+H            → mostrar ayuda
 */

#include "SparseMatrix.h"
#include <cstdint>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>

// ─── Paleta de colores ────────────────────────────────────────────────────────
namespace Clr {
    const sf::Color BG        {14,  15,  20};
    const sf::Color Surface   {22,  24,  32};
    const sf::Color Surface2  {30,  32,  48};
    const sf::Color Surface3  {38,  42,  64};
    const sf::Color Border    {48,  52,  80};
    const sf::Color Border2   {58,  64,  96};
    const sf::Color Accent    {91, 106, 245};
    const sf::Color AccentLt  {130, 148, 255};
    const sf::Color Green     {62, 207, 142};
    const sf::Color GreenDim  {20,  60,  40};
    const sf::Color Red       {240, 107, 107};
    const sf::Color Amber     {245, 166,  35};
    const sf::Color Text      {220, 222, 240};
    const sf::Color Text2     {140, 148, 190};
    const sf::Color Text3     { 80,  88, 120};
    const sf::Color SelBG     { 30,  40,  80};
    const sf::Color HdrSel    { 40,  48,  90};
    const sf::Color White     {255, 255, 255};
    const sf::Color Transparent{0,0,0,0};
}

// ─── Constantes de layout ─────────────────────────────────────────────────────
constexpr int WIN_W     = 1280;
constexpr int WIN_H     = 780;
constexpr int TOPBAR_H  = 44;
constexpr int FBAR_H    = 34;
constexpr int RIBBON_H  = 38;
constexpr int STATUSBAR_H = 24;
constexpr int SIDEBAR_W = 280;
constexpr int CELL_W    = 96;
constexpr int CELL_H    = 28;
constexpr int HDR_W     = 44;
constexpr int COL_HDR_H = 24;
constexpr int VISIBLE_ROWS = 18;
constexpr int VISIBLE_COLS = 10;
constexpr int MAX_ROWS  = 100;
constexpr int MAX_COLS  = 26;

// ─── Helpers de texto ─────────────────────────────────────────────────────────
std::string colLetter(int c) {
    std::string s;
    while (c > 0) {
        s = char('A' + (c-1)%26) + s;
        c = (c-1)/26;
    }
    return s;
}
int colIndex(const std::string& s) {
    int c = 0;
    for (char ch : s) c = c*26 + (toupper(ch)-'A'+1);
    return c;
}
std::string cellRef(int r, int c) { return colLetter(c) + std::to_string(r); }

bool parseRef(const std::string& s, int& row, int& col) {
    size_t i = 0; int c = 0;
    while (i < s.size() && isalpha((unsigned char)s[i])) {
        c = c*26 + (toupper((unsigned char)s[i])-'A'+1); ++i;
    }
    if (i == 0 || i == s.size()) return false;
    for (size_t j = i; j < s.size(); ++j)
        if (!isdigit((unsigned char)s[j])) return false;
    row = stoi(s.substr(i)); col = c;
    return true;
}
bool parseRange(const std::string& s,
                int& r1,int& c1,int& r2,int& c2) {
    auto pos = s.find(':');
    if (pos == std::string::npos) return false;
    return parseRef(s.substr(0,pos),r1,c1)
        && parseRef(s.substr(pos+1),r2,c2);
}
std::string fmtVal(const CellValue& v) {
    if (std::holds_alternative<double>(v)) {
        double d = std::get<double>(v);
        if (d == std::floor(d)) return std::to_string((long long)d);
        std::ostringstream oss; oss << std::setprecision(6) << d;
        return oss.str();
    }
    return std::get<std::string>(v);
}
std::string fmtDouble(double d) {
    if (d == std::floor(d)) return std::to_string((long long)d);
    std::ostringstream oss; oss << std::setprecision(6) << d;
    return oss.str();
}

// ─── Truncar texto con elipsis ────────────────────────────────────────────────
std::string truncate(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen-1) + "~";
}

// ─── Dibujar rectángulo relleno con borde ────────────────────────────────────
void drawRect(sf::RenderTarget& rt,
              float x, float y, float w, float h,
              sf::Color fill, sf::Color border = sf::Color(0,0,0,0),
              float thickness = 1.f) {
    sf::RectangleShape r({w, h});
    r.setPosition(x, y);
    r.setFillColor(fill);
    if (border.a != 0) {
        r.setOutlineThickness(-thickness);
        r.setOutlineColor(border);
    }
    rt.draw(r);
}

// ─── Dibujar texto ────────────────────────────────────────────────────────────
void drawText(sf::RenderTarget& rt, sf::Font& font,
              const std::string& str, unsigned size,
              float x, float y, sf::Color color,
              bool centered = false) {
    sf::Text t(str, font, size);
    t.setFillColor(color);
    if (centered) {
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.width/2.f, b.height/2.f + b.top);
        t.setPosition(x, y);
    } else {
        t.setPosition(x, y);
    }
    rt.draw(t);
}

// ─── Botón simple ─────────────────────────────────────────────────────────────
struct Button {
    sf::FloatRect rect;
    std::string   label;
    sf::Color     color;
    bool          hovered = false;
    bool          active  = false;

    void draw(sf::RenderTarget& rt, sf::Font& font) const {
        sf::Color bg = active  ? Clr::Accent :
                       hovered ? Clr::Surface3 : Clr::Surface2;
        sf::Color fg = active  ? Clr::White  :
                       hovered ? Clr::Text    : color;
        drawRect(rt, rect.left, rect.top, rect.width, rect.height,
                 bg, Clr::Border2, 1.f);
        float cx = rect.left + rect.width/2.f;
        float cy = rect.top  + rect.height/2.f;
        drawText(rt, font, label, 11, cx, cy, fg, true);
    }
    bool contains(float x, float y) const { return rect.contains(x,y); }
};

// ═══════════════════════════════════════════════════════════════════════════════
//  ENUMS & STRUCTS de estado
// ═══════════════════════════════════════════════════════════════════════════════
enum class Mode { Normal, Editing, Input };

struct AggResult {
    bool   visible = false;
    std::string label, value;
    float  timer  = 0.f;
};

struct LogLine {
    std::string text;
    sf::Color   color;
};

// ═══════════════════════════════════════════════════════════════════════════════
//  CLASE PRINCIPAL: SpreadsheetApp
// ═══════════════════════════════════════════════════════════════════════════════
class SpreadsheetApp {
public:
    SpreadsheetApp();
    void run();

private:
    // SFML
    sf::RenderWindow window_;
    sf::Font         font_;
    sf::Clock        clock_;

    // Estado
    SparseMatrix sm_;
    Mode mode_      = Mode::Normal;
    int  selRow_    = 1, selCol_ = 1;
    int  scrollR_   = 0, scrollC_ = 0;   // offset de scroll (0-based)

    // Edición de celda
    std::string editBuf_;

    // Input genérico (ribbon)
    std::string inputPrompt_, inputBuf_, inputExtra_;
    std::function<void(const std::string&)> inputCallback_;

    // Log
    std::vector<LogLine> log_;
    int opCount_ = 0;

    // Resultado de agregación (overlay)
    AggResult aggResult_;

    // Botones del ribbon
    std::vector<Button> ribbonBtns_;

    // Áreas de UI (calculadas en buildLayout)
    struct Layout {
        sf::FloatRect topbar, fbar, ribbon, grid, sidebar, statusbar;
        float gridX, gridY; // top-left de la zona de datos (sin headers)
    } layout_;

    // Ayuda
    bool showHelp_ = false;

    // ─── métodos privados ────────────────────────────────────────────────────
    void buildLayout();
    void buildRibbonButtons();
    void handleEvents();
    void handleKeyNormal(sf::Keyboard::Key key, bool ctrl, bool shift);
    void handleKeyEditing(uint32_t unicode, sf::Keyboard::Key key);
    void handleKeyInput(uint32_t unicode, sf::Keyboard::Key key);
    void handleMouseClick(float x, float y);
    void update(float dt);
    void render();

    // Dibujo de secciones
    void drawTopbar();
    void drawFormulaBar();
    void drawRibbon();
    void drawGrid();
    void drawSidebar();
    void drawStatusBar();
    void drawAggOverlay();
    void drawHelpOverlay();
    void drawInputOverlay();

    // Operaciones
    void commitEdit();
    void deleteSelected();
    void executeCommand(const std::string& cmd);
    void doAgg(const std::string& type, const std::string& range);
    void loadDemo();

    // Helpers
    void log(const std::string& msg, sf::Color c = Clr::Text2);
    void showAgg(const std::string& label, const std::string& value);
    sf::Vector2i cellFromMouse(float mx, float my);
    void ensureVisible(int r, int c);

    // Calcula posición en pantalla de una celda (relativo a window)
    sf::Vector2f cellPos(int r, int c) const;

    // Ribbon prompts
    void promptAgg(const std::string& type);
    void promptDelete(const std::string& what);
    void promptSet();
};

// ─── Constructor ──────────────────────────────────────────────────────────────
SpreadsheetApp::SpreadsheetApp()
    : window_(sf::VideoMode(WIN_W, WIN_H),
              "Hoja de Calculo — Matriz Dispersa",
              sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize)
{
    window_.setFramerateLimit(60);

    // Intentar cargar fuente del sistema (Linux / Windows / macOS)
    bool loaded = false;
    const std::vector<std::string> fontPaths = {
        "arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf",
    };
    for (auto& p : fontPaths)
        if (font_.loadFromFile(p)) { loaded = true; break; }
    if (!loaded) {
        // Fuente de respaldo: DejaVu embebida en SFML (no existe, usamos lo que haya)
        font_.loadFromFile("arial.ttf"); // fallback silencioso
    }

    buildLayout();
    buildRibbonButtons();
    log("Bienvenido. Clic en celda o usa flechas para navegar.", Clr::Text2);
    log("Ctrl+D = Demo  |  Ctrl+H = Ayuda  |  F2/Enter = Editar", Clr::Text3);
}

// ─── Build Layout ─────────────────────────────────────────────────────────────
void SpreadsheetApp::buildLayout() {
    float W = (float)window_.getSize().x;
    float H = (float)window_.getSize().y;
    layout_.topbar    = {0, 0, W, (float)TOPBAR_H};
    layout_.fbar      = {0, (float)TOPBAR_H, W, (float)FBAR_H};
    layout_.ribbon    = {0, (float)(TOPBAR_H+FBAR_H), W, (float)RIBBON_H};
    float gridTop     = (float)(TOPBAR_H + FBAR_H + RIBBON_H);
    float gridH       = H - gridTop - STATUSBAR_H;
    layout_.sidebar   = {W - SIDEBAR_W, gridTop, (float)SIDEBAR_W, gridH};
    layout_.grid      = {0, gridTop, W - SIDEBAR_W, gridH};
    layout_.statusbar = {0, H - STATUSBAR_H, W, (float)STATUSBAR_H};
    layout_.gridX     = layout_.grid.left + HDR_W;
    layout_.gridY     = layout_.grid.top  + COL_HDR_H;
}

void SpreadsheetApp::buildRibbonButtons() {
    ribbonBtns_.clear();
    float y0 = layout_.ribbon.top + 5.f;
    float h  = 28.f;
    float x  = 8.f;
    auto add = [&](const std::string& label, float w, sf::Color clr) {
        Button b; b.rect = {x, y0, w, h}; b.label = label; b.color = clr;
        ribbonBtns_.push_back(b); x += w + 4.f;
        return (int)ribbonBtns_.size()-1;
    };
    auto sep = [&]() { x += 10.f; };

    add("SET",    46, Clr::Green);
    add("GET",    46, Clr::Text2);
    add("DEL",    46, Clr::Red);
    sep();
    add("Del Fila",  70, Clr::Red);
    add("Del Col",   66, Clr::Red);
    add("Del Rango", 76, Clr::Red);
    sep();
    add("SUMA",    52, Clr::Amber);
    add("PROM",    52, Clr::Amber);
    add("MAX",     48, Clr::Amber);
    add("MIN",     48, Clr::Amber);
    add("S.Fila",  58, Clr::Amber);
    add("S.Col",   52, Clr::Amber);
    sep();
    add("Demo",    52, Clr::AccentLt);
    add("Ayuda",   54, Clr::Text2);
}

// ─── Cell position (screen) ───────────────────────────────────────────────────
sf::Vector2f SpreadsheetApp::cellPos(int r, int c) const {
    // r,c are 1-based; scroll is 0-based
    float x = layout_.gridX + (c - 1 - scrollC_) * CELL_W;
    float y = layout_.gridY + (r - 1 - scrollR_) * CELL_H;
    return {x, y};
}

sf::Vector2i SpreadsheetApp::cellFromMouse(float mx, float my) {
    float relX = mx - layout_.gridX;
    float relY = my - layout_.gridY;
    if (relX < 0 || relY < 0) return {-1,-1};
    int c = (int)(relX / CELL_W) + 1 + scrollC_;
    int r = (int)(relY / CELL_H) + 1 + scrollR_;
    if (c < 1 || c > MAX_COLS || r < 1 || r > MAX_ROWS) return {-1,-1};
    return {r, c};
}

void SpreadsheetApp::ensureVisible(int r, int c) {
    int maxVisR = VISIBLE_ROWS;
    int maxVisC = VISIBLE_COLS;
    if (r - 1 < scrollR_) scrollR_ = r - 1;
    if (r - 1 >= scrollR_ + maxVisR) scrollR_ = r - maxVisR;
    if (c - 1 < scrollC_) scrollC_ = c - 1;
    if (c - 1 >= scrollC_ + maxVisC) scrollC_ = c - maxVisC;
    scrollR_ = std::max(0, scrollR_);
    scrollC_ = std::max(0, scrollC_);
}

// ─── Run Loop ─────────────────────────────────────────────────────────────────
void SpreadsheetApp::run() {
    while (window_.isOpen()) {
        float dt = clock_.restart().asSeconds();  // sf::Time → float
        handleEvents();
        update(dt);
        render();
    }
}

// ─── Events ───────────────────────────────────────────────────────────────────
void SpreadsheetApp::handleEvents() {
    sf::Event ev;
    while (window_.pollEvent(ev)) {
        if (ev.type == sf::Event::Closed) window_.close();

        if (ev.type == sf::Event::Resized) {
            sf::FloatRect vp(0,0,(float)ev.size.width,(float)ev.size.height);
            window_.setView(sf::View(vp));
            buildLayout();
            buildRibbonButtons();
        }

        if (ev.type == sf::Event::MouseButtonPressed &&
            ev.mouseButton.button == sf::Mouse::Left) {
            handleMouseClick((float)ev.mouseButton.x,(float)ev.mouseButton.y);
        }

        if (ev.type == sf::Event::MouseMoved) {
            float mx=(float)ev.mouseMove.x, my=(float)ev.mouseMove.y;
            for (auto& b : ribbonBtns_)
                b.hovered = b.contains(mx,my);
        }

        if (ev.type == sf::Event::MouseWheelScrolled) {
            int delta = (ev.mouseWheelScroll.delta > 0) ? -1 : 1;
            scrollR_ = std::max(0, std::min(MAX_ROWS-VISIBLE_ROWS, scrollR_+delta));
        }

        if (mode_ == Mode::Normal && ev.type == sf::Event::KeyPressed) {
            bool ctrl  = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)
                      || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
            bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)
                      || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
            handleKeyNormal(ev.key.code, ctrl, shift);
        }

        if (mode_ == Mode::Editing) {
            if (ev.type == sf::Event::TextEntered)
                handleKeyEditing(ev.text.unicode, sf::Keyboard::Unknown);
            if (ev.type == sf::Event::KeyPressed)
                handleKeyEditing(0, ev.key.code);
        }

        if (mode_ == Mode::Input) {
            if (ev.type == sf::Event::TextEntered)
                handleKeyInput(ev.text.unicode, sf::Keyboard::Unknown);
            if (ev.type == sf::Event::KeyPressed)
                handleKeyInput(0, ev.key.code);
        }
    }
}

void SpreadsheetApp::handleKeyNormal(sf::Keyboard::Key key, bool ctrl, bool shift) {
    if (showHelp_) { showHelp_ = false; return; }
    switch (key) {
        case sf::Keyboard::Up:
            selRow_ = std::max(1, selRow_-1);
            ensureVisible(selRow_, selCol_); break;
        case sf::Keyboard::Down:
            selRow_ = std::min(MAX_ROWS, selRow_+1);
            ensureVisible(selRow_, selCol_); break;
        case sf::Keyboard::Left:
            selCol_ = std::max(1, selCol_-1);
            ensureVisible(selRow_, selCol_); break;
        case sf::Keyboard::Right:
            selCol_ = std::min(MAX_COLS, selCol_+1);
            ensureVisible(selRow_, selCol_); break;
        case sf::Keyboard::F2:
        case sf::Keyboard::Enter: {
            auto val = sm_.queryCell(selRow_, selCol_);
            editBuf_ = val ? fmtVal(*val) : "";
            mode_ = Mode::Editing;
            break;
        }
        case sf::Keyboard::Delete:
        case sf::Keyboard::BackSpace:
            deleteSelected(); break;
        case sf::Keyboard::D:
            if (ctrl) loadDemo(); break;
        case sf::Keyboard::H:
            if (ctrl) showHelp_ = true; break;
        default: break;
    }
}

void SpreadsheetApp::handleKeyEditing(uint32_t unicode, sf::Keyboard::Key key) {
    if (unicode != 0) {
        // TextEntered
        if (unicode == '\r' || unicode == '\n') { commitEdit(); return; }
        if (unicode == 27) { mode_ = Mode::Normal; return; }  // Escape
        if (unicode == 8) { // Backspace
            if (!editBuf_.empty()) editBuf_.pop_back();
            return;
        }
        if (unicode >= 32) editBuf_ += (char)unicode;
    } else {
        // KeyPressed
        if (key == sf::Keyboard::Enter)  { commitEdit(); return; }
        if (key == sf::Keyboard::Escape) { mode_ = Mode::Normal; return; }
        if (key == sf::Keyboard::BackSpace && !editBuf_.empty())
            editBuf_.pop_back();
        // Tab: commit and move right
        if (key == sf::Keyboard::Tab) {
            commitEdit();
            selCol_ = std::min(MAX_COLS, selCol_+1);
            ensureVisible(selRow_, selCol_);
        }
    }
}

void SpreadsheetApp::handleKeyInput(uint32_t unicode, sf::Keyboard::Key key) {
    if (unicode != 0) {
        if (unicode == '\r' || unicode == '\n') {
            mode_ = Mode::Normal;
            if (inputCallback_) inputCallback_(inputBuf_);
            return;
        }
        if (unicode == 27) { mode_ = Mode::Normal; return; }
        if (unicode == 8) { if (!inputBuf_.empty()) inputBuf_.pop_back(); return; }
        if (unicode >= 32) inputBuf_ += (char)unicode;
    } else {
        if (key == sf::Keyboard::Enter) {
            mode_ = Mode::Normal;
            if (inputCallback_) inputCallback_(inputBuf_);
            return;
        }
        if (key == sf::Keyboard::Escape) { mode_ = Mode::Normal; return; }
        if (key == sf::Keyboard::BackSpace && !inputBuf_.empty())
            inputBuf_.pop_back();
    }
}

void SpreadsheetApp::handleMouseClick(float x, float y) {
    if (mode_ == Mode::Editing) { commitEdit(); return; }
    if (mode_ == Mode::Input)   { return; }
    if (showHelp_) { showHelp_ = false; return; }

    // Ribbon buttons
    for (int i = 0; i < (int)ribbonBtns_.size(); ++i) {
        if (ribbonBtns_[i].contains(x,y)) {
            const std::string& lbl = ribbonBtns_[i].label;
            if      (lbl=="SET")      promptSet();
            else if (lbl=="GET")      {
                auto v = sm_.queryCell(selRow_,selCol_);
                if (v) log(cellRef(selRow_,selCol_)+" = "+fmtVal(*v), Clr::Green);
                else   log(cellRef(selRow_,selCol_)+" esta vacia", Clr::Text2);
            }
            else if (lbl=="DEL")      deleteSelected();
            else if (lbl=="Del Fila") promptDelete("fila");
            else if (lbl=="Del Col")  promptDelete("col");
            else if (lbl=="Del Rango")promptDelete("rango");
            else if (lbl=="SUMA")     promptAgg("SUM");
            else if (lbl=="PROM")     promptAgg("AVG");
            else if (lbl=="MAX")      promptAgg("MAX");
            else if (lbl=="MIN")      promptAgg("MIN");
            else if (lbl=="S.Fila")   {
                double v = sm_.sumRow(selRow_);
                log("SUMA fila "+std::to_string(selRow_)+" = "+fmtDouble(v), Clr::Green);
                showAgg("SUMA fila "+std::to_string(selRow_), fmtDouble(v));
            }
            else if (lbl=="S.Col")    {
                double v = sm_.sumCol(selCol_);
                log("SUMA col "+colLetter(selCol_)+" = "+fmtDouble(v), Clr::Green);
                showAgg("SUMA col "+colLetter(selCol_), fmtDouble(v));
            }
            else if (lbl=="Demo")     loadDemo();
            else if (lbl=="Ayuda")    showHelp_ = true;
            return;
        }
    }

    // Click en la grilla
    if (layout_.grid.contains(x, y)) {
        // Doble-clic no distinguido fácilmente con SFML single event;
        // single click = seleccionar, implementamos edición con F2/Enter
        auto cell = cellFromMouse(x, y);
        if (cell.x > 0) {
            selRow_ = cell.x; selCol_ = cell.y;
        }
    }
}

// ─── Update ───────────────────────────────────────────────────────────────────
void SpreadsheetApp::update(float dt) {
    if (aggResult_.visible) {
        aggResult_.timer -= dt;
        if (aggResult_.timer <= 0.f) aggResult_.visible = false;
    }
}

// ─── Commit Edit ──────────────────────────────────────────────────────────────
void SpreadsheetApp::commitEdit() {
    mode_ = Mode::Normal;
    const std::string& raw = editBuf_;
    if (raw.empty()) {
        sm_.deleteCell(selRow_, selCol_);
        return;
    }
    if (!raw.empty() && raw[0] == '=') {
        auto res = sm_.evaluateFormula(raw);
        if (res) {
            sm_.insertCell(selRow_, selCol_, *res);
            log(cellRef(selRow_,selCol_)+" = "+fmtVal(*res), Clr::Green);
        } else {
            sm_.insertCell(selRow_, selCol_, raw);
            log("Formula almacenada como texto", Clr::Text3);
        }
    } else {
        try {
            double d = stod(raw);
            sm_.insertCell(selRow_, selCol_, d);
        } catch (...) {
            sm_.insertCell(selRow_, selCol_, raw);
        }
    }
}

void SpreadsheetApp::deleteSelected() {
    bool ok = sm_.deleteCell(selRow_, selCol_);
    log(ok ? cellRef(selRow_,selCol_)+" eliminada"
           : cellRef(selRow_,selCol_)+" estaba vacia",
        ok ? Clr::Green : Clr::Text3);
}

// ─── Ribbon prompts ───────────────────────────────────────────────────────────
void SpreadsheetApp::promptSet() {
    inputPrompt_ = "SET " + cellRef(selRow_,selCol_) + " = ? (valor o =formula)";
    auto v = sm_.queryCell(selRow_, selCol_);
    inputBuf_    = v ? fmtVal(*v) : "";
    inputExtra_  = cellRef(selRow_, selCol_);
    mode_ = Mode::Input;
    inputCallback_ = [this](const std::string& val) {
        int row, col;
        if (!parseRef(inputExtra_, row, col)) return;
        if (val.empty()) { sm_.deleteCell(row,col); return; }
        if (val[0]=='=') {
            auto res = sm_.evaluateFormula(val);
            if (res) { sm_.insertCell(row,col,*res); log(inputExtra_+" = "+fmtVal(*res), Clr::Green); }
            else { sm_.insertCell(row,col,val); log("Formula guardada", Clr::Text3); }
        } else {
            try { double d=stod(val); sm_.insertCell(row,col,d); }
            catch(...) { sm_.insertCell(row,col,val); }
            log(inputExtra_+" = "+val, Clr::Green);
        }
    };
}

void SpreadsheetApp::promptDelete(const std::string& what) {
    if (what=="fila") {
        inputPrompt_ = "Eliminar fila N:";
        inputBuf_    = std::to_string(selRow_);
        inputCallback_ = [this](const std::string& s) {
            try { int r=stoi(s); sm_.deleteRow(r); log("Fila "+s+" eliminada", Clr::Red); }
            catch(...) { log("Numero de fila invalido", Clr::Red); }
        };
    } else if (what=="col") {
        inputPrompt_ = "Eliminar columna (letra):";
        inputBuf_    = colLetter(selCol_);
        inputCallback_ = [this](const std::string& s) {
            int c = colIndex(s);
            sm_.deleteCol(c); log("Columna "+s+" eliminada", Clr::Red);
        };
    } else {
        inputPrompt_ = "Eliminar rango (ej: A1:C4):";
        inputBuf_    = cellRef(selRow_,selCol_)+":"+
                       cellRef(std::min(selRow_+2,MAX_ROWS),std::min(selCol_+2,MAX_COLS));
        inputCallback_ = [this](const std::string& s) {
            int r1,c1,r2,c2;
            if (!parseRange(s,r1,c1,r2,c2)) { log("Rango invalido", Clr::Red); return; }
            sm_.deleteRange(r1,c1,r2,c2); log("Rango "+s+" eliminado", Clr::Red);
        };
    }
    mode_ = Mode::Input;
}

void SpreadsheetApp::promptAgg(const std::string& type) {
    std::string def = cellRef(selRow_,selCol_)+":"+
                      cellRef(std::min(selRow_+3,MAX_ROWS),std::min(selCol_+3,MAX_COLS));
    std::map<std::string,std::string> labels = {
        {"SUM","SUMA"},{"AVG","PROM"},{"MAX","MAX"},{"MIN","MIN"}
    };
    inputPrompt_ = labels[type]+"(rango) — ej: A1:D5:";
    inputBuf_    = def;
    inputCallback_ = [this, type](const std::string& s) {
        doAgg(type, s);
    };
    mode_ = Mode::Input;
}

void SpreadsheetApp::doAgg(const std::string& type, const std::string& range) {
    int r1,c1,r2,c2;
    std::string rs = range;
    std::transform(rs.begin(),rs.end(),rs.begin(),::toupper);
    if (!parseRange(rs,r1,c1,r2,c2)) { log("Rango invalido", Clr::Red); return; }

    std::map<std::string,std::string> lbl={{"SUM","SUMA"},{"AVG","PROM"},{"MAX","MAX"},{"MIN","MIN"}};
    std::string L = lbl.count(type)?lbl[type]:type;

    if (type=="SUM") {
        double v = sm_.sumRange(r1,c1,r2,c2);
        log(L+"("+range+") = "+fmtDouble(v), Clr::Green);
        showAgg(L+"("+range+")", fmtDouble(v));
    } else if (type=="AVG") {
        double v = sm_.avgRange(r1,c1,r2,c2);
        log(L+"("+range+") = "+fmtDouble(v), Clr::Green);
        showAgg(L+"("+range+")", fmtDouble(v));
    } else if (type=="MAX") {
        auto v = sm_.maxRange(r1,c1,r2,c2);
        if (!v) { log("Sin valores numericos en el rango", Clr::Text3); return; }
        log(L+"("+range+") = "+fmtDouble(*v), Clr::Green);
        showAgg(L+"("+range+")", fmtDouble(*v));
    } else if (type=="MIN") {
        auto v = sm_.minRange(r1,c1,r2,c2);
        if (!v) { log("Sin valores numericos en el rango", Clr::Text3); return; }
        log(L+"("+range+") = "+fmtDouble(*v), Clr::Green);
        showAgg(L+"("+range+")", fmtDouble(*v));
    }
}

// ─── Demo ─────────────────────────────────────────────────────────────────────
void SpreadsheetApp::loadDemo() {
    // Limpiar (reiniciar matrix)
    sm_ = SparseMatrix();

    // Headers
    sm_.insertCell(1,1,std::string("Producto"));
    sm_.insertCell(1,2,std::string("Enero"));
    sm_.insertCell(1,3,std::string("Febrero"));
    sm_.insertCell(1,4,std::string("Marzo"));
    sm_.insertCell(1,5,std::string("TOTAL"));

    // Datos
    struct Row { int r; std::string name; double v[3]; };
    Row rows[] = {
        {2,"Laptops",   {145,162,178}},
        {3,"Monitores", { 88, 95,110}},
        {4,"Teclados",  {230,215,247}},
        {5,"Ratones",   {312,298,341}},
        {6,"Auriculares",{76, 89, 95}},
    };
    for (auto& row : rows) {
        sm_.insertCell(row.r, 1, row.name);
        for (int c=0;c<3;c++) sm_.insertCell(row.r, c+2, row.v[c]);
    }
    // Totales fila
    for (int r=2;r<=6;r++)
        sm_.insertCell(r, 5, sm_.sumRange(r,2,r,4));
    // Totales columna
    sm_.insertCell(7,1,std::string("TOTAL"));
    for (int c=2;c<=5;c++)
        sm_.insertCell(7, c, sm_.sumRange(2,c,6,c));

    log("Demo cargado. 35 nodos activos.", Clr::AccentLt);
    log("Prueba SUMA A2:D6, MAX B2:D6, etc.", Clr::Text3);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
void SpreadsheetApp::log(const std::string& msg, sf::Color c) {
    ++opCount_;
    log_.push_back({"["+std::to_string(opCount_)+"] "+msg, c});
    if (log_.size() > 80) log_.erase(log_.begin());
}

void SpreadsheetApp::showAgg(const std::string& label, const std::string& value) {
    aggResult_.visible = true;
    aggResult_.label   = label;
    aggResult_.value   = value;
    aggResult_.timer   = 4.f;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  RENDER
// ═══════════════════════════════════════════════════════════════════════════════
void SpreadsheetApp::render() {
    window_.clear(Clr::BG);
    drawTopbar();
    drawFormulaBar();
    drawRibbon();
    drawGrid();
    drawSidebar();
    drawStatusBar();
    if (aggResult_.visible) drawAggOverlay();
    if (mode_ == Mode::Input) drawInputOverlay();
    if (showHelp_) drawHelpOverlay();
    window_.display();
}

// ─── Top Bar ─────────────────────────────────────────────────────────────────
void SpreadsheetApp::drawTopbar() {
    auto& r = layout_.topbar;
    drawRect(window_, r.left,r.top,r.width,r.height, Clr::Surface, Clr::Border, 1.f);
    drawText(window_, font_, "SpreadCell", 16, 14, r.top+12, Clr::Text);
    drawText(window_, font_, "Matriz Dispersa", 11, 110, r.top+15, Clr::Text3);

    // Node count badge
    int nodes=0;
    sm_.forEachCell([&](int,int,const CellValue&){nodes++;});
    std::string badge = "Nodos: "+std::to_string(nodes);
    float bw = badge.size()*7.5f + 16;
    drawRect(window_, r.width-bw-10, r.top+10, bw, 22, Clr::Surface2, Clr::Border, 1.f);
    drawText(window_, font_, badge, 11, r.width-bw-10+8, r.top+12, Clr::Text3);
}

// ─── Formula Bar ─────────────────────────────────────────────────────────────
void SpreadsheetApp::drawFormulaBar() {
    auto& r = layout_.fbar;
    drawRect(window_, r.left,r.top,r.width,r.height, Clr::Surface, Clr::Border, 1.f);

    // Cell reference box
    std::string ref = cellRef(selRow_, selCol_);
    drawRect(window_, 8, r.top+5, 62, 24, Clr::Surface2, Clr::Border2, 1.f);
    drawText(window_, font_, ref, 13, 39, r.top+16, Clr::AccentLt, true);

    // fx label
    drawText(window_, font_, "fx", 12, 80, r.top+10, Clr::Text3);

    // Value / formula
    std::string disp;
    if (mode_ == Mode::Editing) {
        disp = editBuf_ + "|";
    } else {
        auto v = sm_.queryCell(selRow_, selCol_);
        disp = v ? fmtVal(*v) : "";
    }
    sf::Color fc = (mode_==Mode::Editing) ? Clr::AccentLt : Clr::Text;
    drawText(window_, font_, disp, 13, 100, r.top+9, fc);
}

// ─── Ribbon ──────────────────────────────────────────────────────────────────
void SpreadsheetApp::drawRibbon() {
    auto& r = layout_.ribbon;
    drawRect(window_, r.left,r.top,r.width,r.height, Clr::Surface, Clr::Border, 1.f);
    for (auto& b : ribbonBtns_) b.draw(window_, font_);
}

// ─── Grid ────────────────────────────────────────────────────────────────────
void SpreadsheetApp::drawGrid() {
    auto& g = layout_.grid;

    // Clipping via scissor-like approach: draw background
    drawRect(window_, g.left, g.top, g.width, g.height, Clr::BG);

    // ── Column headers
    drawRect(window_, g.left, g.top, g.width, COL_HDR_H, Clr::Surface, Clr::Border, 1.f);
    // corner cell
    drawRect(window_, g.left, g.top, HDR_W, COL_HDR_H, Clr::Surface2, Clr::Border, 1.f);

    for (int c = 0; c < VISIBLE_COLS; ++c) {
        int col = c + 1 + scrollC_;
        if (col > MAX_COLS) break;
        float cx = layout_.gridX + c * CELL_W;
        sf::Color hbg = (col == selCol_) ? Clr::HdrSel : Clr::Surface;
        sf::Color hfg = (col == selCol_) ? Clr::AccentLt : Clr::Text3;
        drawRect(window_, cx, g.top, CELL_W, COL_HDR_H, hbg, Clr::Border, 1.f);
        drawText(window_, font_, colLetter(col), 11,
                 cx + CELL_W/2.f, g.top + COL_HDR_H/2.f, hfg, true);
    }

    // ── Rows
    for (int r = 0; r < VISIBLE_ROWS; ++r) {
        int row = r + 1 + scrollR_;
        if (row > MAX_ROWS) break;
        float ry = layout_.gridY + r * CELL_H;

        // Row number header
        sf::Color rhbg = (row == selRow_) ? Clr::HdrSel : Clr::Surface;
        sf::Color rhfg = (row == selRow_) ? Clr::AccentLt : Clr::Text3;
        drawRect(window_, g.left, ry, HDR_W, CELL_H, rhbg, Clr::Border, 1.f);
        drawText(window_, font_, std::to_string(row), 11,
                 g.left + HDR_W/2.f, ry + CELL_H/2.f, rhfg, true);

        // ── Cells in row
        for (int c = 0; c < VISIBLE_COLS; ++c) {
            int col = c + 1 + scrollC_;
            if (col > MAX_COLS) break;
            float cx = layout_.gridX + c * CELL_W;

            auto val = sm_.queryCell(row, col);
            bool isSel  = (row == selRow_ && col == selCol_);
            bool hasVal = val.has_value();
            bool isNum  = hasVal && SparseMatrix::isNumeric(*val);

            // Cell background
            sf::Color cellBg;
            if (isSel)          cellBg = Clr::SelBG;
            else if (hasVal)    cellBg = isNum ? Clr::GreenDim : Clr::Surface2;
            else                cellBg = Clr::BG;

            drawRect(window_, cx, ry, CELL_W, CELL_H, cellBg, Clr::Border, 1.f);

            // Selection outline
            if (isSel) {
                sf::RectangleShape sel({(float)CELL_W-1, (float)CELL_H-1});
                sel.setPosition(cx+0.5f, ry+0.5f);
                sel.setFillColor(sf::Color(0,0,0,0));
                sel.setOutlineThickness(2.f);
                sel.setOutlineColor(Clr::Accent);
                window_.draw(sel);
            }

            // Cell content
            std::string txt;
            sf::Color   fg = Clr::Text;
            if (isSel && mode_ == Mode::Editing) {
                txt = editBuf_ + "|";
                fg  = Clr::AccentLt;
            } else if (hasVal) {
                txt = truncate(fmtVal(*val), 10);
                fg  = isNum ? Clr::Green : Clr::Text2;
            }

            if (!txt.empty()) {
                float tx = isNum ? cx + CELL_W - 6 - txt.size()*7.2f : cx + 5;
                float ty = ry + CELL_H/2.f - 6;
                drawText(window_, font_, txt, 12, tx, ty, fg);
            }
        }
    }
}

// ─── Sidebar ─────────────────────────────────────────────────────────────────
void SpreadsheetApp::drawSidebar() {
    auto& s = layout_.sidebar;
    drawRect(window_, s.left, s.top, s.width, s.height, Clr::Surface, Clr::Border, 1.f);

    float y = s.top + 10;
    float x = s.left + 12;

    // ── Cell info ──
    drawText(window_, font_, "CELDA SELECCIONADA", 10, x, y, Clr::Text3);
    y += 18;

    auto val = sm_.queryCell(selRow_, selCol_);
    std::string refStr = cellRef(selRow_, selCol_);
    drawRect(window_, x, y, s.width-24, 40, Clr::Surface2, Clr::Border, 1.f);
    drawText(window_, font_, refStr, 18, x+10, y+4, Clr::AccentLt);
    std::string typ = val ? (SparseMatrix::isNumeric(*val) ? "numero" : "texto") : "vacia";
    drawText(window_, font_, typ, 10, x+10, y+26, Clr::Text3);
    if (val) {
        std::string vs = fmtVal(*val);
        drawText(window_, font_, truncate(vs,20), 13,
                 x + s.width - 36 - vs.size()*7.5f, y+14,
                 SparseMatrix::isNumeric(*val)?Clr::Green:Clr::Text2);
    }
    y += 52;

    // ── Sparse structure info ──
    drawRect(window_, s.left, y, s.width, 1, Clr::Border);
    y += 8;
    drawText(window_, font_, "ESTRUCTURA INTERNA", 10, x, y, Clr::Text3);
    y += 18;

    int nodes=0;
    std::set<int> rows_, cols_;
    sm_.forEachCell([&](int r,int c,const CellValue&){ nodes++; rows_.insert(r); cols_.insert(c); });

    auto infoLine = [&](const std::string& lbl, const std::string& v) {
        drawText(window_, font_, lbl, 11, x, y, Clr::Text3);
        drawText(window_, font_, v, 11, x+120, y, Clr::Text2);
        y += 16;
    };
    infoLine("Nodos totales:",    std::to_string(nodes));
    infoLine("Filas activas:",    std::to_string(rows_.size()));
    infoLine("Cols activas:",     std::to_string(cols_.size()));
    infoLine("Complejidad ins:",  "O(k)");
    infoLine("Complejidad del:", "O(k)");

    // Current row linked list visualization
    y += 4;
    drawText(window_, font_, "Lista fila "+std::to_string(selRow_)+":", 10, x, y, Clr::Text3);
    y += 16;

    // Draw linked list nodes for current row
    {
        float nx = x; float ny = y;
        int count = 0;
        // Collect nodes
        std::vector<std::pair<int,std::string>> rowNodes;
        sm_.forEachCell([&](int r,int c,const CellValue& v){
            if (r==selRow_) rowNodes.push_back({c, truncate(fmtVal(v),4)});
        });
        std::sort(rowNodes.begin(),rowNodes.end());

        for (auto& [col, vs] : rowNodes) {
            if (count > 3) { // too many: show ellipsis
                drawText(window_, font_, "...", 11, nx, ny+4, Clr::Text3);
                break;
            }
            float bw=50, bh=22;
            drawRect(window_, nx, ny, bw, bh, Clr::Surface3, Clr::Accent, 1.f);
            drawText(window_, font_, colLetter(col), 9, nx+3, ny+2, Clr::AccentLt);
            drawText(window_, font_, vs, 10, nx+3, ny+12, Clr::Green);
            if (count < (int)rowNodes.size()-1 && count < 3) {
                drawText(window_, font_, "->", 10, nx+bw+1, ny+6, Clr::Text3);
            }
            nx += bw + 14; count++;
        }
        if (rowNodes.empty()) drawText(window_, font_, "(vacia)", 11, nx, ny+4, Clr::Text3);
        y += 32;
    }

    // ── Log ──
    y += 4;
    drawRect(window_, s.left, y, s.width, 1, Clr::Border);
    y += 8;
    drawText(window_, font_, "CONSOLA", 10, x, y, Clr::Text3);
    y += 18;

    // Show last N log lines that fit
    float logBottom = s.top + s.height - 8;
    int lineH = 15;
    int maxLines = (int)((logBottom - y) / lineH);
    int start = std::max(0, (int)log_.size() - maxLines);
    for (int i = start; i < (int)log_.size(); ++i) {
        if (y + lineH > logBottom) break;
        drawText(window_, font_, truncate(log_[i].text, 34), 11, x, y, log_[i].color);
        y += lineH;
    }
}

// ─── Status Bar ──────────────────────────────────────────────────────────────
void SpreadsheetApp::drawStatusBar() {
    auto& r = layout_.statusbar;
    drawRect(window_, r.left,r.top,r.width,r.height, Clr::Surface, Clr::Border, 1.f);

    int nodes=0; sm_.forEachCell([&](int,int,const CellValue&){nodes++;});
    std::string txt = "Celda: "+cellRef(selRow_,selCol_)
                    + "  |  Nodos: "+std::to_string(nodes)
                    + "  |  Scroll: "+std::to_string(scrollR_)+","+std::to_string(scrollC_)
                    + "  |  F2=Editar  Del=Borrar  Ctrl+D=Demo  Ctrl+H=Ayuda";
    drawText(window_, font_, txt, 11, 10, r.top+5, Clr::Text3);

    // Mode indicator
    std::string mode_str;
    sf::Color   mode_clr;
    switch(mode_){
        case Mode::Editing: mode_str="EDITANDO"; mode_clr=Clr::Amber; break;
        case Mode::Input:   mode_str="INPUT";    mode_clr=Clr::AccentLt; break;
        default:            mode_str="NORMAL";   mode_clr=Clr::Text3; break;
    }
    drawText(window_, font_, mode_str, 11, r.width-90, r.top+5, mode_clr);
}

// ─── Aggregation Overlay ─────────────────────────────────────────────────────
void SpreadsheetApp::drawAggOverlay() {
    float pw = 280, ph = 72;
    float px = layout_.grid.left + layout_.grid.width - SIDEBAR_W - pw - 20;
    float py = layout_.grid.top + 10;

    // Fade out
    float alpha = std::min(1.f, aggResult_.timer / 0.5f);
    sf::Uint8 a = (sf::Uint8)(alpha * 240);

    drawRect(window_, px, py, pw, ph,
             sf::Color(30,32,48,a), sf::Color(91,106,245,a), 2.f);

    drawText(window_, font_, aggResult_.label, 11, px+12, py+10,
             sf::Color(140,148,190,a));
    drawText(window_, font_, aggResult_.value, 26, px+12, py+32,
             sf::Color(62,207,142,a));
}

// ─── Input Overlay ───────────────────────────────────────────────────────────
void SpreadsheetApp::drawInputOverlay() {
    float W = (float)window_.getSize().x;
    float H = (float)window_.getSize().y;
    // Dim background
    sf::RectangleShape dim({W,H});
    dim.setFillColor({0,0,0,140});
    window_.draw(dim);

    float bw = 420, bh = 110;
    float bx = W/2 - bw/2, by = H/2 - bh/2;
    drawRect(window_, bx, by, bw, bh, Clr::Surface2, Clr::Accent, 2.f);
    drawText(window_, font_, inputPrompt_, 12, bx+14, by+12, Clr::Text2);

    // Input box
    drawRect(window_, bx+14, by+38, bw-28, 32, Clr::Surface3, Clr::AccentLt, 1.5f);
    drawText(window_, font_, inputBuf_+"|", 14, bx+22, by+45, Clr::AccentLt);

    drawText(window_, font_, "Enter = confirmar    Esc = cancelar", 11,
             bx+14, by+84, Clr::Text3);
}

// ─── Help Overlay ────────────────────────────────────────────────────────────
void SpreadsheetApp::drawHelpOverlay() {
    float W = (float)window_.getSize().x;
    float H = (float)window_.getSize().y;
    sf::RectangleShape dim({W,H}); dim.setFillColor({0,0,0,180}); window_.draw(dim);

    float bw=560,bh=420; float bx=W/2-bw/2,by=H/2-bh/2;
    drawRect(window_,bx,by,bw,bh,Clr::Surface2,Clr::Accent,2.f);

    float y=by+16; float x=bx+20;
    drawText(window_,font_,"AYUDA — Hoja de Calcculo con Matriz Dispersa",14,x,y,Clr::AccentLt);
    y+=30;

    const std::vector<std::pair<std::string,std::string>> help = {
        {"Flechas",       "Navegar celdas"},
        {"F2 / Enter",    "Editar celda seleccionada"},
        {"Delete",        "Borrar celda seleccionada"},
        {"Tab",           "Confirmar edicion y mover derecha"},
        {"Esc",           "Cancelar edicion"},
        {"Ctrl+D",        "Cargar datos demo"},
        {"Ctrl+H",        "Mostrar esta ayuda"},
        {"Scroll",        "Desplazar filas con rueda del raton"},
        {"",""},
        {"SET",           "Insertar/modificar celda (ribbon)"},
        {"GET",           "Consultar celda activa (ribbon)"},
        {"DEL",           "Eliminar celda activa (ribbon)"},
        {"Del Fila/Col",  "Eliminar fila o columna completa"},
        {"Del Rango",     "Eliminar rango rectangular (ej: A1:C4)"},
        {"",""},
        {"SUMA/PROM",     "Suma o promedio de un rango"},
        {"MAX/MIN",       "Maximo o minimo de un rango"},
        {"S.Fila/S.Col",  "Suma de fila o columna activa"},
        {"",""},
        {"Formulas",      "Empieza con = ej: =A1+B2*3"},
    };
    for (auto& [k,v] : help) {
        if (k.empty()){y+=6;continue;}
        drawText(window_,font_,k,11,x,y,Clr::Amber);
        drawText(window_,font_,v,11,x+160,y,Clr::Text2);
        y+=17;
    }
    y+=10;
    drawText(window_,font_,"Clic en cualquier lugar para cerrar",11,x,y,Clr::Text3);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════════
int main() {
    SpreadsheetApp app;
    app.run();
    return 0;
}
