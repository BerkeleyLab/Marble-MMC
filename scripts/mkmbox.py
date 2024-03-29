#! /usr/bin/python3

# Make mailbox.h from mailbox definition file (JSON with comments)

# TODO
#   After merging with hashbox, write the hash to the top of the memory map files

import json
import os
import re
import sys
import hashlib
import pickle
import math

import htools

PAGE_SIZE=16
NPAGES=128
MAILBOX_SIZE = PAGE_SIZE * NPAGES

def _int(x):
    try:
        return int(x)
    except:
        return int(x, 16)

class JSONHack():
    """A hack to allow an arbitrary number of comment lines within an otherwise JSON-compliant file.
    The comments are ignored.  Every other line in the file is passed to the JSON interpreter.
    Comments are full line and must begin with a '#' symbol (no end-of-line comments)."""
    _commentChar = "#"
    def __init__(self, filename = None):
        if filename == None:
            filename = "mbox.def"
        self.filename = filename
        self._fileHash = None
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
        #hl = hashlib.sha256(bytes(''.join(s), 'utf-8'))
        #self._fileHash = hl.digest()
        #self._fileHashStr = hl.hexdigest()
        return o

class MailboxInterface():
    _defaultDefinitionFile = "mbox.def"
    _defaultDocumentationFile = "mbox.doc"
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

    @staticmethod
    def _format(fmt, val):
        """Allow for both old-style (printf) and newstyle format strings."""
        if '%' in fmt:
            try:
                s = fmt % val
                return s
            except:
                pass
        return fmt.format(val)

    @staticmethod
    def _combine(*args):
        """Combine the bytes in 'args' into a single integer by shifting and OR'ing.
        Assumes args come in LSB-to-MSB."""
        s = 0
        for n in range(len(args)):
            byte = int(args[n])
            s |= byte << 8*n
        return s

    @staticmethod
    def _splitBytes(val, size):
        l = []
        for m in range(size):
            l.append((val >> 8*m) & 0xff)
        return l

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
        self._pageList = None
        self._fileHash = None
        self._fileHashStr = None
        self._ready = False
        self._reader = JSONHack(self._filename)
        self._includes = []
        self._fd = None # For _fp method

    def load(self):
        return self._reader.load()

    def _computeHash(self):
        if self._pageList is None:
            self.interpret()
        hashPageList = []
        # Dict parameters to include in hash calculation (fields we care about)
        filterList = ("name", "size", "output", "input")
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            hashElementList = []
            for n in range(len(elementList)):
                hashParamDict = {}
                name, paramDict = elementList[n]
                for key, val in paramDict.items():
                    if key in filterList:
                        hashParamDict[key] = val
                hashElementList.append((name, hashParamDict))
            hashPageList.append((npage, hashElementList))
        # Now pickle the structure
        s = pickle.dumps(hashPageList, protocol=4)
        # Then compute the hash
        hl = hashlib.sha256(s)
        self._fileHash = hl.digest()
        self._fileHashStr = hl.hexdigest()
        return

    def getHash(self):
        """Return the SHA-256 hash of the reduced structure of the input (definition) file.
        The Hash is computed on the pickled nested structure incorporating only the critical
        components of the JSON definition file (name, size, input/output functions).
        This is done in an attempt to keep the hash from changing if the definition file
        is changed in purely superficial ways (i.e. changing comments, documentation,
        formatting, etc).
        """
        if self._fileHash is None:
            self._computeHash()
        return self._fileHash

    def getHashHex(self):
        """Return Hex string of SHA-256 hash. See 'getHash' for details."""
        if self._fileHashStr is None:
            self._computeHash()
        return self._fileHashStr

    def getHashHex32(self):
        """Return the 4 least-significant bytes of the SHA-256 hash in hex-ASCII string form."""
        return self.getHashHex()[0:8]

    def interpret(self):
        jdict = self.load()
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
            elementIndex = 0
            for nelement in range(len(mlist)):
                element = mlist[nelement]
                paramDict = self._getDefaultParams()
                name = None
                for param, val in element.items():
                    if param.lower() == "name":
                        name = val
                    else:
                        paramDict[param] = val
                size = self._vetSize(paramDict.get('size', 1))
                paramDict['index'] = elementIndex
                elementIndex += size
                if elementIndex > PAGE_SIZE:
                    raise MailboxError("Maximum size ({} bytes) exceeded for page {}.".format(PAGE_SIZE, npage))
                if name == None:
                    raise MailboxError("Encountered mailbox page element {} which has no 'name' entry.".format(nelement))
                elementList.append((name, paramDict))
            self._pageList.append((npage, elementList))
        self._ready = True
        return

    def _vetSize(self, size):
        if size == "":
            size = 1
        else:
            size = int(size)
        if size > 16 or size < 1:
            print("Invalid size {}".format(size))
            return None
        return size

    def _getDefaultParams(self):
        d = {
            'size' : 1,
            'type' : 'int',
            'desc' : '',
            'fmt'  : None
            }
        return d

    def decode(self, contents = []):
        """'contents' is a list of bytes (length <= 2048) which is all pages concatenated."""
        nbytes = len(contents)
        decoded = []
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            indexStart = npage*PAGE_SIZE
            indexEnd = indexStart + PAGE_SIZE
            pageList = []
            print("Page{}".format(npage))
            if indexEnd <= nbytes:
                #pageData = contents[indexStart:indexEnd]
                for n in range(len(elementList)):
                    name, paramDict = elementList[n]
                    desc = paramDict.get('desc', "")
                    size = paramDict.get('size', 1)
                    elementIndex = paramDict.get('index', None)
                    parts = []
                    index = indexStart + elementIndex
                    for nPart in range(size):
                        # Append in LSB-to-MSB order
                        parts.append(contents[index+size-1-nPart])
                    val = self._combine(*parts)
                    fmt = paramDict.get('fmt', '0x{:x}')
                    if fmt in (None, ''):
                        fmt = '0x{:x}'
                    l = [f"[{elementIndex}]", f"{name}"]
                    if desc not in (None, ''):
                        l.append(f"({desc})")
                    scale = paramDict.get('scale', 1)
                    if scale in (None, ''):
                        scale = 1
                    valString = self._format(fmt, val*scale)
                    print("  {} = {}".format(' '.join(l), valString))
                    pageList.append((index, name, val))
            decoded.append((npage, pageList))
        return decoded

    def _hasPage(self, nPage):
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            if npage == nPage:
                return True
        return False

    def _getElementList(self, nPage):
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            if npage == nPage:
                return elementList
        return None

    def __repr__(self):
        if not self._ready:
            return "MailboxInterface({}) Uninitialized".format(self._filename)
        s = ["MailboxInterface({}): {} pages:".format(self._filename, len(self._pageList))]
        for npage, elementList in self._pageList:
            l = len(elementList)
            plural = 's' if l > 1 else ''
            s.append("  Page {} has {} element{}".format(npage, l, plural))
        return '\n'.join(s)

    def getElementOffsetAddressAndSize(self, nPage, name):
        """If element with name 'name' exists in page number 'nPage', returns the total offset to that
        element from the mailbox base address."""
        for npage, elementList in self._pageList:
            if npage == nPage:
                for ename, params in elementList:
                    if name == ename:
                        size = params.get('size', 1)
                        index = params.get('index', None)
                        if index is not None:
                            return nPage*PAGE_SIZE + index, size
        return None, None

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
            index = 0
            size = 1
            for n in range(len(elementList)):
                name, paramDict = elementList[n]
                size = paramDict.get('size', 1)
                index = paramDict.get('index', None)
                if size > 1:
                    for m in range(size):
                        self._fp("{}{}_{},".format(mbprefix, name, size-m-1))
                else:
                    self._fp(f"{mbprefix}{name},")
            self._fp(f"{mbprefix}SIZE // {index + size}")
            self._fp(f"}} PAGE{npage}_ENUM;\n");
        return

    def makeProtos(self):
        self._fp("uint32_t mailbox_get_hash(void);")
        self._fp("void mailbox_update_input(void);")
        self._fp("void mailbox_update_output(void);")
        self._fp("void mailbox_read_print_all(void);")
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
            hasAck = False
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
                    # TODO - Add 'aspointer' boolean option to mbox.def?
                    aspointer = paramDict.get('aspointer', False)
                    if (size > 4) or aspointer:    # Use array-mode for sizes > 4
                        s = "    {};".format(pinput.replace('@', f"&page[{enumName}_{size-1}]"))
                        s = s.replace("&&", '&')  # Replace any double-ampersands
                        self._fp(s)
                    elif size > 1:
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
                        s = "    {};".format(pinput.replace('@', 'val'))
                        s = s.replace("&&", '&')  # Replace any double-ampersands
                        self._fp(s)
                    else:
                        # size = 1 (nice and easy)
                        s = "    {};".format(pinput.replace('@', f"page[{enumName}]"))
                        s = s.replace("&&", '&')  # Replace any double-ampersands
                        self._fp(s)
                    # Handle acks if needed
                    ack = paramDict.get('ack', None)
                    if ack is None:
                        # try alternate keyword
                        ack = paramDict.get('respond', None)
                    if ack is not None:
                        hasAck = True
                        if size > 1:
                            # Apply the ack operation to the full-sized value
                            self._fp("    val = {};".format(ack.replace('@', 'val')))
                            for n in range(size):
                                member = f"page[{enumName}_{n}]"
                                #self._fp("    {} = {};".format(member, ack.replace('@', member)))
                                self._fp("    mbox_write_entry({}_{}, {});".format(
                                         enumName, n, f"(uint8_t)((val >> {8*n}) & 0xFF)"))
                        else:
                            #self._fp("    page[{}] = {};".format(enumName, ack.replace('@', f'(page[{enumName}])')))
                            member = f"page[{enumName}]"
                            self._fp("    mbox_write_entry({}, {});".format(enumName, ack.replace('@', member)))
            if hasInputs:
                self._fp("  }")
        self._fp("  return;\n}")

    def makeUpdateOutput(self):
        self._fp("void mailbox_update_output(void) {")
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            hasOutputs = False
            hasBigval = False
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
                    hasOutputs = True
                    if not hasattr(output, 'replace'):
                        print("{} is not a valid string")
                        continue
                    enumName = f"{mbprefix}{name}"
                    size = paramDict.get('size', 1)
                    aspointer = paramDict.get('aspointer', False)  # TODO
                    if (size > 4) or aspointer:    # Use array-mode
                        s = "    {};".format(output.replace('@', f"&page[{enumName}_{size-1}]"))
                        s = s.replace("&&", '&')  # Replace any double-ampersands
                        self._fp(s)
                    elif size > 1:
                        if not hasBigval:
                            # We need to instantiate an int
                            self._fp(f"    int val;")
                            hasBigval = True
                        # Break up into bytes
                        # First, get value
                        s = "    {};".format(output.replace('@', 'val'))
                        s = s.replace("&&", '&')  # Replace any double-ampersands
                        self._fp(s)
                        # Then shift, mask, and assign
                        for m in range(size):
                            byteIndex = size-m-1
                            shift = 8*byteIndex
                            elementName = f"{enumName}_{byteIndex}"
                            self._fp(f"    page[{elementName}] = (uint8_t)((val >> {shift}) & 0xff);")
                    else:
                        # size = 1
                        s = "    {};".format(output.replace('@', f"page[{enumName}]"))
                        s = s.replace("&&", '&')  # Replace any double-ampersands
                        self._fp(s)
            if hasOutputs:
                self._fp("    // Write page data")
                self._fp(f"    mbox_write_page({npage}, MB{npage}_SIZE, page);")
                self._fp("  }")
        self._fp("  return;\n}")

    def makeGetHash(self):
        self._fp("uint32_t mailbox_get_hash(void) {")
        self._fp("  return 0x{};\n}}".format(self.getHashHex32()))
        return

    def makePrintAll(self):
        self._fp("void mailbox_read_print_all(void) {")
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            mbsize = f"MB{npage}_SIZE"
            self._fp(f"  {{\n    // Page {npage}")
            self._fp(f"    uint8_t page[{mbsize}];")
            self._fp(f"    mbox_read_page({npage}, {mbsize}, page);")
            self._fp(f"    MBOX_PRINT_PAGE({npage});")
            self._fp("  }");
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
        htools.writeHeader(self._fd, self._prefix, filename=filestem, scriptname=sys.argv[0], includeDate=False)
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
        #dateString, timeString = htools.getDateTimeString()
        self._fp("/* File: {}\n * Desc: Auto-generated by {}\n */\n".format(self._sfilename, sys.argv[0]))
        self._fp(htools.sectionLine("DO NOT EDIT THIS AUTO-GENERATED FILE DIRECTLY"))
        self._fp(htools.sectionLine("Define mailbox structure in scripts/mbox.def"))
        self.makeIncludes()
        self.makeGetHash()
        self._fp("")
        self.makeUpdateOutput()
        self._fp("")
        self.makeUpdateInput()
        self._fp("")
        self.makePrintAll()
        if self._fd is not None:
            self._fd.close()
        return

    @staticmethod
    def _mdSanitize(s):
        """Insert backslash escape before underscores and lt/gt signs."""
        # Attempted idempotency
        l = []
        prior = 0
        for c in s:
            if c in ('_', '>', '<'):
                if prior != '\\':
                    l.append('\\')
            l.append(c)
            prior = c
        return ''.join(l)

    def makeDoc(self, outFilename = "mailbox.md"):
        with open(outFilename, 'w') as fd:
            def printf(*args, **kwargs):
                print(*args, **kwargs, file=fd)
            printf("# Mailbox Documentation\n\n(autogenerated by mkmbox.py)\n")
            for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
                printf(f"# Page {npage}\n")
                printf("Offset|Name|Size|Direction|Desc|Note")
                printf("------|----|----|---------|----|----")
                index = 0
                offset = 0
                size = 1
                for n in range(len(elementList)):
                    name, paramDict = elementList[n]
                    if name.startswith("PAD"):
                        offset += size
                        continue
                    mbprefix = self._mdSanitize(f"MB{npage}_")
                    name = self._mdSanitize(f"{mbprefix}{name}")
                    size = paramDict.get('size', 1)
                    index = paramDict.get('index', None)
                    desc = self._mdSanitize(paramDict.get('desc', ''))
                    inp = paramDict.get('input', None)
                    out = paramDict.get('output', None)
                    if (inp is None) and (out is None):
                        direction = "Invalid!"
                    elif (inp is None):
                        direction = "MCC=>FPGA"
                    elif (out is None):
                        direction = "FPGA=>MMC"
                    else:
                        direction = "MMC<=>FPGA"
                    direction = self._mdSanitize(direction)
                    if size > 1:
                        choices = ','.join([str(n) for n in range(size)])
                        note = "Access by byte as: {}_x (x={})".format(name, choices)
                    else:
                        note = ""
                    note = self._mdSanitize(note)
                    printf(f"{offset}|{name}|{size}|{direction}|{desc}|{note}")
                    offset += size
                printf("")
        return

    def makeMemoryMap(self, fd=sys.stdout, offset=0, style='v', filename=None, paginate=False):
        """Write register map to file descriptor 'fd'.
        Params:
            file descriptor fd : Stream-like interface (has 'write' method) to write output.
            int offset : Memory offset value to be added to all registers
            str style : Specify register map output style. Options:
                'v', 'V', 'Verilog', 'verilog' : Verilog style localparams
                'c', 'C' : C-style preprocessor macros
                'j', 'json', 'JSON' : JSON register map
        """
        cmt = "//"
        def comment(*args, **kwargs):
            if cmt is not None:
                print(cmt, *args, **kwargs, file=fd)
        if fd is None:
            if filename is None:
                fd = sys.stdout
            else:
                fd = open(filename, 'w')
        if filename is None:
            filename = "mailbox_map"
        else:
            filename = str(filename)
            dir, name = os.path.split(filename)
            filename, ext = os.path.splitext(name)
        global_offset = int(offset)
        pre = None
        post = None
        inter = ""
        style = style.lower()[0]
        if style == 'v':
            # Verilog-style
            pre = "`ifndef __{0}_VH\n`define __{0}_VH\n\n".format(filename.upper()) + \
                "localparam MAILBOX_HASH = 32'h{};\n\n".format(self.getHashHex32())
            fmt = "localparam {0}_ADDR = 'h{1:x};\n" \
                + "localparam {0}_SIZE = {2};\n"
            post = "`endif // __{}_VH\n".format(filename.upper())
        elif style == 'c':
            # C-style
            pre = "#ifndef __{0}_H\n#define __{0}_H\n\n".format(filename.upper()) + \
                "#define MAILBOX_HASH (0x{})\n\n".format(self.getHashHex32())
            fmt = "#define {0}_ADDR (0x{1:x})\n" \
                + "#define {0}_SIZE ({2})\n"
            post = "#endif // __{}_H\n".format(filename.upper())
        elif style == 'j':
            # JSON-style
            pre = "{\n"
            inter = ",\n"
            fmt = '  "{0}": {{\n' \
                + '    "access": "r",\n' \
                + '    "addr_width": {3},\n' \
                + '    "sign": "unsigned",\n' \
                + '    "base_addr": {1},\n' \
                + '    "data_width": 8\n' \
                + '  }}'
            post = "\n}\n"
            cmt = None
        else:
            raise Exception(f"Unknown register map style {style}")
        first = True
        if pre is not None:
            fd.write(pre)
        for npage, elementList in self._pageList: # Each entry is (npage, [(name, paramDict),...])
            comment(f" Page {npage}")
            index = 0
            offset = 0
            size = 1
            for n in range(len(elementList)):
                name, paramDict = elementList[n]
                if name.startswith("PAD"):
                    offset += size
                    continue
                mbprefix = f"MB{npage}_"
                if paginate:
                    name = f"{mbprefix}{name}"
                if style == 'j':
                    name = "mbox_" + name.lower()
                size = paramDict.get('size', 1)
                index = paramDict.get('index', None)
                offset = npage*16 + index + global_offset
                if first:
                    first = False
                else:
                    fd.write(inter)
                addr_width = math.ceil(math.log2(size))
                fd.write(fmt.format(name, offset, size, addr_width))
        if post is not None:
            fd.write(post)
        if fd != sys.stdout:
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
    parser = argparse.ArgumentParser(description="JSON-ish mailbox defintion interface")
    parser.add_argument('-d', '--def_file', default=None, help='File name for mailbox definition file to be loaded')
    ofilehelp = ("File name for generated header file. If filename ends in '.h', generates a header file only. "
                 "If filename ends in '.c', generates a source file only. "
                 "If filename has no extension (or any other extension), generates both header and source file "
                 "by appending .h/.c to the filename.")
    parser.add_argument('-o', '--output_file', default=None, help=ofilehelp)
    parser.add_argument('--hash', default=False, action="store_true", help="Return the hash of the input file.")
    parser.add_argument('-m', '--map', action="store_true", default=False, help='Force making a memory map output')
    parser.add_argument('--offset', default=0, help='Global memory offset')
    args = parser.parse_args()
    makeh = False
    makes = False
    makedoc = False
    makemap = False
    if args.output_file is None:
        prefix = args.def_file
        makeh = True
        makes = True
        makemap = True
        style = 'j'
    else:
        prefix, ext = os.path.splitext(args.output_file)
        if ext == "":
            makeh = True
            makes = True
        elif ext == ".c":
            makes = True
        elif ext == ".h":
            if (args.map):
                makemap = True
                style = 'c'
            else:
                makeh = True
        elif ext == ".md":
            makedoc = True
        elif ext == ".json":
            makemap = True
            style = 'j'
        elif ext == ".vh":
            makemap = True
            style = 'v'
        else:
            print("Cannot interpret desired file type based on extension '{}'".format(ext))
            return 1
    mbox = MailboxInterface(inFilename=args.def_file, prefix=prefix)
    if args.hash:
        mbox.load()
        ihash = mbox.getHashHex32()
        print(ihash)
        return 0
    mbox.interpret()
    print(mbox)
    if makeh:
        mbox.makeHeader()
    if makes:
        mbox.makeSource()
    if makedoc:
        mbox.makeDoc(args.output_file)
    if makemap:
        mbox.makeMemoryMap(fd=None, offset=_int(args.offset), style=style, filename=args.output_file)
    return 0

if __name__ == "__main__":
    import argparse
    #sys.exit(testJSONRead(sys.argv))
    #test_extractNumber(sys.argv)
    #test_getShiftOR(sys.argv)
    sys.exit(makeHeader(sys.argv))
