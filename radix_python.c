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
#include "structmember.h"
#include "radix.h"

/* $Id$ */

/* Prototypes */
struct _RadixObject;
struct _RadixIterObject;
static struct _RadixIterObject *newRadixIterObject(struct _RadixObject *);

/* ------------------------------------------------------------------------ */

/* RadixNode: tree nodes */

typedef struct {
	PyObject_HEAD
	PyObject *user_attr;	/* User-specified attributes */
	PyObject *network;
	PyObject *prefix;
	PyObject *prefixlen;
	PyObject *family;
	radix_node_t *rn;	/* Actual radix node (pointer to parent) */
} RadixNodeObject;

static PyTypeObject RadixNode_Type;

static RadixNodeObject *
newRadixNodeObject(PyObject *arg, radix_node_t *rn)
{
	RadixNodeObject *self;
	char network[256], prefix[256];

	self = PyObject_New(RadixNodeObject, &RadixNode_Type);
	if (self == NULL)
		return NULL;

	self->rn = rn;

	/* Format addresses for packing into objects */
	prefix_addr_ntop(rn->prefix, network, sizeof(network));
	prefix_ntop(rn->prefix, prefix, sizeof(prefix));

	self->user_attr = PyDict_New();
	self->network = PyString_FromString(network);
	self->prefix = PyString_FromString(prefix);
	self->prefixlen = PyInt_FromLong(rn->prefix->bitlen);
	self->family = PyInt_FromLong(rn->prefix->family);
	
	if (self->user_attr == NULL || self->prefixlen == NULL || 
	    self->family == NULL || self->network == NULL || 
	    self->prefix == NULL) {
		/* RadixNode_dealloc will clean up for us */
		Py_XDECREF(self);
		return (NULL);		
	}

	return self;
}

/* RadixNode methods */

static void
RadixNode_dealloc(RadixNodeObject *self)
{
	Py_XDECREF(self->user_attr);
	Py_XDECREF(self->prefixlen);
	Py_XDECREF(self->family);
	Py_XDECREF(self->network);
	Py_XDECREF(self->prefix);
	PyObject_Del(self);
}

static PyMemberDef RadixNode_members[] = {
	{"data",	T_OBJECT, offsetof(RadixNodeObject, user_attr),	READONLY},
	{"network",	T_OBJECT, offsetof(RadixNodeObject, network),	READONLY},
	{"prefix",	T_OBJECT, offsetof(RadixNodeObject, prefix),	READONLY},
	{"prefixlen",	T_OBJECT, offsetof(RadixNodeObject, prefixlen),	READONLY},
	{"family",	T_OBJECT, offsetof(RadixNodeObject, family),	READONLY},
	{NULL}
};

PyDoc_STRVAR(RadixNode_doc, 
"Node in a radix tree");

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
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
	0,			/*tp_call*/
	0,			/*tp_str*/
	0,			/*tp_getattro*/
	0,			/*tp_setattro*/
	0,			/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,	/*tp_flags*/
	RadixNode_doc,		/*tp_doc*/
	0,			/*tp_traverse*/
	0,			/*tp_clear*/
	0,			/*tp_richcompare*/
	0,			/*tp_weaklistoffset*/
	0,			/*tp_iter*/
	0,			/*tp_iternext*/
	0,			/*tp_methods*/
	RadixNode_members,	/*tp_members*/
	0,			/*tp_getset*/
	0,			/*tp_base*/
	0,			/*tp_dict*/
	0,			/*tp_descr_get*/
	0,			/*tp_descr_set*/
	0,			/*tp_dictoffset*/
	0,			/*tp_init*/
	0,			/*tp_alloc*/
	0,			/*tp_new*/
	0,			/*tp_free*/
	0,			/*tp_is_gc*/
};

/* ------------------------------------------------------------------------ */

