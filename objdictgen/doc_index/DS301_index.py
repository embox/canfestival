#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys
import subprocess

DS301_PDF_INDEX = {0x1000: 86, 0x1001: 87, 0x1002: 87, 0x1003: 88, 0x1005: 89, 0x1006: 90, 0x1007: 90, 0x1008: 91, 0x1009: 91, 0x100A: 91, 0x100C: 92, 0x100D: 92, 0x1010: 92, 0x1011: 94, 0x1012: 97, 0x1013: 98, 0x1014: 98, 0x1015: 99, 0x1016: 100, 0x1017: 101, 0x1018: 101, 0x1020: 117, 0x1200: 103, 0x1201: 103, 0x1280: 105, 0x1400: 106, 0x1600: 109, 0x1800: 111, 0x1A00: 112}


def open_pdf(filepath):
    """Open a PDF file with the system's default viewer."""
    if sys.platform == 'win32':
        os.startfile(filepath)
    elif sys.platform == 'darwin':
        subprocess.Popen(['open', filepath])
    else:
        subprocess.Popen(['xdg-open', filepath])


def OpenPDFDocIndex(index, cwd):
    pdfpath = os.path.join(cwd, "doc", "301_v04000201.pdf")
    if not os.path.isfile(pdfpath):
        return _("""No documentation file available.
Please read can festival documentation to know how to obtain one.""")
    try:
        open_pdf(pdfpath)
        return True
    except Exception:
        return _("Unable to open PDF. Check that a PDF viewer is installed.")
