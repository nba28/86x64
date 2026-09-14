#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static void shim_note(const char *sym) {
  static const char *seen[2048]; static int n;
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < n; i++)
    if (seen[i] == sym) { pthread_mutex_unlock(&mtx); return; }
  if (n < 2048) seen[n++] = sym;
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "Python", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long PyArg_ParseTuple(long a, long b, long c_, long d, long e, long f) __asm("_PyArg_ParseTuple");
long PyArg_ParseTuple(long a, long b, long c_, long d, long e, long f) { shim_note("_PyArg_ParseTuple"); return 0; }

long PyArg_UnpackTuple(long a, long b, long c_, long d, long e, long f) __asm("_PyArg_UnpackTuple");
long PyArg_UnpackTuple(long a, long b, long c_, long d, long e, long f) { shim_note("_PyArg_UnpackTuple"); return 0; }

long PyBool_FromLong(long a, long b, long c_, long d, long e, long f) __asm("_PyBool_FromLong");
long PyBool_FromLong(long a, long b, long c_, long d, long e, long f) { shim_note("_PyBool_FromLong"); return 0; }

long PyCFunction_Type(long a, long b, long c_, long d, long e, long f) __asm("_PyCFunction_Type");
long PyCFunction_Type(long a, long b, long c_, long d, long e, long f) { shim_note("_PyCFunction_Type"); return 0; }

long PyCObject_FromVoidPtr(long a, long b, long c_, long d, long e, long f) __asm("_PyCObject_FromVoidPtr");
long PyCObject_FromVoidPtr(long a, long b, long c_, long d, long e, long f) { shim_note("_PyCObject_FromVoidPtr"); return 0; }

long PyCObject_Import(long a, long b, long c_, long d, long e, long f) __asm("_PyCObject_Import");
long PyCObject_Import(long a, long b, long c_, long d, long e, long f) { shim_note("_PyCObject_Import"); return 0; }

long PyDict_GetItem(long a, long b, long c_, long d, long e, long f) __asm("_PyDict_GetItem");
long PyDict_GetItem(long a, long b, long c_, long d, long e, long f) { shim_note("_PyDict_GetItem"); return 0; }

long PyDict_New(long a, long b, long c_, long d, long e, long f) __asm("_PyDict_New");
long PyDict_New(long a, long b, long c_, long d, long e, long f) { shim_note("_PyDict_New"); return 0; }

long PyDict_SetItem(long a, long b, long c_, long d, long e, long f) __asm("_PyDict_SetItem");
long PyDict_SetItem(long a, long b, long c_, long d, long e, long f) { shim_note("_PyDict_SetItem"); return 0; }

long PyDict_SetItemString(long a, long b, long c_, long d, long e, long f) __asm("_PyDict_SetItemString");
long PyDict_SetItemString(long a, long b, long c_, long d, long e, long f) { shim_note("_PyDict_SetItemString"); return 0; }

long PyErr_Clear(long a, long b, long c_, long d, long e, long f) __asm("_PyErr_Clear");
long PyErr_Clear(long a, long b, long c_, long d, long e, long f) { shim_note("_PyErr_Clear"); return 0; }

long PyErr_Occurred(long a, long b, long c_, long d, long e, long f) __asm("_PyErr_Occurred");
long PyErr_Occurred(long a, long b, long c_, long d, long e, long f) { shim_note("_PyErr_Occurred"); return 0; }

long PyErr_SetString(long a, long b, long c_, long d, long e, long f) __asm("_PyErr_SetString");
long PyErr_SetString(long a, long b, long c_, long d, long e, long f) { shim_note("_PyErr_SetString"); return 0; }

long PyExc_AttributeError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_AttributeError");
long PyExc_AttributeError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_AttributeError"); return 0; }

long PyExc_IOError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_IOError");
long PyExc_IOError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_IOError"); return 0; }

long PyExc_IndexError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_IndexError");
long PyExc_IndexError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_IndexError"); return 0; }

long PyExc_MemoryError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_MemoryError");
long PyExc_MemoryError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_MemoryError"); return 0; }

long PyExc_NameError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_NameError");
long PyExc_NameError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_NameError"); return 0; }

long PyExc_OverflowError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_OverflowError");
long PyExc_OverflowError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_OverflowError"); return 0; }

long PyExc_RuntimeError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_RuntimeError");
long PyExc_RuntimeError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_RuntimeError"); return 0; }

long PyExc_SyntaxError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_SyntaxError");
long PyExc_SyntaxError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_SyntaxError"); return 0; }

long PyExc_SystemError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_SystemError");
long PyExc_SystemError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_SystemError"); return 0; }

long PyExc_TypeError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_TypeError");
long PyExc_TypeError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_TypeError"); return 0; }

long PyExc_ValueError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_ValueError");
long PyExc_ValueError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_ValueError"); return 0; }

long PyExc_ZeroDivisionError(long a, long b, long c_, long d, long e, long f) __asm("_PyExc_ZeroDivisionError");
long PyExc_ZeroDivisionError(long a, long b, long c_, long d, long e, long f) { shim_note("_PyExc_ZeroDivisionError"); return 0; }

