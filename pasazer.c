#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/sem.h>

#define NA_STATEK 3
#define ZE_STATKU 1
#define ROZMIAR_PASAZERA (sizeof(pid_t))
#define MIEJSCE_NA_MOSTKU 0
#define SZLABAN 1

int mostek;  //kolejka komunikatow
int szlabany; //semafor

// Struktura pasażera
struct pasazer {
    long type;
    pid_t pas_pid;
};

// Obsługa sygnału SIGINT
void handler(int sig) {
    printf("Sygnal SIGINT! Koniec działania\n");
    exit(EXIT_SUCCESS);
}

// Funkcja tworząca semafor
void utworz_semafor(key_t klucz, int nr) {
    szlabany = semget(klucz, nr, 0600 | IPC_CREAT);
    if (szlabany == -1) {
        perror("Nie udalo sie utworzyc semafora");
        exit(EXIT_FAILURE);
    }
}

// Funkcja ustawiająca wartość semafora
void ustaw_wartosc_semafora(int wartosc, int nr) {
    if (semctl(szlabany, nr, SETVAL, wartosc) == -1) {
        perror("Blad ustawienia semafora");
        exit(EXIT_FAILURE);
    }
}

// Funkcja opuszczania semafora -1
void opusc_semafor(int nr) {
    struct sembuf op = {nr, -1, 0};
    if (semop(szlabany, &op, 1) == -1) {
        perror("Blad opuszczania semafora");
        exit(EXIT_FAILURE);
    }
}

// Funkcja podnoszenia semafora +1
void podnies_semafor(int nr) {
    struct sembuf op = {nr, 1, 0};
    if (semop(szlabany, &op, 1) == -1) {
        perror("Blad podnoszenia semafora");
        exit(EXIT_FAILURE);
    }
}


int main() {
    struct pasazer pass;
    int i=0;

    // Obsługa sygnałów
    signal(SIGINT, handler);

    // Połączenie z kolejką komunikatów
    while ((mostek = msgget(123, 0666)) == -1) {
        i++;
        if(i==12) exit(1); //jesli program kapitana nie zostanie włączony w przeciagu minuty, zamknij program
        sleep(5);
    }

    utworz_semafor(100,2);

    //5 pasażerów
    for(int i=0; i<21; i++) {
        if(fork() == 0) {  // Proces dziecka (pasażer)
            pass.type = NA_STATEK;
            pass.pas_pid = getpid();

            // Próba wejścia na statek
            printf("Do kolejki w rejs ustawił się pasażer %d!\n", pass.pas_pid);
            opusc_semafor(SZLABAN);
            opusc_semafor(MIEJSCE_NA_MOSTKU);
            if (msgsnd(mostek, &pass, ROZMIAR_PASAZERA, 0) == -1) {
                perror("Nie udalo sie wejsc na statek\n");
                exit(EXIT_FAILURE);
            }

            printf("Pasażer %d wszedł na mostek\n", pass.pas_pid);

            // Czekanie na zejście ze statku
            if (msgrcv(mostek, &pass, ROZMIAR_PASAZERA, ZE_STATKU, 0) == -1) {
                perror("Blad przy czekaniu na zejscie ze statku\n");
                exit(EXIT_FAILURE);
            }
            printf("Pasażer %d zszedłem na ląd\n", pass.pas_pid);
            podnies_semafor(MIEJSCE_NA_MOSTKU);

            exit(0);  // Kończymy proces dziecka
        }
    }

    // Czekanie na zakończenie wszystkich procesów dzieci
    for(int i=0; i<21; i++) {
        wait(NULL);
    }

    return 0;
}
