#! /usr/bin/python3

# Make mailbox.h from mailbox definition file (JSON with comments)

# TODO
#   I need to include a bunch of files in the source file.  I think I will need to add these
#   as an additional top-level element in mbox.def
#   Can I pass two arguments to write to two separate locations? .c/.h files need to be in
#   different directories

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

    @staticmethod
    def _getShiftOR(fmt, size):
        l = []
        for m in range(size):
            byteIndex = size-m-1
            shift = 8*byteIndex
            l.append("({} << {})".format(fmt.format(byteIndex), shift))
        return " | ".join(l)

    def __init__(self, inFilename=None, headerFilename=None, sourceFilename=None, prefix="Mailbox_Def"):
        self._prefix = os.path.splitext(prefix)[0]
        if inFilename is None:
            self._filename = self._defaultDefinitionFile
        else:
            self._filename = inFilename
        if headerFilename is None:
            self._hfilename = htools.makeFileName(prefix)
        else:
            self._hfilename = headerFilename
        if sourceFilename is None:
            self._sfilename = htools.makeFileName(prefix, '.c')
        else:
            self._sfilename = sourceFilename
        self._ready = False
        self._reader = JSONHack(self._filename)
        self._includes = []
        self._fd = None # For _fp method

    def interpret(self):
        jdict = self._reader.load()
        self._pageNumbers = []
        self._pageList = [] # Each entry is (npage, [(name, paramDict),...])
        for page, mlist in jdict.items():
            if page == "include":
                self._includes = mlist
                continue
            npage = self._extractNumber(page)
            if npage == None:
                print("Skipping page {}".format(page))
                continue
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

    def _fp(self, *args, **kwargs):
        if self._fd == None:
            return print(*args, **kwargs)
        else:
            return print(*args, **kwargs, file = self._fd)

    def makeEnums(self):
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            self._fp("typedef enum {")
            mbprefix = f"  MB{npage}_"
            for n in range(len(elementList)):
                name, paramDict = elementList[n]
                size = paramDict.get('size', 1)
                if size > 1:
                    for m in range(size):
                        self._fp("{}{}_{},".format(mbprefix, name, size-m-1))
                else:
                    self._fp(f"{mbprefix}{name},")
            self._fp(f"{mbprefix}SIZE")
            self._fp(f"}} PAGE{npage}_ENUM;\n");
        return

    def makeProtos(self):
        self._fp("void mailbox_update_input(void);")
        self._fp("void mailbox_update_output(void);")
        return

    def makeIncludes(self):
        hfilename = os.path.split(self._hfilename)[1]
        self._fp("\n#include \"{}\"".format(hfilename))
        for name in self._includes:
            self._fp("#include \"{}\"".format(name))
        self._fp("")
        return

    def makeUpdateInput(self):
        self._fp("void mailbox_update_input(void) {")
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            hasInputs = False
            hasBigval = False
            mbprefix = f"MB{npage}_"
            for n in range(len(elementList)):
                name, paramDict = elementList[n]
                pinput = paramDict.get('input', None)
                if pinput is not None:
                    if not hasInputs:
                        # Delay opening the code block until we know it has inputs.
                        # If the page doesn't have inputs, will not generate the block
                        self._fp(f"  {{\n    // Page {npage}")
                        self._fp(f"    uint8_t page[MB{npage}_SIZE];")
                        self._fp(f"    mbox_read_page({npage}, MB{npage}_SIZE, page);")
                    hasInputs = True
                    if not hasattr(pinput, 'replace'):
                        print("{} is not a valid string")
                        continue
                    enumName = f"{mbprefix}{name}"
                    size = paramDict.get('size', 1)
                    if size > 1:
                        if not hasBigval:
                            # We need to instantiate an int
                            self._fp(f"    int val;")
                            hasBigval = True
                        # Break up into bytes
                        # First, get value
                        # TODO - What to do here?
                        #        I think val = (int)((page[N_3] << 24) | (page[N_2} << 16) | (page[N_1] << 8) | page[N_0])
                        fmt = f"page[{enumName}_" + "{}]"
                        v = self._getShiftOR(fmt, size)
                        # Assign shifted and OR'd value to temporary variable 'val'
                        self._fp("    val = (int)({});".format(v))
                        # Use the 'input' param string to return 'val' wherever it needs to go
                        self._fp("    {};".format(pinput.replace('@', 'val')))
                    else:
                        # size = 1 (nice and easy)
                        self._fp("    {};".format(pinput.replace('@', f"page[{enumName}]")))
            if hasInputs:
                self._fp("  }")
        self._fp("  return;\n}")

    def makeUpdateOutput(self):
        self._fp("void mailbox_update_output(void) {")
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            hasOutputs = False
            mbprefix = f"MB{npage}_"
            for n in range(len(elementList)):
                name, paramDict = elementList[n]
                output = paramDict.get('output', None)
                if output is not None:
                    if not hasOutputs:
                        # Delay opening the code block until we know it has outputs.
                        # If the page doesn't have outputs, will not generate the block
                        self._fp(f"  {{\n    // Page {npage}")
                        self._fp(f"    uint8_t page[MB{npage}_SIZE];")
                        self._fp(f"    int val;")
                    hasOutputs = True
                    if not hasattr(output, 'replace'):
                        print("{} is not a valid string")
                        continue
                    enumName = f"{mbprefix}{name}"
                    size = paramDict.get('size', 1)
                    if size > 1:
                        # Break up into bytes
                        # First, get value
                        self._fp("    {};".format(output.replace('@', 'val')))
                        # Then shift, mask, and assign
                        for m in range(size):
                            byteIndex = size-m-1
                            shift = 8*byteIndex
                            elementName = f"{enumName}_{byteIndex}"
                            self._fp(f"    page[{elementName}] = (uint8_t)((val >> {shift}) & 0xff);")
                    else:
                        # size = 1
                        self._fp("    {};".format(output.replace('@', f"page[{enumName}]")))
            if hasOutputs:
                self._fp("    // Write page data")
                self._fp(f"    mbox_write_page({npage}, MB{npage}_SIZE, page);")
                self._fp("  }")
        self._fp("  return;\n}")

    def makeHeader(self):
        try:
            self._fd = open(self._hfilename, 'w')
            print("Writing to {}".format(self._hfilename))
        except (TypeError, OSError):
            self._fd = None
        if self._hfilename is None:
            filestem = self._prefix
        else:
            filestem = os.path.split(self._hfilename)[1]
        htools.writeHeader(self._fd, self._prefix, filename=filestem, scriptname=sys.argv[0])
        self._fp(htools.sectionLine("DO NOT EDIT THIS AUTO-GENERATED FILE DIRECTLY"))
        self._fp(htools.sectionLine("Define mailbox structure in scripts/mbox.def"))
        self._fp("")
        self.makeEnums()
        self._fp("")
        self.makeProtos()
        self._fp("")
        htools.writeFooter(self._fd, self._prefix)
        if self._fd is not None:
            self._fd.close()
        return

    def makeSource(self):
        try:
            self._fd = open(self._sfilename, 'w')
            print("Writing to {}".format(self._sfilename))
        except (TypeError, OSError):
            self._fd = None
        dateString, timeString = htools.getDateTimeString()
        self._fp("/* File: {}\n * Date: {}\n * Desc: Auto-generated by {}\n */\n".format(self._sfilename, dateString, sys.argv[0]))
        self._fp(htools.sectionLine("DO NOT EDIT THIS AUTO-GENERATED FILE DIRECTLY"))
        self._fp(htools.sectionLine("Define mailbox structure in scripts/mbox.def"))
        self.makeIncludes()
        self.makeUpdateOutput()
        self._fp("")
        self.makeUpdateInput()
        if self._fd is not None:
            self._fd.close()
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

