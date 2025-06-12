#include "daistd.h"

#include <stdio.h>

#include "dai_canvas.h"

bool
daistd_init(DaiVM* vm) {
    if (!dai_canvas_init(vm)) {
        fprintf(stderr, "Error: cannot initialize canvas\n");
        return false;
    }
    return true;
}
void
daistd_quit(DaiVM* vm) {
    dai_canvas_quit(vm);
}