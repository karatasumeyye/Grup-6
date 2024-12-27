#include <stdio.h>
#include <stdlib.h>

int main() {
    int number;
    if (scanf("%d", &number) != 1) {
        fprintf(stderr, "Hatalı giriş.\n");
        return 1;
    }
    printf("%d\n", number + 1);
    return 0;
}