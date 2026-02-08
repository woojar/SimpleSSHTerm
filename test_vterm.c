#include <vterm.h>
#include <stdio.h>

int main() {
    VTerm *vt = vterm_new(24, 80);
    if (vt) {
        printf("libvterm initialized successfully!\n");
        vterm_free(vt);
        return 0;
    }
    return 1;
}
