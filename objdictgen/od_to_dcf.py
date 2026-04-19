#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# This file is part of CanFestival, a library implementing CanOpen Stack.
#
# See COPYING file for copyrights details.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

"""Convert a legacy gnosis-XML .od file to a CanFestival .dcf file.

The .od files produced by the Python 2 + gnosis-based objdictedit were XML
serialisations of the Node object. This tool parses that XML directly (no
gnosis dependency) and saves the Node in the current DCF-based format.

Usage:
    od_to_dcf.py INPUT.od OUTPUT.dcf
    od_to_dcf.py --batch DIR          # convert every .od under DIR in place
"""

import os
import sys
import xml.etree.ElementTree as ET

# Ensure _() is available when node/eds_utils are imported standalone
import builtins
builtins.__dict__.setdefault('_', lambda x: x)

# Allow running from anywhere: import siblings from this script's directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import node
import eds_utils


def _parse_gnosis(elem):
    """Parse a gnosis XML element (attr/val/key/item) to a Python value."""
    t = elem.get("type")
    v = elem.get("value")

    if t == "numeric":
        # int or float; Python 2 long suffix L may be present
        if v is None:
            v = (elem.text or "").strip()
        v = v.rstrip("Ll")
        try:
            return int(v)
        except ValueError:
            return float(v)
    if t == "string":
        return elem.text if v is None else v
    if t == "True":
        return True
    if t == "False":
        return False
    if t == "None":
        return None
    if t == "list":
        return [_parse_gnosis(item) for item in elem.findall("item")]
    if t == "tuple":
        return tuple(_parse_gnosis(item) for item in elem.findall("item"))
    if t == "dict":
        result = {}
        for entry in elem.findall("entry"):
            key = entry.find("key")
            val = entry.find("val")
            if key is not None and val is not None:
                result[_parse_gnosis(key)] = _parse_gnosis(val)
        return result
    raise ValueError("Unknown gnosis type: %r" % t)


def parse_od_file(filepath):
    """Parse a gnosis-XML .od file and return a populated Node object."""
    with open(filepath, 'r') as f:
        content = f.read()
    # Strip DOCTYPE so ElementTree doesn't try to resolve the external DTD
    content = content.replace(
        '<!DOCTYPE PyObject SYSTEM "PyObjects.dtd">', ''
    )

    root = ET.fromstring(content)
    n = node.Node()
    for attr in root.findall("attr"):
        setattr(n, attr.get("name"), _parse_gnosis(attr))
    return n


def convert(od_path, dcf_path):
    n = parse_od_file(od_path)
    err = eds_utils.SaveNodeAsDCF(dcf_path, n)
    if err:
        raise RuntimeError(err)
    return n


def _usage():
    sys.stderr.write(__doc__)
    sys.exit(2)


def main(argv):
    if len(argv) == 3 and argv[1] == "--batch":
        root = argv[2]
        count = 0
        for dirpath, _dirs, files in os.walk(root):
            for name in files:
                if name.endswith(".od"):
                    od = os.path.join(dirpath, name)
                    dcf = os.path.splitext(od)[0] + ".dcf"
                    n = convert(od, dcf)
                    print("%s -> %s  (%s, %d entries)" % (
                        od, dcf, n.Name, len(n.Dictionary)))
                    count += 1
        print("Converted %d file(s)" % count)
        return 0

    if len(argv) != 3 or argv[1] in ("-h", "--help"):
        _usage()

    od_path, dcf_path = argv[1], argv[2]
    if not os.path.isfile(od_path):
        sys.stderr.write("Input file not found: %s\n" % od_path)
        return 1
    n = convert(od_path, dcf_path)
    print("%s -> %s  (%s, %d entries)" % (
        od_path, dcf_path, n.Name, len(n.Dictionary)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
