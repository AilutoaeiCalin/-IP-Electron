#include <SFML/Graphics.hpp>
#include <Windows.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <commdlg.h> /// pentru fisiere(save si load)

using namespace std;
using namespace sf;

#define MAX_COMPONENTE 30
#define MAX_LEGATURI 100
#define MAX_TIP 30
#define MAX_PINS 10
#define MAX 100

// ---------------------------------------------------------------------
// Structuri globale
// ---------------------------------------------------------------------
struct pin {
    int c1, c2;
};

struct Legatura {
    pin p1, p2;
} Legaturi[MAX_LEGATURI + 1];

int nr_Legaturi = 0;

struct Componenta {
    int id;
    int tip;
    int x, y;
    int orientare; // 0..3
    int nrPini;
    pin pini[MAX_PINS];
    float scale;
    char nume[30];
    float parametru;
    bool esteSelectata;
    char paramAsString[32];
};

Componenta Componente[MAX_COMPONENTE + 1];
int nr_Componente = 0;

struct Descriere {
    char tipComponenta[MAX];
    char comanda[MAX];
    unsigned nrComenzi;
    unsigned nrPini;
    float x1[MAX], y1[MAX];
    float x2[MAX], y2[MAX];
    float piniX[MAX], piniY[MAX];
};

struct butonMeniu {
    RectangleShape forma;
    Text text;
};

Descriere tipuriPiese[MAX_TIP];
int nrTipuriPiese = 0;

int pinOffsets[MAX_TIP + 1][MAX_PINS][2];
int numPinsForTip[MAX_TIP + 1];

enum Stare {
    MENIU_PRINCIPAL,
    APLICATIE_LUCRU,
    INFORMATII,
};

Stare stareAplicatie = MENIU_PRINCIPAL;
// ---------------------------------------------------------------------
// Buton
// ---------------------------------------------------------------------
struct Buton {
    float x, y;
    float latime, inaltime;
    int indexPiesa;
    bool esteHover;
    bool esteSelectat;
};

Buton butoane[MAX_TIP];
int nrButoane = 0;

// marginea stânga a zonei de lucru
float startWorkX = 350.f;

// ---------------------------------------------------------------------
// Undo/Redo
// ---------------------------------------------------------------------
enum OperationType {
    OP_ADD_COMPONENT,
    OP_REMOVE_COMPONENT,
    OP_MOVE_COMPONENT,
    OP_ADD_LINK,
    OP_ROTATE_COMPONENT,
    OP_SCALE_COMPONENT
};

struct Operation {
    OperationType type;

    int compID;
    Componenta compData;

    int oldX, newX;
    int oldY, newY;

    pin p1, p2;
    int linkIndex;

    int oldOrient, newOrient;

    float oldScale, newScale;

    // Salvăm și starea de selecție
    bool butoaneSelectate[MAX_TIP];
    bool componenteSelectate[MAX_COMPONENTE + 1];
};

vector<Operation> undoStack;
vector<Operation> redoStack;

bool canUndo() { return !undoStack.empty(); }
bool canRedo() { return !redoStack.empty(); }

void pushUndo(const Operation& op) {
    undoStack.push_back(op);
    redoStack.clear();
}

// ---------------------------------------------------------------------
// Semnăturile funcțiilor interne
// ---------------------------------------------------------------------
void undoOperation(const Operation& op);
void redoOperation(const Operation& op);

void undo() {
    if (!canUndo()) return;
    Operation op = undoStack.back();
    undoStack.pop_back();
    undoOperation(op);

    // Restaurăm starea de selecție
    for (int i = 0; i < nrButoane; i++) {
        butoane[i].esteSelectat = op.butoaneSelectate[i];
    }
    for (int i = 1; i <= nr_Componente; i++) {
        Componente[i].esteSelectata = op.componenteSelectate[i];
    }

    redoStack.push_back(op);
}

void redo() {
    if (!canRedo()) return;
    Operation op = redoStack.back();
    redoStack.pop_back();
    redoOperation(op);

    // Restaurăm starea de selecție
    for (int i = 0; i < nrButoane; i++) {
        butoane[i].esteSelectat = op.butoaneSelectate[i];
    }
    for (int i = 1; i <= nr_Componente; i++) {
        Componente[i].esteSelectata = op.componenteSelectate[i];
    }

    undoStack.push_back(op);
}
char ultimulFisierSalvat[250] = "circuit_salvat.txt";
// ---------------------------------------------------------------------
// Salvare / încărcare
// ---------------------------------------------------------------------
bool deschideDialogOpenTXT(char* outPath, int outSize) {
    OPENFILENAME file;
    ZeroMemory(&file, sizeof(file));
    file.lStructSize = sizeof(file);

    file.hwndOwner = NULL;

    wchar_t widePath[MAX_PATH] = { 0 };
    file.lpstrFile = widePath;
    file.nMaxFile = MAX_PATH;

    file.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
    file.nFilterIndex = 1;

    file.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&file)) {
        // Conversia de la wchar_t* la char*
        WideCharToMultiByte(CP_UTF8, 0, widePath, -1, outPath, outSize, NULL, NULL);
        return true;
    }
    return false;
}


void saveCircuitInFisier(const string& filename) {
    ofstream fout(filename);
    fout << nr_Componente << " " << nr_Legaturi << "\n";
    for (int i = 1; i <= nr_Componente; i++) {
        fout << Componente[i].tip << " "
            << Componente[i].x << " "
            << Componente[i].y << " "
            << Componente[i].orientare << " "
            << Componente[i].scale << " "
            << Componente[i].parametru << " "
            << Componente[i].nume << " "
            << Componente[i].nrPini << " ";
        for (int p = 0; p < Componente[i].nrPini; p++) {
            fout << Componente[i].pini[p].c1 << " " << Componente[i].pini[p].c2 << " ";
        }
        fout << "\n";
    }
    for (int l = 1; l <= nr_Legaturi; l++) {
        fout << Legaturi[l].p1.c1 << " " << Legaturi[l].p1.c2 << " "
            << Legaturi[l].p2.c1 << " " << Legaturi[l].p2.c2 << "\n";
    }
    fout.close();
}

void handleSaveCircuit(bool ctrlPressed)
{
    if (ctrlPressed) {
        // Deschidem File Explorer pentru a alege fișierul
        char path[260];
        if (deschideDialogOpenTXT(path, 260)) {
            saveCircuitInFisier(path);
            //saveCircuitInFisier(ultimulFisierSalvat);
            // Actualizăm "ultimulFisierSalvat"
            //strcpy_s(ultimulFisierSalvat, path);
        }
        else {
            cout << "Operațiune de salvare anulată.\n";
        }
    }
    else {
        // Doar S => salvăm direct în ultimulFisierSalvat
        saveCircuitInFisier(ultimulFisierSalvat);
    }
}

