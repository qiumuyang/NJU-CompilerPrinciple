#ifndef __OPTIMIZE_H__
#define __OPTIMIZE_H__

#include <assert.h>
#include <stdbool.h>

#include "block.h"
#include "intercode.h"
#include "map.h"

void optimize(root_t* funtable);

extern root_t InlineTable;

#endif