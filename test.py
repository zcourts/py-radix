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

import radix;
import unittest
import socket

class TestRadix(unittest.TestCase):
	def testcreatedestroy(self):
		tree = radix.Radix()
		self.assertEqual(str(type(tree)), "<type 'radix.Radix'>")
		del tree

	def testcreatenode(self):
		tree = radix.Radix()
		node = tree.add("10.0.0.0/8")
		self.assertEqual(str(type(node)), "<type 'radix.RadixNode'>")
		self.assertEqual(node.prefix, "10.0.0.0/8")
		self.assertEqual(node.network, "10.0.0.0")
		self.assertEqual(node.prefixlen, 8)
		self.assertEqual(node.family, socket.AF_INET)

	def testnodeuserdata(self):
		tree = radix.Radix()
		node = tree.add("10.0.0.0/8")
		node.data["blah"] = "abc123"
		node.data["foo"] = 12345
		self.assertEqual(node.data["blah"], "abc123")
		self.assertEqual(node.data["foo"], 12345)
		self.assertRaises(AttributeError, lambda x: x.nonexist, node)
		del node.data["blah"]
		self.assertRaises(KeyError, lambda x: x.data["blah"], node)

	def testsearchexact(self):
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
				
	def testsearchbest(self):
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
				
	def testconcurrenttrees(self):
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

	def testdeletes(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		node3 = tree.add("10.0.0.0/24")
		tree.delete("10.0.0.0/24")
		self.assertRaises(KeyError, tree.delete, "127.0.0.1")
		self.assertRaises(KeyError, tree.delete, "10.0.0.0/24")
		node = tree.search_best("10.0.0.10");
		self.assertEqual(node, node1)

	def testnodes(self):
		tree = radix.Radix()
		prefixes = [ "10.0.0.0/8", "127.0.0.1/32", "::1/128", 
		    "2000::/16", "10.1.0.0/16", "10.100.100.100/32" ]
		prefixes.sort()
		for prefix in prefixes:
			tree.add(prefix)
		nodes = tree.nodes()
		addrs = map(lambda x: x.prefix, nodes)
		addrs.sort()
		self.assertEqual(addrs, prefixes)

	def testuseafterfree(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		del tree
		self.assertEquals(node1.prefix, "10.0.0.0/8")

	def testuniqueinstance(self):
		tree = radix.Radix()
		node1 = tree.add("10.0.0.0/8")
		node2 = tree.add("10.0.0.0/8")
		self.assert_(node1 is node2)
		self.assert_(node1.prefix is node2.prefix)	

def main():
	unittest.main()

if __name__ == '__main__':
	main()