void loadCircuitDinFisier(const string& filename) {
    ifstream fin(filename);
    fin >> nr_Componente >> nr_Legaturi;
    for (int i = 1; i <= nr_Componente; i++) {
        fin >> Componente[i].tip
            >> Componente[i].x
            >> Componente[i].y
            >> Componente[i].orientare
            >> Componente[i].scale
            >> Componente[i].parametru;
        fin >> Componente[i].nume;
        fin >> Componente[i].nrPini;
        for (int p = 0; p < Componente[i].nrPini; p++) {
            fin >> Componente[i].pini[p].c1 >> Componente[i].pini[p].c2;
        }
        Componente[i].id = i;
        Componente[i].esteSelectata = false;
    }
    for (int l = 1; l <= nr_Legaturi; l++) {
        fin >> Legaturi[l].p1.c1 >> Legaturi[l].p1.c2
            >> Legaturi[l].p2.c1 >> Legaturi[l].p2.c2;
    }
    fin.close();
}
void handleLoadCircuit(bool ctrlPressed) {
    if (ctrlPressed) {
        // Deschidem File Explorer
        char path[260];
        if (deschideDialogOpenTXT(path, 260)) {
            // Dacă userul a ales un fișier
            loadCircuitDinFisier(path);

            // Actualizăm și 'ultimulFisierSalvat' dacă vrei
            // strcpy_s(ultimulFisierSalvat, path);
        }
        else {
            cout << "Operațiune de load anulată.\n";
        }
    }
    else {
        // Fără Ctrl => Load din ultimul fișier salvat
        loadCircuitDinFisier(ultimulFisierSalvat);
    }
}

// ---------------------------------------------------------------------
// Funcții citire piese
// ---------------------------------------------------------------------
int citesteComponenteDinFisiere(const char* fisierPiese, Descriere comp[], int& nrPiese) {
    FILE* fisierIn = fopen(fisierPiese, "r");
    if (!fisierIn) {
        printf("Eroare: Nu s-a putut deschide fișierul %s\n", fisierPiese);
        return -1;
    }

    char fisierComponenta[MAX];
    nrPiese = 0;

    while (fscanf(fisierIn, "%s", fisierComponenta) != EOF) {
        FILE* fisierCompIn = fopen(fisierComponenta, "r");
        if (!fisierCompIn) {
            printf("Eroare: Nu s-a putut deschide fișierul componentei %s\n", fisierComponenta);
            fclose(fisierIn);
            return -1;
        }

        fscanf(fisierCompIn, "%s", comp[nrPiese].tipComponenta);
        fscanf(fisierCompIn, "%u", &comp[nrPiese].nrPini);

        for (unsigned i = 0; i < comp[nrPiese].nrPini; i++) {
            fscanf(fisierCompIn, "%f %f", &comp[nrPiese].piniX[i], &comp[nrPiese].piniY[i]);
        }

        char buffer[MAX];
        fscanf(fisierCompIn, "%s", buffer);
        fscanf(fisierCompIn, "%u", &comp[nrPiese].nrComenzi);

        for (unsigned i = 0; i < comp[nrPiese].nrComenzi; i++) {
            fscanf(fisierCompIn, " %c %f %f %f %f",
                &comp[nrPiese].comanda[i],
                &comp[nrPiese].x1[i], &comp[nrPiese].y1[i],
                &comp[nrPiese].x2[i], &comp[nrPiese].y2[i]);
        }

        fclose(fisierCompIn);
        nrPiese++;
    }

    fclose(fisierIn);
    return 0;
}

// ---------------------------------------------------------------------
// Rotire manuală a unui punct (x,y) în jurul (cx, cy) cu unghi rad
// ---------------------------------------------------------------------
void rotatePoint(float cx, float cy, float& x, float& y, float rad) {
    float tx = x - cx;
    float ty = y - cy;
    float rx = tx * cos(rad) - ty * sin(rad);
    float ry = tx * sin(rad) + ty * cos(rad);
    x = cx + rx;
    y = cy + ry;
}

// ---------------------------------------------------------------------
// Desenare generică
// ---------------------------------------------------------------------




void deseneazaComponenta(RenderWindow& window, const Descriere& comp, float cx, float cy, float factorScalare, int orientare, bool esteSelectata)
{
    Color lineColor = (esteSelectata ? Color(241, 64, 64) : Color::White);
    float rad = 3.1415926535f * 0.5f * orientare; // [0..3]*90

    for (unsigned i = 0; i < comp.nrComenzi; i++) {
        char c = comp.comanda[i];
        float X1 = cx + comp.x1[i] * factorScalare;
        float Y1 = cy + comp.y1[i] * factorScalare;
        float X2 = cx + comp.x2[i] * factorScalare;
        float Y2 = cy + comp.y2[i] * factorScalare;

        // rotim manual in jurul (cx, cy)
        rotatePoint(cx, cy, X1, Y1, rad);
        rotatePoint(cx, cy, X2, Y2, rad);

        if (c == 'L') {
            Vertex linie[] = {
                Vertex(Vector2f(X1, Y1), lineColor),
                Vertex(Vector2f(X2, Y2), lineColor)
            };
            window.draw(linie, 2, Lines);
        }
        else if (c == 'R') {
            // e un dreptunghi => extragem colțuri
            // pentru simplitate desenăm 4 linii
            float X1Y1x = X1, X1Y1y = Y1;
            float X2Y1x = X2, X2Y1y = Y1;
            float X2Y2x = X2, X2Y2y = Y2;
            float X1Y2x = X1, X1Y2y = Y2;

            Vertex arr[8];
            arr[0] = Vertex(Vector2f(X1Y1x, X1Y1y), lineColor);
            arr[1] = Vertex(Vector2f(X2Y1x, X2Y1y), lineColor);
            arr[2] = Vertex(Vector2f(X2Y1x, X2Y1y), lineColor);
            arr[3] = Vertex(Vector2f(X2Y2x, X2Y2y), lineColor);
            arr[4] = Vertex(Vector2f(X2Y2x, X2Y2y), lineColor);
            arr[5] = Vertex(Vector2f(X1Y2x, X1Y2y), lineColor);
            arr[6] = Vertex(Vector2f(X1Y2x, X1Y2y), lineColor);
            arr[7] = Vertex(Vector2f(X1Y1x, X1Y1y), lineColor);
            window.draw(arr, 8, Lines);
        }
        else if (c == 'A') {
            float raza = sqrt((X2 - X1) * (X2 - X1) + (Y2 - Y1) * (Y2 - Y1));
            const int numSegments = 50;
            VertexArray arc(LineStrip, numSegments + 1);
            float startAngle = 0.f;
            float endAngle = 3.14159f;
            for (int j = 0; j <= numSegments; j++) {
                float alpha = startAngle + (endAngle - startAngle) * (j / (float)numSegments);
                float px = X1 + raza * cos(alpha);
                float py = Y1 + raza * sin(alpha);
                arc[j].position = Vector2f(px, py);
                arc[j].color = lineColor;
            }
            window.draw(arc);
        }
        else if (c == 'O') {
            float raza = sqrt((X2 - X1) * (X2 - X1) + (Y2 - Y1) * (Y2 - Y1));
            CircleShape cerc(raza);
            cerc.setPosition(X1 - raza, Y1 - raza);
            cerc.setFillColor(Color::Transparent);
            cerc.setOutlineColor(lineColor);
            cerc.setOutlineThickness(1);
            window.draw(cerc);
        }
    }

    

}
void deseneazaComponenta2(RenderWindow& window, const Descriere& comp, const Componenta& compo, float cx, float cy, float factorScalare, int orientare, bool esteSelectata, const Font& font)
{
    Color lineColor = (esteSelectata ? Color::Green : Color::White);
    float rad = 3.1415926535f * 0.5f * orientare; // [0..3]*90

    for (unsigned i = 0; i < comp.nrComenzi; i++) {
        char c = comp.comanda[i];
        float X1 = cx + comp.x1[i] * factorScalare;
        float Y1 = cy + comp.y1[i] * factorScalare;
        float X2 = cx + comp.x2[i] * factorScalare;
        float Y2 = cy + comp.y2[i] * factorScalare;

        // rotim manual in jurul (cx, cy)
        rotatePoint(cx, cy, X1, Y1, rad);
        rotatePoint(cx, cy, X2, Y2, rad);

        if (c == 'L') {
            Vertex linie[] = {
                Vertex(Vector2f(X1, Y1), lineColor),
                Vertex(Vector2f(X2, Y2), lineColor)
            };
            window.draw(linie, 2, Lines);
        }
        else if (c == 'R') {
            // e un dreptunghi => extragem colțuri
            // pentru simplitate desenăm 4 linii
            float X1Y1x = X1, X1Y1y = Y1;
            float X2Y1x = X2, X2Y1y = Y1;
            float X2Y2x = X2, X2Y2y = Y2;
            float X1Y2x = X1, X1Y2y = Y2;

            Vertex arr[8];
            arr[0] = Vertex(Vector2f(X1Y1x, X1Y1y), lineColor);
            arr[1] = Vertex(Vector2f(X2Y1x, X2Y1y), lineColor);
            arr[2] = Vertex(Vector2f(X2Y1x, X2Y1y), lineColor);
            arr[3] = Vertex(Vector2f(X2Y2x, X2Y2y), lineColor);
            arr[4] = Vertex(Vector2f(X2Y2x, X2Y2y), lineColor);
            arr[5] = Vertex(Vector2f(X1Y2x, X1Y2y), lineColor);
            arr[6] = Vertex(Vector2f(X1Y2x, X1Y2y), lineColor);
            arr[7] = Vertex(Vector2f(X1Y1x, X1Y1y), lineColor);
            window.draw(arr, 8, Lines);
        }
        else if (c == 'A') {
            float raza = sqrt((X2 - X1) * (X2 - X1) + (Y2 - Y1) * (Y2 - Y1));
            const int numSegments = 50;
            VertexArray arc(LineStrip, numSegments + 1);
            float startAngle = 0.f;
            float endAngle = 3.14159f;
            for (int j = 0; j <= numSegments; j++) {
                float alpha = startAngle + (endAngle - startAngle) * (j / (float)numSegments);
                float px = X1 + raza * cos(alpha);
                float py = Y1 + raza * sin(alpha);
                arc[j].position = Vector2f(px, py);
                arc[j].color = lineColor;
            }
            window.draw(arc);
        }
        else if (c == 'O') {
            float raza = sqrt((X2 - X1) * (X2 - X1) + (Y2 - Y1) * (Y2 - Y1));
            CircleShape cerc(raza);
            cerc.setPosition(X1 - raza, Y1 - raza);
            cerc.setFillColor(Color::Transparent);
            cerc.setOutlineColor(lineColor);
            cerc.setOutlineThickness(1);
            window.draw(cerc);
        }
    }
    


}

