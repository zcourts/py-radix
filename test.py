#!/usr/bin/env python

# Copyright (c) 2004 Damien Miller <djm@mindrot.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $Id$

import radix;
import unittest
import socket

class TestRadix(unittest.TestCase):
	def test_00__create_destroy(self):
		tree = radix.Radix()
		self.assertEqual(str(type(tree)), "<type 'radix.Radix'>")
		del tree

	def test_01__create_node(self):
		tree = radix.Radix()
		node = tree.add("10.0.0.0/8")
		self.assertEqual(str(type(node)), "<type 'radix.RadixNode'>")
		self.assertEqual(node.prefix, "10.0.0.0/8")
		self.assertEqual(node.network, "10.0.0.0")
		self.assertEqual(node.prefixlen, 8)
		self.assertEqual(node.family, socket.AF_INET)
		node = tree.add("10.0.0.0", 16)
		self.assertEqual(node.network, "10.0.0.0")
		self.assertEqual(node.prefixlen, 16)
		node = tree.add(network = "10.0.0.0", masklen = 24)
		self.assertEqual(node.network, "10.0.0.0")
		self.assertEqual(node.prefixlen, 24)

	def test_02__node_userdata(self):
		tree = radix.Radix()
		node = tree.add(network = "10.0.0.0", masklen=28)
		node.data["blah"] = "abc123"
		node.data["foo"] = 12345
		self.assertEqual(node.data["blah"], "abc123")
		self.assertEqual(node.data["foo"], 12345)
		self.assertRaises(AttributeError, lambda x: x.nonexist, node)
		del node.data["blah"]
		self.assertRaises(KeyError, lambda x: x.data["blah"], node)

	def test_03__search_exact(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		node2 = tree.add("10.0.0.0/16")
		node3 = tree.add("10.0.0.0/24")
		node2.data["foo"] = 12345
		node = tree.search_exact("127.0.0.1");
		self.assertEqual(node, None)
		node = tree.search_exact("10.0.0.0");
		self.assertEqual(node, None)
		node = tree.search_exact("10.0.0.0/24");
		self.assertEqual(node, node3)
		node = tree.search_exact("10.0.0.0/8");
		self.assertEqual(node, node1)
		node = tree.search_exact("10.0.0.0/16");
		self.assertEqual(node.data["foo"], 12345)
				
	def test_04__search_best(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		node2 = tree.add("10.0.0.0/16")
		node3 = tree.add("10.0.0.0/24")
		node = tree.search_best("127.0.0.1");
		self.assertEqual(node, None)
		node = tree.search_best("10.0.0.0");
		self.assertEqual(node, node3)
		node = tree.search_best("10.0.0.0/24");
		self.assertEqual(node, node3)
		node = tree.search_best("10.0.1.0/24");
		self.assertEqual(node, node2)
				
	def test_05__concurrent_trees(self):
		tree1 = radix.Radix()
		node1_1 = tree1.add("20.0.0.0/8")
		node1_1 = tree1.add("10.0.0.0/8")
		node1_2 = tree1.add("10.0.0.0/16")
		node1_3 = tree1.add("10.0.0.0/24")
		node1_3.data["blah"] = 12345
		tree2 = radix.Radix()
		node2_1 = tree2.add("30.0.0.0/8")
		node2_1 = tree2.add("10.0.0.0/8")
		node2_2 = tree2.add("10.0.0.0/16")
		node2_3 = tree2.add("10.0.0.0/24")
		node2_3.data["blah"] = 45678
		self.assertNotEqual(tree1, tree2)
		self.assertNotEqual(node1_2, node2_2)
		node = tree1.search_best("10.0.1.0/24");
		self.assertEqual(node, node1_2)
		self.assertNotEqual(node, node2_2)
		node = tree2.search_best("20.0.0.0/24");
		self.assertEqual(node, None)
		node = tree2.search_best("10.0.0.10");
		self.assertEqual(node.data["blah"], 45678)

	def test_06__deletes(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		node3 = tree.add("10.0.0.0/24")
		tree.delete("10.0.0.0/24")
		self.assertRaises(KeyError, tree.delete, "127.0.0.1")
		self.assertRaises(KeyError, tree.delete, "10.0.0.0/24")
		node = tree.search_best("10.0.0.10");
		self.assertEqual(node, node1)

	def test_07__nodes(self):
		tree = radix.Radix()
		prefixes = [
			"10.0.0.0/8", "127.0.0.1/32",
			"10.1.0.0/16", "10.100.100.100/32", 
		]
		prefixes.sort()
		for prefix in prefixes:
			tree.add(prefix)
		nodes = tree.nodes()
		addrs = map(lambda x: x.prefix, nodes)
		addrs.sort()
		self.assertEqual(addrs, prefixes)

	def test_08__nodes_empty_tree(self):
		tree = radix.Radix()
		nodes = tree.nodes()
		self.assertEqual(nodes, [])

	def test_09__prefixes(self):
		tree = radix.Radix()
		prefixes = [
			"10.0.0.0/8", "127.0.0.1/32",
			"10.1.0.0/16", "10.100.100.100/32", 
		]
		prefixes.sort()
		for prefix in prefixes:
			tree.add(prefix)
		addrs = tree.prefixes()
		addrs.sort()
		self.assertEqual(addrs, prefixes)

	def test_10__use_after_free(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		del tree
		self.assertEquals(node1.prefix, "10.0.0.0/8")

	def test_11__unique_instance(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		node2 = tree.add("10.0.0.0/8")
		self.assert_(node1 is node2)
		self.assert_(node1.prefix is node2.prefix)	

	def test_12__mixed_address_family(self):
		tree = radix.Radix()
		node1 = tree.add("127.0.0.1")
		self.assertRaises(ValueError, tree.add, "::1")

	def test_13__iterator(self):
		tree = radix.Radix()
		prefixes = [
			"::1/128", "2000::/16", "2000::/8", "dead:beef::/64"
		]
		prefixes.sort()
		for prefix in prefixes:
			tree.add(prefix)
		iterprefixes = []
		for node in tree:
			iterprefixes.append(node.prefix)
		iterprefixes.sort()
		self.assertEqual(iterprefixes, prefixes)

	def test_14__iterate_on_empty(self):
		tree = radix.Radix()
		prefixes = []
		for node in tree:
			prefixes.append(node.prefix)
		self.assertEqual(prefixes, [])

	def test_15__iterate_and_modify_tree(self):
		tree = radix.Radix()
		prefixes = [
			"::1/128", "2000::/16", "2000::/8", "dead:beef::/64"
		]
		prefixes.sort()
		for prefix in prefixes:
			tree.add(prefix)
		self.assertRaises(RuntimeWarning, map, lambda x: tree.delete(x.prefix), tree)

	def test_16__lots_of_prefixes(self):
		tree = radix.Radix()
		num_nodes_in = 0
		for i in range(0,128):
			for j in range(0,128):
				k = ((i + j) % 8) + 24
				node = tree.add("1.%d.%d.0" % (i, j), k)
				node.data["i"] = i
				node.data["j"] = j
				num_nodes_in += 1

		num_nodes_del = 0
		for i in range(0,128,5):
			for j in range(0,128,3):
				k = ((i + j) % 8) + 24
				tree.delete("1.%d.%d.0" % (i, j), k)
				num_nodes_del += 1

		num_nodes_out = 0
		for node in tree:
			i = node.data["i"]
			j = node.data["j"]
			k = ((i + j) % 8) + 24
			prefix = "1.%d.%d.0/%d" % (i, j, k)
			self.assertEquals(node.prefix, prefix)
			num_nodes_out += 1

		self.assertEquals(num_nodes_in - num_nodes_del, num_nodes_out)
		self.assertEquals(num_nodes_in - num_nodes_del,
		    len(tree.nodes()))

def main():
	unittest.main()

if __name__ == '__main__':
	main()
