import sys
import os
import platform
from typing import Dict, List, Tuple

__version__ = '1.0.2'


def error(*arg):
    print(*arg, file=sys.stderr)
    exit(1)


class IRSyntaxError(Exception):
    pass


class DuplicatedLabelError(Exception):
    pass


class UndefinedLabelError(Exception):
    pass


class DuplicatedVariableError(Exception):
    pass


class CurrentFunctionNoneError(Exception):
    pass


class IRSimCli():

    def __init__(self, inputFile: str, stdin: List[int]) -> None:
        self.ip: int = -1
        self.entranceIP: int = -1
        self.offset: int = 0
        self.instrCnt: int = 0
        self.codes: List[tuple] = list()
        self.rawcodes: List[str] = list()
        self.mem: List[int] = None
        self.functionDict: Dict[str, List[str]] = dict()
        self.currentFunction = None
        self.symTable: Dict[str, List[Tuple[int, int, bool]]] = dict()
        self.labelTable = dict()
        self.callStack = list()
        self.argumentStack = list()
        self.input = stdin
        self.loadFile(inputFile)

    def loadFile(self, fname: str):
        if not os.path.exists(fname):
            error(f"Input file not found: {fname}.")

        with open(fname) as fp:
            for lineno, line in enumerate(fp.readlines()):
                if line.isspace():
                    continue
                self.sanityCheck(line, lineno)
                self.rawcodes.append(line)

        if self.entranceIP == -1:
            error(
                "Cannot find program entrance. Please make sure the 'main' function does exist.")

        self.labelCheck()
        if self.offset > 1048576 or self.entranceIP == -1:
            error('Loading failed.')

        # SUCCESS
        self.mem = [0] * 262144

    def run(self):
        self.stop()
        self.ip = self.entranceIP
        while True:
            if self.ip < 0 or self.ip >= len(self.codes):
                error_code = 3
                break
            code = self.codes[self.ip]
            error_code = self.executeCode(code)
            if error_code > 0:
                break
            self.ip += 1

        if error_code == 1:
            if len(self.input) > 0:
                error(
                    f"Program exited with input remaining:", *self.input)
            print(
                f'Program has exited gracefully.\nTotal instructions = {self.instrCnt}')
            exit(0)
        elif error_code == 2:
            error('An error occurred at line %d: Illegal memory access. \nIf this message keeps popping out, please reload the source file' % (self.ip + 1))
        elif error_code == 3:
            error(
                'Program Counter goes out of bound. The running program will be terminated instantly.')
        self.ip = -1

    def stop(self):
        self.ip = -1
        self.instrCnt = 0
        self.mem = [0] * 262144
        self.callStack = list()
        self.argumentStack = list()

    def sanityCheck(self, code: str, lineno: int) -> None:
        strs = code.split()
        relops = ['>', '<', '>=', '<=', '==', '!=']
        arithops = ['+', '-', '*', '/']
        try:
            if strs[0] == 'LABEL' or strs[0] == 'FUNCTION':
                if len(strs) != 3 or strs[2] != ':':
                    raise IRSyntaxError
                if strs[1] in self.labelTable:
                    raise DuplicatedLabelError
                self.labelTable[strs[1]] = lineno
                if strs[1] == 'main':
                    if strs[0] == 'LABEL':
                        raise IRSyntaxError
                    self.entranceIP = lineno
                if strs[0] == 'FUNCTION':
                    self.currentFunction = strs[1]
                    self.functionDict[strs[1]] = list()
                self.codes.append(('LABEL', strs[1]))
            else:
                if self.currentFunction == None:
                    raise CurrentFunctionNoneError
                if strs[0] == 'GOTO':
                    if len(strs) != 2:
                        raise IRSyntaxError
                    self.codes.append(('GOTO', strs[1]))
                elif strs[0] == 'RETURN' or strs[0] == 'READ' or strs[0] == 'WRITE' or strs[0] == 'ARG' or strs[0] == 'PARAM':
                    if len(strs) != 2:
                        raise IRSyntaxError
                    if (strs[0] == 'READ' or strs[0] == 'PARAM') and not strs[1][0].isalpha():
                        raise IRSyntaxError
                    self.tableInsert(strs[1])
                    self.codes.append((strs[0], strs[1]))
                elif strs[0] == 'DEC':
                    if len(strs) != 3 or int(strs[2]) % 4 != 0:
                        raise IRSyntaxError
                    if strs[1] in self.symTable:
                        raise DuplicatedVariableError
                    self.tableInsert(strs[1], int(strs[2]), True)
                    self.codes.append('DEC')
                elif strs[0] == 'IF':
                    if len(strs) != 6 or strs[4] != 'GOTO' or strs[2] not in relops:
                        raise IRSyntaxError
                    self.tableInsert(strs[1])
                    self.tableInsert(strs[3])
                    self.codes.append(
                        ('IF', strs[1], strs[2], strs[3], strs[5]))
                else:
                    if strs[1] != ':=' or len(strs) < 3:
                        raise IRSyntaxError
                    if strs[0][0] == '&' or strs[0][0] == '#':
                        raise IRSyntaxError
                    self.tableInsert(strs[0])
                    if strs[2] == 'CALL':
                        if len(strs) != 4:
                            raise IRSyntaxError
                        self.codes.append(('CALL', strs[0], strs[3]))
                    elif len(strs) == 3:
                        self.tableInsert(strs[2])
                        self.codes.append(('MOV', strs[0], strs[2]))
                    elif len(strs) == 5 and strs[3] in arithops:
                        self.tableInsert(strs[2])
                        self.tableInsert(strs[4])
                        self.codes.append(
                            ('ARITH', strs[0], strs[2], strs[3], strs[4]))
                    else:
                        raise IRSyntaxError
        except (IRSyntaxError, ValueError):
            error('Syntax error at line %d:\n\n%s' % (lineno + 1, code))
        except DuplicatedLabelError:
            error('Duplicated label %s at line %d:\n\n%s' % (
                strs[1], lineno + 1, code))
        except DuplicatedVariableError:
            error('Duplicated variable %s at line %d:\n\n%s' % (
                strs[1], lineno + 1, code))
        except CurrentFunctionNoneError:
            error('Line %d does not belong to any function:\n\n%s' %
                  (lineno + 1, code))

    def labelCheck(self) -> None:
        try:
            for i, code in enumerate(self.rawcodes):
                strs = code.split()
                if strs[0] == 'GOTO':
                    if strs[1] not in self.labelTable:
                        raise UndefinedLabelError
                elif strs[0] == 'IF':
                    if strs[5] not in self.labelTable:
                        raise UndefinedLabelError
                elif len(strs) > 2 and strs[2] == 'CALL':
                    if strs[3] not in self.labelTable:
                        raise UndefinedLabelError
        except UndefinedLabelError:
            error('Undefined label at line %d:\n\n%s' % (i + 1, code))

    def tableInsert(self, var: str, size: int = 4, array: bool = False) -> None:
        if var.isdigit():
            raise IRSyntaxError
        if var[0] == '&' or var[0] == '*':
            var = var[1:]
        elif var[0] == '#':
            test = int(var[1:])
            return
        if var in self.symTable:
            return
        self.functionDict[self.currentFunction].append(var)
        if self.currentFunction == 'main':
            self.symTable[var] = (self.offset, size, array)
            self.offset += size
        else:
            self.symTable[var] = (-1, size, array)

    def getValue(self, var):
        if var[0] == '#':
            return int(var[1:])
        else:
            if var[0] == '&':
                return self.symTable[var[1:]][0]
            if var[0] == '*':
                return self.mem[(self.mem[(self.symTable[var[1:]][0] // 4)] // 4)]
            return self.mem[(self.symTable[var][0] // 4)]

    def executeCode(self, code):
        self.instrCnt += 1
        try:
            if code[0] == 'READ':
                if self.input:
                    self.mem[self.symTable[code[1]][0] // 4] = self.input[0]
                    self.input.pop(0)
                else:
                    error("Input exhausted but still asked for another input")
            elif code[0] == 'WRITE':
                self.console.append(str(self.getValue(code[1])))
            elif code[0] == 'GOTO':
                self.ip = self.labelTable[code[1]]
            elif code[0] == 'IF':
                value1 = self.getValue(code[1])
                value2 = self.getValue(code[3])
                if eval(str(value1) + code[2] + str(value2)):
                    self.ip = self.labelTable[code[4]]
            elif code[0] == 'MOV':
                value = self.getValue(code[2])
                if code[1][0] == '*':
                    self.mem[self.mem[(
                        self.symTable[code[1][1:]][0] // 4)] // 4] = value
                else:
                    self.mem[self.symTable[code[1]][0] // 4] = value
            elif code[0] == 'ARITH':
                value1 = self.getValue(code[2])
                value2 = self.getValue(code[4])
                self.mem[self.symTable[code[1]][0] //
                         4] = eval(str(value1) + code[3] + str(value2))
            elif code[0] == 'RETURN':
                if len(self.callStack) == 0:
                    return 1
                returnValue = self.getValue(code[1])
                stackItem = self.callStack.pop()
                self.ip = stackItem[0]
                for key in stackItem[2].keys():
                    self.symTable[key] = stackItem[2][key]

                self.offset = stackItem[3]
                self.mem[self.symTable[stackItem[1]][0] // 4] = returnValue
            elif code[0] == 'CALL':
                oldAddrs = dict()
                oldOffset = self.offset
                for key in self.functionDict[code[2]]:
                    oldAddrs[key] = self.symTable[key]
                    self.symTable[key] = (self.getNewAddr(
                        self.symTable[key][1]), self.symTable[key][1], self.symTable[key][2])

                self.callStack.append((self.ip, code[1], oldAddrs, oldOffset))
                self.ip = self.labelTable[code[2]]
            elif code[0] == 'ARG':
                self.argumentStack.append(self.getValue(code[1]))
            elif code[0] == 'PARAM':
                self.mem[self.symTable[code[1]][0] //
                         4] = self.argumentStack.pop()
        except IndexError:
            return 2

        return 0

    def getNewAddr(self, size):
        ret = self.offset
        self.offset = self.offset + size
        return ret


if __name__ == '__main__':
    if len(sys.argv) == 1:
        error('Usage: python3 irsim_cli.py <input.ir> [<input integer> ...]')
    filename = sys.argv[1]
    for arg in sys.argv[2:]:
        if not arg.isdigit():
            error(f'Input should be an integer, {arg} found')
    inputList = [int(arg) for arg in sys.argv[2:]]
    irsim = IRSimCli(filename, inputList)
    irsim.run()