// ---------------------------------------------------------------------
// Creare butoane
// ---------------------------------------------------------------------
void creeazaButoane(Buton butoane[], int nrPiese, Vector2u dimFereastra) {
    float spatiuSus = dimFereastra.x / 100;
    float spatiuJos = dimFereastra.y / 100;

    const int maxRanduri = 7;
    const float latimeButon = dimFereastra.y / 10;
    const float inaltimeButon = dimFereastra.x / 20;

    const float xOffset = 25.f;
    const float yOffset = dimFereastra.y / 25;
    const float spatiuIntreColoane = 30.f;

    for (int i = 0; i <= nrPiese; i++) {
        int col = i / maxRanduri;
        int rand = i % maxRanduri;

        butoane[i].x = xOffset + col * (latimeButon + spatiuIntreColoane);
        butoane[i].y = spatiuSus + yOffset + rand * (inaltimeButon + yOffset);
        butoane[i].latime = latimeButon;
        butoane[i].inaltime = inaltimeButon;
        butoane[i].indexPiesa = i;
        butoane[i].esteHover = false;
        butoane[i].esteSelectat = false;
    }
    nrButoane = nrPiese;
}

int indexButonCuHover = -1;
void stergereMediu() {
    nr_Componente = 0;
    nr_Legaturi = 0;
    cout << "Toate componentele si legaturile au fost sterse!\n";
}

void clicButon(Buton butoane[], int nrPiese, Vector2i pozMouse) {
    for (int i = 0; i <= nrPiese; i++) {
        if (pozMouse.x >= butoane[i].x && pozMouse.x <= butoane[i].x + butoane[i].latime &&
            pozMouse.y >= butoane[i].y && pozMouse.y <= butoane[i].y + butoane[i].inaltime)
        {
            if (butoane[i].esteSelectat) {
                butoane[i].esteSelectat = false;
            }
            else {
                for (int j = 0; j <= nrPiese; j++) {
                    butoane[j].esteSelectat = false;
                }
                butoane[i].esteSelectat = true;
            }
            if (butoane[nrPiese].esteSelectat == true) {
                stergereMediu();
                butoane[nrPiese].esteSelectat = false;
            }

        }
    }

}

void hoverButon(Buton butoane[], int nrPiese, Vector2i pozMouse,
    RenderWindow& window, Text& tooltip)
{
    indexButonCuHover = -1;
    for (int i = 0; i <= nrPiese; i++) {
        bool inside = (pozMouse.x >= butoane[i].x &&
            pozMouse.x <= butoane[i].x + butoane[i].latime &&
            pozMouse.y >= butoane[i].y &&
            pozMouse.y <= butoane[i].y + butoane[i].inaltime);
        if (inside) {
            butoane[i].esteHover = true;
            indexButonCuHover = i;
        }
        else {
            butoane[i].esteHover = false;
        }
    }
    if (indexButonCuHover >= 0) {
        if (indexButonCuHover == nrPiese) {
            tooltip.setString("Clear");
            tooltip.setPosition((float)pozMouse.x + 10.f, (float)pozMouse.y + 10.f);
        }
        else {
            tooltip.setString(tipuriPiese[butoane[indexButonCuHover].indexPiesa].tipComponenta);
            tooltip.setPosition((float)pozMouse.x + 10.f, (float)pozMouse.y + 10.f);
        }

    }
}

// ---------------------------------------------------------------------
// Operații "low-level" (fără Undo/Redo)
// ---------------------------------------------------------------------
int doAddComponent(int tip, int x, int y, int orient, float scale, float param, const char* nume) {
    nr_Componente++;
    int newID = nr_Componente;
    Componenta& C = Componente[newID];
    C.id = newID;
    C.tip = tip;
    C.x = x;
    C.y = y;
    C.orientare = orient;
    C.scale = scale;
    strcpy_s(C.nume, nume);
    C.parametru = param;
    C.nrPini = numPinsForTip[tip];
    C.esteSelectata = false;

    // inițiere pinii => urmează updatePins
    for (int p = 0; p < C.nrPini; p++) {
        C.pini[p].c1 = x + pinOffsets[tip][p][0];
        C.pini[p].c2 = y + pinOffsets[tip][p][1];
    }
    return newID;
}

