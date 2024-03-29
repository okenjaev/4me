#include "interp.h"

#include <Python.h>

#include "sys.h"
#include "buffer.h"
#include "editor.h"
#include "input.h"

static PyObject*
insert(PyObject *self, PyObject *args)
{
    fchar* c;
    if(!PyArg_ParseTuple(args, "s", &c))
        return NULL;

    while(*c)
    {
	if (*c == '\n')
	{
	    buffer_insert_row();
	}
	else
	{
	    buffer_insert_fchar(*c);
	}
	c++;
    }
    
    return Py_None;
}

static PyObject*
previous(PyObject* self, PyObject *args)
{
    fint32 t = 1;
    if (!PyArg_ParseTuple(args, "|i", &t))
	return NULL;

    for(fint32 i = 0; i < t; i++)
	buffer_cursor_previous();

    return Py_None;
}

static PyObject*
next(PyObject* self, PyObject *args)
{
    fint32 t = 1;
    if (!PyArg_ParseTuple(args, "|i", &t))
	return NULL;

    for(fint32 i = 0; i < t; i++)
	buffer_cursor_next();

    return Py_None;
}

static PyObject*
forward(PyObject* self, PyObject *args)
{
    fint32 t = 1;
    if (!PyArg_ParseTuple(args, "|i", &t))
	return NULL;

    for(fint32 i = 0; i < t; i++)
	buffer_cursor_forward();

    return Py_None;
}

static PyObject*
backward(PyObject* self, PyObject *args)
{
    fint32 t = 1;
    if (!PyArg_ParseTuple(args, "|i", &t))
	return NULL;

    for(fint32 i = 0; i < t; i++)
        buffer_cursor_backward();

    return Py_None;
}

static PyObject*
quit(PyObject* self, PyObject* Py_UNUSED(args))
{
    editor_exit();
    return Py_None;
}

static PyObject*
save(PyObject* self, PyObject* Py_UNUSED(args))
{
    buffer_save();
    return Py_None;
}

static PyObject*
open_file(PyObject* self, PyObject* args)
{
    fchar* s;
    if(!PyArg_ParseTuple(args, "s", &s))
        return NULL;

    buffer_open_file(s);
    
    return Py_None;
}

static PyObject*
set_kbd(PyObject* self, PyObject* args)
{
    PyObject *call_back = NULL;
    if (!PyArg_ParseTuple(args, "O:set_callback", &call_back))
    {
	return NULL;
    }

    if (!PyCallable_Check(call_back))
    {
	PyErr_SetString(PyExc_TypeError, "parameter must be callable");
	return NULL;
    }
    
    /* Py_XINCREF(call_back); */
    input_add_hotkey("cx", call_back);
    return Py_None;
}

static PyMethodDef fme_methods[] = {
    {"insert", insert, METH_VARARGS, "insert string in the current buffer" },
    {"previous", previous, METH_VARARGS, "moves cursor to previous line" },
    {"next", next, METH_VARARGS, "moves cursor to next line" },
    {"forward", forward, METH_VARARGS, "moves cursor to forward at the same line" },
    {"backward", backward, METH_VARARGS, "moves cursor to backward at the same line" },
    {"quit", quit, METH_NOARGS, "exit 4me" },
    {"save", save, METH_NOARGS, "save the current buffer" },
    {"open_file", open_file, METH_VARARGS, "opens file with relative path" },
    {"set_kbd", set_kbd, METH_VARARGS, "set keyboard binding" },
    {NULL, NULL, 0, NULL}
};

static PyModuleDef fme_module = {
    PyModuleDef_HEAD_INIT, "fme", NULL, -1, fme_methods,
    NULL, NULL, NULL, NULL
};

static PyObject*
PyInit_fme(void)
{
    return PyModule_Create(&fme_module);
}

void
interp_init(void)
{
    FILE *fp = fopen("py/init.py", "r");

    if (!fp)
    {
	return;
    }

    if (PyImport_AppendInittab("fme", &PyInit_fme) == -1 )
    {
	fprintf(stderr, "Error: could not fme\n");
	die("Error");
    }

    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    config.isolated = 1;

    PyStatus status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);

    if (PyStatus_Exception(status))
    {
	Py_ExitStatusException(status);
	fclose(fp);
	return;
    }

    PyRun_SimpleFile(fp, "py/init.py");

    fclose(fp);
}

void
interp_deinit()
{
    // TODO: research on finalize
    // https://docs.python.org/3/c-api/init_config.html
    
    if (Py_FinalizeEx() < 0)
    {
	die("py finalize ex");
    }
}

void
interp_call(void* func)
{
    PyObject_CallObject(func, NULL);
}

void
interp_release(void* func)
{
    Py_XDECREF((PyObject*)func);
}
