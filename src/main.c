#include "editor.h"
#include "py_int.h"

int
main(int argc, char *argv[])
{
    py_run(argv);

    editor_init();

    if (argc >= 2)
    {
	editor_open(argv[1]);
    }

    while(1)
    {
	editor_update();
    }

    return 0;
}