void doRemoveComponent(int idC) {
    int nPini = Componente[idC].nrPini;
    pin piniStersi[MAX_PINS];
    for (int i = 0; i < nPini; i++) {
        piniStersi[i] = Componente[idC].pini[i];
    }
    for (int l = 1; l <= nr_Legaturi; ) {
        bool sterge = false;
        for (int p = 0; p < nPini; p++) {
            if ((Legaturi[l].p1.c1 == piniStersi[p].c1 && Legaturi[l].p1.c2 == piniStersi[p].c2) ||
                (Legaturi[l].p2.c1 == piniStersi[p].c1 && Legaturi[l].p2.c2 == piniStersi[p].c2))
            {
                sterge = true;
                break;
            }
        }
        if (sterge) {
            Legaturi[l] = Legaturi[nr_Legaturi];
            nr_Legaturi--;
        }
        else {
            l++;
        }
    }
    Componente[idC] = Componente[nr_Componente];
    nr_Componente--;
    Componente[idC].id = idC;
}

void doMoveComponent(int idC, int newX, int newY) {
    if (newX < (int)startWorkX) {
        return;
    }
    Componente[idC].x = newX;
    Componente[idC].y = newY;
}

int doAddLink(pin p1, pin p2) {
    nr_Legaturi++;
    Legaturi[nr_Legaturi].p1 = p1;
    Legaturi[nr_Legaturi].p2 = p2;
    return nr_Legaturi;
}

void doRotateComponent(int idC, int newOrient) {
    Componente[idC].orientare = newOrient;
}

void doScaleComponent(int idC, float newScale) {
    if (newScale < 0.1f) {
        newScale = 0.1f;
    }
    Componente[idC].scale = newScale;
}

// ---------------------------------------------------------------------
// updatePins()
void updatePins(int idComp) {
    Componenta& C = Componente[idComp];

    pin oldPins[MAX_PINS];
    for (int i = 0; i < C.nrPini; i++) {
        oldPins[i] = C.pini[i];
    }

    int tip = C.tip;
    float sc = C.scale;
    int cx = C.x, cy = C.y;
    int o = C.orientare;

    float rad = 3.1415926535f * 0.5f * o;  // 0..3 => 0, 90,180,270

    for (int p = 0; p < C.nrPini; p++) {
        float dx = (float)pinOffsets[tip][p][0] * sc;
        float dy = (float)pinOffsets[tip][p][1] * sc;

        rotatePoint(0.f, 0.f, dx, dy, rad);

        C.pini[p].c1 = (int)(cx + dx);
        C.pini[p].c2 = (int)(cy + dy);
    }

    // actualizare legături
    for (int l = 1; l <= nr_Legaturi; l++) {
        pin& lp1 = Legaturi[l].p1;
        pin& lp2 = Legaturi[l].p2;
        for (int p = 0; p < C.nrPini; p++) {
            if (lp1.c1 == oldPins[p].c1 && lp1.c2 == oldPins[p].c2) {
                lp1 = C.pini[p];
            }
            if (lp2.c1 == oldPins[p].c1 && lp2.c2 == oldPins[p].c2) {
                lp2 = C.pini[p];
            }
        }
    }
}

// ---------------------------------------------------------------------
// "Public" - cu Undo/Redo
// ---------------------------------------------------------------------
void addComponent(int tip, int x, int y, int orient, float scale, float param, const char* name) {
    int newID = doAddComponent(tip, x, y, orient, scale, param, name);
    updatePins(newID);

    Operation op;
    op.type = OP_ADD_COMPONENT;
    op.compID = newID;
    op.compData = Componente[newID];

    for (int i = 0; i < nrButoane; i++) {
        op.butoaneSelectate[i] = butoane[i].esteSelectat;
    }
    for (int i = 1; i <= nr_Componente; i++) {
        op.componenteSelectate[i] = Componente[i].esteSelectata;
    }

    pushUndo(op);
}

void removeComponent(int idC) {
    Componenta cpy = Componente[idC];
    doRemoveComponent(idC);

    Operation op;
    op.type = OP_REMOVE_COMPONENT;
    op.compID = cpy.id;
    op.compData = cpy;

    for (int i = 0; i < nrButoane; i++) {
        op.butoaneSelectate[i] = butoane[i].esteSelectat;
    }
    for (int i = 1; i <= nr_Componente; i++) {
        op.componenteSelectate[i] = Componente[i].esteSelectata;
    }

    pushUndo(op);
}

void moveComponent(int idC, int newX, int newY) {
    int oldX = Componente[idC].x;
    int oldY = Componente[idC].y;
    doMoveComponent(idC, newX, newY);
    updatePins(idC);

    Operation op;
    op.type = OP_MOVE_COMPONENT;
    op.compID = idC;
    op.oldX = oldX;
    op.oldY = oldY;
    op.newX = Componente[idC].x;
    op.newY = Componente[idC].y;

    for (int i = 0; i < nrButoane; i++) {
        op.butoaneSelectate[i] = butoane[i].esteSelectat;
    }
    for (int i = 1; i <= nr_Componente; i++) {
        op.componenteSelectate[i] = Componente[i].esteSelectata;
    }

    pushUndo(op);
}

void addLink(pin p1, pin p2) {
    int idx = doAddLink(p1, p2);

    Operation op;
    op.type = OP_ADD_LINK;
    op.linkIndex = idx;
    op.p1 = p1;
    op.p2 = p2;

    for (int i = 0; i < nrButoane; i++) {
        op.butoaneSelectate[i] = butoane[i].esteSelectat;
    }
    for (int i = 1; i <= nr_Componente; i++) {
        op.componenteSelectate[i] = Componente[i].esteSelectata;
    }

    pushUndo(op);
}

void rotateComponent(int idC, int newOrient) {
    int oldOrient = Componente[idC].orientare;
    doRotateComponent(idC, newOrient);
    updatePins(idC);

    Operation op;
    op.type = OP_ROTATE_COMPONENT;
    op.compID = idC;
    op.oldOrient = oldOrient;
    op.newOrient = newOrient;

    for (int i = 0; i < nrButoane; i++) {
        op.butoaneSelectate[i] = butoane[i].esteSelectat;
    }
    for (int i = 1; i <= nr_Componente; i++) {
        op.componenteSelectate[i] = Componente[i].esteSelectata;
    }

    pushUndo(op);
}

void scaleComponent(int idC, float newScale) {
    float oldS = Componente[idC].scale;
    doScaleComponent(idC, newScale);
    updatePins(idC);

    Operation op;
    op.type = OP_SCALE_COMPONENT;
    op.compID = idC;
    op.oldScale = oldS;
    op.newScale = Componente[idC].scale;

    for (int i = 0; i < nrButoane; i++) {
        op.butoaneSelectate[i] = butoane[i].esteSelectat;
    }
    for (int i = 1; i <= nr_Componente; i++) {
        op.componenteSelectate[i] = Componente[i].esteSelectata;
    }

    pushUndo(op);
}

// ---------------------------------------------------------------------
// Undo/Redo Operation
// ---------------------------------------------------------------------
void undoOperation(const Operation& op) {
    switch (op.type) {
    case OP_ADD_COMPONENT:
        doRemoveComponent(op.compID);
        break;
    case OP_REMOVE_COMPONENT:
        nr_Componente++;
        Componente[nr_Componente] = op.compData;
        Componente[nr_Componente].id = nr_Componente;
        updatePins(nr_Componente);
        break;
    case OP_MOVE_COMPONENT:
        doMoveComponent(op.compID, op.oldX, op.oldY);
        updatePins(op.compID);
        break;
    case OP_ADD_LINK: {
        Legaturi[op.linkIndex] = Legaturi[nr_Legaturi];
        nr_Legaturi--;
    } break;
    case OP_ROTATE_COMPONENT:
        doRotateComponent(op.compID, op.oldOrient);
        updatePins(op.compID);
        break;
    case OP_SCALE_COMPONENT:
        doScaleComponent(op.compID, op.oldScale);
        updatePins(op.compID);
        break;
    }
}

