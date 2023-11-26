/*
* Trabalho Realizado pela Disciplina de Sistemas Operacionais I
* Data: 27/11/2023
* Professor: Marcos Paulo Moro
* Aluno: Mateus Souza Silva
*/

// Bibliotecas Utilizadas
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <windows.h>

#define TEXTO_MENU "-----------------------------------------------------------------\n\n\
1) Definir o tamanho da matriz. Exemplo 10.000 X 10.000\n\
2) Definir semente para o gerador de numeros aleatorios\n\
3) Preencher a matriz com numeros aleatorios\n\
4) Definir o tamanho das submatrizes\n\
5) Definir o numero de Threads\n\
6) Executar\n\
7) Visualizar o tempo de execucao e quantidade de numeros primos\n\
8) Encerrar\n\n\
-----------------------------------------------------------------"
#define TAM_MAX_RAND 99999999


// Estruturas das threads e submatrizes
using namespace std;

typedef struct {
    int id, numTh;
    time_t tempoProcessamento;
} PARAM;

typedef struct {
    int ini[2];
    int fim[2];
    bool processada;
} SUBMAT;

long int numPrimosSerial = 0;
atomic<long int> numPrimos(0);
vector<vector<long int>> matriz;
vector<HANDLE> hThreads;
vector<PARAM> params;
vector<SUBMAT> subMatrizes;
HANDLE iniciaThreads, secCritPrimo;

void criarMatriz(int l, int c) {
    matriz = vector<vector<long int>>(l, vector<long int>(c, 0));
}

bool inicializarMatriz(int l, int c, int semente) {
    srand(semente);

    if (l*c >= 0) {
        for (int i = 0; i < l; i++) {
            for (int j = 0; j < c; j++) {
                matriz[i][j] = rand() % TAM_MAX_RAND;
            }
        }
    }
    else {
        return false;
    }
    return true;
}

// As submatrizes são criadas a partir de um tamanho definido pelo usuário
// Utilizando uma lógica que evita que qualquer dado não seja adicionado
// nas submatrizes ao mesmo tempo que respeita a dimensão da matriz original
void criarSubMatrizes(long int linhas, long int colunas, int l, int c) {
    subMatrizes.clear();

    for (long int i = 0; i < linhas; i += l) {
        for (long int j = 0; j < colunas; j += c) {
            SUBMAT subMatriz;
            subMatriz.ini[0] = i;
            subMatriz.ini[1] = j;
            subMatriz.fim[0] = min(i + l - 1, linhas - 1);
            subMatriz.fim[1] = min(j + c - 1, colunas - 1);
            subMatriz.processada = false;

            subMatrizes.push_back(subMatriz);
        }
    }
}

// O algoritmo mais eficiente para verificar números primos
bool ehPrimo(long int v) {
    if (v <= 2) {
        return false;
    }
    for (int i = 2; i <= (int)sqrt(v); i++) {
        if (v % i == 0) {
            return false;
        }
    }
    return true;
}

// Função que têm como paramêtros inicio e o fim de uma submatriz
// Retorna a quantidade de primos na submatriz 
long int processaPrimos(int ini[2], int fim[2]) {
    long int cont = 0;

    for (int i = ini[0]; i <= fim[0]; i++) {
        for (int j = ini[1]; j <= fim[1]; j++) {
            if (ehPrimo(matriz[i][j])) cont++;
        }
    }
    return cont;
}

