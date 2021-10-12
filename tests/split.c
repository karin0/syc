#include "stdio.h"

int main() {
    for (int x; scanf("%d", &x) == 1; )
        printf("%d\n", x);
    return 0;
}