void redoOperation(const Operation& op) {
    switch (op.type) {
    case OP_ADD_COMPONENT:
        nr_Componente++;
        Componente[nr_Componente] = op.compData;
        Componente[nr_Componente].id = nr_Componente;
        updatePins(nr_Componente);
        break;
    case OP_REMOVE_COMPONENT:
        doRemoveComponent(op.compID);
        break;
    case OP_MOVE_COMPONENT:
        doMoveComponent(op.compID, op.newX, op.newY);
        updatePins(op.compID);
        break;
    case OP_ADD_LINK:
        nr_Legaturi++;
        Legaturi[nr_Legaturi].p1 = op.p1;
        Legaturi[nr_Legaturi].p2 = op.p2;
        break;
    case OP_ROTATE_COMPONENT:
        doRotateComponent(op.compID, op.newOrient);
        updatePins(op.compID);
        break;
    case OP_SCALE_COMPONENT:
        doScaleComponent(op.compID, op.newScale);
        updatePins(op.compID);
        break;
    }
}

// ---------------------------------------------------------------------
// Detectare mouse
// ---------------------------------------------------------------------
bool mousePePin(Vector2i m, int& idC, pin& pSel) {
    for (int i = 1; i <= nr_Componente; i++) {
        int n = Componente[i].nrPini;
        for (int j = 0; j < n; j++) {
            float dx = (float)(m.x - Componente[i].pini[j].c1);
            float dy = (float)(m.y - Componente[i].pini[j].c2);
            float dist = sqrt(dx * dx + dy * dy);
            if (dist < 15.f) {
                idC = i;
                pSel = Componente[i].pini[j];
                return true;
            }
        }
    }
    return false;
}

bool mousePeComponenta(Vector2i m, int& idC) {
    // Poți schimba aici condiția de "hit" pentru componente
    for (int i = nr_Componente; i >= 1; i--) {
        float dx = (float)(m.x - Componente[i].x);
        float dy = (float)(m.y - Componente[i].y);
        float dist = sqrt(dx * dx + dy * dy);
        // test simplu: de ex. un radius mic, 20, 30, etc.
        if (dist < 2.f * Componente[i].scale) {
            idC = i;
            return true;
        }
    }
    return false;
}

