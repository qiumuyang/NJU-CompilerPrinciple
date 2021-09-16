#ifndef __IR_H__
#define __IR_H__

#include <stdio.h>

#include "block.h"
#include "header.h"
#include "intercode.h"
#include "map.h"

// use this to get an array of interCode*, ends with NULL
interCode** interCodeGenerate();
void interCodeOutput(FILE* fp, interCode** codes);

#endif