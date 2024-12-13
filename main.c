// POSIX standartlarını kullanabilmek için gereken makro tanımı
// Bu tanım, POSIX.1-2008 standartlarında tanımlanan özellikleri kullanmamızı sağlar
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>// Sinyal işleme fonksiyonlarını kullanmak için gerekli başlık dosyası

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_SIZE 100

// Signal handler için gerekli değişkenler ve fonksiyonlar
volatile sig_atomic_t child_exit_status;  // Çocuk prosesin çıkış durumunu saklar
volatile pid_t child_pid;                 // Çocuk prosesin kimlik numarasını saklar

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
 
// & komutu için
void handle_sigchld(int sig) {
    (void)sig;  // Kullanılmayan parametre uyarısını engellemek için
    int status;    // Çocuk prosesin çıkış durumunu saklamak için
    pid_t pid;     // Çocuk prosesin PID'sini saklamak için
    
    // Sonlanan tüm çocuk prosesleri kontrol et
    // WNOHANG: Bekleyen çocuk yoksa hemen dön
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        child_pid = pid;
        // Proses normal bir şekilde çıkış yaptı mı kontrol et
        if (WIFEXITED(status)) {
            child_exit_status = WEXITSTATUS(status);  // Çıkış kodunu al
            printf("[%d] retval: %d\n", pid, child_exit_status);  // Sonucu yazdır
        }
    }
}

