/*
 * Copyright (c) 2004 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "Python.h"
#include "radix.h"

/* $Id$ */

/* Radix tree nodes */

typedef struct {
	PyObject_HEAD
	PyObject *user_attr;	/* User-specified attributes */
	radix_node_t *rn;	/* Actual radix node (pointer to parent) */
	char *prefix;		/* Text representation of prefix */
} RadixNodeObject;

static PyTypeObject RadixNode_Type;

static RadixNodeObject *
newRadixNodeObject(PyObject *arg)
{
	RadixNodeObject *self;
	self = PyObject_New(RadixNodeObject, &RadixNode_Type);
	if (self == NULL)
		return NULL;
	self->user_attr = NULL;
	self->rn = NULL;
	return self;
}

/* RadixNode methods */

static void
RadixNode_dealloc(RadixNodeObject *self)
{
	Py_XDECREF(self->user_attr);
	PyObject_Del(self);
}

static PyMethodDef RadixNode_methods[] = {
	{NULL, NULL} /* no methods */
};

static PyObject *
RadixNode_getattr(RadixNodeObject *self, char *name)
{
	/* XXX - specialcase prefix */
	if (self->user_attr != NULL) {
		PyObject *v = PyDict_GetItemString(self->user_attr, name);
		if (v != NULL) {
			Py_INCREF(v);
			return v;
		}
	}
	return Py_FindMethod(RadixNode_methods, (PyObject *)self, name);
}

static int
RadixNode_setattr(RadixNodeObject *self, char *name, PyObject *v)
{
	if (self->user_attr == NULL) {
		self->user_attr = PyDict_New();
		if (self->user_attr == NULL)
			return -1;
	}
	if (v == NULL) {
		int rv = PyDict_DelItemString(self->user_attr, name);
		if (rv < 0)
			PyErr_SetString(PyExc_AttributeError,
			        "delete non-existing Radix attribute");
		return rv;
	}
	else
		return PyDict_SetItemString(self->user_attr, name, v);
}

static PyTypeObject RadixNode_Type = {
	/* The ob_type field must be initialized in the module init function
	 * to be portable to Windows without using C++. */
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"radix.RadixNode",	/*tp_name*/
	sizeof(RadixNodeObject),/*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)RadixNode_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
	(getattrfunc)RadixNode_getattr, /*tp_getattr*/
	(setattrfunc)RadixNode_setattr, /*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,                      /*tp_methods*/
        0,                      /*tp_members*/
        0,                      /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        0,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,                      /*tp_new*/
        0,                      /*tp_free*/
        0,                      /*tp_is_gc*/
};
/* --------------------------------------------------------------------- */

typedef struct {
	PyObject_HEAD
	radix_tree_t *rt;	/* Actual radix tree */
} RadixObject;

static PyTypeObject Radix_Type;

static RadixObject *
newRadixObject(PyObject *arg)
{
	RadixObject *self;
	radix_tree_t *rt;

	if ((rt = New_Radix()) == NULL)
		return (NULL);
	if ((self = PyObject_New(RadixObject, &Radix_Type)) == NULL)
		return (NULL);
	self->rt = rt;
	return (self);
}

/* Radix methods */

static void
rt_dealloc_cb(radix_node_t *rn, void *cbctx)
{
	RadixNodeObject *node;

	if (rn->data != NULL) {
		node = rn->data;
		/*
		 * Decrement refcount on nodes and invalidate their 
		 * underlying parent.
		 * XXX this makes them useless unless i duplicate prefix into
		 * them
		 */
		Py_XDECREF(node);
		node->rn = NULL;
	}
}

static void
Radix_dealloc(RadixObject *self)
{
	Destroy_Radix(self->rt, rt_dealloc_cb, NULL);
	PyObject_Del(self);
}

static PyObject *
Radix_add(RadixObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":add"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Radix_del(RadixObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":del"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Radix_search_exact(RadixObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":search_exact"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Radix_search_best(RadixObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":search_best"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

#if 0
struct walk_ctx {
	RadixObject *self;
};

static void
walk_helper(radix_node_t *rn, void *cbctx)
{
	PyObject *arglist, *result;
	struct walk_ctx *ctx = (struct walk_ctx *)cbctx;

	arglist = Py_BuildValue("(i)", XXX);
	result = PyEval_CallObject(callback, arglist);
	Py_DECREF(arglist);	

	if (result == NULL)
		return (NULL);

	/* XXX Otherwise push result into list */
}

static PyObject *
Radix_walk(RadixObject *self, PyObject *args)
{
	PyObject *callback = NULL;

	if (!PyArg_ParseTuple(args, ":search_best"))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}
#endif

static PyMethodDef Radix_methods[] = {
	{"add",		(PyCFunction)Radix_add,		METH_VARARGS,
		PyDoc_STR("add() -> XXX")},
	{"del",		(PyCFunction)Radix_del,		METH_VARARGS,
		PyDoc_STR("del() -> XXX")},
	{"search_exact",(PyCFunction)Radix_search_exact,METH_VARARGS,
		PyDoc_STR("search_exact() -> XXX")},
	{"search_best",	(PyCFunction)Radix_search_best,	METH_VARARGS,
		PyDoc_STR("search_best() -> XXX")},
	{NULL,		NULL}		/* sentinel */
};

static PyTypeObject Radix_Type = {
	/* The ob_type field must be initialized in the module init function
	 * to be portable to Windows without using C++. */
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"radix.Radix",		/*tp_name*/
	sizeof(RadixObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)Radix_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,                      /*tp_methods*/
        0,                      /*tp_members*/
        0,                      /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        0,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,                      /*tp_new*/
        0,                      /*tp_free*/
        0,                      /*tp_is_gc*/
};
/* --------------------------------------------------------------------- */


/* Function of no arguments returning new Radix object */

static PyObject *
radix_Radix(PyObject *self, PyObject *args)
{
	RadixObject *rv;

	if (!PyArg_ParseTuple(args, ":Radix"))
		return NULL;
	rv = newRadixObject(args);
	if (rv == NULL)
		return NULL;
	return (PyObject *)rv;
}

/* ---------- */

/* List of functions defined in the module */

static PyMethodDef radix_methods[] = {
	{"Radix",	radix_Radix,		METH_VARARGS,
		PyDoc_STR("Radix() -> new radix object")},
	{NULL,		NULL}		/* sentinel */
};

PyDoc_STRVAR(module_doc,
"XXX");

PyMODINIT_FUNC
initradix(void)
{
	PyObject *m;

	if (PyType_Ready(&Radix_Type) < 0)
		return;
	if (PyType_Ready(&RadixNode_Type) < 0)
		return;
	m = Py_InitModule3("radix", radix_methods, module_doc);
	PyModule_AddStringConstant(m, "__version__", PROGVER);
}
