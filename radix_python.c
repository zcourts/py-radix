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

/* ------------------------------------------------------------------------ */

/* RadixNode: tree nodes */

typedef struct {
	PyObject_HEAD
	PyObject *user_attr;	/* User-specified attributes */
	radix_node_t *rn;	/* Actual radix node (pointer to parent) */
	char *network;		/* Text representation of network */
	char *prefix;		/* Text representation of prefix */
	int prefixlen;
	int family;
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
	if (self->network != NULL)
		free(self->network);
	if (self->prefix != NULL)
		free(self->prefix);
	if (self->rn != NULL)
		self->rn->data = NULL;
	Py_XDECREF(self->user_attr);
	PyObject_Del(self);
}

static PyObject *
RadixNode_getattr(RadixNodeObject *self, char *name)
{
	PyObject *user_obj;

	if (strcmp(name, "network") == 0)
		return PyString_FromString(self->network);
	else if (strcmp(name, "prefix") == 0)
		return PyString_FromString(self->prefix);
	else if (strcmp(name, "prefixlen") == 0)
		return PyInt_FromLong(self->prefixlen);
	else if (strcmp(name, "family") == 0)
		return PyInt_FromLong(self->family);

	if (self->user_attr != NULL) {
		user_obj = PyDict_GetItemString(self->user_attr, name);
		Py_XINCREF(user_obj);
		return user_obj;
	}
	return NULL;
}

static int
RadixNode_setattr(RadixNodeObject *self, char *name, PyObject *v)
{
	int rv;

	if (strcmp(name, "network") == 0 || strcmp(name, "prefix") == 0 ||
	    strcmp(name, "prefixlen") == 0 || strcmp(name, "family") == 0) {
		PyErr_SetString(PyExc_AttributeError,
		    "attempt to modify read-only RadixNode attribute");
		return -1;
	}

	if (self->user_attr == NULL) {
		self->user_attr = PyDict_New();
		if (self->user_attr == NULL)
			return -1;
	}

	/* Handle deletes */
	if (v == NULL) {
		if ((rv = PyDict_DelItemString(self->user_attr, name)) < 0)
			PyErr_SetString(PyExc_AttributeError,
			    "delete non-existing RadixNode attribute");
		return (rv);
	}

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

/* ------------------------------------------------------------------------ */

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
		 * underlying parent. This decouples the Python nodes from
		 * their radix.c node parents, allowing them to be used
		 * after the tree is gone
		 */
		node->rn = NULL;
		Py_XDECREF(node);
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
	char *addr, buf1[256], buf2[256];
	prefix_t *prefix;
	radix_node_t *node;
	RadixNodeObject *node_obj;

	if (!PyArg_ParseTuple(args, "s:add", &addr))
		return NULL;
	if ((prefix = prefix_pton(addr)) == NULL) {
		PyErr_SetString(PyExc_ValueError, "Invalid address format");
		return NULL;
	}
	if ((node = radix_lookup(self->rt, prefix)) == NULL) {
		Deref_Prefix(prefix);
		PyErr_SetString(PyExc_MemoryError, "Couldn't add prefix");
		return NULL;
	}
	Deref_Prefix(prefix);

	/*
	 * Create a RadixNode object in the data area of the node
	 * We duplicate most of the node's identity, because the radix.c:node 
	 * itself has a lifetime indepenant of the Python node object
	 * Confusing? yeah...
	 */
	if (node->data == NULL) {
		if ((node_obj = newRadixNodeObject(NULL)) == NULL)
			return (NULL);
		node_obj->rn = node;
		node_obj->prefixlen = node->prefix->bitlen;
		node_obj->family = node->prefix->family;
		prefix_addr_ntop(node->prefix, buf1, sizeof(buf1));
		snprintf(buf2, sizeof(buf2), "%s/%d", buf1,
		    node->prefix->bitlen);
		if ((node_obj->network = strdup(buf1)) == NULL) {
			node->data = NULL;
			Py_XDECREF(node_obj);
			PyErr_SetString(PyExc_MemoryError, "strdup failed");
			return NULL;
		}
		if ((node_obj->prefix = strdup(buf2)) == NULL) {
			node->data = NULL;
			Py_XDECREF(node_obj);
			PyErr_SetString(PyExc_MemoryError, "strdup failed");
			return NULL;
		}
		node->data = node_obj;
	} else
		node_obj = node->data;

	Py_XINCREF(node_obj);
	return (PyObject *)node_obj;
}

