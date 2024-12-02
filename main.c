#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_SIZE 100

// Komut ve argümanları tokenize eden bir fonksiyon
void parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " ");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;  // Argümanların sonuna NULL koymak zorundayız
}

int main() {
    char input[MAX_INPUT_SIZE];  // Kullanıcı girişini saklamak için buffer
    char *args[MAX_ARG_SIZE];   // Argümanları saklamak için dizi
    pid_t bg_process = 0;       // Arka plan işlemlerini takip için

    while (1) {
        // Prompt yazdırma
        printf("> ");
        fflush(stdout);

        // Kullanıcı girdisini okuma
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
            // EOF (Ctrl+D) veya hata durumunda çıkış
            printf("\nExiting shell...\n");
            break;
        }

        // Girdinin sonundaki yeni satır karakterini temizleme
        input[strcspn(input, "\n")] = '\0';

        // Boş giriş kontrolü
        if (strlen(input) == 0) {
            continue;
        }

        // Quit komutu kontrolü
        if (strcmp(input, "quit") == 0) {
            if (bg_process > 0) {
                printf("Waiting for background process (PID: %d) to complete...\n", bg_process);
                // Arka plan işlemi bitene kadar bekle
                int status;
                waitpid(bg_process, &status, 0);
                printf("Background process completed. Exiting shell...\n");
            }
            printf("Exiting shell...\n");
            break;
        }

        // Arka plan komutu kontrolü (& işareti)
        int is_background = 0;
        if (input[strlen(input) - 1] == '&') {
            is_background = 1;
            input[strlen(input) - 1] = '\0';  // '&' karakterini kaldır
        }

        // Kullanıcı girişini tokenize et
        parse_input(input, args);

        // Çocuk proses oluşturma
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
        } else if (pid == 0) {
            // Çocuk proses
            execvp(args[0], args);  // Komutu ve argümanları çalıştır
            perror("Execution failed");  // Eğer execvp başarısız olursa
            exit(1);
        } else {
            // Ana proses
            if (is_background) {
                bg_process = pid;  // Arka plan prosesin PID'sini sakla
                printf("Background process started (PID: %d)\n", pid);
            } else {
                // Ön plan işlem için bekle
                int status;
                waitpid(pid, &status, 0);
            }
        }
    }

    return 0;
}
