#include "editor.h"

int
main(int argc, char *argv[])
{
    editor_init();

    if (argc >= 2)
    {
	editor_open(argv[1]);
    }

    while(1)
    {
	editor_draw_update();
	editor_input_update();
    }

    return 0;
}