// Função que as Threads executam para processar a matriz, seguindo 
// o algoritmo de busca de serviço, onde quando uma thread termina
// busca a próxima submatriz livre
DWORD WINAPI processaSubMatriz(LPVOID paramF) {
    PARAM* p = (PARAM*)paramF;
    time_t tempoInicial, tempoFinal;
    long int cont = 0;

    WaitForSingleObject(iniciaThreads, INFINITE);

    tempoInicial = time(0);

    cont = processaPrimos(subMatrizes[p->id].ini, subMatrizes[p->id].fim);
    subMatrizes[p->id].processada = true;

    for (int i = p->id + p->numTh; i < (int)subMatrizes.size(); i += p->numTh) {
        if (!subMatrizes[i].processada)
        {
            subMatrizes[i].processada = true;

            cont += processaPrimos(subMatrizes[i].ini, subMatrizes[i].fim);
        }
    }
    WaitForSingleObject(secCritPrimo, INFINITE);
    numPrimos += cont;
    ReleaseMutex(secCritPrimo);

    tempoFinal = time(0);
    params[p->id].tempoProcessamento = tempoFinal - tempoInicial;

    ResetEvent(iniciaThreads);

    return 0;
}

void criarThreads(int numThreads) {
    PARAM add;

    params.clear();

    for (int i = 0; i < numThreads; i++) {
        add.id = i;
        add.tempoProcessamento = 0;
        add.numTh = numThreads;
        params.push_back(add);
    }

    for (int i = 0; i < numThreads; i++) {
        hThreads.push_back(CreateThread(NULL, 0, processaSubMatriz, &params[i], 0, NULL));
    }
}

time_t modoSerial(int l, int c) {
    long int cont = 0;
    time_t tempoInicial, tempoFinal;

    if (numPrimosSerial > 0) numPrimosSerial = 0;

    tempoInicial = time(0);

    for (int i = 0; i < l; i++) {
        for (int j = 0; j < c; j++) {
            if (ehPrimo(matriz[i][j])) cont++;
        }
    }

    numPrimosSerial = cont;

    tempoFinal = time(0);

    return tempoFinal - tempoInicial;
}

// Para a execução das threads foi utilizado um evento ao invés de 
// "acordar" todas as threads de uma vez
time_t modoParalelo() {
    time_t tempoInicial = 0, tempoFinal = 0;

    if (numPrimos > 0) numPrimos.store(0);

    tempoInicial = time(0);

    SetEvent(iniciaThreads);

    WaitForMultipleObjects((int)hThreads.size(), hThreads.data(), TRUE, INFINITE);

    tempoFinal = time(0);

    return tempoFinal - tempoInicial;
}

void executar(time_t *tempoTotalSerial, time_t *tempoTotalParalelo, int linha,
    int coluna, bool *serialExec, bool *paraleloExec) 
{
    short int modo;
    bool ctrl = true;

    do
    {
        cout << "1 -> Execucao Serial" << endl << "2 -> Execucao Paralela" << endl <<
            "3 -> Voltar ao menu principal" << endl;
        cin >> modo;

        switch (modo) {
        case 1:
            *tempoTotalSerial = modoSerial(linha, coluna);
            *serialExec = true;
            break;
        case 2:
            *tempoTotalParalelo = modoParalelo();
            *paraleloExec = true;
            break;
        case 3:
            ctrl = false;
            break;
        }
    }
    while (ctrl);
}
void relatorioSerial(time_t tempoTotalSerial) {
    cout << endl << "Tempo de execucao: " << tempoTotalSerial << "s" << endl;
    cout << "Quantidade de numeros primos:" << numPrimosSerial << endl;
}

void relatorioParalelo(time_t tempoTotalParalelo) {
    cout << endl << "Processamentos das Threads Individualmente" << endl << endl;

    for (auto i : params) {
        cout << "Processamento Thread " << i.id << ":" << i.tempoProcessamento << "s" << endl;
    }

    cout << "-----------------------------------------------------------------" << endl;
    cout << "Quantidade de numeros primos:" << numPrimos << endl;
    cout << "Tempo Total: " << tempoTotalParalelo << "s" << endl;
}

void naoExecutadoStr() {
    cout << "-----------------------------------------------------------------" << endl << endl;
    cout << "-----------------------NAO EXECUTADO-----------------------------" << endl << endl;
    cout << "-----------------------------------------------------------------" << endl;
}

