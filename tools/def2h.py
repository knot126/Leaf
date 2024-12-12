#!/usr/bin/env python3
"""
Compile LeafHook def's to headers
"""

from enum import Enum
import re
from pathlib import Path
import sys

class TokenType(Enum):
	ID = 1
	STRING = 2
	COLON = 3
	OPEN = 4
	CLOSE = 5
	NEWLINE = 6

class Token:
	def __init__(self, type, data = None):
		self.type = type
		self.data = data
	
	def getType(self):
		return self.type
	
	def getData(self):
		return self.data

class TokenStream:
	def __init__(self):
		self.tokens = []
		self.head = 0
	
	def atEnd(self):
		return self.head == len(self.tokens)
	
	def append(self, type, data = None):
		self.tokens.append(Token(type, data))
	
	def peek(self, offset = 0):
		return self.tokens[self.head + offset]
	
	def consume(self):
		tok = self.peek()
		self.head += 1
		return tok
	
	def expect(self, type):
		tok = self.consume()
		if tok.getType() != type:
			raise Exception(f"Expected token of type {type}, got {tok.getType()}")
		return tok

class BitClass:
	LITERAL = 1
	PARAM = 2

class Bits:
	def __init__(self, type = BitClass.LITERAL, *, name = None, size = None, data = None, offset = 0):
		if type == BitClass.LITERAL:
			self.type = BitClass.LITERAL
			self.data = data
		else:
			self.type = BitClass.PARAM
			self.name = name
			self.size = size
			self.offset = offset
	
	def getType(self):
		return self.type
	
	def getSize(self):
		if self.type == BitClass.LITERAL:
			return len(self.data)
		else:
			return self.size
	
	def getSymbolic(self, shift):
		if self.type == BitClass.LITERAL:
			return f"(0b{self.data} << {shift})"
		else:
			return f"((({self.name}{f' >> {self.offset}' if self.offset != 0 else ''}) & {hex((1 << self.size) - 1)}) << {shift})"
	
	def getDecodeExpr(self, input_name, shift):
		return f"((({input_name} >> {shift}) & {hex((1 << self.getSize()) - 1)}) << {self.offset})"
	
	def getMask(self, shift):
		return hex(((1 << self.getSize()) - 1) << shift)
	
	def __repr__(self):
		return f"0b{self.data}" if self.type == BitClass.LITERAL else f"[{self.name}:{self.size}:{self.offset}]"

class Instr:
	def __init__(self, name):
		self.name = name
		self.bits = []
	
	def append(self, b):
		self.bits.append(b)
	
	def getVars(self):
		v = []
		
		for b in self.bits:
			if b.getType() == BitClass.PARAM:
				if b.name not in v:
					v.append(b.name)
		
		return v
	
	def getMakeDefine(self):
		s = f"#define MAKE_{self.name.upper()}({', '.join(self.getVars())}) "
		
		l = []
		p = 0
		
		for b in reversed(self.bits):
			l.append(b.getSymbolic(p))
			p += b.getSize()
		
		s += "(" + " | ".join(l) + ")"
		
		return s
	
	def getOffsetFor(self, bit):
		l = 0
		
		for b in self.bits:
			l += b.getSize()
			
			if b is bit:
				break
		
		return 32 - l
	
	def getDecodeDefines(self):
		bits_by_var = {}
		
		for b in self.bits:
			if b.getType() == BitClass.PARAM:
				if b.name in bits_by_var:
					bits_by_var[b.name].append(b)
				else:
					bits_by_var[b.name] = [b]
		
		s = ""
		
		for name, bits in bits_by_var.items():
			s += f"#define {self.name.upper()}_DECODE_{name.upper()}(input) "
			l = []
			
			for b in bits:
				l.append(b.getDecodeExpr('input', self.getOffsetFor(b)))
			
			s += "(" + " | ".join(l) + ")\n"
		
		return s
	
	def getIsDefine(self):
		p = 0
		mask = 0
		val = 0
		
		for b in reversed(self.bits):
			if b.getType() == BitClass.LITERAL:
				mask |= eval(b.getMask(p))
				val |= (eval("0b" + b.data) << p)
			
			p += b.getSize()
		
		return f"#define IS_{self.name.upper()}(input) ((input & {hex(mask)}) == {hex(val)})"
	
	def __repr__(self):
		return f"[Instr: {' '.join([repr(x) for x in self.bits])}]"

def tokenise(string):
	tokens = TokenStream()
	
	while True:
		if len(string) == 0:
			break
		
		# print(repr(string))
		
		if string[0] in " \r\t":
			string = string[1:]
			continue
		
		if string[0] == "\n":
			tokens.append(TokenType.NEWLINE)
			string = string[1:]
			continue
		
		if string[0] == "<":
			tokens.append(TokenType.OPEN)
			string = string[1:]
			continue
		
		if string[0] == ">":
			tokens.append(TokenType.CLOSE)
			string = string[1:]
			continue
		
		if string[0] == ":":
			tokens.append(TokenType.COLON)
			string = string[1:]
			continue
		
		m = re.match(r"[0-9]+", string)
		
		if m:
			data = m.group(0)
			tokens.append(TokenType.STRING, data)
			string = string[len(data):]
			continue
		
		m = re.match(r"[a-zA-Z_][a-zA-Z0-9_]*", string)
		
		if m:
			data = m.group(0)
			tokens.append(TokenType.ID, data)
			string = string[len(data):]
			continue
		
		m = re.match(r"#.*", string)
		
		if m:
			string = string[len(m.group(0)):]
			continue
		
		raise Exception(f"Unknown character '{string[0]}'")
	
	return tokens

def parse(tokens):
	items = []
	
	while not tokens.atEnd():
		tok = tokens.consume()
		
		if tok.getType() == TokenType.ID:
			name = tok.getData()
			ins = Instr(name)
			
			while not tokens.atEnd():
				tok = tokens.consume()
				
				if tok.getType() == TokenType.STRING:
					ins.append(Bits(BitClass.LITERAL, data=tok.getData()))
				if tok.getType() == TokenType.OPEN:
					param_name = tokens.expect(TokenType.ID).getData()
					tokens.expect(TokenType.COLON)
					param_size = int(tokens.expect(TokenType.STRING).getData())
					param_shift = 0
					
					if tokens.peek().getType() == TokenType.COLON:
						tokens.consume()
						param_shift = int(tokens.expect(TokenType.STRING).getData())
					
					ins.append(Bits(BitClass.PARAM, name=param_name, size=param_size, offset=param_shift))
					
					tokens.expect(TokenType.CLOSE)
				elif tok.getType() == TokenType.NEWLINE:
					break
			
			items.append(ins)
		if tok.getType() == TokenType.NEWLINE:
			continue
		else:
			raise Exception(f"Encountered unsupported token type trying to start an entry")
	
	return items

def main():
	ins = parse(tokenise(Path(sys.argv[1]).read_text()))
	
	print("//", ins)
	
	for x in ins:
		print(x.getMakeDefine())
		print(x.getDecodeDefines())
		print(x.getIsDefine())

if __name__ == "__main__":
	main()