long PyFloat_AsDouble(long a, long b, long c_, long d, long e, long f) __asm("_PyFloat_AsDouble");
long PyFloat_AsDouble(long a, long b, long c_, long d, long e, long f) { shim_note("_PyFloat_AsDouble"); return 0; }

long PyFloat_FromDouble(long a, long b, long c_, long d, long e, long f) __asm("_PyFloat_FromDouble");
long PyFloat_FromDouble(long a, long b, long c_, long d, long e, long f) { shim_note("_PyFloat_FromDouble"); return 0; }

long PyFloat_Type(long a, long b, long c_, long d, long e, long f) __asm("_PyFloat_Type");
long PyFloat_Type(long a, long b, long c_, long d, long e, long f) { shim_note("_PyFloat_Type"); return 0; }

long PyInstance_NewRaw(long a, long b, long c_, long d, long e, long f) __asm("_PyInstance_NewRaw");
long PyInstance_NewRaw(long a, long b, long c_, long d, long e, long f) { shim_note("_PyInstance_NewRaw"); return 0; }

long PyInt_AsLong(long a, long b, long c_, long d, long e, long f) __asm("_PyInt_AsLong");
long PyInt_AsLong(long a, long b, long c_, long d, long e, long f) { shim_note("_PyInt_AsLong"); return 0; }

long PyInt_FromLong(long a, long b, long c_, long d, long e, long f) __asm("_PyInt_FromLong");
long PyInt_FromLong(long a, long b, long c_, long d, long e, long f) { shim_note("_PyInt_FromLong"); return 0; }

long PyList_Append(long a, long b, long c_, long d, long e, long f) __asm("_PyList_Append");
long PyList_Append(long a, long b, long c_, long d, long e, long f) { shim_note("_PyList_Append"); return 0; }

long PyList_New(long a, long b, long c_, long d, long e, long f) __asm("_PyList_New");
long PyList_New(long a, long b, long c_, long d, long e, long f) { shim_note("_PyList_New"); return 0; }

long PyList_SetItem(long a, long b, long c_, long d, long e, long f) __asm("_PyList_SetItem");
long PyList_SetItem(long a, long b, long c_, long d, long e, long f) { shim_note("_PyList_SetItem"); return 0; }

long PyLong_AsDouble(long a, long b, long c_, long d, long e, long f) __asm("_PyLong_AsDouble");
long PyLong_AsDouble(long a, long b, long c_, long d, long e, long f) { shim_note("_PyLong_AsDouble"); return 0; }

long PyLong_AsLong(long a, long b, long c_, long d, long e, long f) __asm("_PyLong_AsLong");
long PyLong_AsLong(long a, long b, long c_, long d, long e, long f) { shim_note("_PyLong_AsLong"); return 0; }

long PyLong_AsUnsignedLong(long a, long b, long c_, long d, long e, long f) __asm("_PyLong_AsUnsignedLong");
long PyLong_AsUnsignedLong(long a, long b, long c_, long d, long e, long f) { shim_note("_PyLong_AsUnsignedLong"); return 0; }

long PyLong_FromUnsignedLong(long a, long b, long c_, long d, long e, long f) __asm("_PyLong_FromUnsignedLong");
long PyLong_FromUnsignedLong(long a, long b, long c_, long d, long e, long f) { shim_note("_PyLong_FromUnsignedLong"); return 0; }

long PyLong_FromVoidPtr(long a, long b, long c_, long d, long e, long f) __asm("_PyLong_FromVoidPtr");
long PyLong_FromVoidPtr(long a, long b, long c_, long d, long e, long f) { shim_note("_PyLong_FromVoidPtr"); return 0; }

long PyModule_AddObject(long a, long b, long c_, long d, long e, long f) __asm("_PyModule_AddObject");
long PyModule_AddObject(long a, long b, long c_, long d, long e, long f) { shim_note("_PyModule_AddObject"); return 0; }

long PyModule_GetDict(long a, long b, long c_, long d, long e, long f) __asm("_PyModule_GetDict");
long PyModule_GetDict(long a, long b, long c_, long d, long e, long f) { shim_note("_PyModule_GetDict"); return 0; }

