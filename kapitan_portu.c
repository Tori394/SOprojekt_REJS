#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/sem.h>

pid_t pid_kapitana; 
int czas = 60; // Czas rejsu
int plyn = 1;  // Flaga działania programu
int szlabany;

void zakoncz_program(int sig) {
    printf("\nOtrzymano sygnał zakończenia (SIGINT). Kończenie programu...\n");
    plyn = 0;
}

// Funkcja pobierająca PID kapitana z FIFO
void pobierz_dane() {
    int fd = open("./fifo", O_RDONLY);
    if (fd == -1) {
        perror("Błąd otwarcia FIFO");
        exit(EXIT_FAILURE);
    }

    if (read(fd, &pid_kapitana, sizeof(pid_t)) == -1) {
        perror("Błąd odczytu PID");
        close(fd);
        exit(EXIT_FAILURE);
    }

    unlink("./fifo");
    close(fd);
}

void utworz_semafor(key_t klucz, int nr) {
    szlabany = semget(klucz, nr, 0600 | IPC_CREAT);
    if (szlabany == -1) {
        perror("Nie udalo sie utworzyc semafora");
        exit(EXIT_FAILURE);
    }
}

int sprawdz_wartosc_semafora(int nr) {
    int val = semctl(szlabany, nr, GETVAL);
    if (val == -1) {
        perror("Blad odczytu wartosci semafora");
        exit(EXIT_FAILURE);
    }
    return val;
}

// Funkcja wątku, który wysyła sygnały startu co określony czas
void* wyslij_sygnal_start(void* arg) {
    while (plyn) {
        sleep(czas/2); // Czekaj czas trwania rejsu
        if (sprawdz_wartosc_semafora(1)==0 && rand() % 2 == 1) { // Jesli statek zapelni sie przed czasem jest 50% szans, że kapitan wysle sygnał do wcześniejszego startu
            if (kill(pid_kapitana, SIGUSR1) == -1) {
                perror("Błąd wysyłania sygnału startu");
                exit(EXIT_FAILURE);
            }
        }
        else {
            sleep(czas/2);
            if (kill(pid_kapitana, SIGUSR1) == -1) {
                perror("Błąd wysyłania sygnału startu");
                exit(EXIT_FAILURE);
            }
        }
        printf("Wysłano sygnał startu do kapitana (PID: %d)\n", pid_kapitana);
    }
    return NULL;
}

// Funkcja wątku, który wypatruje burzy i wysyła sygnał zakończenia
void* wyslij_sygnal_stop(void* arg) {
    int czas_oczekiwania;
    while (plyn) {
        czas_oczekiwania = rand() % (czas * 2) + (czas / 2); // Losowanie czasu między czas/2 a czas*2 sekund
        sleep(czas_oczekiwania);

        if (rand() % 20 == 1) { // Szansa na wykrycie burzy (5%)
            printf("Nadchodzi burza! Kapitan portu nakazał zakończenie rejsów!\n");
            if (kill(pid_kapitana, SIGINT) == -1) {
                perror("Błąd wysyłania sygnału zakończenia (SIGINT)");
                exit(EXIT_FAILURE);
            }
            printf("Wysłano sygnał zakończenia (SIGINT) do kapitana (PID: %d)\n", pid_kapitana);
            plyn = 0; // Przerwanie działania programu
        }
    }
    return NULL;
}

int main() {
    pthread_t sygnal_start, sygnal_stop;

    // Inicjalizacja generatora liczb losowych
    srand(time(NULL));

    // Obsługa sygnału SIGINT
    signal(SIGINT, zakoncz_program);

    // Pobranie PID kapitana
    pobierz_dane();
    printf("Odczytano PID kapitana: %d\n", pid_kapitana);
    utworz_semafor(100,2);

    // Tworzenie wątku wysyłającego sygnał startu
    if (pthread_create(&sygnal_start, NULL, wyslij_sygnal_start, NULL) != 0) {
        perror("Błąd tworzenia wątku sygnału startu");
        exit(EXIT_FAILURE);
    }

    // Tworzenie wątku wypatrującego burzy
    if (pthread_create(&sygnal_stop, NULL, wyslij_sygnal_stop, NULL) != 0) {
        perror("Błąd tworzenia wątku sygnału stop");
        exit(EXIT_FAILURE);
    }

    // Oczekiwanie na zakończenie wątków
    pthread_join(sygnal_start, NULL);
    pthread_join(sygnal_stop, NULL);

    printf("Program zakończony poprawnie.\n");
    return 0;
}