int selecteazaComponenta() {
    for (int c = 1; c <= nr_Componente; c++) {
        if (Componente[c].esteSelectata) {
            return c;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------
// Variabile globale pentru Drag & Drop
// ---------------------------------------------------------------------
bool dragging = false;
int compInDrag = -1;
int offsetX = 0, offsetY = 0;

// ---------------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------------
int main() {
    if (citesteComponenteDinFisiere("piese.txt", tipuriPiese, nrTipuriPiese) == -1) {
        cout << "Eroare citire piese.txt\n";
        return -1;
    }
    Font font;
    if (!font.loadFromFile("BionicTypeExpandedBold.ttf"))
        return -1;

    Texture wallpaperTexture;
    if (!wallpaperTexture.loadFromFile("wallpaper3.jpg")) {
        cout << "Eroare la încărcarea imaginii de fundal!" << endl;
        return -1;
    }

    Texture gunoiTexture;
    if (!gunoiTexture.loadFromFile("gunoi.jpg")) {
        cout << "Eroare la încărcarea imaginii pentru buton!" << endl;
        return -1;
    }
    

    for (int t = 0; t < nrTipuriPiese; t++) {
        numPinsForTip[t] = tipuriPiese[t].nrPini;
        for (int p = 0; p < (int)tipuriPiese[t].nrPini; p++) {
            pinOffsets[t][p][0] = (int)tipuriPiese[t].piniX[p];
            pinOffsets[t][p][1] = (int)tipuriPiese[t].piniY[p];
        }
    }

    RenderWindow window(VideoMode::getDesktopMode(), "Electron", Style::Fullscreen);
    window.setFramerateLimit(60);
    window.setMouseCursorVisible(true);



    Vector2u s = window.getSize();
    float latime = (float)s.x, inaltime = (float)s.y;
    butonMeniu butonStart;
    butonStart.forma.setSize(Vector2f(200, 50));
    butonStart.forma.setPosition((latime - 200) / 2, (inaltime - 50) / 2);
    butonStart.forma.setFillColor(Color(84, 231, 238));
    butonStart.text.setFont(font);
    butonStart.text.setString("Start");
    butonStart.text.setCharacterSize(24);
    butonStart.text.setFillColor(Color::White);
    butonStart.text.setPosition(butonStart.forma.getPosition().x + 60, butonStart.forma.getPosition().y + 10);

    butonMeniu butonInfo;
    butonInfo.forma.setSize(Vector2f(200, 50));
    butonInfo.forma.setPosition((latime - 200) / 2, (inaltime - 50) / 2 + 100);
    butonInfo.forma.setFillColor(Color(84, 231, 238));
    butonInfo.text.setFont(font);
    butonInfo.text.setString("Info");
    butonInfo.text.setCharacterSize(24);
    butonInfo.text.setFillColor(Color::White);
    butonInfo.text.setPosition(butonInfo.forma.getPosition().x + 70, butonInfo.forma.getPosition().y + 10);

    butonMeniu butonExit;
    butonExit.forma.setSize(Vector2f(200, 50));
    butonExit.forma.setPosition((latime - 200) / 2, (inaltime - 50) / 2 + 200);
    butonExit.forma.setFillColor(Color(84, 231, 238));
    butonExit.text.setFont(font);
    butonExit.text.setString("Exit");
    butonExit.text.setCharacterSize(24);
    butonExit.text.setFillColor(Color::White);
    butonExit.text.setPosition(butonExit.forma.getPosition().x + 70, butonExit.forma.getPosition().y + 10);



    Text Meniu;
    Meniu.setString("ELECTRON");
    Meniu.setFont(font);
    Meniu.setCharacterSize(150);
    Meniu.setFillColor(Color::White);
    Meniu.setPosition(latime / 3.5, inaltime / 5);

    Sprite wallpaperSprite;
    wallpaperSprite.setTexture(wallpaperTexture);

    // Dimensionează wallpaper-ul pentru a se potrivi ferestrei
    Vector2u windowSize = window.getSize();
    Vector2u textureSize = wallpaperTexture.getSize();
    float scaleX = static_cast<float>(windowSize.x) / textureSize.x;
    float scaleY = static_cast<float>(windowSize.y) / textureSize.y;
    wallpaperSprite.setScale(scaleX, scaleY);


    Sprite gunoiSprite;
    gunoiSprite.setTexture(gunoiTexture);

    Vector2u  butonSize = window.getSize();
    Vector2u textureSizeGunoi = gunoiTexture.getSize();
    float scaleX1 = static_cast<float>(windowSize.x / 100) / textureSizeGunoi.x;
    float scaleY1 = static_cast<float>(windowSize.y / 100) / textureSizeGunoi.y;
    gunoiSprite.setScale(scaleX1, scaleY1);


    creeazaButoane(butoane, nrTipuriPiese, window.getSize());



    Text tooltip;
    tooltip.setFont(font);
    tooltip.setCharacterSize(14);
    tooltip.setFillColor(Color::White);

    FloatRect zonaMediu(startWorkX, 0.f,
        (float)window.getSize().x - startWorkX,
        (float)window.getSize().y);
    RectangleShape conturMediu(Vector2f(zonaMediu.width, zonaMediu.height));
    conturMediu.setPosition(zonaMediu.left, zonaMediu.top);
    conturMediu.setFillColor(Color(9, 52, 56));
    conturMediu.setOutlineColor(Color::White);
    conturMediu.setOutlineThickness(1.f);

    bool pin1selectat = false, pin2selectat = false;
    pin pin1, pin2;

    bool showEditMeniu = false;
    bool editingNume = false;
    bool editingParametru = false;
    int idCompEdit = 0;
    FloatRect rectNume, rectParam;



    while (window.isOpen()) {
        Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == Event::Closed) {
                window.close();
            }
            // CLICK STÂNGA
            if (ev.type == Event::MouseButtonPressed && ev.mouseButton.button == Mouse::Left) {
                Vector2i m = Mouse::getPosition(window);

                // 1) Dacă am dat click în zona butoanelor
                if (m.x < (int)startWorkX) {
                    clicButon(butoane, nrButoane, m);
                }
                else {
                    // 2) Vedem dacă e selectat un buton => plasăm componenta
                    int butonSelectat = -1;
                    for (int i = 0; i < nrButoane; i++) {
                        if (butoane[i].esteSelectat) {
                            butonSelectat = i;
                            break;
                        }
                    }
                    if (butonSelectat >= 0) {
                        float scala = 10.f;
                        addComponent(butoane[butonSelectat].indexPiesa, m.x, m.y, 0, scala, 0.f, tipuriPiese[butoane[butonSelectat].indexPiesa].tipComponenta);

                        int newID = nr_Componente;
                        // Deselectez altele
                        for (int c = 1; c <= nr_Componente; c++) {
                            Componente[c].esteSelectata = false;
                        }
                        // Noua componentă e selectată
                        Componente[newID].esteSelectata = true;
                    }
                    else {
                        // 3) Nu e buton => vedem pin 
                        int idC; pin pSel;
                        if (mousePePin(m, idC, pSel)) {
                            if (!pin1selectat) {
                                pin1 = pSel;
                                pin1selectat = true;
                            }
                            else if (!pin2selectat) {
                                pin2 = pSel;
                                pin2selectat = true;
                            }
                            if (pin1selectat && pin2selectat) {
                                if (!(pin1.c1 == pin2.c1 && pin1.c2 == pin2.c2)) {
                                    addLink(pin1, pin2);
                                }
                                pin1selectat = false;
                                pin2selectat = false;
                            }
                        }
                        else {
                            // 4) Poate e click pe o componentă
                            int idC2;
                            if (mousePeComponenta(m, idC2)) {
                                // selectez doar asta
                                for (int c = 1; c <= nr_Componente; c++) {
                                    Componente[c].esteSelectata = false;
                                }
                                Componente[idC2].esteSelectata = true;

                                // [MODIFICARE pentru drag] intrăm în drag
                                dragging = true;
                                compInDrag = idC2;
                                offsetX = m.x - Componente[idC2].x;
                                offsetY = m.y - Componente[idC2].y;
                            }
                            else {
                                // 5) Nimic => deselectez tot
                                pin1selectat = false;
                                pin2selectat = false;
                                for (int c = 1; c <= nr_Componente; c++) {
                                    Componente[c].esteSelectata = false;
                                }
                            }
                        }
                    }
                }
                // Meniu edit?
                if (showEditMeniu) {
                    Vector2f mp((float)Mouse::getPosition(window).x, (float)Mouse::getPosition(window).y);
                    if (rectNume.contains(mp)) {
                        editingNume = true;
                        editingParametru = false;
                    }
                    else if (rectParam.contains(mp)) {
                        editingNume = false;
                        editingParametru = true;
                    }
                    else {
                        editingNume = false;
                        editingParametru = false;
                    }
                }
            }
            int newX = 0, newY = 0;
            // Mouse Moved
            // [MODIFICARE pentru drag] dacă suntem în stare de dragging, mutăm componenta
            if (ev.type == Event::MouseMoved) {
                if (dragging && compInDrag != -1) {
                    Vector2i m = Mouse::getPosition(window);
                    newX = m.x - offsetX;
                    newY = m.y - offsetY;
                    doMoveComponent(compInDrag, newX, newY);
                    updatePins(compInDrag);
                }
            }


            // CLICK STÂNGA ELIBERAT
            // [MODIFICARE pentru drag] oprim drag
            if (ev.type == Event::MouseButtonReleased && ev.mouseButton.button == Mouse::Left) {
                moveComponent(compInDrag, newX, newY);
                dragging = false;
                compInDrag = -1;
            }

            // KEY
            if (ev.type == Event::KeyPressed) {
                // scap un pic
                if (ev.key.code == Keyboard::Escape) {
                    showEditMeniu = false;
                    editingNume = false;
                    editingParametru = false;
                    char numep[2000];

                    Text numeprov(numep, font, 20);
                }
                if (showEditMeniu == false)
                {
                    if (ev.key.code == Keyboard::S) {
                        bool ctrlIsPressed = ev.key.control;
                        // Apelăm funcția de salvare
                        handleSaveCircuit(ctrlIsPressed);
                    }
                    if (ev.key.code == Keyboard::L) {
                        bool ctrlIsPressed = ev.key.control;
                        handleLoadCircuit(ctrlIsPressed);
                    }
                    // Undo/redo
                    if (ev.key.control && ev.key.code == Keyboard::Z) {
                        undo();
                    }
                    if (ev.key.control && ev.key.code == Keyboard::Y) {
                        redo();
                    }
                    if (ev.key.code == Keyboard::B) {
                        stareAplicatie = MENIU_PRINCIPAL;
                    }
                }


                // Deselectăm butoanele la orice operație
                for (int i = 0; i < nrButoane; i++) {
                    butoane[i].esteSelectat = false;
                }

                // Găsim comp selectat
                int compSel = selecteazaComponenta();

                // Rotire
                if (ev.key.code == Keyboard::R) {
                    if (compSel != -1) {
                        int newOrient = (Componente[compSel].orientare + 1) % 4;
                        rotateComponent(compSel, newOrient);
                    }
                }
                // Scale +
                if (ev.key.code == Keyboard::Add || ev.key.code == Keyboard::Equal) {
                    if (compSel != -1) {
                        float sc = Componente[compSel].scale * 1.2f;
                        scaleComponent(compSel, sc);
                    }
                }
                // Scale -
                if (ev.key.code == Keyboard::Subtract || ev.key.code == Keyboard::Hyphen) {
                    if (compSel != -1) {
                        float sc = Componente[compSel].scale * 0.8f;
                        scaleComponent(compSel, sc);
                    }
                }
                // Stergere (D)
                if (ev.key.code == Keyboard::D) {
                    if (compSel != -1) {
                        removeComponent(compSel);
                    }
                }
                // Edit (E)
                if (ev.key.code == Keyboard::E) {
                    if (compSel != -1) {
                        showEditMeniu = true;
                        idCompEdit = compSel;
                        editingNume = false;
                        editingParametru = false;
                    }
                }
            }

            // TEXT EDIT (pt meniu E)
            if (showEditMeniu && ev.type == Event::TextEntered) {
                char c = (char)ev.text.unicode;
                if (c == 27) {
                    showEditMeniu = false;
                    editingNume = false;
                    editingParametru = false;
                }
                else {
                    if (editingNume) {
                        if (c >= 32 && c <= 126) {
                            int len = (int)strlen(Componente[idCompEdit].nume);
                            if (len < 29) {
                                Componente[idCompEdit].nume[len] = c;
                                Componente[idCompEdit].nume[len + 1] = '\0';
                            }
                        }
                        if (c == 8) { // backspace
                            int len = (int)strlen(Componente[idCompEdit].nume);
                            if (len > 0) {
                                Componente[idCompEdit].nume[len - 1] = '\0';
                            }
                        }
                    }
                    if (editingParametru) {
                        if ((c >= '0' && c <= '9')) {
                            int val = c - '0';
                            Componente[idCompEdit].parametru =
                                Componente[idCompEdit].parametru * 10 + val;

                        }
                        if (c == 8) { // backspace
                            Componente[idCompEdit].parametru =
                                floor(Componente[idCompEdit].parametru / 10.f);
                        }
                    }
                }
            }
        } // pollEvent

        // Hover butoane
        Vector2i pozMouse = Mouse::getPosition(window);
        hoverButon(butoane, nrButoane, pozMouse, window, tooltip);

        // RENDER
        window.clear(Color(74, 157, 164));
        if (stareAplicatie == MENIU_PRINCIPAL) {
            window.draw(wallpaperSprite);
            window.draw(butonStart.forma);
            window.draw(butonStart.text);
            window.draw(butonInfo.forma);
            window.draw(butonInfo.text);
            window.draw(butonExit.forma);
            window.draw(butonExit.text);
            window.draw(Meniu);

            if (Mouse::isButtonPressed(Mouse::Left)) {
                Vector2i m = Mouse::getPosition(window);
                if (butonStart.forma.getGlobalBounds().contains((float)m.x, (float)m.y)) {
                    stareAplicatie = APLICATIE_LUCRU;
                }
                else if (butonExit.forma.getGlobalBounds().contains((float)m.x, (float)m.y)) {
                    window.close();
                }
                else if (butonInfo.forma.getGlobalBounds().contains((float)m.x, (float)m.y)) {
                    stareAplicatie = INFORMATII;
                }
            }
        }
        else if (stareAplicatie == APLICATIE_LUCRU) {

            // 1) Butoane
            for (int i = 0; i <= nrButoane; i++) {
                RectangleShape r(Vector2f(butoane[i].latime, butoane[i].inaltime));
                r.setPosition(butoane[i].x, butoane[i].y);

                Color c(150, 150, 150);
                if (i < nrButoane) {
                    if (butoane[i].esteSelectat) c = Color(100, 100, 255);
                    else if (butoane[i].esteHover) c = Color(200, 150, 255);
                }
                else if (i == nrButoane) {
                    if (butoane[i].esteSelectat) c = Color(184, 15, 10);
                    else if (butoane[i].esteHover) c = Color(240, 128, 128);
                    r.setTexture(&gunoiTexture);

                }


                r.setFillColor(c);
                window.draw(r);

                float fb = 7.0f;
                float cx = butoane[i].x + butoane[i].latime / 2.f;
                float cy = butoane[i].y + butoane[i].inaltime / 2.f;

                deseneazaComponenta(window, tipuriPiese[butoane[i].indexPiesa], cx, cy, fb, 0, 0);
            }

            // 2) Contur mediu
            window.draw(conturMediu);

            // 3) Componente
            for (int i = 1; i <= nr_Componente; i++) {
                // Desenez piesa
                deseneazaComponenta(window, tipuriPiese[Componente[i].tip], (float)Componente[i].x, (float)Componente[i].y, Componente[i].scale, Componente[i].orientare, Componente[i].esteSelectata);

                // Pini
                for (int p = 0; p < Componente[i].nrPini; p++) {
                    CircleShape c(5);
                    c.setPosition(Componente[i].pini[p].c1 - 5.f, Componente[i].pini[p].c2 - 5.f);
                    c.setFillColor(Color(100, 100, 100));
                    if(Componente[i].esteSelectata==true)
                        c.setOutlineColor(Color(241, 64, 64));
                    else
                        c.setOutlineColor(Color::White);
                    c.setOutlineThickness(1.f);
                    window.draw(c);
                }

                // Dacă e selectată, desenăm un mic contur galben
                /*if (Componente[i].esteSelectata) {
                    RectangleShape selRect(Vector2f(10.f * Componente[i].scale, 10.f * Componente[i].scale));
                    selRect.setPosition(Componente[i].x - 5.f * Componente[i].scale, Componente[i].y - 5.f * Componente[i].scale);
                    selRect.setFillColor(Color::Transparent);
                    selRect.setOutlineColor(Color::Yellow);
                    selRect.setOutlineThickness(1.f);
                    window.draw(selRect);
                }*/
            }

            // 4) Legături
           // 4) Legături
            float thickness = 3.0f; // Ajustează această valoare pentru a mări sau micșora grosimea

            for (int i = 1; i <= nr_Legaturi; i++) {
                float x1 = (float)Legaturi[i].p1.c1;
                float y1 = (float)Legaturi[i].p1.c2;
                float x2 = (float)Legaturi[i].p2.c1;
                float y2 = (float)Legaturi[i].p2.c2;

                // ===== Segmentul Vertical =====
                float lengthV = std::abs(y2 - y1);  // lungimea liniei verticale
                sf::RectangleShape rectVertical(sf::Vector2f(thickness, lengthV));
                rectVertical.setFillColor(sf::Color::Red);

                // Setăm originea pe centrul grosimii, ca să fie aliniată corect pe x
                rectVertical.setOrigin(thickness / 2.f, 0.f);

                // Poziționăm dreptunghiul la x = x1 și la y = min(y1, y2) (linia începe de sus dacă y1 < y2)
                rectVertical.setPosition(x1, std::min(y1, y2));

                // ===== Segmentul Orizontal =====
                float lengthH = std::abs(x2 - x1);  // lungimea liniei orizontale
                sf::RectangleShape rectHorizontal(sf::Vector2f(lengthH, thickness));
                rectHorizontal.setFillColor(sf::Color::Red);

                // Setăm originea pe centrul grosimii, ca să fie aliniată corect pe y
                rectHorizontal.setOrigin(0.f, thickness / 2.f);

                // Poziționăm dreptunghiul la x = min(x1, x2) și la y = y2 (linia începe de la stânga dacă x1 < x2)
                rectHorizontal.setPosition(std::min(x1, x2), y2);

                // Desenăm dreptunghiurile (liniile) în fereastră
                window.draw(rectVertical);
                window.draw(rectHorizontal);
            }

            // 5) Meniu edit
            if (showEditMeniu) {
                RectangleShape menuRect(Vector2f(300.f, 150.f));
                menuRect.setPosition((float)window.getSize().x - 310.f, 100.f);
                menuRect.setFillColor(Color(80, 80, 80));
                window.draw(menuRect);

                // Nume
                RectangleShape campNume(Vector2f(280.f, 40.f));
                campNume.setPosition((float)window.getSize().x - 300.f, 110.f);
                campNume.setFillColor(editingNume ? Color(120, 120, 120) : Color(100, 100, 100));
                window.draw(campNume);

                rectNume = FloatRect(campNume.getPosition().x,
                    campNume.getPosition().y,
                    campNume.getSize().x,
                    campNume.getSize().y);

                Text txtNume(Componente[idCompEdit].nume, font, 20);
                txtNume.setFillColor(Color::White);
                txtNume.setPosition(campNume.getPosition().x + 5.f,
                    campNume.getPosition().y + 5.f);
                window.draw(txtNume);

                // Param
                RectangleShape campParam(Vector2f(280.f, 40.f));
                campParam.setPosition((float)window.getSize().x - 300.f, 160.f);
                campParam.setFillColor(editingParametru ? Color(120, 120, 120) : Color(100, 100, 100));
                window.draw(campParam);

                rectParam = FloatRect(campParam.getPosition().x,
                    campParam.getPosition().y,
                    campParam.getSize().x,
                    campParam.getSize().y);

                char buf[64];
                sprintf_s(buf, "%.2f", Componente[idCompEdit].parametru);
                Text txtParam(buf, font, 20);
                txtParam.setFillColor(Color::White);
                txtParam.setPosition(campParam.getPosition().x + 5.f,
                    campParam.getPosition().y + 5.f);
                window.draw(txtParam);
            }

            // 6) Tooltip
            if (indexButonCuHover >= 0) {
                window.draw(tooltip);
            }
        }
        else {
            // Fundal pentru secțiunea de informații
            RectangleShape background(Vector2f(window.getSize().x, window.getSize().y));
            background.setFillColor(Color(30, 30, 30));
            window.draw(background);

            // Text pentru titlu
            Text titlu("Informatii despre control", font, 40);
            titlu.setFillColor(Color::White);
            titlu.setPosition((window.getSize().x - titlu.getGlobalBounds().width) / 2, 50);
            window.draw(titlu);

            // Dreptunghiuri pentru informații


            RectangleShape rect1(Vector2f(400, 150)); //R
            rect1.setFillColor(Color::Transparent);
            rect1.setOutlineColor(Color::White);
            rect1.setOutlineThickness(2);
            rect1.setPosition(window.getSize().x / 4 - rect1.getSize().x / 2, window.getSize().y / 2 - 60);
            window.draw(rect1);


            RectangleShape rect3(Vector2f(400, 150)); //Click stanga apasat
            rect3.setFillColor(Color::Transparent);
            rect3.setOutlineColor(Color::White);
            rect3.setOutlineThickness(2);
            rect3.setPosition(window.getSize().x / 4 - rect1.getSize().x / 2, window.getSize().y / 2 - 60 + 200);
            window.draw(rect3);

            RectangleShape rect5(Vector2f(400, 150)); //E
            rect5.setFillColor(Color::Transparent);
            rect5.setOutlineColor(Color::White);
            rect5.setOutlineThickness(2);
            rect5.setPosition(window.getSize().x / 4 - rect5.getSize().x / 2, window.getSize().y / 2 - 60 - 200);
            window.draw(rect5);

            RectangleShape rect7(Vector2f(400, 150)); // Save/Load
            rect7.setFillColor(Color::Transparent);
            rect7.setOutlineColor(Color::White);
            rect7.setOutlineThickness(2);
            rect7.setPosition(window.getSize().x / 4 - rect7.getSize().x / 2, window.getSize().y / 2 - 60 + 400);
            window.draw(rect7);



            RectangleShape rect2(Vector2f(400, 150)); //+-
            rect2.setFillColor(Color::Transparent);
            rect2.setOutlineColor(Color::White);
            rect2.setOutlineThickness(2);
            rect2.setPosition(3 * window.getSize().x / 4 - rect2.getSize().x / 2, window.getSize().y / 2 - 60);
            window.draw(rect2);

            RectangleShape rect4(Vector2f(400, 150)); //D
            rect4.setFillColor(Color::Transparent);
            rect4.setOutlineColor(Color::White);
            rect4.setOutlineThickness(2);
            rect4.setPosition(3 * window.getSize().x / 4 - rect4.getSize().x / 2, window.getSize().y / 2 - 60 + 200);
            window.draw(rect4);

            RectangleShape rect6(Vector2f(400, 150)); //Undo+redo
            rect6.setFillColor(Color::Transparent);
            rect6.setOutlineColor(Color::White);
            rect6.setOutlineThickness(2);
            rect6.setPosition(3 * window.getSize().x / 4 - rect6.getSize().x / 2, window.getSize().y / 2 - 60 - 200);
            window.draw(rect6);

            RectangleShape rect8(Vector2f(400, 150));// s + d
            rect8.setFillColor(Color::Transparent);
            rect8.setOutlineColor(Color::White);
            rect8.setOutlineThickness(2);
            rect8.setPosition(3 * window.getSize().x / 4 - rect6.getSize().x / 2, window.getSize().y / 2 - 60 + 400);
            window.draw(rect8);


            // Text pentru primul dreptunghi
            Text text1("R -> Rotirea componentei\ncu 90 de grade", font, 20);
            text1.setFillColor(Color::White);
            text1.setPosition(rect1.getPosition().x + 10, rect1.getPosition().y + 20);
            window.draw(text1);


            Text text2("+/- -> Marirea/Micsorarea\ncomponentei", font, 20);
            text2.setFillColor(Color::White);
            text2.setPosition(rect2.getPosition().x + 10, rect2.getPosition().y + 20);
            window.draw(text2);

            Text text3("Click stanga apasat \n -> muta piesa", font, 20);
            text3.setFillColor(Color::White);
            text3.setPosition(rect3.getPosition().x + 10, rect3.getPosition().y + 20);
            window.draw(text3);

            Text text4("D -> stergerea piesei", font, 20);
            text4.setFillColor(Color::White);
            text4.setPosition(rect4.getPosition().x + 10, rect4.getPosition().y + 20);
            window.draw(text4);

            Text text5("E -> Editarea componentei", font, 20);
            text5.setFillColor(Color::White);
            text5.setPosition(rect5.getPosition().x + 10, rect5.getPosition().y + 20);
            window.draw(text5);

            Text text6(" Ctrl + Z/Y -> Undo/Redo", font, 20);
            text6.setFillColor(Color::White);
            text6.setPosition(rect6.getPosition().x + 10, rect6.getPosition().y + 20);
            window.draw(text6);

            Text text7(" Ctrl + S/L \n -> salvare/incarcare dintr-un\n fisier", font, 20);
            text7.setFillColor(Color::White);
            text7.setPosition(rect7.getPosition().x + 10, rect7.getPosition().y + 20);
            window.draw(text7);

            Text text8(" S/L  -> salvare/incarcare in\n  fisierul localcircuit.txt", font, 20);
            text8.setFillColor(Color::White);
            text8.setPosition(rect8.getPosition().x + 10, rect8.getPosition().y + 20);
            window.draw(text8);


            
            if (ev.type == Event::KeyPressed) {

                if (ev.key.code == Keyboard::B) {
                    stareAplicatie = MENIU_PRINCIPAL;
                }
            }

        }

        window.display();
    }

    return 0;
}   