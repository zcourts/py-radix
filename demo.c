/*
 * $Id$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "radix.h"


radix_node_t *make_and_lookup(radix_tree_t * tree, char *string);
void lookup_then_remove(radix_tree_t * tree, char *string);
radix_node_t *try_search_exact(radix_tree_t * tree, char *string);
radix_node_t *try_search_best(radix_tree_t * tree, char *string);
const char *prefix_toa(prefix_t * p);

int
main(int argc, char **argv)
{
	radix_tree_t *tree;
	radix_node_t *node;

	tree = New_Radix();

	make_and_lookup(tree, "::1/80");
	make_and_lookup(tree, "::1/80");
	make_and_lookup(tree, "::1/100");
	make_and_lookup(tree, "::1/64");
	make_and_lookup(tree, "::1/128");
	make_and_lookup(tree, "::2/128");
	make_and_lookup(tree, "::3/128");
	make_and_lookup(tree, "::3/128");
	make_and_lookup(tree, "::1/128");
	make_and_lookup(tree, "::0/128");
	make_and_lookup(tree, "::0/126");

	RADIX_WALK(tree->head, node) {
		printf("node: %s/%d\n",
		    prefix_toa(node->prefix), node->prefix->bitlen);
	} RADIX_WALK_END;

	try_search_exact(tree, "::0/126");
	try_search_exact(tree, "::1/126");
	try_search_exact(tree, "::1/125");
	try_search_best(tree, "::1/125");

	lookup_then_remove(tree, "::1/80");
	lookup_then_remove(tree, "::1/100");
	lookup_then_remove(tree, "::1/64");
	lookup_then_remove(tree, "::1/128");

	RADIX_WALK(tree->head, node) {
		printf("node: %s/%d\n",
		    prefix_toa(node->prefix), node->prefix->bitlen);
	} RADIX_WALK_END;

	lookup_then_remove(tree, "::2/128");
	lookup_then_remove(tree, "::3/128");
	lookup_then_remove(tree, "::1/128");
	lookup_then_remove(tree, "::0/128");
	lookup_then_remove(tree, "::0/126");

	RADIX_WALK(tree->head, node) {
		printf("node: %s/%d\n", 
		    prefix_toa(node->prefix), node->prefix->bitlen);
	} RADIX_WALK_END;

	exit(0);
}

radix_node_t *
make_and_lookup(radix_tree_t * tree, char *string)
{
	prefix_t *prefix;
	radix_node_t *node;

	prefix = prefix_pton(string);
	printf("make_and_lookup: %s/%d\n", prefix_toa(prefix), prefix->bitlen);
	node = radix_lookup(tree, prefix);
	Deref_Prefix(prefix);
	return (node);
}

void
lookup_then_remove(radix_tree_t * tree, char *string)
{
	radix_node_t *node;

	if ((node = try_search_exact(tree, string)) != NULL)
		radix_remove(tree, node);
}

radix_node_t *
try_search_exact(radix_tree_t * tree, char *string)
{
	prefix_t *prefix;
	radix_node_t *node;

	prefix = prefix_pton(string);
	printf("try_search_exact: %s/%d\n", prefix_toa(prefix), prefix->bitlen);
	if ((node = radix_search_exact(tree, prefix)) == NULL) {
		printf("try_search_exact: not found\n");
	} else {
		printf("try_search_exact: %s/%d found\n",
		    prefix_toa(node->prefix), node->prefix->bitlen);
	}
	Deref_Prefix(prefix);
	return (node);
}

radix_node_t *
try_search_best(radix_tree_t * tree, char *string)
{
	prefix_t *prefix;
	radix_node_t *node;

	prefix = prefix_pton(string);
	printf("try_search_best: %s/%d\n", prefix_toa(prefix), prefix->bitlen);
	if ((node = radix_search_best(tree, prefix)) == NULL)
		printf("try_search_best: not found\n");
	else
		printf("try_search_best: %s/%d found\n",
		    prefix_toa(node->prefix), node->prefix->bitlen);
	Deref_Prefix(prefix);

	return (node);
}

const char *
prefix_toa(prefix_t *p)
{
	static char buf[256];

	return prefix_addr_ntop(p, buf, sizeof(buf));
}