void relatorio(time_t tempoTotalSerial, time_t tempoTotalParalelo, bool serialExec, bool paraleloExec) {
    cout << "--------------------Relatorio execucao Serial--------------------" << endl;
    if (serialExec)
        relatorioSerial(tempoTotalSerial);
    else
        naoExecutadoStr();

    cout << endl << "--------------------------------X--------------------------------" << endl << endl;

    cout << "-------------------Relatorio execucao Paralelo-------------------" << endl;
    if (paraleloExec)
        relatorioParalelo(tempoTotalParalelo);
    else
        naoExecutadoStr();
}

void limpaMatriz(int numElementos) {
    for (int i = 0; i < numElementos; i++)
        matriz[i].clear();
    matriz.clear();
}

void encerrar(int numElementos) {
    for (int i = 0; i < hThreads.size(); i++)
        CloseHandle(hThreads[i]);

    limpaMatriz(numElementos);
}

//Função que organiza as execuções baseada em menu
// Todos os tratamentos necessários foram feitos nesta função
// Fazendo com que o usuário apenas execute na ordem correta
void menu() {
    time_t tempoTotalSerial = 0,  tempoTotalParalelo = 0;
    int op, semente = 0, numThreads = 0, linha = 0, coluna = 0, subMatL, subMatC;
    bool ctrl = true, paraleloExec = false, serialExec = false, matrizCriada = false;
    bool initM =  false, relatPronto = false, subMatCriada = false, threadsCriada = false;


    while (ctrl) {
        cout << TEXTO_MENU << endl;
        cin >> op;

        system("cls");

        switch (op) {
        case 1:
            if (initM)
                limpaMatriz(coluna);

            do {
                cout << "\nDigite a dimensão da matriz, ordem LXC:";
                cin >> linha >> coluna;
            } while (linha <= 0 or coluna <= 0);

            criarMatriz(linha, coluna);
            matrizCriada = true;
            break;
        case 2:
            do {
                cout << "\nDigite a semente que deseja:";
                cin >> semente;
            } while (semente <= 0);
            break;
        case 3:
            initM = inicializarMatriz(linha, coluna, semente);
            if (initM)
                cout << "Matriz inicializada com numeros aleatorios com sucesso" << endl;
            else
                cout << "Dimensão da matriz ainda nao definida ou invalida" << endl;
            break;
        case 4:
            if (matrizCriada) {
                do {
                    cout << "\nDigite o tamanho das submatrizes(LXC):" << endl;
                    cin >> subMatL >> subMatC;
                } while (subMatL <= 0 or subMatC <= 0);

                criarSubMatrizes(linha, coluna, subMatL, subMatC);
                cout << "Submatrizes criadas com sucesso" << endl;
                subMatCriada = true;
            }
            else
                cout << "Matriz nao inicializada" << endl;
            break;
        case 5:
            do {
                cout << "\nDigite o numero de threads desejavel:" << endl;
                cin >> numThreads;
            } while (numThreads <= 0);

            criarThreads(numThreads);
            threadsCriada = true;
            break;
        case 6:
            if(matrizCriada and initM and subMatCriada and threadsCriada)
            {
                executar(&tempoTotalSerial, &tempoTotalParalelo, linha, coluna, &serialExec, &paraleloExec);
                relatPronto = true;
            }
            break;
        case 7:
            if (relatPronto)
                relatorio(tempoTotalSerial, tempoTotalParalelo, serialExec, paraleloExec);
            else
                cout << "Algoritmo nao executado" << endl;
            break;
        case 8:
            encerrar(coluna);
            ctrl = false;
            break;
        default:
            cout << "Opcao Invalida" << endl;
        }
    }
}

int main() {
    iniciaThreads = CreateEvent(NULL, TRUE, FALSE, NULL);
    secCritPrimo = CreateMutex(NULL, FALSE, NULL);

    menu();

    return 0;
}
