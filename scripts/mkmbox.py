#! /usr/bin/python3

# Make mailbox.h from mailbox definition file (JSON with comments)

import json
import os
import re
import sys

import htools

class JSONHack():
    """A hack to allow an arbitrary number of comment lines within an otherwise JSON-compliant file.
    The comments are ignored.  Every other line in the file is passed to the JSON interpreter.
    Comments are full line and must begin with a '#' symbol (no end-of-line comments)."""
    _commentChar = "#"
    def __init__(self, filename = None):
        if filename == None:
            filename = "mbox.def"
        self.filename = filename
        if not os.path.exists(self.filename):
            print("File {} doesn't appear to exist".format(self.filename))

    def load(self):
        if not os.path.exists(self.filename):
            print("Cannot load. File {} doesn't appear to exist".format(self.filename))
            return {}
        s = []
        with open(self.filename, 'r') as fd:
            line = True
            while line:
                #line = fd.readline().strip("\n") # severely undercounts lines...
                line = fd.readline()
                if line.strip().startswith(self._commentChar):
                    # Skip any lines that begin with the comment char
                    s.append("") # Append a blank line to ensure accurate line count on error
                    continue
                if len(line.strip('\n').replace('\r', '')) > 0:
                    #print("len({}) = {}".format(line, len(line)))
                    s.append(line.strip('\n'))
                else:
                    s.append("") # Append a blank line to ensure accurate line count on error
        try:
            o = json.loads('\n'.join(s))
        except json.decoder.JSONDecodeError as jerr:
            # This line number is not correct. Why?
            print("JSON Decoder Error:\n{}".format(jerr))
            o = {}
        return o

class MailboxInterface():
    _defaultDefinitionFile = "mbox.def"
    @staticmethod
    def _extractNumber(s):
        r = re.search("(\d+)", s)
        if r:
            sn = r.groups()[0]
            try:
                return int(sn)
            except:
                print("Could not interpret {} as integer")
                return None
        return None

    def __init__(self, infilename=None, headerFilename=None, prefix="Mailbox_Def"):
        self._prefix = prefix
        if infilename is None:
            self._filename = self._defaultDefinitionFile
        else:
            self._filename = infilename
        if headerFilename is None:
            self._hfilename = htools.makeFileName(prefix)
        else:
            self._hfilename = headerFilename
        self._ready = False
        self._reader = JSONHack(self._filename)

    def interpret(self):
        jdict = self._reader.load()
        self._pageNumbers = []
        self._pageList = [] # Each entry is (npage, [(name, paramDict),...])
        for page, mlist in jdict.items():
            npage = self._extractNumber(page)
            self._pageNumbers.append(npage)
            elementList = []
            for nelement in range(len(mlist)):
                element = mlist[nelement]
                paramDict = {}
                name = None
                for param, val in element.items():
                    if param.lower() == "name":
                        name = val
                    else:
                        paramDict[param] = val
                if name == None:
                    raise MailboxError("Encountered mailbox page element {} which has no 'name' entry.".format(nelement))
                elementList.append((name, paramDict))
            self._pageList.append((npage, elementList))
        self._ready = True
        return

    def __repr__(self):
        if not self._ready:
            return "MailboxInterface({}) Uninitialized".format(self._filename)
        s = ["MailboxInterface({}): {} pages:".format(self._filename, len(self._pageList))]
        for npage, elementList in self._pageList:
            l = len(elementList)
            plural = 's' if l > 1 else ''
            s.append("  Page {} has {} element{}".format(npage, l, plural))
        return '\n'.join(s)

    def printAllPages(self):
        for n, elementList in self._pageList:
            print(f"  Page {n}")
            self._printElements(elementList, indent = 4)

    def printPage(self, npage):
        for n, elementList in self._pageList:
            if n == npage:
                return self._printElements(elementList)
        print(f"Cannot find definition of page {npage}")
        return

    def _printElements(self, elementList, indent = 0):
        """elementList is expected to be [(name, paramDict),...]"""
        sn = " "*indent
        for n in range(len(elementList)):
            name, params = elementList[n]
            print(f"{sn}{n} : {name}")
            for pName, pVal in params.items():
                print(f"{sn}  {pName} = {pVal}")
        return

    def makeHeader(self):
        try:
            fd = open(self._hfilename, 'w')
            print("Writing to {}".format(self._hfilename))
        except (TypeError, OSError):
            fd = None
        def fp(*args, **kwargs):
            if fd == None:
                return print(*args, **kwargs)
            else:
                return print(*args, **kwargs, file = fd)
        if self._hfilename is None:
            filestem = self._prefix
        else:
            filestem = os.path.split(self._hfilename)[1]
        htools.writeHeader(fd, self._prefix, filename=filestem, scriptname=sys.argv[0])
        fp(htools.sectionLine("DO NOT EDIT THIS AUTO-GENERATED FILE DIRECTLY"))
        fp(htools.sectionLine("Define mailbox structure in scripts/mbox.def"))
        fp("")
        for npage, elementList in self._pageList:
            fp("typedef enum {")
            mbprefix = f"  MB{npage}_"
            for n in range(len(elementList)):
                name = elementList[n][0]
                fp(f"{mbprefix}{name},")
            fp(f"{mbprefix}SIZE")
            fp(f"}} PAGE{npage}_ENUM;\n");
        htools.writeFooter(fd, self._prefix)
        if fd is not None:
            fd.close()
        return

def write(msg, fd=None):
    if fd is None:
        print(msg)
    else:
        fd.write(msg + '\n')

def test_extractNumber(argv):
    if len(argv) < 2:
        print("gimme a number")
        return 1
    s = argv[1]
    print("{} => {}".format(s, MailboxInterface._extractNumber(s)))
    return 0

def testJSONRead(argv):
    parser = argparse.ArgumentParser(description="JSON-ish mailbox defintion reader")
    parser.add_argument('filename', default=None, help='File name for command script to be loaded')
    args = parser.parse_args()
    if args.input_filename is None:
        print("Missing mandatory filename")
        return 1
    rdr = JSONHack(args.filename)
    jdict = rdr.load()
    print(jdict)
    return 0

def makeHeader(argv):
    parser = argparse.ArgumentParser(description="JSON-ish mailbox defintion reader")
    parser.add_argument('-i', '--input_filename', default=None, help='File name for command script to be loaded')
    parser.add_argument('-o', '--output_filename', default=None, help='File name for generated header file')
    args = parser.parse_args()
    mbox = MailboxInterface(args.input_filename, args.output_filename)
    mbox.interpret()
    print(mbox)
    #mbox.printAllPages()
    mbox.makeHeader()
    return 0

if __name__ == "__main__":
    import argparse
    #sys.exit(testJSONRead(sys.argv))
    #test_extractNumber(sys.argv)
    sys.exit(makeHeader(sys.argv))