static PyObject *
Radix_delete(RadixObject *self, PyObject *args)
{
	char *addr;
	radix_node_t *node;
	RadixNodeObject *node_obj;
	prefix_t *prefix;

	if (!PyArg_ParseTuple(args, "s:del", &addr))
		return NULL;

	if ((prefix = prefix_pton(addr)) == NULL) {
		Deref_Prefix(prefix);
		PyErr_SetString(PyExc_ValueError, "Invalid address format");
		return NULL;
	}
	if ((node = radix_search_exact(self->rt, prefix)) == NULL) {
		Deref_Prefix(prefix);
		PyErr_SetString(PyExc_KeyError, "no such address");
		return NULL;
	}
	Deref_Prefix(prefix);
	if (node->data != NULL) {
		node_obj = node->data;
		node_obj->rn = NULL;
		Py_XDECREF(node_obj);
	}

	radix_remove(self->rt, node);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
Radix_search_exact(RadixObject *self, PyObject *args)
{
	char *addr;
	radix_node_t *node;
	RadixNodeObject *node_obj;
	prefix_t *prefix;

	if (!PyArg_ParseTuple(args, "s:search_exact", &addr))
		return NULL;
	if ((prefix = prefix_pton(addr)) == NULL) {
		Deref_Prefix(prefix);
		PyErr_SetString(PyExc_ValueError, "Invalid address format");
		return NULL;
	}
	if ((node = radix_search_exact(self->rt, prefix)) == NULL || 
	    node->data == NULL) {
		Deref_Prefix(prefix);
		Py_INCREF(Py_None);
		return Py_None;
	}
	Deref_Prefix(prefix);
	node_obj = node->data;
	Py_XINCREF(node_obj);
	return (PyObject *)node_obj;
}

static PyObject *
Radix_search_best(RadixObject *self, PyObject *args)
{
	char *addr;
	radix_node_t *node;
	RadixNodeObject *node_obj;
	prefix_t *prefix;

	if (!PyArg_ParseTuple(args, "s:search_best", &addr))
		return NULL;
	if ((prefix = prefix_pton(addr)) == NULL) {
		Deref_Prefix(prefix);
		PyErr_SetString(PyExc_ValueError, "Invalid address format");
		return NULL;
	}
	if ((node = radix_search_best(self->rt, prefix)) == NULL || 
	    node->data == NULL) {
		Deref_Prefix(prefix);
		Py_INCREF(Py_None);
		return Py_None;
	}
	Deref_Prefix(prefix);
	node_obj = node->data;
	Py_XINCREF(node_obj);
	return (PyObject *)node_obj;
}

struct walk_ctx {
	RadixObject *self;
	PyObject *ret;
};

static void
nodes_helper(radix_node_t *rn, void *cbctx)
{
	struct walk_ctx *ctx = (struct walk_ctx *)cbctx;
	RadixNodeObject *node_obj;

	if (rn->data != NULL)
		PyList_Append(ctx->ret, (PyObject *)rn->data);
}

static PyObject *
Radix_nodes(RadixObject *self, PyObject *args)
{
	struct walk_ctx cbctx;

	if (!PyArg_ParseTuple(args, ":nodes"))
		return NULL;

	cbctx.self = self;
	cbctx.ret = PyList_New(0);
	radix_process(self->rt, nodes_helper, &cbctx);

	return (cbctx.ret);
}

static PyMethodDef Radix_methods[] = {
	{"add",		(PyCFunction)Radix_add,		METH_VARARGS,
		PyDoc_STR("add() -> XXX")},
	{"delete",	(PyCFunction)Radix_delete,	METH_VARARGS,
		PyDoc_STR("del() -> XXX")},
	{"search_exact",(PyCFunction)Radix_search_exact,METH_VARARGS,
		PyDoc_STR("search_exact() -> XXX")},
	{"search_best",	(PyCFunction)Radix_search_best,	METH_VARARGS,
		PyDoc_STR("search_best() -> XXX")},
	{"nodes",	(PyCFunction)Radix_nodes,	METH_VARARGS,
		PyDoc_STR("nodes() -> XXX")},
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
        Radix_methods,		/*tp_methods*/
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

/* ------------------------------------------------------------------------ */

/* Radix object creator */

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
