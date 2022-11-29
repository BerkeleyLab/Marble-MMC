#!/usr/bin/python3

# A quick script for creating memory map header templates
# Generates macros of register address definitions as well as an X-Macro
# construct called for all registers.
#
# Requires a memory-map definition file with the following syntax
# ==== memory-map definition file syntax summary ====
#   # single-line comments (no in-line)
#   Comma-delimited
#   Line 0 (after comments) = device macro prefix,starting register number
#   Subsequent lines are:
#     If startswith plus sign (+), increment register count
#     Else if startswith plus sign (-), increment register count
#     Else if startswith greater-than sign (>), set register count
#     Else, postfix,description

import time

TIME_FORMAT_MDY = "mdy"

def write(msg, fd=None):
    if fd is None:
        print(msg)
    else:
        fd.write(msg + '\n')

def _int(s):
    # Try to interpret as decimal-string/integer/float
    try:
        return int(s)
    except:
        pass
    # Assuming s is string from this point
    if hasattr(s, 'lower'):
        # Try hex format 0x...
        if 'x' in s.lower():
            try:
                return int(s, 16)
            except:
                pass
        # Try hex format ...h
        elif 'h' in s.lower():
            hindex = s.index('h') # Assuming this method exists
            try:
                return int(s[:hindex],16)
            except:
                pass
    return None

def _makeFileName(s):
    s = str(s).lower()
    return "{}.h".format(s)

def _makeHDef(s):
    return "__{}_H_".format(s.upper())

def _sectionLine(s, width = 80):
    width = max(width, len(s) + 6)
    fmt = "/* {:=^" + str(width - 6) + "} */"
    return fmt.format(" {} ".format(s))

def _makeRegDef(prefix, nReg, width = 80):
    # Bound the width so we hopefully end up with a string spanning 'width' chars in total
    width = max(width - 8 - len(prefix), 4)
    fmt = "#define {}".format(prefix) + "{:>" + str(width) + "}"
    return fmt.format("(" + hex(nReg) + ")")

def _makeXMacro(name, xStr = 'X', args = []):
    """Return an X-macro string call with 'name' as first arg and additional args in 'args'.
    'xStr' is the name of the macro to call."""
    argList = [str(name)]
    for arg in args:
        if arg != None:
            argList.append(str(arg))
    argStr = ', '.join(argList)
    return "{}({})".format(xStr, argStr)

def getDateTimeString(fmt = None):
    ts = time.localtime()
    if fmt is not None and fmt.lower() == TIME_FORMAT_MDY:
        date = "{1}/{2}/{0}".format(ts.tm_year, ts.tm_mon, ts.tm_mday)
    else:
        date = "{0:04}-{1:02}-{2:02}".format(ts.tm_year, ts.tm_mon, ts.tm_mday)
    ltime = "{:02}:{:02}".format(ts.tm_hour, ts.tm_min)
    return (date, ltime)