def test_getShiftOR(argv):
    fmtNext = False
    fmt = "foo_{}"
    size = 1
    for arg in argv[1:]:
        if fmtNext:
            fmt = arg
            fmtNext = False
        elif arg == '-f':
            fmtNext = True
        else:
            size = int(arg)
    print(MailboxInterface._getShiftOR(fmt, size))
    return 0

def makeHeader(argv):
    parser = argparse.ArgumentParser(description="JSON-ish mailbox defintion reader")
    parser.add_argument('-i', '--input_filename', default=None, help='File name for command script to be loaded')
    parser.add_argument('-o', '--output_filename', default=None, help='File name for generated header file')
    args = parser.parse_args()
    makeh = False
    makes = False
    if args.output_filename is None:
        prefix = args.input_filename
        makeh = True
        makes = True
    else:
        prefix, ext = os.path.splitext(args.output_filename)
        if ext == "":
            makeh = True
            makes = True
        elif ext == ".c":
            makes = True
        elif ext == ".h":
            makeh = True
        else:
            print("Cannot interpret desired file type based on extension '{}'".format(ext))
            return 1
    mbox = MailboxInterface(inFilename=args.input_filename, prefix=prefix)
    mbox.interpret()
    print(mbox)
    if makeh:
        mbox.makeHeader()
    if makes:
        mbox.makeSource()
    return 0

if __name__ == "__main__":
    import argparse
    #sys.exit(testJSONRead(sys.argv))
    #test_extractNumber(sys.argv)
    #test_getShiftOR(sys.argv)
    sys.exit(makeHeader(sys.argv))