typedef struct _RadixObject {
	PyObject_HEAD
	radix_tree_t *rt;	/* Actual radix tree */
	int family;		/* XXX - hack until we fix shared v4/v6 trees */
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
	self->family = -1;
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

PyDoc_STRVAR(Radix_add_doc,
"Radix.add(prefix) -> new RadixNode object\n\
\n\
Adds the network specified by 'prefix' to the radix tree. 'prefix' \n\
may be an address (indicating a unicast host) or CIDR formatted \n\
network. Both IPv4 and IPv6 addresses/networks are supported. \n\
Returns a RadixNode object, which can store arbitrary data.");

static PyObject *
Radix_add(RadixObject *self, PyObject *args)
{
	char *addr;
	prefix_t *prefix;
	radix_node_t *node;
	RadixNodeObject *node_obj;

	if (!PyArg_ParseTuple(args, "s:add", &addr))
		return NULL;
	if ((prefix = prefix_pton(addr, -1)) == NULL) {
		PyErr_SetString(PyExc_ValueError, "Invalid address format");
		return NULL;
	}
	if (self->family == -1)
		self->family = prefix->family;
	else if (prefix->family != self->family) {
		Deref_Prefix(prefix);
		PyErr_SetString(PyExc_ValueError,
		    "Mixing IPv4 and IPv6 in a single tree is not supported");
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
		if ((node_obj = newRadixNodeObject(NULL, node)) == NULL)
			return (NULL);
		node->data = node_obj;
	} else
		node_obj = node->data;

	Py_XINCREF(node_obj);
	return (PyObject *)node_obj;
}

PyDoc_STRVAR(Radix_delete_doc,
"Radix.delete(prefix) -> None\n\
\n\
Deletes the specified prefix (a unicast address or a CIDR network)\n\
from the radix tree.");

static PyObject *
Radix_delete(RadixObject *self, PyObject *args)
{
	char *addr;
	radix_node_t *node;
	RadixNodeObject *node_obj;
	prefix_t *prefix;

	if (!PyArg_ParseTuple(args, "s:del", &addr))
		return NULL;

	if ((prefix = prefix_pton(addr, -1)) == NULL) {
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

PyDoc_STRVAR(Radix_search_exact_doc,
"Radix.search_exact(prefix) -> RadixNode or None\n\
\n\
Search for the specified 'prefix' (a unicast address or a CIDR\n\
network) in the radix tree. In order to match, the 'prefix' must\n\
be specified exactly. Contrast with Radix.search_best. If no match\n\
is found, then returns None.");

static PyObject *
Radix_search_exact(RadixObject *self, PyObject *args)
{
	char *addr;
	radix_node_t *node;
	RadixNodeObject *node_obj;
	prefix_t *prefix;

	if (!PyArg_ParseTuple(args, "s:search_exact", &addr))
		return NULL;
	if ((prefix = prefix_pton(addr, -1)) == NULL) {
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

PyDoc_STRVAR(Radix_search_best_doc,
"Radix.search_best(prefix) -> None\n\
\n\
Search for the specified 'prefix' (a unicast address or a CIDR\n\
network) in the radix tree. search_best will return the best\n\
(longest) entry that includes the specified 'prefix'.If no match\n\
is found, then returns None.");

static PyObject *
Radix_search_best(RadixObject *self, PyObject *args)
{
	char *addr;
	radix_node_t *node;
	RadixNodeObject *node_obj;
	prefix_t *prefix;

	if (!PyArg_ParseTuple(args, "s:search_best", &addr))
		return NULL;
	if ((prefix = prefix_pton(addr, -1)) == NULL) {
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

PyDoc_STRVAR(Radix_nodes_doc,
"Radix.nodes(prefix) -> List of RadixNode\n\
\n\
Returns a list containing RadixNode for each prefix that has been\n\
entered into the tree. This list may be empty if no prefixes have\n\
been entered.");

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

static PyObject *
Radix_getiter(RadixObject *self)
{
	return (PyObject *)newRadixIterObject(self);
}

PyDoc_STRVAR(Radix_doc, "Radix tree");

static PyMethodDef Radix_methods[] = {
	{"add",		(PyCFunction)Radix_add,		METH_VARARGS,	Radix_add_doc		},
	{"delete",	(PyCFunction)Radix_delete,	METH_VARARGS,	Radix_delete_doc	},
	{"search_exact",(PyCFunction)Radix_search_exact,METH_VARARGS,	Radix_search_exact_doc	},
	{"search_best",	(PyCFunction)Radix_search_best,	METH_VARARGS,	Radix_search_best_doc	},
	{"nodes",	(PyCFunction)Radix_nodes,	METH_VARARGS,	Radix_nodes_doc		},
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
	0,			/*tp_call*/
	0,			/*tp_str*/
	0,			/*tp_getattro*/
	0,			/*tp_setattro*/
	0,			/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,	/*tp_flags*/
	Radix_doc,		/*tp_doc*/
	0,			/*tp_traverse*/
	0,			/*tp_clear*/
	0,			/*tp_richcompare*/
	0,			/*tp_weaklistoffset*/
	(getiterfunc)Radix_getiter, /*tp_iter*/
	0,			/*tp_iternext*/
	Radix_methods,		/*tp_methods*/
	0,			/*tp_members*/
	0,			/*tp_getset*/
	0,			/*tp_base*/
	0,			/*tp_dict*/
	0,			/*tp_descr_get*/
	0,			/*tp_descr_set*/
	0,			/*tp_dictoffset*/
	0,			/*tp_init*/
	0,			/*tp_alloc*/
	0,			/*tp_new*/
	0,			/*tp_free*/
	0,			/*tp_is_gc*/
};

/* ------------------------------------------------------------------------ */

/* RadixIter: radix tree iterator */

typedef struct _RadixIterObject {
	PyObject_HEAD
	RadixObject *parent;
        radix_node_t *iterstack[RADIX_MAXBITS+1];
        radix_node_t **sp;
        radix_node_t *rn;
} RadixIterObject;

static PyTypeObject RadixIter_Type;

static RadixIterObject *
newRadixIterObject(RadixObject *parent)
{
	RadixIterObject *self;

	self = PyObject_New(RadixIterObject, &RadixIter_Type);
	if (self == NULL)
		return NULL;

	self->parent = parent;
	Py_XINCREF(self->parent);

	self->sp = self->iterstack;
	self->rn = self->parent->rt->head;

	return self;
}

/* RadixIter methods */

static void
RadixIter_dealloc(RadixIterObject *self)
{
	PyObject_Del(self);
}

static PyObject *
RadixIter_iternext(RadixIterObject *self)
{
        radix_node_t *node;
	PyObject *ret;

 again:
	if ((node = self->rn) == NULL) {
		Py_XDECREF(self->parent);
		return NULL;
	}

	/* Get next node */
	if (self->rn->l) {
		if (self->rn->r)
			*self->sp++ = self->rn->r;
		self->rn = self->rn->l;
	} else if (self->rn->r)
		self->rn = self->rn->r;
	else if (self->sp != self->iterstack)
		self->rn = *(--self->sp);
	else
		self->rn = NULL;

	if (node->prefix == NULL || node->data == NULL)
		goto again;

	ret = node->data;
	Py_INCREF(ret);
	return (ret);
}

PyDoc_STRVAR(RadixIter_doc, 
"Radix tree iterator");

static PyTypeObject RadixIter_Type = {
	/* The ob_type field must be initialized in the module init function
	 * to be portable to Windows without using C++. */
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"radix.RadixIter",	/*tp_name*/
	sizeof(RadixIterObject),/*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)RadixIter_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
	0,			/*tp_call*/
	0,			/*tp_str*/
	0,			/*tp_getattro*/
	0,			/*tp_setattro*/
	0,			/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,	/*tp_flags*/
	RadixIter_doc,		/*tp_doc*/
	0,			/*tp_traverse*/
	0,			/*tp_clear*/
	0,			/*tp_richcompare*/
	0,			/*tp_weaklistoffset*/
	0,			/*tp_iter*/
	(iternextfunc)RadixIter_iternext, /*tp_iternext*/
	0,			/*tp_methods*/
	0,			/*tp_members*/
	0,			/*tp_getset*/
	0,			/*tp_base*/
	0,			/*tp_dict*/
	0,			/*tp_descr_get*/
	0,			/*tp_descr_set*/
	0,			/*tp_dictoffset*/
	0,			/*tp_init*/
	0,			/*tp_alloc*/
	0,			/*tp_new*/
	0,			/*tp_free*/
	0,			/*tp_is_gc*/
};

/* ------------------------------------------------------------------------ */

/* Radix object creator */

PyDoc_STRVAR(radix_Radix_doc,
"Radix() -> new Radix tree object\n\
\n\
Instantiate a new radix tree object.");

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
	{"Radix",	radix_Radix,	METH_VARARGS,	radix_Radix_doc	},
	{NULL,		NULL}		/* sentinel */
};

PyDoc_STRVAR(module_doc,
"Implementation of a radix tree data structure for network prefixes.\n\
\n\
The radix tree is the data structure most commonly used for routing\n\
table lookups. It efficiently stores network prefixes of varying\n\
lengths and allows fast lookups of containing networks.\n\
\n\
Simple example:\n\
\n\
	import radix\n\
\n\
	# Create a new tree\n\
	rtree = radix.Radix()\n\
\n\
	# Adding a node returns a RadixNode object. You can create\n\
	# arbitrary members in its 'data' dict to store your data\n\
	rnode = rtree.add(\"10.0.0.0/8\")\n\
	rnode.data[\"blah\"] = \"whatever you want\"\n\
\n\
	# Exact search will only return prefixes you have entered\n\
	rnode = rtree.search_exact(\"10.0.0.0/8\")\n\
	# Get your data back out\n\
	print rnode.data[\"blah\"]\n\
\n\
	# Best-match search will return the longest matching prefix\n\
	# that contains the search term (routing-style lookup)\n\
	rnode = rtree.search_best(\"10.123.45.6\")\n\
\n\
	# There are a couple of implicit members of a RadixNode:\n\
	print rnode.network	# -> \"10.0.0.0\"\n\
	print rnode.prefix	# -> \"10.0.0.0/8\"\n\
	print rnode.prefixlen	# -> 8\n\
	print rnode.family	# system-dependant (same as socket.AF_INET)\n\
\n\
	# IPv6 prefixes are fully supported\n\
	# NB. Don't mix IPv4 and IPv6 in the same tree!\n\
	rnode = rtree.add(\"2001:200::/32\")\n\
	rnode = rtree.add(\"::/0\")\n\
\n\
	# Use the nodes() function to return all prefixes entered\n\
	nodes = rtree.nodes()\n\
	for rnode in nodes:\n\
  		print rnode.prefix\n\
\n\
	# You can also directly iterate over the tree itself\n\
	# this would save some memory if the tree is big\n\
	for rnode in rtree:\n\
  		print rnode.prefix\n\
");

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