class HeaderMaker():
    _commentChar = '#'
    _delimiter = ','
    _incChar = '+'
    _decChar = '-'
    _gotoChar = '>'
    def __init__(self, inFilename = None, outFilename = None, xStr = 'X', scriptName = ""):
        self.inFilename = inFilename
        self.outFilename = outFilename
        self._scriptName = str(scriptName)
        self.xStr = str(xStr)

    def getNextLine(self, fd, regNum):
        line = None
        nextRegNum = None
        postfix = None
        desc = None
        while True:
            line = fd.readline()
            if not line:
                # End-of-file
                break
            line = line.strip().strip('\n')
            if len(line) > 0 and not line.startswith(self._commentChar):
                if line[0] in (self._incChar, self._decChar, self._gotoChar):
                    n = _int(line[1:])
                    if n == None:
                        print("Cannot evaluate line {}".format(line))
                        continue
                    if line[0] == self._incChar:
                        nextRegNum = regNum + n + 1
                    elif line[0] == self._decChar:
                        nextRegNum = regNum - n + 1
                    elif line[0] == self._gotoChar:
                        nextRegNum = n
                else:
                    words = line.split(self._delimiter)
                    postfix = words[0]
                    if len(words) > 1:
                        desc = words[1]
                    if nextRegNum == None:
                        nextRegNum = regNum + 1
                    break
        return (postfix, desc, nextRegNum)

    def getFirstLine(self, fd):
        """Rewind fd if not at head and read the first non-comment line.
        If input file is properly written, should return (prefix, starting register number).
        Check for None in either slot for parsing errors."""
        if not hasattr(fd, 'seek'):
            raise Exception("fd is not valid file descriptor")
        fd.seek(0)
        prefix, regStart, ignore = self.getNextLine(fd, 0)
        return (prefix, _int(regStart))

    def writeHeader(self, fd, prefix):
        dateString, timeString = getDateTimeString()
        hdef = _makeHDef(prefix)
        l = (
            "/*",
            " * File: {}".format(self.outFilename),
            " * Date: {}".format(dateString),
            " * Desc: Header file for {}. Auto-generated by {}.".format(prefix, self._scriptName),
            " */\n",
            "#ifndef {0}\n#define {0}\n\n#ifdef __cplusplus\n extern \"C\" {{\n#endif\n".format(hdef)
            )
        s = '\n'.join(l)
        write(s, fd)
        return

    def writeFooter(self, fd, prefix):
        # Write footer
        hdef = _makeHDef(prefix)
        s = "\n#ifdef __cplusplus\n}\n#endif"
        write(s, fd)
        s = "\n#endif /* {} */".format(hdef)
        write(s, fd)

    def writeMemoryMap(self, fd, regDefs):
        # Write Memory Map
        s = _sectionLine("Memory Map")
        write(s, fd)
        for line in regDefs:
            write(line, fd)
        write("", fd) # Extra line break for prettiness

    def writeXMacros(self, fd, xMacros, prefix = ""):
        # Write X-Macros
        s = _sectionLine("X-Macros")
        write(s, fd)
        s = "#define {}_FOR_EACH_REGISTER() \\".format(prefix)
        write(s, fd)
        for n in range(len(xMacros)):
            if n == len(xMacros) - 1:
                line = "  " + str(xMacros[n])
            else:
                line = "  " + str(xMacros[n]) + " \\"
            write(line, fd)

    def parse(self, fd, prefix = "", nStart = 0):
        # Register definitions
        regDefs = []
        # X-Macros
        xMacros = []
        line = True
        nReg = nStart-1 # HACK ALERT (avoid inc by 1 after first line)
        while True:
            postfix, desc, nReg = self.getNextLine(fd, nReg)
            if desc != None:
                desc = '"' + desc + '"'
            if postfix == None:
                # Assume end-of-file
                break
            fullRegName = prefix + '_' + postfix
            s = _makeRegDef(fullRegName, nReg)
            x = _makeXMacro(fullRegName, self.xStr, (desc,))
            regDefs.append(s)
            xMacros.append(x)
        return regDefs, xMacros

    def makeMapFile(self):
        """Make a memory map header file based on header description file 'inFilename'
        Outputs to file 'outFilename'.
        'xStr' (string) determines the name for the quasi-anonymous macro called during
        the x-macro expansion."""
        if self.inFilename == None:
            print("Must specify input file name")
            return False
        try:
            ifd = open(self.inFilename, 'r')
        except:
            print("Could not open {} for reading".format(self.inFilename))
            return False
        prefix, nStart = self.getFirstLine(ifd)
        if self.outFilename == None:
            self.outFilename = _makeFileName(prefix)
        try:
            ofd = open(self.outFilename, 'w')
        except:
            print("Cannot open {} for writing".format(self.outFilename))
            ofd = None
        self.writeHeader(ofd, prefix)
        regDefs, xMacros = self.parse(ifd, prefix, nStart)
        self.writeMemoryMap(ofd, regDefs)
        self.writeXMacros(ofd, xMacros, prefix)
        self.writeFooter(ofd, prefix)
        if ofd == None:
            return False
        ofd.close()
        if ifd != None:
            ifd.close()
        return True

def mkheader(argv):
    USAGE = ("python3 {} inputFileName\n".format(argv[0]) \
             + "    Generate header file for device with memory map defined in 'inputFileName'")
    if len(argv) < 2:
        print(USAGE)
        return False
    ifname = argv[1]
    maker = HeaderMaker(ifname, scriptName = argv[0])
    success = maker.makeMapFile()
    ofname = maker.outFilename
    if success:
        print("Created memory map file {}".format(ofname))
        return True
    else:
        print("Failed to create memory map file {}".format(ofname))
        return False

if __name__ == "__main__":
    import sys
    mkheader(sys.argv)