long PyObject_Call(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_Call");
long PyObject_Call(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_Call"); return 0; }

long PyObject_CallFunctionObjArgs(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_CallFunctionObjArgs");
long PyObject_CallFunctionObjArgs(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_CallFunctionObjArgs"); return 0; }

long PyObject_Free(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_Free");
long PyObject_Free(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_Free"); return 0; }

long PyObject_GenericGetAttr(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_GenericGetAttr");
long PyObject_GenericGetAttr(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_GenericGetAttr"); return 0; }

long PyObject_GetAttr(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_GetAttr");
long PyObject_GetAttr(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_GetAttr"); return 0; }

long PyObject_GetAttrString(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_GetAttrString");
long PyObject_GetAttrString(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_GetAttrString"); return 0; }

long PyObject_Init(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_Init");
long PyObject_Init(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_Init"); return 0; }

long PyObject_IsTrue(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_IsTrue");
long PyObject_IsTrue(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_IsTrue"); return 0; }

long PyObject_Malloc(long a, long b, long c_, long d, long e, long f) __asm("_PyObject_Malloc");
long PyObject_Malloc(long a, long b, long c_, long d, long e, long f) { shim_note("_PyObject_Malloc"); return 0; }

long PyString_AsString(long a, long b, long c_, long d, long e, long f) __asm("_PyString_AsString");
long PyString_AsString(long a, long b, long c_, long d, long e, long f) { shim_note("_PyString_AsString"); return 0; }

long PyString_AsStringAndSize(long a, long b, long c_, long d, long e, long f) __asm("_PyString_AsStringAndSize");
long PyString_AsStringAndSize(long a, long b, long c_, long d, long e, long f) { shim_note("_PyString_AsStringAndSize"); return 0; }

long PyString_ConcatAndDel(long a, long b, long c_, long d, long e, long f) __asm("_PyString_ConcatAndDel");
long PyString_ConcatAndDel(long a, long b, long c_, long d, long e, long f) { shim_note("_PyString_ConcatAndDel"); return 0; }

long PyString_Format(long a, long b, long c_, long d, long e, long f) __asm("_PyString_Format");
long PyString_Format(long a, long b, long c_, long d, long e, long f) { shim_note("_PyString_Format"); return 0; }

long PyString_FromFormat(long a, long b, long c_, long d, long e, long f) __asm("_PyString_FromFormat");
long PyString_FromFormat(long a, long b, long c_, long d, long e, long f) { shim_note("_PyString_FromFormat"); return 0; }

long PyString_FromString(long a, long b, long c_, long d, long e, long f) __asm("_PyString_FromString");
long PyString_FromString(long a, long b, long c_, long d, long e, long f) { shim_note("_PyString_FromString"); return 0; }

long PyString_FromStringAndSize(long a, long b, long c_, long d, long e, long f) __asm("_PyString_FromStringAndSize");
long PyString_FromStringAndSize(long a, long b, long c_, long d, long e, long f) { shim_note("_PyString_FromStringAndSize"); return 0; }

long PyTuple_GetItem(long a, long b, long c_, long d, long e, long f) __asm("_PyTuple_GetItem");
long PyTuple_GetItem(long a, long b, long c_, long d, long e, long f) { shim_note("_PyTuple_GetItem"); return 0; }

long PyTuple_New(long a, long b, long c_, long d, long e, long f) __asm("_PyTuple_New");
long PyTuple_New(long a, long b, long c_, long d, long e, long f) { shim_note("_PyTuple_New"); return 0; }

long PyTuple_SetItem(long a, long b, long c_, long d, long e, long f) __asm("_PyTuple_SetItem");
long PyTuple_SetItem(long a, long b, long c_, long d, long e, long f) { shim_note("_PyTuple_SetItem"); return 0; }

long PyTuple_Size(long a, long b, long c_, long d, long e, long f) __asm("_PyTuple_Size");
long PyTuple_Size(long a, long b, long c_, long d, long e, long f) { shim_note("_PyTuple_Size"); return 0; }

long PyType_IsSubtype(long a, long b, long c_, long d, long e, long f) __asm("_PyType_IsSubtype");
long PyType_IsSubtype(long a, long b, long c_, long d, long e, long f) { shim_note("_PyType_IsSubtype"); return 0; }

long PyType_Type(long a, long b, long c_, long d, long e, long f) __asm("_PyType_Type");
long PyType_Type(long a, long b, long c_, long d, long e, long f) { shim_note("_PyType_Type"); return 0; }

long PyInstance_Lookup(long a, long b, long c_, long d, long e, long f) __asm("__PyInstance_Lookup");
long PyInstance_Lookup(long a, long b, long c_, long d, long e, long f) { shim_note("__PyInstance_Lookup"); return 0; }

long PyObject_GetDictPtr(long a, long b, long c_, long d, long e, long f) __asm("__PyObject_GetDictPtr");
long PyObject_GetDictPtr(long a, long b, long c_, long d, long e, long f) { shim_note("__PyObject_GetDictPtr"); return 0; }

long PyWeakref_CallableProxyType(long a, long b, long c_, long d, long e, long f) __asm("__PyWeakref_CallableProxyType");
long PyWeakref_CallableProxyType(long a, long b, long c_, long d, long e, long f) { shim_note("__PyWeakref_CallableProxyType"); return 0; }

long PyWeakref_ProxyType(long a, long b, long c_, long d, long e, long f) __asm("__PyWeakref_ProxyType");
long PyWeakref_ProxyType(long a, long b, long c_, long d, long e, long f) { shim_note("__PyWeakref_ProxyType"); return 0; }

long Py_NoneStruct(long a, long b, long c_, long d, long e, long f) __asm("__Py_NoneStruct");
long Py_NoneStruct(long a, long b, long c_, long d, long e, long f) { shim_note("__Py_NoneStruct"); return 0; }

long Py_TrueStruct(long a, long b, long c_, long d, long e, long f) __asm("__Py_TrueStruct");
long Py_TrueStruct(long a, long b, long c_, long d, long e, long f) { shim_note("__Py_TrueStruct"); return 0; }