// Pipe işlemlerini kontrol eden yardımcı fonksiyon
void execute_piped_commands(char *input) {
    char *commands[MAX_ARG_SIZE];    // Her bir komut için dizi
    char *args[MAX_ARG_SIZE][MAX_ARG_SIZE];  // Her komutun argümanları için 2D dizi
    int cmd_count = 0;               // Toplam komut sayısı
    int pipe_count = 0;              // Pipe sayısı

    // Komutları ayır (pipe karakterine göre)
    char *cmd = strtok(input, "|");
    while (cmd != NULL && cmd_count < MAX_ARG_SIZE) {
        commands[cmd_count++] = cmd;
        cmd = strtok(NULL, "|");
    }
    pipe_count = cmd_count - 1;

    // Her bir komut için redirection kontrolü ve parse
    char *redirect_files[MAX_ARG_SIZE][2];  // [0] input, [1] output için
    for (int i = 0; i < cmd_count; i++) {
        redirect_files[i][0] = NULL;
        redirect_files[i][1] = NULL;
        
        // Input redirection kontrolü
        char *input_redirect = strstr(commands[i], "<");
        if (input_redirect) {
            *input_redirect = '\0';
            input_redirect++;
            while (*input_redirect == ' ') input_redirect++;
            redirect_files[i][0] = strdup(input_redirect);
            
            // Dosya adından sonraki olası boşlukları temizle
            char *end = redirect_files[i][0];
            while (*end && *end != ' ') end++;
            *end = '\0';
        }

        // Output redirection kontrolü
        char *output_redirect = strstr(commands[i], ">");
        if (output_redirect) {
            *output_redirect = '\0';
            output_redirect++;
            while (*output_redirect == ' ') output_redirect++;
            redirect_files[i][1] = strdup(output_redirect);
            
            // Dosya adından sonraki olası boşlukları temizle
            char *end = redirect_files[i][1];
            while (*end && *end != ' ') end++;
            *end = '\0';
        }

        // Komutu parse et
        parse_input(commands[i], args[i]);
    }

    // Pipe'ları oluştur
    int pipes[MAX_ARG_SIZE - 1][2];
    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("Pipe creation failed");
            return;
        }
    }

    // Her komut için bir proses oluştur
    for (int i = 0; i < cmd_count; i++) {
        pid_t pid = fork();

        if (pid == 0) {  // Çocuk proses
            // Input redirection
            if (redirect_files[i][0]) {
                int fd = open(redirect_files[i][0], O_RDONLY);
                if (fd < 0) {
                    perror("Input file open failed");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            // İlk komut değilse ve input redirection yoksa, önceki pipe'ın çıkışını stdin'e bağla
            else if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }

            // Output redirection
            if (redirect_files[i][1]) {
                int fd = open(redirect_files[i][1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("Output file open failed");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            // Son komut değilse ve output redirection yoksa, sonraki pipe'ın girişini stdout'a bağla
            else if (i < pipe_count) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Kullanılmayan pipe'ları kapat
            for (int j = 0; j < pipe_count; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Komutu çalıştır
            execvp(args[i][0], args[i]);
            perror("Command execution failed");
            exit(1);
        }
    }

    // Ana proses: Tüm pipe'ları kapat
    for (int i = 0; i < pipe_count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Memory cleanup
    for (int i = 0; i < cmd_count; i++) {
        if (redirect_files[i][0]) free(redirect_files[i][0]);
        if (redirect_files[i][1]) free(redirect_files[i][1]);
    }

    // Tüm çocuk proseslerin bitmesini bekle
    for (int i = 0; i < cmd_count; i++) {
        wait(NULL);
    }
}


int main() {
    char input[MAX_INPUT_SIZE];  // Kullanıcı girişini saklamak için buffer
    char *args[MAX_ARG_SIZE];   // Argümanları saklamak için dizi
    pid_t bg_process = 0;       // Arka plan işlemlerini takip için

    // Signal handler yapılandırması
    struct sigaction sa;                     // Sinyal işleyici yapısı
    sa.sa_handler = &handle_sigchld;         // Kullanılacak handler fonksiyonunu belirt
    sigemptyset(&sa.sa_mask);               // Sinyal maskesini temizle
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Handler bayraklarını ayarla
                                            // SA_RESTART: Sistem çağrılarının yeniden başlatılmasını sağlar
                                            // SA_NOCLDSTOP: Sadece sonlanan çocuklar için sinyal üret
    // Signal handler'ı sisteme kaydet
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror("sigaction");
        exit(1);
    }

    while (1) {  // Kullanıcı quit yazana kadar çalışır
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
        // Pipe kontrolü
        if (strchr(input, '|') != NULL) {
         execute_piped_commands(input);
         continue;
        }


        // Giriş yeniden yönlendirme kontrolü
        char *input_redirect = strstr(input, "<");
        char *redirect_file = NULL;  // Giriş dosyasının adını saklamak için

        if (input_redirect) {
            *input_redirect = '\0';  // Komut kısmını ayır
            input_redirect++;        // Dosya adını işaret et
            while (*input_redirect == ' ') input_redirect++;  // Boşlukları temizle

            redirect_file = input_redirect;  // Dosya adı olarak sakla

            // Dosyanın varlığını kontrol et
            if (access(redirect_file, F_OK) != 0) {
                printf("Giriş dosyası bulunamadı: %s\n", redirect_file);
                continue;
            }
        }

        // Çıkış yeniden yönlendirme kontrolü
        char *output_redirect = strstr(input, ">");
        char *redirect_out_file = NULL;

        if (output_redirect) {
            *output_redirect = '\0';  // '>' karakterini ayır ve komut kısmını sonlandır
            output_redirect++;        // '>' karakterinin hemen sonrasını işaret et
            while (*output_redirect == ' ')  // Boşlukları atla
                output_redirect++;
            redirect_out_file = output_redirect;
        }

        // Kullanıcı girişini tokenize et
        parse_input(input, args);

        // Çocuk proses oluşturma
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
        } else if (pid == 0) {
            // Çocuk proses
            if (redirect_file) {
                // Dosyayı aç ve stdin'i yönlendir
                int fd = open(redirect_file, O_RDONLY);
                if (fd < 0) {
                    perror("Dosya açılamadı");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);  // Standart giriş akışını yönlendir
                close(fd);              // Kullanılmayan dosya tanımlayıcısını kapat
            }
            if (redirect_out_file) {
                int fd_out = open(redirect_out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("Çıkış dosyası açılamadı"); 
                    exit(1);
                }
                dup2(fd_out, STDOUT_FILENO); // Standart çıktıyı dosyaya yönlendir
                close(fd_out); // Dosya tanımlayıcıyı kapat
            }
            
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